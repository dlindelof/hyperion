#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "logger.h"
#include "lzss.h"

typedef struct {
  LogWriter writer;
  LogSeverity severity;
  bool is_encoded;
} LogWriterInfo;

typedef struct {
  LogEntry *entries;
  size_t count;
} LogEntryGroup;

/*
 * use logger_log when a log entry is frequently used in NiQ.
 * use logger_printf for temporary logging or entries that are
 * not logger frequently.
 *
 * each log entry must be less than LOG_LINE_SIZE or otherwise
 * it gets truncated.
 * logger uses some special characters that if are used in a log entry
 * will get replaced: '|', '\n', and '\0' will be replaced by
 * '!', '\r', '0' respectively.
 *
 * a maximum of MAX_LOG_WRITERS log writers and MAX_LOG_ENTRIES log
 * entries are supported by the logger.
 *
 * to add a new log entry add it to 'logger.def' file.
 */

/*
 * supported formatting is a subset of standard c printf:
 * %[flag][width][.precision]specifier
 *
 * flag: - + space # 0
 * width: [1-9][0-9]
 * precision: [1-9][0-9]
 * specifiers: d i u x X f F c s p %
 *
 */

#define MAX_LOG_WRITERS ((int) 4)
#define MAX_LOG_ENTRIES ((int) 6)
#define MAX_LOG_FORMATTING_SIZE ((int) 12) // %[flag][flag][flag][flag][digit][digit].[digit][digit][specifier]\0

#define LOG_LINE_SIZE ((int) 128)


static const int ERROR_BUFFER_OVERFLOW = -1;
static const int ERROR_FORMATTING      = -2;
static const int ERROR_DECODING        = -3;


static LogEntryGroup log_entries[MAX_LOG_ENTRIES];
static unsigned int log_entries_count = 0;

static LogWriterInfo log_writers[MAX_LOG_WRITERS];
static unsigned int log_writers_count = 0;

static LogEntry all_log_entries[] = {
#define LOG_ENTRY(_id_, _value_, _format_) { .id = _id_, .format = _format_},
#include "logger.defs"
#undef LOG_ENTRY
};

static bool initialized = false;



static inline long minimum(long a, long b) {
  return a <= b ? a : b;
}

/*
 * copy minimum(n, max) bytes from source to destination.
 * add a null character to destination if there is room for it and n is positive.
 * return the number of bytes copied excluding the null character.
 */

static long memnmcpy(char *dst, const char *src, long n, long max) {
  long l = minimum(n, max);
  n = l;

  while(l > 0) {
    *dst ++ = *src ++;
    -- l;
  }
  if(max > n && n > 0) {
    *dst = 0;
  }

  return n >= 0 ? n : 0;
}



bool logger_register_log_entries_helper(LogEntry *entries, size_t count) {
  if(log_entries_count < MAX_LOG_ENTRIES) {
    log_entries[log_entries_count].entries = entries;
    log_entries[log_entries_count].count = count;
    log_entries_count ++;
    return true;
  }
  return false;
}

static void logger_initialize_all_log_entries(void) {
  if(!initialized) {
    const unsigned long L_MAX_POSSIBLE_LOG_ENTRIES = 65536;
    unsigned long i;
    for(i = 0; i < L_MAX_POSSIBLE_LOG_ENTRIES; i ++) {
      if(all_log_entries[i].id == LOGGER_INVALID_ID && i >= 1) {
        logger_register_log_entries_helper(all_log_entries, i - 1);
        break;
      }
    }
    initialized = true;
  }
}

static LogEntry* logger_find_log_entry_in_entries(uint16_t id, LogEntryGroup *log_entries) {
  size_t i;
  for(i = 0; i < log_entries->count; i ++) {
    if(id == log_entries->entries[i].id) {
      return &log_entries->entries[i];
    }
  }
  return NULL;
}

static LogEntry* logger_find_log_entry(uint16_t id) {
  LogEntryGroup *entries;
  LogEntry *result;
  size_t i;

  for(i = 0; i < log_entries_count; i ++) {
    entries = &log_entries[i];
    result = logger_find_log_entry_in_entries(id, entries);
    if(result != NULL) {
      return result;
    }
  }
  return NULL;
}



static bool logger_is_formatting_character_flag_or_digit(char c) {
  return c == '+' || c == '-' || c == ' ' || c == '#' // flags
      || c == '.' || ('0' <= c && c <= '9'); // numbers
}

/*
 * update format to point to the start of the first formatting specifier.
 * return length of the specifier if found or zero otherwise.
 */

static long logger_find_next_specifier(char **format) {
  char c;
  char *pos;
  uint16_t len;
  const char *specifiers = "diuxXfFcsp";

  pos = strchr(*format, '%');

  while(pos != NULL) {
    len = 1;
    c = pos[1];

    if(c == '%') { // skip "%%"
      pos = strchr(pos + 2, '%');
      continue;
    }

    while(logger_is_formatting_character_flag_or_digit(c)) {
      c = pos[++ len];
    }

    if(strchr(specifiers, c)) {
      *format = pos;
      return ++len;
    }

    return ERROR_FORMATTING;
  }

  *format = NULL;
  return 0;
}

static void logger_replace_special_characters(char *buffer, int length) {
  int i;
  for(i = 0; i < length; i ++) {
    if(buffer[i] == '|' ) buffer[i] = '!' ;
    if(buffer[i] == '\n') buffer[i] = '\r';
    if(buffer[i] == '\0') buffer[i] = '0' ;
  }
}

static int logger_snvprintf_id(char *buffer, int length, bool encode, uint16_t id) {
  int len;

  if(encode) {
    len = snprintf(buffer, length, "%04X|", id);
  } else {
    len = snprintf(buffer, length, "[0x%04X] ", id);
  }

  return len <= length ? len : ERROR_BUFFER_OVERFLOW;
}

static int logger_sprintf_parameter_separator(char *buffer, int length) {
  if(length < 1) {
    return ERROR_BUFFER_OVERFLOW;
  }
  buffer[0] = '|';
  return 1;
}

static int logger_snvprintf_parameters_encoded(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;

  int slen, len;

  while((slen = logger_find_next_specifier(&fmt)) > 0) {
    char formatting[MAX_LOG_FORMATTING_SIZE];
    if(slen >= MAX_LOG_FORMATTING_SIZE) {
      return ERROR_FORMATTING;
    }
    strncpy(formatting, fmt, slen);
    formatting[slen] = 0;
    fmt += slen;

    char specifier = formatting[slen - 1]; // get the specifier

    switch(specifier) {
      case 'c': {
        char c = (char) va_arg(params, int32_t);
        len = snprintf(buffer, length, formatting, c);
        logger_replace_special_characters(buffer, minimum(len, length));
        break;
      }
      case 'd': case 'i': {
        int32_t i = va_arg(params, int32_t);
        len = snprintf(buffer, length, formatting, i);
        break;
      }
      case 'u': case 'x': case 'X': {
        uint32_t u = va_arg(params, uint32_t);
        len = snprintf(buffer, length, formatting, u);
        break;
      }
      case 'f': case 'F': {
        float f = (float) va_arg(params, double);
        len = snprintf(buffer, length, formatting, f);
        break;
      }
      case 'p': {
        void *p = va_arg(params, void*);
        len = snprintf(buffer, length, formatting, p);
        break;
      }
      case 's': {
        char *s = va_arg(params, char *);
        len = snprintf(buffer, length, formatting, s);
        logger_replace_special_characters(buffer, minimum(len, length));
        break;
      }
      default:
        assert(0);
        break;
    }
    if(len < 0) {
      return ERROR_FORMATTING;
    } else if(len > length) {
      return ERROR_BUFFER_OVERFLOW;
    }
    buffer += len;
    length -= len;

    len = logger_sprintf_parameter_separator(buffer, length);
    if(len < 0) {
      return len;
    }
    buffer += len;
    length -= len;
  }
  if(slen < 0) { // formatting error
    return ERROR_FORMATTING;
  }

  return buffer - buf;
}

static int logger_snvprintf_parameters(char *buffer, int length, const char *format, va_list params) {
  int len = vsnprintf(buffer, length, format, params);
  logger_replace_special_characters(buffer, len);

  if(len < 0) {
    return ERROR_FORMATTING;
  } else if(len > length) {
    return ERROR_BUFFER_OVERFLOW;
  }
  return len;
}

static int logger_snvprintf_parameters_encoded_with_printf(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;

  int len = logger_snvprintf_parameters(buffer, length, format, params);

  if(len < 0) {
    return len;
  }
  buffer += len;
  length -= len;

  len = logger_sprintf_parameter_separator(buffer, length);
  if(len < 0) {
    return len;
  }
  buffer += len;
  length -= len;

  return buffer - buf;
}

static int logger_snvprintf_entry(char *buffer, int length, uint16_t id, bool encode, bool is_printf, const char *format, va_list params) {
  char *buf = buffer;
  int len;

  len = logger_snvprintf_id(buffer, length, encode, id);

  if(len < 0) {
    return len;
  }
  buffer += len;
  length -= len;

  if(encode && is_printf) {
    len = logger_snvprintf_parameters_encoded_with_printf(buffer, length, format, params);
  } else if(encode) {
    len = logger_snvprintf_parameters_encoded(buffer, length, format, params);
  } else {
    len = logger_snvprintf_parameters(buffer, length, format, params);
  }

  if(len < 0) {
    return len;
  }

  buffer += len;
  length -= len;

  if(length < 1) {
    return ERROR_BUFFER_OVERFLOW;
  }
  buffer[0] = '\n';
  buffer ++;
  length --;

  return buffer - buf;
}

static void logger_log_helper(LogSeverity severity, bool is_printf, uint16_t id, const char *format, va_list params) {
  size_t i;
  char buffer[LOG_LINE_SIZE] = {0};
  char *fmt;
  int len;

  if(is_printf) {
    fmt = (char *) format;
  } else { // has a registered id
    LogEntry *entry = logger_find_log_entry(id);
    if(entry == NULL) {
      return;
    }
    fmt = (char *) entry->format;
  }

  for(i = 0; i < log_writers_count; i ++) {
    LogWriterInfo *writer = &log_writers[i];

    if(severity >= writer->severity) {
      va_list params_copy;
      va_copy(params_copy, params);

      len = logger_snvprintf_entry(buffer, LOG_LINE_SIZE, id, writer->is_encoded, is_printf, fmt, params_copy);

      if(len == ERROR_BUFFER_OVERFLOW) {
        const char *msg = ".. truncated ..|\n";
        const int n = strlen(msg);
        memnmcpy(buffer + LOG_LINE_SIZE - n, msg, n, n);
        len = LOG_LINE_SIZE;
      } else if(len == ERROR_FORMATTING) {
        len = logger_snvprintf_id(buffer, LOG_LINE_SIZE, writer->is_encoded, id);
        const char *msg = "this log entry has formatting errors|\n";
        const int n = strlen(msg) + 1; // include the null character
        memnmcpy(buffer + len, msg, n, len);
        len = strlen(buffer);
      }

      writer->writer((uint8_t *) buffer, len);

      va_end(params_copy);
    }
  }

}



/*
 * assume there are no errors in the input.
 * count until next parameter separator and return the count (excluding the separator itself).
 */
static size_t logger_decoder_get_length_of_next_parameter(const char *entry, size_t entry_length) {
  size_t len;
  for(len = 0; len < entry_length; len ++) {
    if(entry[len] == '|') {
      return len + 1;
    }
  }
  return 0;
}

/*
 * assume there are no errors in the entry's beginning and end.
 * decode the entry and write to destination as long as it has space.
 * return the number of characters that are written to the destination or would have been written if it was large enough.
 */
static int logger_decoder_decode_entry_helper(char *dst, int d_len, uint16_t id, const char *format, const char *entry, int entry_length) {
  char *fmt = (char *) format;
  const long d_length = d_len; // number of characters that are copied if destination was large enough
  int specifier_len, formatting_len, parameter_len;

  int len = snprintf(dst, d_len, "[0x%04X] ", id);

  dst += len;
  d_len -= len;
  entry += 5;
  entry_length -= 5;

  char *previous_fmt = fmt;

  while((specifier_len = logger_find_next_specifier(&fmt)) > 0) {
    formatting_len = fmt - previous_fmt;
    memnmcpy(dst, previous_fmt, formatting_len, d_len);
    fmt += specifier_len;
    previous_fmt = fmt;
    dst += formatting_len;
    d_len -= formatting_len;

    parameter_len = logger_decoder_get_length_of_next_parameter(entry, entry_length);

    if(parameter_len == 0) {
      break;
    }

    memnmcpy(dst, entry, parameter_len - 1, d_len); // do not copy '|'

    dst += parameter_len - 1; // do not copy '|'
    d_len -= parameter_len - 1; // do not copy '|'
    entry += parameter_len;
    entry_length -= parameter_len;
  }

  // copy the remaining of the formatting string
  int n = strlen(previous_fmt);
  memnmcpy(dst, previous_fmt, n, d_len);
  dst += n;
  d_len -= n;
  // copy the remaining of the entry including the '\n'
  memnmcpy(dst, entry, entry_length, d_len);
  d_len -= entry_length;

  return d_length - d_len;
}

static uint16_t logger_decoder_get_id(const char *entry) {
  char id[5];
  memnmcpy(id, entry, 4, 5);
  return (uint16_t) strtol(id, NULL, 16); // returns zero there are invalid characters in source
}

/*
 * a valid entry starts with XXXX| and ends with |\n.
 * these are examples of valid entries:
 * 8023|\n
 * 8026|20.89|\n
 * 8029|48010||1258.05|\n
 * 8037|string with numbers 1554.71|2.57|\n
 */
static long logger_decoder_is_entry_decodable(const char *entry, size_t entry_length) {
  return entry_length >= 6 &&
    entry[4] == '|' && // a valid entry starts with XXXX|
    entry[entry_length - 2] == '|' && entry[entry_length - 1] == '\n'; // and must have '|\n' at the end
}

static long logger_decoder_decode_entry(char *dst, size_t d_len, const char *entry, size_t entry_length) {
  if(logger_decoder_is_entry_decodable(entry, entry_length)) {
    uint16_t id = logger_decoder_get_id(entry);

    LogEntry *le = logger_find_log_entry(id);

    if(le != NULL) {
      const char *format = le->format;
      return logger_decoder_decode_entry_helper(dst, d_len, id, format, entry, entry_length);
    } else {
      const char *format = "%s";
      return logger_decoder_decode_entry_helper(dst, d_len, id, format, entry, entry_length);
    }
  }
  return ERROR_DECODING;
}

static size_t logger_decoder_get_length_of_next_entry(const char *entry, size_t max) {
  size_t i;
  for(i = 0; i < max; i ++, entry ++) {
    if(*entry == '\n') {
      return i + 1;
    }
  }
  return 0;
}



void logger_initialize(void) {
  if(!initialized) {
    logger_initialize_all_log_entries();
    initialized = true;
  }
}

bool logger_register_log_entries(LogEntry *entries, size_t count) {
  return logger_register_log_entries_helper(entries, count);
}

bool logger_register_log_writer(LogWriter writer, LogSeverity severity, bool encode) {
  unsigned int i;
  for(i = 0; i < log_writers_count; i ++) { // do not register duplicates
    if(log_writers[i].writer == writer
        && log_writers[i].severity == severity
        && log_writers[i].is_encoded == encode) {
      return false;
    }
  }
  if(log_writers_count < MAX_LOG_WRITERS) {
    log_writers[log_writers_count].writer = writer;
    log_writers[log_writers_count].severity = severity;
    log_writers[log_writers_count].is_encoded = encode;
    log_writers_count ++;
    return true;
  }
  return false;
}



void logger_log(uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  logger_log_helper(SEVERITY_INFO, false, id, "", varargs);

  va_end(varargs);
}

void logger_severity_log(LogSeverity severity, uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  logger_log_helper(severity, false, id, "", varargs);

  va_end(varargs);
}

/*
 * Use this function to write log entries that are infrequent or only for debugging purpose.
 * In the production code and for log entries that are frequent consider using logger_log.
 */
void logger_printf(const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(SEVERITY_INFO, true, 0, format, varargs);

  va_end(varargs);
}

void logger_severity_printf(LogSeverity severity, const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(severity, true, 0, format, varargs);

  va_end(varargs);
}

/*
 * decode from source to destination as long as it has empty space.
 * if decoding fails copy the source directly to the input.
 * update s_unused_bytes to show the number of unused bytes in the source.
 * the destination buffer must be large enough to at least hold one decoded entry,
 * otherwise no decoding will happen.
 * return the number of characters that are written to the destination.
 */
size_t logger_decode(char *dst, size_t d_len, const char *src, size_t s_len, size_t *s_unused_bytes) {
  const size_t original_d_len = d_len;
  char *entry = (char *) src;
  size_t entry_length;

  while((entry_length = logger_decoder_get_length_of_next_entry(entry, s_len)) > 0) {
    int len = logger_decoder_decode_entry(dst, d_len, entry, entry_length);

    if(len == ERROR_DECODING) { // decoding failed
      if(entry_length > d_len) {
        break;
      }
      // copy the failed entry to the output as it is
      memnmcpy(dst, entry, entry_length, d_len);
      dst += entry_length;
      d_len -= entry_length;
    } else {
      if((size_t)len > d_len) {
        break;
      }
      dst += len;
      d_len -= len;
    }
    entry += entry_length;
    s_len -= entry_length;
  }

  *s_unused_bytes = s_len;
  return original_d_len - d_len;
}

size_t logger_get_max_buffer_size() {
  return LOG_LINE_SIZE;
}


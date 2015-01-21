#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "logger.h"
#include "lzss.h"

typedef struct {
  LogWriter writer;
  LogSeverity severity;
  int is_compressed;
} LogWriterInfo;

typedef struct {
  LogEntry *entries;
  int count;
} LogEntryGroup;

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
#define MAX_LOG_LINE_SIZE ((int) 128)
#define MAX_LOG_FORMATTING_SIZE ((int) 12) // %[flag][flag][flag][flag][digit][digit].[digit][digit][specifier]\0


static LogEntryGroup log_entries[MAX_LOG_ENTRIES];
static int log_entries_count = 0;

static LogWriterInfo log_writers[MAX_LOG_WRITERS];
static int log_writers_count = 0;


static const int ERROR_BUFFER_OVERFLOW = -1;
static const int ERROR_FORMATTING      = -2;
static const int ERROR_DECODING        = -3;


static LogEntry* logger_find_log_entry_in_entries(uint16_t id, LogEntryGroup *log_entries) {
  int i;

  for(i = 0; i < log_entries->count; i ++) {
    if(id == log_entries->entries[i].id) {
      return &log_entries->entries[i];
    }
  }
  return NULL;
}

static LogEntry* logger_find_log_entry(uint16_t id) {
  int i;
  LogEntryGroup *entries;
  LogEntry *result;

  for(i = 0; i < log_entries_count; i ++) {
    entries = &log_entries[i];
    result = logger_find_log_entry_in_entries(id, entries);
    if(result != NULL) {
      return result;
    }
  }
  return NULL;
}




static int logger_is_formatting_character_flag_or_digit(char c) {
  return c == '+' || c == '-' || c == ' ' || c == '#' // flags
      || c == '.' || ('0' <= c && c <= '9'); // numbers
}

/*
 * update format to point to the start of the first formatting specifier.
 * return length of the specifier if found or zero otherwise.
 */

static int logger_find_next_specifier(char **format) {
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
  for(int i = 0; i < length; i ++) {
    if(buffer[i] == '|' ) buffer[i] = '!' ;
    if(buffer[i] == '\n') buffer[i] = '\r';
    if(buffer[i] == '\0') buffer[i] = '0' ;
  }
}

static int logger_snvprintf_id(char *buffer, int length, int compressed, int id) {
  int len;

  if(compressed) {
    if(length < 6) {
      return ERROR_BUFFER_OVERFLOW;
    }
    len = snprintf(buffer, 6, "%04X|", id);
  } else {
    if(length < 10) {
      return ERROR_BUFFER_OVERFLOW;
    }
    len = snprintf(buffer, 10, "[0X%04X] ", id);
  }

  return len;// < 0 ? ERROR_FORMATTING : len > length ? ERROR_BUFFER_OVERFLOW : len;
}

static int logger_sprintf_parameter_separator(char *buffer, int length) {
  if(length < 1) {
    return ERROR_BUFFER_OVERFLOW;
  }
  buffer[0] = '|';
  return 1;
}

static int logger_snvprintf_parameters_compressed_and_encoded(char *buffer, int length, const char *format, va_list params) {
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

static int logger_snvprintf_parameters_compressed(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;

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

static int logger_snvprintf_entry(char *buffer, int length, uint16_t id, int compressed, int encode, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;
  int len;

  len = logger_snvprintf_id(buffer, length, compressed, id);

  if(len < 0) {
    return len;
  }
  buffer += len;
  length -= len;

  if(compressed && encode) {
    len = logger_snvprintf_parameters_compressed_and_encoded(buffer, length, format, params);
  } else if(compressed) {
    len = logger_snvprintf_parameters_compressed(buffer, length, format, params);
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

static void logger_log_helper(LogSeverity severity, int has_registered_id, uint16_t id, const char *format, va_list params) {
  int i;
  char buffer[MAX_LOG_LINE_SIZE];
  int encode = 1;
  char *fmt;
  int len;

  if(has_registered_id) {
    LogEntry *entry = logger_find_log_entry(id);
    assert(entry != NULL);
    fmt = (char *) entry->message;
  } else {
    fmt = (char *) format;
    encode = 0;
  }

  for(i = 0; i < log_writers_count; i ++) {
    LogWriterInfo *writer = &log_writers[i];

    if(severity >= writer->severity) {
      va_list params_copy;
      va_copy(params_copy, params);

      len = logger_snvprintf_entry(buffer, MAX_LOG_LINE_SIZE, id, writer->is_compressed, encode, fmt, params_copy);

      assert(len > 0);

      //compress_before_writing();

      if(len > 0) {
        writer->writer(buffer, len);
      }

      va_end(params_copy);
    }
  }

}



/*
 * Decoding functions
 */

/*
 * assume there are no errors in the input.
 * count until next parameter separator and return the count (excluding the separator itself).
 */
static int logger_decoder_get_length_of_next_parameter(const char *entry, int entry_length) { // return the length of the next parameter in the entry or ERROR_DECODING if fails
  int len;
  for(len = 0; len < entry_length; len ++) {
    if(entry[len] == '|') {
      break;
    }
  }
  return len;
}

/*
 * assume there are no errors in the input entry.
 * decode the entry and write to destination as long as it has space.
 * return the number of characters that are written to the destination or would have been written if it was large enough.
 *
 *
 */
static int logger_decoder_decode_entry_helper(char *dst, int d_len, uint16_t id, const char *format, const char *entry, int entry_length) {
  char *fmt = (char *) format;
  const int d_length = d_len; // number of characters that are copied if destination was large enough
  int specifier_len, formatting_len, parameter_len;

  int len = snprintf(dst, d_len, "[%0#6X] ", id);

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
    d_len -= formatting_len;

    parameter_len = logger_decoder_get_length_of_next_parameter(entry, entry_length);

    memnmcpy(dst, entry, parameter_len, d_len);
    d_len -= parameter_len;

    dst += formatting_len + parameter_len;
    entry += parameter_len + 1; // skip '|'
    entry_length -= parameter_len + 1; // skip '|'
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
static int logger_decoder_is_entry_decodable(const char *entry, const int entry_length) {
  return entry_length >= 6 &&
    entry[4] == '|' && // a valid entry starts with XXXX|
    entry[entry_length - 2] == '|' && entry[entry_length - 1] == '\n'; // and must have '|\n' at the end
}

static int logger_decoder_decode_entry(char *dst, int d_len, const char *entry, int entry_length) {
  if(logger_decoder_is_entry_decodable(entry, entry_length)) {
    uint16_t id = logger_decoder_get_id(entry);

    LogEntry *le = logger_find_log_entry(id);

    if(le != NULL) {
      const char *format = le->message;
      return logger_decoder_decode_entry_helper(dst, d_len, id, format, entry, entry_length);
    }
  }
  return ERROR_DECODING;
}

static int logger_decoder_get_length_of_next_entry(const char *entry, int max) {
  for(int i = 0; i < max; i ++, entry ++) {
    if(*entry == '\n') {
      return i + 1;
    }
  }
  return 0;
}



/*
 * public functions
 */

void logger_register_log_entries(LogEntry *entries, int count) {
  if(log_entries_count < MAX_LOG_ENTRIES) {
    log_entries[log_entries_count].entries = entries;
    log_entries[log_entries_count].count = count;
    log_entries_count ++;
  }
}

void logger_register_log_writer(LogWriter writer, LogSeverity severity, int is_compressed) {
  if(log_writers_count < MAX_LOG_WRITERS) {
    log_writers[log_writers_count].writer = writer;
    log_writers[log_writers_count].severity = severity;
    log_writers[log_writers_count].is_compressed = is_compressed;

    log_writers_count ++;
  }
}



void logger_log(uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  logger_log_helper(SEVERITY_NORMAL, 1, id, "", varargs);

  va_end(varargs);
}

void logger_severity_log(LogSeverity severity, uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  logger_log_helper(severity, 1, id, "", varargs);

  va_end(varargs);
}

/*
 * Use this function to write log entries that are infrequent or only for debugging purpose.
 * In the production code and for log entries that are frequent consider using logger_log.
 */
void logger_printf(const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(SEVERITY_NORMAL, 0, 0, format, varargs);

  va_end(varargs);
}

void logger_severity_printf(LogSeverity severity, const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(severity, 0, 0, format, varargs);

  va_end(varargs);
}

void logger_printf_with_id(uint16_t id, const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(SEVERITY_NORMAL, 0, id, format, varargs);

  va_end(varargs);
}

void logger_severity_printf_with_id(LogSeverity severity, uint16_t id, const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(severity, 0, id, format, varargs);

  va_end(varargs);
}

static void logger_assert_no_duplicate_entries() {
}

/*
 * decode from source to destination as long as it has empty space.
 * if decoding fails copy the source directly to the input.
 * update s_len to show the number of unused bytes in the source.
 * return the number of characters that are written to the destination or would have been written if it was large enough.
 */
int logger_decode(char *dst, int d_len, const char *src, int *s_len) {
  const int d_length = d_len; // number of characters that are copied if destination was large enough
  char *entry = (char *) src;
  int entry_length;
  int remaining_length = *s_len;

  while((entry_length = logger_decoder_get_length_of_next_entry(entry, remaining_length)) > 0) {
    int len = logger_decoder_decode_entry(dst, d_len, entry, entry_length);

    if(len == ERROR_DECODING) { // decoding failed
      // copy the failed entry to the output
      memnmcpy(dst, entry, entry_length, d_len);
      dst += entry_length;
      d_len -= entry_length;
    }
    dst += len;
    d_len -= len;
    entry += entry_length;
    remaining_length -= entry_length;
  }

  *s_len = remaining_length;
  return d_length - d_len;
}














/*
 * delete-me
 */
inline int minimum(int a, int b) {
  return a <= b ? a : b;
}

/*
 * copy minimum(n, max) bytes from source to destination.
 * add a null character to destination if there is room for it and n is positive.
 * return the number of bytes copied excluding the null character.
 */

int memnmcpy(char *dst, const char *src, int n, int max) {
  int l = minimum(n, max);
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


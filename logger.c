#include "logger.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  LogWriter writer;
  int is_compressed;
  log_severity_t severity;
} LogWriterEntry;

typedef struct {
  LogEntry *entries;
  int count;
} LogEntries;


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


static LogEntries log_entries[MAX_LOG_ENTRIES];
static int log_entries_count = 0;

static LogWriterEntry log_writers[MAX_LOG_WRITERS];
static int log_writers_count = 0;



static LogEntry* logger_find_log_entry_in_entries(uint16_t id, LogEntries *log_entries) {
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
  LogEntries *entries;
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



void logger_register_log_entries(LogEntry *entries, int count) {
  assert(log_entries_count < MAX_LOG_ENTRIES);
  log_entries[log_entries_count].entries = entries;
  log_entries[log_entries_count].count = count;
  log_entries_count ++;
}

void logger_register_log_writer(LogWriter writer, int is_compressed, log_severity_t severity) {
  assert(log_writers_count < MAX_LOG_WRITERS);
  log_writers[log_writers_count].writer = writer;
  log_writers[log_writers_count].is_compressed = is_compressed;
  log_writers[log_writers_count].severity = severity;
  log_writers_count ++;
}



static int logger_formatting_character_is_flag_or_digit(char c) {
  return c == '+' || c == '-' || c == ' ' || c == '#' // flags
      || c == '.' || ('0' <= c && c <= '9'); // numbers
}

static int logger_get_next_specifier(char **format) { // this function updates format to point to the start of the formatting specifier
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

    while(logger_formatting_character_is_flag_or_digit(c)) {
      c = pos[++len];
    }

    if(strchr(specifiers, c)) {
      *format = pos;
      return ++len;
    }

    return -1;
  }

  *format = NULL;
  return 0;
}

static void logger_replace_special_characters(char *buffer, int length) {
  for(--length; length >= 0; length --) {
    if(buffer[length] == '|') buffer[length] = '!';
    if(buffer[length] == '\n') buffer[length] = '\r';
  }
}

static int logger_snvprintf_id(char *buffer, int length, int compressed, int id) {
  int len;

  if(compressed) {
    len = snprintf(buffer, 7, "[%04X|", id);
  } else {
    len = snprintf(buffer, 10, "[%0#6X] ", id);
  }
  if(len < 0 || len > length) {
    return -1;
  }

  return len;
}

static int logger_snvprintf_parameters_compressed_and_encoded(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;

  int slen, len;

  while((slen = logger_get_next_specifier(&fmt)) > 0) {
    char formatting[MAX_LOG_FORMATTING_SIZE];
    if(slen >= MAX_LOG_FORMATTING_SIZE) {
      return -1;
    }
    strncpy(formatting, fmt, slen);
    formatting[slen] = 0;
    fmt += slen;

    char specifier = formatting[slen - 1]; // get the specifier
    switch(specifier) {
      case 'c': {
        char c = (char) va_arg(params, int32_t);
        len = snprintf(buffer, length, formatting, c);
        break;
      }
      case 'd':
      case 'i': {
        int32_t i = va_arg(params, int32_t);
        len = snprintf(buffer, length, formatting, i);
        break;
      }
      case 'u':
      case 'x':
      case 'X': {
        uint32_t u = va_arg(params, uint32_t);
        len = snprintf(buffer, length, formatting, u);
        break;
      }
      case 'f':
      case 'F': {
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
        break;
      }
      default:
        assert(0);
        break;
    }
    if(len < 0 || len > length) {
      return -1;
    }
    if(specifier == 's' || specifier == 'c') {
      logger_replace_special_characters(buffer, len);
    }
    buffer += len;
    length -= len;

    if(length < 1) {
      return -1;
    }
    buffer[0] = '|';
    buffer ++;
    length --;
  }
  if(slen < 0) { // formatting error
    return -1;
  }

  return buffer - buf;
}

static int logger_snvprintf_parameters(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;
  int len;

  len = vsnprintf(buffer, length, format, params);

  if(len < 0 || len > length) {
    return -1;
  }

  logger_replace_special_characters(buffer, len);

  buffer += len;
  length -= len;

  return buffer - buf;
}

static int logger_snvprintf_parameters_compressed(char *buffer, int length, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;

  int len = logger_snvprintf_parameters(buffer, length, format, params);

  if(len < 0) {
    return -1;
  }
  buffer += len;
  length -= len;

  if(length < 1) {
    return -1;
  }
  buffer[0] = '|';
  buffer ++;
  length --;

  return buffer - buf;
}

static int logger_snvprintf_entry(char *buffer, int length, uint16_t id, int compressed, int encode, const char *format, va_list params) {
  char *buf = buffer;
  char *fmt = (char *) format;
  int len;

  len = logger_snvprintf_id(buffer, length, compressed, id);

  if(len < 0) {
    return -1;
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

  if(len < 0 || len > length) {
    return -1;
  }

  buffer += len;
  length -= len;

  if(length < 1) {
    return -1;
  }
  buffer[0] = '\n';
  buffer ++;
  length --;

  return buffer - buf;
}




static int memnmcpy(char *dst, const char *src, int n, int max) {
  if(n > max) {
    return -1;
  }
  while(n > 0) {
    *dst++ = *src++;
    --n;
  }
  if(n > 0) {
    *dst = 0;
  }

  return n;
}



static uint16_t logger_decode_get_id(const char *entry) {
  char id[5];
  memnmcpy(id, &entry[1], 4, 5);
  return (uint16_t) strtol(id, NULL, 16);
}

static int logger_get_next_encoded_parameter(const char *entry, int entry_length) { // return the length of the next parameter in the entry or -2 if fails
  int len = 0;
  while(len < entry_length) {
    if(entry[len] == '|') {
      return len;
    }
    if(entry[len] == '\n') {
      return -2;
    }
    len ++;
  }

  return -2;
}

/*
 * decodes one entry from the entry. if entry has more characters than that then does not decode beyond that.
 * return the number of characters that are decoded or -2 if decoding fails or -1 if buffer is not large enough. 0?
 * [8023|
 * [8026|20.89|
 * [8029|48010||1258.05|
 * [8037|string with numbers 1554.71|2.57|
 *
 */
static int logger_decode_compressed_and_encoded_entry_helper(char *buffer, int length, const char *format, const char *entry, int entry_length) {
  char *buf = buffer;
  char *fmt = (char *) format;
  char *entry_ = (char *) entry;

  int slen, flen, plen;

  memnmcpy( buffer   , "[0x", 3, 3);
  memnmcpy(&buffer[3], &entry_[1], 4, 4);
  memnmcpy(&buffer[7], "] ", 2, 2);

  buffer += 9;
  length -= 9;
  entry_ += 6;
  entry_length -= 6;

  char *previous_fmt = fmt;

  while((slen = logger_get_next_specifier(&fmt)) > 0) {
    flen = fmt - previous_fmt;
    if(memnmcpy(buffer, previous_fmt, flen, length) != flen) {
      return -1;
    }
    fmt += slen;
    previous_fmt = fmt;

    plen = logger_get_next_encoded_parameter(entry_, entry_length);
    if(plen < 0) {
      return plen;
    }

    if(memnmcpy(buffer, entry_, plen, length) != plen) {
      return -1;
    }

    buffer += plen + 1; // skip '|'
    length -= plen + 1;
    entry_ += plen + 1;
    entry_length -= plen + 1;

    if(entry_length < 0) {
      return -2;
    }
  }
  if(entry_length < 1 || entry_[0] != '\n') {
    return -2;
  }
  entry_ ++;
  if(length < 1) {
    return -1;
  }
  buffer[0] = '\n';

  return entry_ - entry;
}

static int logger_decode_compressed_and_encoded_entry(char *buffer, int length, const char *entry, int entry_length) {

  if(entry_length < 7 || entry[0] != '[' || entry[5] != '|') { // minimum valid format is [XXXX|\n
    return -2;
  }
  if(length < 10) { // minimum output is [0xXXXX] \n
    return -1;
  }

  uint16_t id = logger_decode_get_id(entry);

  LogEntry *ent = logger_find_log_entry(id);
  assert(ent != NULL);
  const char *format = ent->message;

  return logger_decode_compressed_and_encoded_entry_helper(buffer, length, format, entry, entry_length);
}

static char *strnchr(const char *s, char c, int n) {
  while(n > 0 && *s != 0) {
    if(*s == c) {
      return (char *)s;
    }
    ++ s;
    -- n;
  }
  return 0;
}
/*
 * skips the first invalid characters
 */
int logger_decode_text(char *buffer, int length, const char *text, int text_length) {
  int len;
  char *entry = (char *) text;
  int entry_length = text_length;
  char *temp;

  while((temp = strnchr(entry, '[', entry_length))) {
    entry = temp;
    entry_length = text_length - (entry - text);

    len = logger_decode_compressed_and_encoded_entry(buffer, length, entry, entry_length);

    if(len == -1) { // buffer is not large enough, cannot do anything
      return -1;
    } else if(len == -2) { // decode failed try from the next character
      if(entry_length < 1) {
        return -2;
      }
      entry ++;
      entry_length --;
    } else {
      buffer += len;
      length -= len;
      entry += len;
      entry_length -= len;
    }
  }

  return entry - text;
}



static void logger_log_helper(log_severity_t severity, int has_id, uint16_t id, const char *format, va_list params) {
  int i;
  char buffer[MAX_LOG_LINE_SIZE];
  int encode = 1;
  char *fmt;
  int len;

  if(has_id) {
    LogEntry *entry = logger_find_log_entry(id);
    assert(entry != NULL);
    fmt = (char *) entry->message;
  } else {
    id = 0x00FF;
    fmt = (char *) format;
    encode = 0;
  }

  for(i = 0; i < log_writers_count; i ++) {
    LogWriterEntry *writer = &log_writers[i];

    if(severity >= writer->severity) {
      va_list params_copy;
      va_copy(params, params_copy);

      len = logger_snvprintf_entry(buffer, MAX_LOG_LINE_SIZE, id, writer->is_compressed, encode, fmt, params_copy);

      assert(len > 0);

      if(len > 0) {
        writer->writer((const uint8_t*) buffer, len);
      }

      va_end(params_copy);
    }
  }

}

void logger_log(uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  logger_log_helper(SEVERITY_NORMAL, 1, id, "", varargs);

  va_end(varargs);
}

void logger_severity_log(log_severity_t severity, uint16_t id, ...) {
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

void logger_severity_printf(log_severity_t severity, const char * format, ...) {
  va_list varargs;
  va_start(varargs, format);

  logger_log_helper(severity, 0, 0, format, varargs);

  va_end(varargs);
}

static void logger_assert_no_duplicate_entries() {
}

int logger_decode_entry(char *buffer, const int length, LogEntry *entry) {
  return 0;
}

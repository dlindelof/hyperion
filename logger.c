#include "logger.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  LogWriter writer;
  int is_compressed;
  int severity;
} LogWriterEntry;

typedef struct {
  LogEntry *entries;
  int count;
} LogEntries;


#define MAX_LOG_ENTRIES ((int) 6)
static LogEntries log_entries[MAX_LOG_ENTRIES];
static int log_entries_count = 0;

#define MAX_LOG_WRITERS ((int) 4)
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

static int logger_has_registered_log_writers() {
  return log_writers_count != 0;
}

static int logger_has_compressed_log_writer() {
  int i;
  for(i = 0; i < log_writers_count; i ++) {
    if(log_writers[i].is_compressed) {
      return 1;
    }
  }
  return 0;
}

static char logger_get_next_specifier(char **format) { // this function updates format
  char c;
  char *pos;
  uint16_t len;
  const char *specifiers = "diuoxXfFeEgGaAcspnTS";

  pos = strchr(*format, '%');

  while(pos != NULL) {
    c = *++pos;

    if(c == '%') { // skip "%%"
      pos = strchr(pos + 1, '%');
      continue;
    }

    while(c) {
      const char *pos_specifier = strchr(specifiers, c);
      if(pos_specifier) {
        *format = pos;
        return *pos;
      }
      c = *++pos;
    }
  }

  *format = NULL;
  return 0;
}



static int logger_memory_copy(void *to, const void *from, int size, int remaining_buffer_size) {
  if(remaining_buffer_size < size) {
    return -1;
  }
  memcpy(to, from, size);
  return size;
}

static int logger_get_required_buffer_size(LogEntry *entry, va_list params) {
  char *format = (char*)entry->message;
  char c;
  int len = 2;
  uint32_t u;
  char *s;

  while((c = logger_get_next_specifier(&format))) {
    switch(c) {
      case 'c':
        va_arg(params, int32_t);
        len ++;
        break;
      case 'd':
      case 'i':
        va_arg(params, int32_t);
        len += sizeof(int32_t);
        break;
      case 'u':
        va_arg(params, uint32_t);
        len += sizeof(uint32_t);
        break;
      case 'f':
      case 'F':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
      case 'a':
      case 'A':
        va_arg(params, double);
        len += sizeof(float);
        break;
      case 'p':
        va_arg(params, void*);
        len += sizeof(void*);
        break;
      case 's':
        s = va_arg(params, char *);
        u = strlen(s);
        u &= 0x0ff;
        len ++;
        len += u;
        break;
      default:
        assert(0);
        break;
    }
  }

  return len;
}

static int logger_encode_entry(uint8_t *buffer, const int length, LogEntry *entry, va_list params) {
  int len = 0;
  uint16_t log_number = entry->id;
  char *format = (char *) entry->message;
  char c = 0;

  int32_t i;
  float f;
  void *p;
  uint32_t u;
  char *s;

  int remaining_buffer_size = length;

  buffer[len ++] = (uint8_t)(log_number & 0xFF);
  buffer[len ++] = (uint8_t)(log_number >> 8);

  while((c = logger_get_next_specifier(&format))) {
    switch(c) {
      case 'c':
        buffer[len ++] = (uint8_t) va_arg(params, int32_t);
        break;
      case 'd':
      case 'i':
        i = va_arg(params, int32_t);
        memcpy(&buffer[len], &i, sizeof(i));
        len += sizeof(i);
        break;
      case 'u':
        u = va_arg(params, uint32_t);
        memcpy(&buffer[len], &u, sizeof u);
        len += sizeof u;
        break;
      case 'f':
      case 'F':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
      case 'a':
      case 'A':
        f = (float) va_arg(params, double);
        memcpy(&buffer[len], &f, sizeof(f));
        len += sizeof(f);
        break;
      case 'p':
        p = va_arg(params, void*);
        memcpy(&buffer[len], &p, sizeof p);
        len += sizeof p;
        break;
      case 's':
        s = va_arg(params, char *);
        u = strlen(s);
        u &= 0x0ff; // max allowed string length is 255 characters
        buffer[len ++] = (uint8_t) u; // write the string size
        memcpy(&buffer[len], s, u);
        len += u;
        break;
      default:
        assert(0);
        break;
    }
  }

  return len;
}

static void logger_write_log_compressed(LogWriterEntry *writer, LogEntry *entry, va_list params) {
  const int MAX_BUFFER_SIZE = 256;
  uint8_t buffer[MAX_BUFFER_SIZE];

  int len = logger_encode_entry(buffer, MAX_BUFFER_SIZE, entry, params);

  writer->writer(buffer, len);
}

static void logger_write_log(LogWriterEntry *writer, LogEntry *entry, va_list params) {
  const int MAX_BUFFER_SIZE = 256;
  uint8_t buffer[MAX_BUFFER_SIZE];

  int len = vsnprintf((char*)buffer, MAX_BUFFER_SIZE, entry->message, params);

  assert(len >= 0);

  if(len >= MAX_BUFFER_SIZE) {
    len = MAX_BUFFER_SIZE - 1;
  }

  writer->writer(buffer, len);
}



void logger_register_log_entries(LogEntry *entries, int count) {
  assert(log_entries_count < MAX_LOG_ENTRIES);
  log_entries[log_entries_count].entries = entries;
  log_entries[log_entries_count].count = count;
  log_entries_count ++;
}

void logger_register_log_writer(LogWriter writer, int is_compressed) {
  assert(log_writers_count < MAX_LOG_WRITERS);
  log_writers[log_writers_count].writer = writer;
  log_writers[log_writers_count].is_compressed = is_compressed;
  log_writers_count ++;
}

void logger_log(uint16_t id, ...) {
  va_list varargs;
  va_start(varargs, id);

  int i;
  LogEntry *entry = logger_find_log_entry(id);
  assert(entry != NULL);

  for(i = 0; i < log_writers_count; i ++) {
    va_list params;
    va_copy(varargs, params);

    LogWriterEntry *writer = &log_writers[i];

    if(writer->is_compressed) {
      logger_write_log_compressed(writer, entry, params);
    }
    else {
      logger_write_log(writer, entry, params);
    }
    va_end(params);
  }

  va_end(varargs);
}


void logger_log_printf(const char * const format, ...) {
}

static void logger_assert_no_duplicate_entries() {
}


int logger_decode_entry(uint8_t *buffer, const int length, LogEntry *entry) {
  return 0;
}













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

static void logger_write_log(LogWriterEntry *writer, LogEntry *entry, va_list params) {
  const int MAX_BUFFER_SIZE = 256;
  uint8_t buffer[MAX_BUFFER_SIZE];

  int len = vsnprintf((char*)buffer, MAX_BUFFER_SIZE, entry->message, params);

  assert(len >= 0);

  if(len >= MAX_BUFFER_SIZE) {
    len = MAX_BUFFER_SIZE - 1;
  }

  uint8_t header[9];
  snprintf((char *)header, 9, "[0x%04X]", entry->id);

  writer->writer(header, 8);
  writer->writer(buffer, len);
}

static void logger_write_log_compressed(LogWriterEntry *writer, LogEntry *entry, va_list params) {
  logger_write_log(writer, entry, params);
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
  log_writers[log_writers_count].severity = SEVERITY_NORMAL;
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











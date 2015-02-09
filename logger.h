#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

// LogWriter is a function that must be registered in the logger
// to be able to write a buffer of bytes into persistent memory
typedef void (*LogWriter)(const uint8_t *data, const size_t length);

typedef uint16_t LogId;

typedef enum {
  SEVERITY_VERBOSE,
  SEVERITY_DEBUG,
  SEVERITY_INFO,
  SEVERITY_WARNING,
  SEVERITY_ERROR,
  SEVERITY_FATAL
} LogSeverity;

typedef struct {
  uint16_t id;
  const char * const format;
} LogEntry;

enum {
#define LOG_ENTRY(_id_, _value_, _format_) _id_ = _value_,
#include "logger.defs"
#undef LOG_ENTRY
};

void logger_initialize(void);

bool logger_register_log_entries(LogEntry *entries, size_t count);
bool logger_register_log_writer(LogWriter writer, LogSeverity severity, bool encode);

void logger_log(uint16_t id, ...);
void logger_severity_log(LogSeverity severity, uint16_t id, ...);
void logger_printf(const char * format, ...) __attribute__ ((format (printf, 1, 2)));
void logger_severity_printf(LogSeverity severity, const char * format, ...) __attribute__ ((format (printf, 2, 3)));

size_t logger_decode(char *dst, size_t d_len, const char *src, size_t s_len, size_t *s_unused_bytes);

size_t logger_get_max_buffer_size(void);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H_

#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define LOG_ENTRIES \
    LOG_ENTRY(NB_LOG_ERROR_SIMULATED_ANNEALING, 0x1000, "!!! SA: infinite cost") \
    LOG_ENTRY(NB_LOG_ERROR_MALLOC_OOM,          0x1001, "!!! Malloc") \
    LOG_ENTRY(NB_LOG_ERROR_CALLOC_OOM,          0x1002, "!!! Calloc") \
    LOG_ENTRY(NB_LOG_MAX_TFLOW,                 0x1003, "MAX_TFLOW: %3.2f") \
    LOG_ENTRY(NB_LOG_ASSERT_BUILDINGMODEL,      0x1004, "!!! Assertion failed in BM at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_CONTROLLERIMPL,     0x1005, "!!! Assertion failed in ControllerImpl at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_NMOA,               0x1006, "!!! Assertion failed in NMOA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_SAOA,               0x1007, "!!! Assertion failed in SAOA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_OA,                 0x1008, "!!! Assertion failed in OA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_PRESENCEDETECTION,  0x1009, "!!! Assertion failed in presence_detection at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_SCHEDULEDETECTION,  0x100A, "!!! Assertion failed in schedule_detection at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_LOG,                0x100B, "!!! Assertion failed in logger at line %d") \
    LOG_ENTRY(NB_LOG_SCHEDULE_CONFIDENCE,       0x100C, "Schedule confidence %4.2f") \
    LOG_ENTRY(NB_LOG_PREDICTION_ERROR,          0x100D, "Building model prediction: %.2f C; Error: %.2f C") \
    \
    //


// the LogWriter is a function that needs to be registered in the logger
// to be able to write a buffer of byte into persistent memory
typedef int (*LogWriter)(const char *data, const unsigned int length);

typedef enum {
  SEVERITY_VERBOSE,
  SEVERITY_DEBUG,
  SEVERITY_NORMAL,
  SEVERITY_INFO,
  SEVERITY_WARNING,
  SEVERITY_ERROR,
  SEVERITY_FATAL
} LogSeverity;

typedef struct {
  uint16_t id;
  LogSeverity severity;
  const char * const message;
} LogEntry;

void logger_register_log_entries(LogEntry *entries, int count);
void logger_register_log_writer(LogWriter writer, LogSeverity severity, int is_compressed);

void logger_log(uint16_t id, ...);
void logger_severity_log(LogSeverity severity, uint16_t id, ...);
void logger_printf(const char * format, ...);
void logger_severity_printf(LogSeverity severity, const char * format, ...);
void logger_printf_with_id(uint16_t id, const char * format, ...);
void logger_severity_printf_with_id(LogSeverity severity, uint16_t id, const char * format, ...);

int logger_decode(char *destination, int destination_length, const char *source, int *source_length);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H_

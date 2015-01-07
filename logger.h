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
typedef void (*LogWriter)(const uint8_t *data, const size_t size);

typedef enum {
  SEVERITY_VERBOSE,
  SEVERITY_DEBUG,
  SEVERITY_NORMAL,
  SEVERITY_INFO,
  SEVERITY_WARNING,
  SEVERITY_ERROR,
  SEVERITY_FATAL
} log_severity_t;

typedef struct {
  uint16_t id;
  log_severity_t severity;
  const char * const message;
} LogEntry;

void logger_register_log_entries(LogEntry *entries, int count);
void logger_register_log_writer(LogWriter writer, int is_compressed, log_severity_t severity);

void logger_log(uint16_t id, ...);
void logger_severity_log(log_severity_t severity, uint16_t id, ...);
void logger_printf(const char * format, ...);
void logger_severity_printf(log_severity_t severity, const char * format, ...);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H_

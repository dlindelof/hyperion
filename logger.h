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
    LOG_ENTRY(NB_LOG_ERROR_SIMULATED_ANNEALING, "[N] !!! SA: infinite cost") \
    LOG_ENTRY(NB_LOG_ERROR_MALLOC_OOM,          "[N] !!! Malloc") \
    LOG_ENTRY(NB_LOG_ERROR_CALLOC_OOM,          "[N] !!! Calloc") \
    LOG_ENTRY(NB_LOG_MAX_TFLOW,                 "[N] MAX_TFLOW: %3.2f") \
    LOG_ENTRY(NB_LOG_ASSERT_BUILDINGMODEL,      "[N] !!! Assertion failed in BM at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_CONTROLLERIMPL,     "[N] !!! Assertion failed in ControllerImpl at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_NMOA,               "[N] !!! Assertion failed in NMOA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_SAOA,               "[N] !!! Assertion failed in SAOA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_OA,                 "[N] !!! Assertion failed in OA at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_PRESENCEDETECTION,  "[N] !!! Assertion failed in presence_detection at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_SCHEDULEDETECTION,  "[N] !!! Assertion failed in schedule_detection at line %d") \
    LOG_ENTRY(NB_LOG_ASSERT_LOG,                "[N] !!! Assertion failed in logger at line %d") \
    LOG_ENTRY(NB_LOG_SCHEDULE_CONFIDENCE,       "[N] Schedule confidence %4.2f") \
    LOG_ENTRY(NB_LOG_PREDICTION_ERROR,          "[N] Building model prediction: %.2f C; Error: %.2f C") \
    \
    //




#define print_pointer(p) printf("%p\n", ((void *)p))


// the LogWriter is a function that needs to be registered in the logger
// to be able to write a buffer of byte into persistent memory
typedef void (*LogWriter)(const uint8_t *data, const size_t size);
// the StringWriterCallback is a function that needs to be registered in the logger
// in case you want to call the logger_decode_buffer()
// this will allow to output a log as a null terminated string
typedef void (*StringWriterCallback)(const char* pString);
// the Tracer is a function that needs to be registered in the logger
// to be able to output the traces(logs in clear text) has it goes
// not supported yet
typedef void (*Tracer)(const char *format, va_list params);

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

typedef struct {
  struct {
    uint32_t sector;
    uint32_t page;
    uint32_t size;
  } info;
  StringWriterCallback cb;
  uint8_t* data;
} NbLogBuffer;

//void logger_init(LogWriter);

//void logger_decode_buffer(NbLogBuffer* buffer);

void logger_register_log_entries(LogEntry *entries, int count);

void logger_register_log_writer(LogWriter writer, int is_compressed);
//void logger_register_log_writer_compressed(LogWriter writer);

void logger_log(uint16_t id, ...);
void logger_log_printf(const char * format, ...);
//void logger_assert_no_duplicate_entries();



//LogEntry* logger_find_log_entry(uint16_t id);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H_

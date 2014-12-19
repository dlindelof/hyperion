//#include "neurobat.h"
#include "logger.h"

extern "C"
{
#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include "../logger.c"
}

//CppUTest includes should be after your and system includes
#include "CppUTest/TestHarness.h"

#define TEST_LOG_ENTRIES_1 \
  LOG_ENTRY(TEST_LOG_EMPTY_STRING_1,                  "") \
  LOG_ENTRY(TEST_LOG_SOME_ENTRY_IN_THE_MIDDLE_1,      "This is not a special entry") \
  LOG_ENTRY(TEST_LOG_STRING_1,                        "This is a string.") \
  LOG_ENTRY(TEST_LOG_STRING_WITH_PARAMETERS_1,        "This is a string with parameters {integer: %d}") \
  // Insert log entries before this line

#define TEST_LOG_ENTRIES_2 \
  LOG_ENTRY(TEST_LOG_STRING_WITH_PARAMETERS_2,        "This is a string with parameters {integer: %d}") \
  // Insert log entries before this line




enum {
  TEST_LOG_FIRST_ENTRY_1 = 0x0000,

#define LOG_ENTRY(name, message) name,
  TEST_LOG_ENTRIES_1
#undef LOG_ENTRY

  TEST_LOG_LAST_ENTRY_1
};

static LogEntry entries_1[] = {

#define LOG_ENTRY(name, message) {name, SEVERITY_NORMAL, message},
  TEST_LOG_ENTRIES_1
#undef LOG_ENTRY

};

enum {
  TEST_LOG_FIRST_ENTRY_2 = 0x8000,

#define LOG_ENTRY(name, message) name,
  TEST_LOG_ENTRIES_2
#undef LOG_ENTRY

  TEST_LOG_LAST_ENTRY_2
};

static LogEntry entries_2[] = {

#define LOG_ENTRY(name, message) {name, SEVERITY_NORMAL, message},
  TEST_LOG_ENTRIES_2
#undef LOG_ENTRY

};




TEST_GROUP(LOGGER) {

  void setup() {
  }

  void teardown() {
  }

};

TEST(LOGGER, Logger_FindEntry_ReturnsEntry) {
  int count;
  LogEntry *entry;

  count = TEST_LOG_LAST_ENTRY_1 - TEST_LOG_FIRST_ENTRY_1 - 1;
  logger_register_log_entries(entries_1, count);
  count = TEST_LOG_LAST_ENTRY_2 - TEST_LOG_FIRST_ENTRY_2 - 1;
  logger_register_log_entries(entries_2, count);

  entry = logger_find_log_entry(TEST_LOG_SOME_ENTRY_IN_THE_MIDDLE_1);
  POINTERS_EQUAL(&entries_1[1], entry);
  entry = logger_find_log_entry(TEST_LOG_STRING_WITH_PARAMETERS_2);
  POINTERS_EQUAL(&entries_2[0], entry);
}

TEST(LOGGER, Logger_DoNotFindEntry_ReturnsNULL) {
  int count;
  LogEntry *entry;

  count = TEST_LOG_LAST_ENTRY_1 - TEST_LOG_FIRST_ENTRY_1 - 1;
  logger_register_log_entries(entries_1, count);

  entry = logger_find_log_entry(TEST_LOG_STRING_WITH_PARAMETERS_2);
  POINTERS_EQUAL(NULL, entry);

  //printf("% d %", 1);
}






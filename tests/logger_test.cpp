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





TEST_GROUP(LOGGER_LOG_ENTRIES) {

  void setup() {
  }

  void teardown() {
  }

};

TEST(LOGGER_LOG_ENTRIES, Logger_FindEntry_ReturnsEntry) {
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

TEST(LOGGER_LOG_ENTRIES, Logger_DoNotFindEntry_ReturnsNULL) {
  int count;
  LogEntry *entry;

  count = TEST_LOG_LAST_ENTRY_1 - TEST_LOG_FIRST_ENTRY_1 - 1;
  logger_register_log_entries(entries_1, count);

  entry = logger_find_log_entry(TEST_LOG_STRING_WITH_PARAMETERS_2);
  POINTERS_EQUAL(NULL, entry);

  //printf("% d %", 1);
}





TEST_GROUP(LOGGER_PRINTF) {
};

TEST(LOGGER_PRINTF, Logger_GetNextSpecifier_ReturnsTheSpecifier) {
  char format[] = " S%%%s%#0+-33.33f %% blah blah %c%s%12iiii% X    %.12F%# -+12p";
  char *fmt = format;
  int n;

  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%s", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%#0+-33.33f", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%c", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%s", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%12i", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("% X", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%.12F", fmt, n);
  fmt += n;
  n = logger_get_next_specifier(&fmt);
  STRNCMP_EQUAL("%# -+12p", fmt, n);
}

TEST(LOGGER_PRINTF, Logger_GetNextSpecifierFromStringWithNoSpecifier_ReturnsZero) {
  char format[] = "%% this string has no formatting specifier and only has %%";
  char *fmt = format;
  int n;

  n = logger_get_next_specifier(&fmt);
  CHECK_EQUAL(0, n);
}





#define BUFSIZE (128)

TEST_GROUP(LOGGER_PRINTF_ENTRY) {

  char buffer[BUFSIZE];
  int n;

  va_list none;

  void setup() {
  }

  void teardown() {
  }

  int logger_snvprintf_entry_test_helper(char *buffer, int length, uint16_t id, int compressed, int encode, const char *format, ...) {
    va_list varargs;
    va_start(varargs, format);
    int len = logger_snvprintf_entry(buffer, length, id, compressed, encode, format, varargs);
    va_end(varargs);
    return len;
  }

};

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintEmptyString_PrintsEmptyStringAddsIdAndNewLine) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 0, 0, "", none);
  STRNCMP_EQUAL("[0X002A] \n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintString_PrintsStringAddsIdAndNewLine) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 0, 0, "Simple text with no parameters", none);
  STRNCMP_EQUAL("[0X002A] Simple text with no parameters\n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintStringWithSpecialCharacters_PrintsStringReplacesSpecialCharacters) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 0, 0, "|Simple text with no parameters and with special characters |\n", none);
  STRNCMP_EQUAL("[0X002A] !Simple text with no parameters and with special characters !\r\n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintStringWithParameters_PrintsFormattedString) {
  n = logger_snvprintf_entry_test_helper(buffer, BUFSIZE, 42, 0, 0, "Text with %c %s %5.2f, %d, and %#X", 'A', "String and", 12.2, 42, 42);
  STRNCMP_EQUAL("[0X002A] Text with A String and 12.20, 42, and 0X2A\n", buffer, n);
  n = logger_snvprintf_entry_test_helper(buffer, BUFSIZE, 42, 0, 0, "Text with %c %s %c %s", '|', "|||", 0, "");
  STRNCMP_EQUAL("[0X002A] Text with ! !!! \0\n", buffer, n);
  n = logger_snvprintf_entry_test_helper(buffer, BUFSIZE, 42, 0, 0, "Text with %c %s", '\n', "   ");
  STRNCMP_EQUAL("[0X002A] Text with \r    \n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintCompressedEmptyString_PrintsEmptyStringAddsIdAndNewLineAddsSeparators) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 1, 0, "", none);
  STRNCMP_EQUAL("[002A||\n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintCompressedString_PrintsStringAddsIdAndNewLineAddsSeparators) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 1, 0, "Text with no parameters [0x00] does not matter what is inside", none);
  STRNCMP_EQUAL("[002A|Text with no parameters [0x00] does not matter what is inside|\n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintCompressedStringWithSpecialCharacters_PrintsStringReplacesSpecialCharacters) {
  n = logger_snvprintf_entry(buffer, BUFSIZE, 42, 1, 0, "||\n\n||", none);
  STRNCMP_EQUAL("[002A|!!\r\r!!|\n", buffer, n);
}

TEST(LOGGER_PRINTF_ENTRY, Logger_PrintCompressedStringWithFormatting_PrintsFormatterString) {
  n = logger_snvprintf_entry_test_helper(buffer, BUFSIZE, 42, 1, 0, "Text with %c %s %d", 'A', "string", 2);
  STRNCMP_EQUAL("[002A|Text with A string 2|\n", buffer, n);
}




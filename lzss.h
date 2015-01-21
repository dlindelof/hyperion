#ifndef LZSS_H_
#define LZSS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * do not change these values or otherwise older logs won't be readable in new
 * firmwares.
 */
#define DICT_BITS 11                      // must be less than or equal to 11
#define DICTIONARY_SIZE (1 << DICT_BITS)  // dictionary size
#define MATCH_BITS 4                      // must be between 2..4 inclusive
#define THRESHOLD 2                       // minimum match length. do not change!
#define LOOKAHEAD_SIZE ((1 << MATCH_BITS) + THRESHOLD - 1) // lookahead buffer size

typedef struct {
  char buffer[DICTIONARY_SIZE];
  unsigned int tail;
} Dictionary;

void dictionray_init(Dictionary *dictionary);

int compress(Dictionary *dictionary, char *dst, int d_len, const char *src, int *s_len, int packet_len);
int decompress(Dictionary *dictionary, char *dst, int d_len, const char *src, int *s_len, int packet_len);


#ifdef __cplusplus
}
#endif

#endif // LZSS_H_

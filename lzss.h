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

typedef struct {
  uint8_t buffer[DICTIONARY_SIZE];
  size_t tail;
} Dictionary;

void lzss_dictionary_init(Dictionary *dictionary);

size_t lzss_compress(Dictionary *dictionary, uint8_t *dst, size_t d_len, const uint8_t *src, size_t s_len, size_t *s_unused_bytes, size_t d_remaining_packet_len);
size_t lzss_decompress(Dictionary *dictionary, uint8_t *dst, size_t d_len, const uint8_t *src, size_t s_len, size_t *s_unused_bytes, size_t s_remaining_packet_len);


#ifdef __cplusplus
}
#endif

#endif // LZSS_H_

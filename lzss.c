#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "lzss.h"

/*
 * this is an implementation of LZSS compression algorithm.
 * this algorithm uses the last few bytes of the input stream as a dictionary.
 * when compressing, it tries to find the longest possible match of the input
 * in the dictionary; if a match is not found or if its length is less than 3
 * then it writes literal characters from the input to output. but if a match
 * is found it write a pair of position and length instead of the whole match
 * (referred as a copy).
 *
 * a literal is one byte and a copy is two bytes. these two are distinguished
 * by the MSB bit which is '0' for literals and '1' for copies.
 * +-+-+-+-+-+-+-+-+
 * |0|             | literal
 * +-+-+-+-+-+-+-+-+
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |1|       position      |length | copy
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * non-ASCII literals that have a '1' MSB are written as a sequence of two bytes
 * where the second byte is 0x0f.
 *
 * in this specific implementation, the value of the length field for copies
 * can be from 1 to 15 which corresponds to an actual copy length of 3 to 17.
 *
 *
 * the dictionary must be always initialized to the same value for compressor
 * and decompressor.
 *
 * this algorithm is optimized for compressing ASCII text. do not use it for
 * binary compression.
 *
 * the output never generates two consequent 0xff bytes and it ignores the same
 * consequence when decompressing. the is achieved by avoiding setting match length
 * to all 1s.
 */

/*
 * do not change these values or otherwise older logs won't be readable in new
 * firmwares.
 */
#define MATCH_BITS 4                      // must be between 2..4 inclusive
#define THRESHOLD 3                       // minimum match length. do not change!
#define LOOKAHEAD_SIZE ((1 << MATCH_BITS) + THRESHOLD - 2) // lookahead buffer size


#define MINIMUM(_a_,_b_) (((_a_) <= (_b_)) ? (_a_) : (_b_))

/*
 * functions for writing/reading to/from the byte stream.
 */

static size_t write_literal(uint8_t *dst, size_t d_len, uint8_t c) {
  if(c & 128) {
    if(d_len >= 2) {
      *dst ++ = c;
      *dst = 0x0f;
      return 2;
    }
    return 0;
  } else if(d_len >= 1) {
    *dst = c;
    return 1;
  }
  return 0;
}

static size_t write_two_literals(uint8_t *dst, size_t d_len, uint8_t c1, uint8_t c2) {
  uint8_t tmp[4];
  size_t len = write_literal(tmp, 4, c1);
  len += write_literal(&tmp[len], 4 - len, c2);
  if(len <= d_len) {
    memcpy(dst, tmp, len);
    return len;
  }
  return 0;
}

static size_t write_copy(uint8_t *dst, size_t d_len, unsigned int position, unsigned int match_length) {
  assert(match_length >= THRESHOLD && match_length <= LOOKAHEAD_SIZE);
  if(d_len >= 2) {
    match_length -= THRESHOLD;
    uint8_t c = 128;
    c |= (uint8_t)(position >> 4);
    *dst ++ = c;
    c = (uint8_t)((position << 4) & 0x0f0) | (match_length & 0x0f);
    *dst = c;
    return 2;
  }
  return 0;
}

static size_t skip_ffff_sequences(const uint8_t *buffer, size_t length) {
  const uint8_t invalid_byte = 0x0ff;
  size_t i = 0;
  while(i < (length - 1)) {
    if(buffer[i] == invalid_byte && buffer[i + 1] == invalid_byte) {
      i += 2;
    } else {
      break;
    }
  }
  return i;
}



static size_t read_literal_or_copy(const uint8_t *buffer, size_t length, uint8_t *character, unsigned int *position, unsigned int *match_length) {
  *match_length = 0;
  *position = 0;
  *character = 0;
  if(length >= 1) {
    uint8_t c1 = *buffer;
    if(c1 & 128) { // non-ASCII literal or copy
      if(length >= 2) {
        buffer ++;
        uint8_t c2 = *buffer;
        *match_length = c2 & 0x0f;
        if(*match_length == 15) { // non-ASCII literal ( > 127)
          *character = c1;
          *match_length = 1;
        } else { // copy
          *position = ((c1 & 127) << 4) | ((c2 & 0x0f0) >> 4);
          *match_length += THRESHOLD;
        }
        return 2;
      } else { // maybe is a filler? caller must check
        *character = c1;
        *match_length = 0;
        return 1;
      }
    } else { // literal
      *character = c1;
      *match_length = 1;
      return 1;
    }
  }
  return 0; // nothing to read or not enough to read
}



static void dictionary_copy_from_buffer(Dictionary *dictionary, const uint8_t *s, unsigned int n) {
  assert(n <= DICTIONARY_SIZE);
  size_t i = dictionary->tail;
  while(n) {
    ++ i;
    i %= DICTIONARY_SIZE;
    dictionary->buffer[i] = *s ++;
    -- n;
  }
  dictionary->tail = i;
}

static void dictionary_copy_to_buffer(Dictionary *dictionary, uint8_t *buf, unsigned int position, unsigned int n) {
  assert(position <= DICTIONARY_SIZE);
  while(n) {
    *buf ++ = dictionary->buffer[position];
    ++ position;
    position %= DICTIONARY_SIZE;
    -- n;
  }
}

static uint8_t dictionary_get_at(Dictionary *dictionary, unsigned int index) {
  return dictionary->buffer[index % DICTIONARY_SIZE];
}

static int dictionary_find_longest_match(Dictionary *dictionary, const uint8_t *src, unsigned int max, unsigned int *position) {
  unsigned int match_length = 0;
  size_t i = dictionary->tail;
  unsigned int c = DICTIONARY_SIZE;
  while(c) {
    if(dictionary->buffer[i] == *src) {
      size_t j;
      for(j = 1; j < max; j ++) {
        if(dictionary_get_at(dictionary,(i+j)) != src[j]) {
          break;
        }
      }
      if(j > match_length) {
        *position = i;
        match_length = j;
      }
      if(j == max) { // stop searching if we already found the longest possible match
        break;
      }
    }
    i = i == 0 ? DICTIONARY_SIZE - 1 : i - 1;
    -- c;
  }
  return match_length;
}



void dictionary_init(Dictionary *dictionary) {
  /*
   * initial content of the dictionary must be consistent across
   * firmwares. do not change this line.
   */
  memset(dictionary->buffer, 0, DICTIONARY_SIZE);

  dictionary->tail = DICTIONARY_SIZE - 1;
}

/*
 * compress the source and write in destination.
 * do not write more than d_remaining_packet_len.
 * update s_len to the number of unused bytes in the source.
 * return the number of bytes written into the destination.
 */
size_t compress(Dictionary *dictionary, uint8_t *dst, size_t d_len, const uint8_t *src, size_t s_len, size_t *s_unused_bytes, size_t d_remaining_packet_len) {
  const uint8_t *original_dst = dst;
  unsigned int position = 0, match_length = 0, max = 0;

  d_len = MINIMUM(d_len, d_remaining_packet_len);
    
  while(s_len > 0) {
    unsigned int len;
    max = MINIMUM(LOOKAHEAD_SIZE, s_len);
    match_length = dictionary_find_longest_match(dictionary, src, max, &position);

    if(match_length == 0 || match_length == 1) { // symbol is not in the dictionary or is a literal
      len = write_literal(dst, d_len, *src);
      match_length = 1;
    } else if(match_length == 2) { // write two literals instead of a copy
      len = write_two_literals(dst, d_len, *src, *(src + 1));
    } else { // match_length >= 3, write a copy
      len = write_copy(dst, d_len, position, match_length);
    }

    if(len == 0) { // not have enough space in the destination or in the packet
      if(d_len <= 3) { // try to add a literal if packet is not completely filled
        len = write_literal(dst, d_len, *src);
        match_length = len == 0 ? 0 : 1;
        dst += len;
        d_len -= len;
        d_remaining_packet_len -= len;
        dictionary_copy_from_buffer(dictionary, src, match_length);
        src += match_length;
        s_len -= match_length;
      }
      if(d_remaining_packet_len == 1 && d_len >= 1) { // if packet is not filled yet then write a filler
        *dst ++ = 0xff;
        d_len --;
        d_remaining_packet_len --;
      }
      break;
    } else {
      dst += len;
      d_len -= len;
      d_remaining_packet_len -= len;
      dictionary_copy_from_buffer(dictionary, src, match_length);
      src += match_length;
      s_len -= match_length;
    }
  }

  *s_unused_bytes = s_len;
  return dst - original_dst;
}

/*
 * decompress the source and write in destination.
 * do not decompress more than s_remaining_packet_len.
 * update s_unused_bytes to the number of unused bytes in the source.
 * return the number of bytes written into the destination.
 */

size_t decompress(Dictionary *dictionary, uint8_t *dst, size_t d_len, const uint8_t *src, size_t s_len, size_t *s_unused_bytes, size_t s_remaining_packet_len) {
  const uint8_t *original_dst = dst;
  const size_t original_s_len = s_len;
  unsigned int position, match_length, len;
  uint8_t character;
  size_t bytes_decmopressed = 0;

  s_len = MINIMUM(s_len, s_remaining_packet_len);

  while(s_len > 0) {
    len = skip_ffff_sequences(src, s_len);
    src += len;
    s_len -= len;
    bytes_decmopressed += len;
    s_remaining_packet_len -= len;
    len = read_literal_or_copy(src, s_len, &character, &position, &match_length);
    if(len != 0 && d_len >= match_length) {
      if(match_length == 0) { // len must be 1
        if(s_remaining_packet_len == 1) {
          // a filler, ignore it
        } else if(s_len == 1) { // first byte of a copy
          break;
        }
      } else if(len == 1 || (len == 2 && match_length == 1)) { // literal
        *dst = character;
        dictionary_copy_from_buffer(dictionary, src, 1);
      } else { // copy
        dictionary_copy_to_buffer(dictionary, dst, position, match_length);
        dictionary_copy_from_buffer(dictionary, dst, match_length);
      }
      src += len;
      s_len -= len;
      s_remaining_packet_len -= len;
      bytes_decmopressed += len;
      dst += match_length;
      d_len -= match_length;
    } else {
      break;
    }
  }

  *s_unused_bytes = original_s_len - bytes_decmopressed;
  return dst - original_dst;
}

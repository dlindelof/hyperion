#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
 * non-ASCII literals that have a '1' MSB are written as a copy with length
 * zero.
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
 */

static int first_was_2 = 0;

static inline int minimum(int a, int b) {
  return a <= b ? a : b;
}

/*
 * functions for writing/reading to/from the byte stream.
 */

static int write_literal(char *dst, int d_len, char c) {
  if(c & 128) {
    if(d_len >= 2) {
      *dst ++ = c;
      *dst = 0;
      return 2;
    }
    return 0;
  } else if(d_len >= 1) {
    *dst = c;
    return 1;
  }
  return 0;
}

static int write_two_literals(char *dst, int d_len, char c1, char c2) {
  char tmp[4];
  int len = write_literal(tmp, 4, c1);
  first_was_2 = len == 2 ? 1 : 0;
  len += write_literal(&tmp[len], 4 - len, c2);
  if(len <= d_len) {
    memcpy(dst, tmp, len);
    return len;
  }
  return 0;
}

static int write_copy(char *dst, int d_len, int position, int match_length) {
  assert(match_length > THRESHOLD && match_length <= LOOKAHEAD_SIZE);
  if(d_len >= 2) {
    match_length -= THRESHOLD;
    char c = 128;
    c |= (char)(position >> 4);
    *dst ++ = c;
    c = (char)((position << 4) & 0x0f0) | (match_length & 0x0f);
    *dst = c;
    return 2;
  }
  return 0;
}

static int read_literal_or_copy(const char *buffer, int length, char *character, int *position, int *match_length) {
  *match_length = 0;
  if(length >= 1) {
    char c1 = *buffer;
    if(c1 & 128) { // non-ASCII literal or copy
      if(length >= 2) {
        buffer ++;
        char c2 = *buffer;
        *match_length = c2 & 0x0f;
        if(*match_length == 0) { // non-ASCII literal ( > 127)
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

/*
 * functions for working with dictionaries.
 */

static void dictionary_copy_from_buffer(Dictionary *dictionary, const char *s, int n) {
  assert(n <= DICTIONARY_SIZE);
  int i = dictionary->tail;
  while(n) {
    ++ i;
    i %= DICTIONARY_SIZE;
    dictionary->buffer[i] = *s ++;
    -- n;
  }
  dictionary->tail = i;
}

static void dictionary_copy_to_buffer(Dictionary *dictionary, char *buf, int position, int n) {
  assert(position <= DICTIONARY_SIZE);
  while(n) {
    *buf ++ = dictionary->buffer[position];
    ++ position;
    position %= DICTIONARY_SIZE;
    -- n;
  }
}

static char dictionary_get_at(Dictionary *dictionary, int index) {
  return dictionary->buffer[index % DICTIONARY_SIZE];
}

static int dictionary_find_lonest_match(Dictionary *dictionary, const char *src, int max, int *position) {
  int m = 0;
  int i = dictionary->tail;
  int c = DICTIONARY_SIZE;
  while(c) {
    if(dictionary->buffer[i] == *src) {
      int j;
      for(j = 1; j < max; j ++) {
        if(dictionary_get_at(dictionary,(i+j)) != src[j]) {
          break;
        }
      }
      if(j > m) {
        *position = i;
        m = j;
      }
      if(j == max) { // stop searching if we already found the longest possible match
        break;
      }
    }
    i = i == 0 ? DICTIONARY_SIZE - 1 : i - 1;
    -- c;
  }
  return m;
}

/*
 * public functions.
 */

void dictionray_init(Dictionary *dictionary) {
  memset(dictionary->buffer, 0, DICTIONARY_SIZE);
  //const char *ss = "|00.01|02.03|04.05|06.07|08.09|10.11|12.13|14.15|16.17|18.19|20.21|22.23|24.25|26.27|28.29|30.31|32.33|34.35|36.37|38.39|40.41|42.43|44.45|46.47|48.49|50.51|52.53|54.55|56.57|58.59|60.61|62.63|64.65|66.67|68.69|70.71|72.73|74.75|76.77|78.79|80.81|82.83|84.85|86.87|88.89|90.91|92.93|94.95|96.97|98.99|";
  //strncpy(dictionary->buffer, ss, DICTIONARY_SIZE);

  dictionary->tail = DICTIONARY_SIZE - 1;
}

/*
 * compress the source and write in destination.
 * do not write more than packet_len.
 * update s_len to the number of unused bytes in the source.
 * return the number of bytes written into the destination.
 */

int compress(Dictionary *dictionary, char *dst, int d_len, const char *src, int *s_len, int packet_len) {
  const char *original_dst = dst;
  int position, m, max;
  int s_length = *s_len;
  d_len = minimum(d_len, packet_len);
    
  while(s_length > 0) {
    int len;
    max = minimum(LOOKAHEAD_SIZE, s_length);
    m = dictionary_find_lonest_match(dictionary, src, max, &position);

    if(m == 0 || m == 1) { // symbol is not in the dictionary or is a literal
      len = write_literal(dst, d_len, *src);
      m = 1;
    } else if(m == 2) { // write two literals instead of a copy
      len = write_two_literals(dst, d_len, *src, *(src + 1));
    } else { // m >= 3, write a copy
      len = write_copy(dst, d_len, position, m);
    }

    if(len == 0) { // not have enough space in the destination or in the packet
      if(d_len <= 3) { // try to add a literal if packet is not completely filled
        len = write_literal(dst, d_len, *src);
        m = len == 0 ? 0 : 1;
        dst += len;
        d_len -= len;
        packet_len -= len;
        dictionary_copy_from_buffer(dictionary, src, m);
        src += m;
        s_length -= m;
      }
      if(packet_len == 1 && d_len >= 1) { // if packet is not filled yet then write a filler
        *dst ++ = 0xff;
        d_len --;
        packet_len --;
      }
      break;
    } else {
      dst += len;
      d_len -= len;
      packet_len -= len;
      dictionary_copy_from_buffer(dictionary, src, m);
      src += m;
      s_length -= m;
    }
  }

  *s_len = s_length;
  return dst - original_dst;
}

/*
 * decompress the source and write in destination.
 * do not decompress more than packet_len.
 * update s_len to the number of unused bytes in the source.
 * return the number of bytes written into the destination.
 */

int decompress(Dictionary *dictionary, char *dst, int d_len, const char *src, int *s_len, int packet_len) {
  const char *original_dst = dst;
  int position, m, len;
  char character;
  int s_length = minimum(*s_len, packet_len);
  int bytes_decmopressed = 0;

  while(s_length > 0) {
    len = read_literal_or_copy(src, s_length, &character, &position, &m);
    if(len != 0 && d_len >= m) {
      if(m == 0) {
        assert(len == 1);
        if(packet_len == 1) { // a filler, ignore it
          assert(character == (char)0x0ff);
        } else if(s_length == 1) { // first bye of a copy
          break;
        }
      } else if(len == 1 || (len == 2 && m == 1)) { // literal
        *dst = character;
        dictionary_copy_from_buffer(dictionary, src, 1);
      } else { // copy
        dictionary_copy_to_buffer(dictionary, dst, position, m);
        dictionary_copy_from_buffer(dictionary, dst, m);
      }
      src += len;
      s_length -= len;
      packet_len -= len;
      bytes_decmopressed += len;
      dst += m;
      d_len -= m;
    } else {
      break;
    }
  }

  *s_len -= bytes_decmopressed;
  return dst - original_dst;
}

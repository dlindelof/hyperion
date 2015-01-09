/* LZSS encoder-decoder  (c) Haruhiko Okumura */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NNN 11  /* typically 10..13 */
#define MMM  4  /* typically 4..5 */
#define THRESHOLD 1
#define DICTIONARY_SIZE (1 << NNN)  /* buffer size */
#define LOOKAHEAD_SIZE ((1 << MMM))  /* lookahead buffer size */

int bit_buffer = 0, bit_mask = 128;
unsigned long codecount = 0, textcount = 0;

int putbit1(char *d_buffer) {
  bit_buffer |= bit_mask;
  bit_mask >>= 1;
  if(bit_mask == 0) {
    *d_buffer = bit_buffer;
    bit_buffer = 0;
    bit_mask = 128;
    return 1;
  }
  return 0;
}

int putbit0(char *d_buffer) {
  bit_mask >>= 1;
  if(bit_mask == 0) {
    *d_buffer = bit_buffer;
    bit_buffer = 0;
    bit_mask = 128;
    return 1;
  }
  return 0;
}

int putbitstream(char *d_buffer, int c, int initial) {
  int mask = initial;
  int len = 0;
  while(mask >>= 1) {
    len += (c & mask) ? putbit1(d_buffer) : putbit0(d_buffer);
  }
  return len;
}

int flush_bit_buffer(char *d_buffer) {
  if(bit_mask != 128) {
    *d_buffer = bit_buffer;
    return 1;
  }
  return 0;
}

int write_literal(char *d_buffer, int d_len, char c) {
  return putbit1(d_buffer) +
         putbitstream(d_buffer, c, 256);
}

int write_copy(char *d_buffer, int position, int match_length) {
  int len = 0;
  len += putbit0(d_buffer);
  len += putbitstream(d_buffer, position, DICTIONARY_SIZE);
  len += putbitstream(d_buffer, match_length, LOOKAHEAD_SIZE - THRESHOLD);
  return len;
}










typedef struct {
  char buffer[DICTIONARY_SIZE];
  unsigned int tail;
} Dictionary;



static void dictionray_init(Dictionary *d) {
  memset(d->buffer, 0, DICTIONARY_SIZE);
  d->tail = DICTIONARY_SIZE - 1;
}

static void dictionary_copy_from_string(Dictionary *d, const char *s, unsigned int n) {
  assert(n <= DICTIONARY_SIZE);
  int i = d->tail;
  while(n) {
    ++ i;
    i %= DICTIONARY_SIZE;
    d->buffer[i] = *s ++;
    -- n;
  }
  d->tail = i;
}

static char dictionary_get_at(Dictionary *d, int index) {
  return d->buffer[index % DICTIONARY_SIZE];
}

static int dictionary_find_lonest_match(Dictionary *d, const char *s_buffer, int max, int *position) {
  int m = 0;
  int i = d->tail;
  int c = DICTIONARY_SIZE;
  while(c) {
    if(d->buffer[i] == *s_buffer) {
      int j;
      for(j = 1; j < max; j ++) {
        if(d->buffer[(i+j) % DICTIONARY_SIZE] != s_buffer[j]) {
          break;
        }
      }
      if(j > m) {
        *position = i;
        m = j;
      }
      if(j == max) { // stop search if we already found the longest possible match
        break;
      }
    }
    i = i == 0 ? DICTIONARY_SIZE - 1 : i - 1;
    -- c;
  }
  return m;
}

int compress(Dictionary *d, char *d_buffer, int d_len, const char *s_buffer, int s_len, int *encoded_len)
{
  const char *src = s_buffer;
  int position, m, max;
  *encoded_len = 0;
    
  while(s_len > 0) {
    int len;
    max = LOOKAHEAD_SIZE <= s_len ? LOOKAHEAD_SIZE : s_len;
    m = dictionary_find_lonest_match(d, s_buffer, max, &position);
    if(m == 0) { // symbol is not in the dictionary
      len = write_literal(d_buffer, *s_buffer);
      m = 1;
    } else if(m < THRESHOLD) {
      len = write_literal(d_buffer, *s_buffer);
    } else {
      len = write_copy(d_buffer, position, m);
    }
    if(d_len <= len) { // d_buffer gets full after; we must preserve one byte
      break;
    }
    d_buffer += len;
    d_len -= len;
    *encoded_len += len;
    dictionary_copy_from_string(d, s_buffer, m);
    s_buffer += m;
    s_len -= m;
  }
  *encoded_len += flush_bit_buffer(d_buffer);

  return s_buffer - src;
}

#include <sys/time.h>
#define BUFFER_SIZE 180000
char s_buffer[BUFFER_SIZE+1];
char d_buffer[2*BUFFER_SIZE+1];

int main(int argc, char *argv[])
{
  Dictionary dictionary;
  int enc;
  char *s;
  struct timeval t1, t2;
  FILE *s_file, *d_file;
    
  if(argc != 4) {
    printf("Usage: lzss e/d infile outfile\n\te = encode\td = decode\n");
    return 1;
  }
  s = argv[1];
  if(s[1] == 0 && (*s == 'd' || *s == 'D' || *s == 'e' || *s == 'E'))
    enc = (*s == 'e' || *s == 'E');
  else {
    printf("Usage: lzss e/d infile outfile\n\te = encode\td = decode\n\n");
    return 1;
  }
  if((s_file  = fopen(argv[2], "rb")) == NULL) {
    printf("? %s\n", argv[2]);
    return 1;
  }
  if((d_file = fopen(argv[3], "wb")) == NULL) {
    printf("? %s\n", argv[3]);
    return 1;
  }
  gettimeofday(&t1, NULL);

  {
    dictionray_init(&dictionary);

    int s_len;
    while((s_len = fread(s_buffer, 1, BUFFER_SIZE, s_file)) != 0) {
      int i;
      int d_len;
      int encoded_len;
      s_len = compress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, s_len, &encoded_len); // just make sure that the output buffer is large enough here,
                                                                              // dealing with the other case is not worth the time
      fwrite(d_buffer, 1, encoded_len, d_file);
      textcount += s_len;
      codecount += encoded_len;
    }
  }

  gettimeofday(&t2, NULL);
  fprintf(stderr, "Finished in about %.0f milliseconds. \n", (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0);
  printf("text:  %ld bytes\n", textcount);
  printf("code:  %ld bytes (%ld%%)\n", codecount, (codecount * 100) / textcount);
  fclose(d_file);
  fclose(s_file);
  return 0;
}


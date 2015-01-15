#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * do not change these values or otherwise older logs won't be readable in new firmwares
 */
#define NN 11                           // must be less than or equal to 11
#define DICTIONARY_SIZE (1 << NN)       // buffer size
#define MM 4                            // must be between 2..4 inclusive
#define THRESHOLD 2                     // minimum match length. do not change
#define LOOKAHEAD_SIZE ((1 << MM) + THRESHOLD - 1) // lookahead buffer size


/*
 * +-+-+-+-+-+-+-+-+
 * |0|             |
 * +-+-+-+-+-+-+-+-+
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |1|                     |       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

unsigned long codecount = 0, textcount = 0;

#define AAA ((1<<MM) + 4)
int matches[AAA] = {0};


int write_literal(char *d_buffer, int d_len, char c) {
  if(c & 128) {
    if(d_len >= 2) {
      *d_buffer ++ = c;
      *d_buffer = 0;
    }
    return 2;
  } else if(d_len >= 1) {
    *d_buffer = c;
    return 1;
  }
  return 0; // not enough space in the buffer
}

int write_copy(char *d_buffer, int d_len, int position, int match_length) {
  assert(match_length > THRESHOLD && match_length <= LOOKAHEAD_SIZE);
  if(d_len >= 2) {
    match_length -= THRESHOLD;
    char c = 128;
    c |= (char)(position >> 4);
    *d_buffer ++ = c;
    c = (char)((position << 4) & 0x0f0) | (match_length & 0x0f);
    *d_buffer = c;
    return 2;
  }
  return 0; // not enough space in the buffer
}










typedef struct {
  char buffer[DICTIONARY_SIZE];
  unsigned int tail;
} Dictionary;



static void dictionray_init(Dictionary *d) {
  memset(d->buffer, 0, DICTIONARY_SIZE);
  const char *ss = "|00.01|02.03|04.05|06.07|08.09|10.11|12.13|14.15|16.17|18.19|20.21|22.23|24.25|26.27|28.29|30.31|32.33|34.35|36.37|38.39|40.41|42.43|44.45|46.47|48.49|50.51|52.53|54.55|56.57|58.59|60.61|62.63|64.65|66.67|68.69|70.71|72.73|74.75|76.77|78.79|80.81|82.83|84.85|86.87|88.89|90.91|92.93|94.95|96.97|98.99|";
  //strncpy(d->buffer, ss, DICTIONARY_SIZE);

  d->tail = DICTIONARY_SIZE - 1;
}

static inline void dictionary_copy_from_buffer(Dictionary *d, const char *s, int n) {
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


static inline int dictionary_find_lonest_match(Dictionary *d, const char *s_buffer, int max, int *position) {
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
  matches[m] ++;
  return m;
}

int compress(Dictionary *d, char *d_buffer, int d_len, const char *s_buffer, int s_len, int *compressed_len) {
  const char *src = s_buffer, *dst = d_buffer;
  int position, m, max;
    
  while(s_len > 0) {
    int len;
    max = LOOKAHEAD_SIZE <= s_len ? LOOKAHEAD_SIZE : s_len;
    m = dictionary_find_lonest_match(d, s_buffer, max, &position);
    if(m == 0) { // symbol is not in the dictionary
      len = write_literal(d_buffer, d_len, *s_buffer);
      m = 1;
    } else if(m == 1) {
      len = write_literal(d_buffer, d_len, *s_buffer);
    } else if(m == 2) { // write two literals instead of a copy
      len = write_literal(d_buffer, d_len, *s_buffer);
      len += write_literal(d_buffer + len, d_len - len, *(s_buffer+1));
    } else {
      len = write_copy(d_buffer, d_len, position, m);
    }
    if(len != 0) { // d_buffer does not have enough space
      d_buffer += len;
      d_len -= len;
      dictionary_copy_from_buffer(d, s_buffer, m);
      s_buffer += m;
      s_len -= m;
    } else {
      break;
    }
  }
  *compressed_len = d_buffer - dst;
  return s_buffer - src;
}

int read_symbol(const char *s_buffer, int s_len, char *character, int *position, int *m) {
  if(s_len >= 1) {
    char c1 = *s_buffer;
    if(c1 & 128) { // literal or copy
      if(s_len >= 2) {
        s_buffer ++;
        char c2 = *s_buffer;
        *m = c2 & 0x0f;
        if(*m == 0) { // literal > 127
          *character = c1;
          *m = 1;
        } else {
          *position = ((c1 & 127) << 4) | ((c2 & 0x0f0) >> 4);
          *m += THRESHOLD;
        }
        return 2;
      }
    } else { // literal
      *character = c1;
      *m = 1;
      return 1;
    }
  }
  return 0; // nothing to read or not enough to read
}

static inline void dictionary_copy_to_buffer(Dictionary *d, char *buf, int position, int n) {
  assert(position <= DICTIONARY_SIZE);
  while(n) {
    *buf ++ = d->buffer[position];
    ++ position;
    position %= DICTIONARY_SIZE;
    -- n;
  }
}

int decompress(Dictionary *d, char *d_buffer, int d_len, const char *s_buffer, int s_len, int *decompressed_len) {
  const char *src = s_buffer, *dst = d_buffer;
  int position, m, len;
  char character;

  while(s_len > 0) {
    len = read_symbol(s_buffer, s_len, &character, &position, &m);
    if(len != 0 && d_len >= m) {
      if(len == 1 || (len == 2 && m == 1)) { // literal
        *d_buffer = character;
        dictionary_copy_from_buffer(d, s_buffer, 1);
      } else { // copy
        dictionary_copy_to_buffer(d, d_buffer, position, m);
        dictionary_copy_from_buffer(d, d_buffer, m);
      }
      s_buffer += len;
      s_len -= len;
      d_buffer += m;
      d_len -= m;
    } else {
      break;
    }
  }

  *decompressed_len = d_buffer - dst;
  return s_buffer - src;
}
/*
int encode(Dictionary *d, char *d_buffer, int d_len, const char *s_buffer, int s_len, int *encoded_len) {
  const char *src = s_buffer, *dst = d_buffer;
  int i, j, max, position, m, r, s, bufferend, len, c;
  char buffer[DICTIONARY_SIZE * 2];

  for(i = 0; i < DICTIONARY_SIZE - LOOKAHEAD_SIZE; i++) {
    buffer[i] = ' ';
  }
  for(i = DICTIONARY_SIZE - LOOKAHEAD_SIZE; i < DICTIONARY_SIZE * 2; i++) {
    if(s_len == 0) break;
    buffer[i] = *s_buffer ++;
    s_len --;
    textcount++;
  }
  bufferend = i;
  r = DICTIONARY_SIZE - LOOKAHEAD_SIZE;
  s = 0;
  while(r < bufferend) {
    max = (LOOKAHEAD_SIZE <= bufferend - r) ? LOOKAHEAD_SIZE : bufferend - r;
    position = 0;
    m = 1;
    c = buffer[r];
    for(i = r - 1; i >= s; i--) {
      if(buffer[i] == c) {
        for(j = 1; j < max; j++) {
          if(buffer[i + j] != buffer[r + j]) {
            break;
          }
        }
        if(j > m) {
          position = i;
          m = j;
        }
      }
    }
    matches[m] ++;
    if(m <= 1) {
      len = write_literal(d_buffer, d_len, *s_buffer);
      m = 1;
    } else {
      len = write_copy(d_buffer, d_len, position, m - 1);
    }
    if(len != 0) {
      d_buffer += len;
      r += m;
      s += m;
      if(r >= DICTIONARY_SIZE * 2 - LOOKAHEAD_SIZE) {
        for(i = 0; i < DICTIONARY_SIZE; i++) {
          buffer[i] = buffer[i + DICTIONARY_SIZE];
        }
        bufferend -= DICTIONARY_SIZE;
        r -= DICTIONARY_SIZE;
        s -= DICTIONARY_SIZE;
        while(bufferend < DICTIONARY_SIZE * 2) {
          if(s_len == 0) {
            break;
          }
          buffer[bufferend++] = *s_buffer ++;
          s_len --;
        }
      }
    } else { // d_buffer does not have enough space
      break;
    }
  }
  *encoded_len = d_buffer - dst;
  return s_buffer - src;
}
*/

#include <sys/time.h>
#define BUFFER_SIZE 180000
char s_buffer[BUFFER_SIZE+1];
char d_buffer[BUFFER_SIZE+1];

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
    int i;
    int d_len;
    int len;

    s_len = fread(s_buffer, 1, BUFFER_SIZE, s_file);
    if(enc) {
      compress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, s_len, &len); // just make sure that the output buffer is large enough here,
                                                                                   // dealing with the other case is not worth the time
      fwrite(d_buffer, 1, len, d_file);
      textcount += s_len;
      codecount += len;
    } else {
      decompress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, s_len, &len); // just make sure that the output buffer is large enough here,
                                                                                   // dealing with the other case is not worth the time
      fwrite(d_buffer, 1, len, d_file);
      textcount += s_len;
      codecount += len;
    }
  }

  gettimeofday(&t2, NULL);
  fprintf(stderr, "Finished in about %.0f milliseconds. \n", (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0);
  printf("text:  %ld bytes\n", textcount);
  printf("code:  %ld bytes (%ld%%)\n", codecount, (codecount * 100) / textcount);
  int i;
  for(i = 0; i < AAA; i ++) {
    printf("match[%2d] %5d\n", i, matches[i]);
  }
  fclose(d_file);
  fclose(s_file);
  return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include "lzss.h"

struct Dictionary;

#define BUFFER_SIZE (128)
// dictionary will reset when these many bytes have been written to input.
// set it to zero to disable this behavior.
#define PACKET_SIZE (8*BUFFER_SIZE)

int main(int argc, char *argv[]) {

  Dictionary dictionary;
  uint8_t s_buffer[BUFFER_SIZE];
  uint8_t d_buffer[BUFFER_SIZE];
  unsigned long codecount = 0, textcount = 0;
  struct timeval t1, t2;
  FILE *s_file, *d_file;

  if(argc != 4 || (!strcmp(argv[1], "c") && !strcmp(argv[1], "d"))) {
    printf("Usage: lzss c/d infile outfile\n\tc = compress\td = decompress\n\n");
    return 1;
  }

  if((s_file  = fopen(argv[2], "rb")) == NULL) {
    printf("cannot open infile %s\n", argv[2]);
    return 1;
  }
  if((d_file = fopen(argv[3], "wb")) == NULL) {
    printf("cannot open outfile %s\n", argv[3]);
    fclose(s_file);
    return 1;
  }

  gettimeofday(&t1, NULL);

  lzss_dictionary_init(&dictionary);

  size_t s_len = 0;
  size_t len;
  long packet_len = PACKET_SIZE;

  size_t bytes_read;

  const int compressing = strcmp(argv[1], "c") == 0;

  size_t s_unused_bytes = 0;

  while((bytes_read = fread(s_buffer + s_unused_bytes, 1, BUFFER_SIZE - s_unused_bytes, s_file)) > 0 || s_len > 0) {
    s_len = bytes_read + s_unused_bytes;

    if(PACKET_SIZE == 0) { // disable packeting
      packet_len = BUFFER_SIZE;
    }

    if(compressing) {
      len = lzss_compress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, s_len, &s_unused_bytes, packet_len);
      packet_len -= len;
    } else {
      len = lzss_decompress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, s_len, &s_unused_bytes, packet_len);
      packet_len -= s_len - s_unused_bytes;
    }

    fwrite(d_buffer, 1, len, d_file);

    textcount += bytes_read;
    codecount += len;

    assert(packet_len >= 0);

    if(packet_len == 0) {
      packet_len = PACKET_SIZE;
      lzss_dictionary_init(&dictionary);
    }

    memmove(s_buffer, s_buffer + s_len - s_unused_bytes, s_unused_bytes); // copy unused bytes to be decompressed next
  }

  gettimeofday(&t2, NULL);
  fprintf(stderr, "Finished in about %.0f milliseconds. \n", (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0);
  printf("text:  %ld bytes\n", textcount);
  printf("code:  %ld bytes (%ld%%)\n", codecount, (codecount * 100) / textcount);

  fclose(d_file);
  fclose(s_file);
  return 0;
}


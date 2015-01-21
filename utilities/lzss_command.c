#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include "../lzss.h"


struct Dictionary;


#define BUFFER_SIZE (128)
// dictionary will reset when these many bytes have been written to input.
// set it to zero to disable this behavior.
#define PACKET_SIZE (8*BUFFER_SIZE)

int main(int argc, char *argv[]) {

  Dictionary dictionary;
  char s_buffer[BUFFER_SIZE];
  char d_buffer[BUFFER_SIZE];
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

  dictionray_init(&dictionary);

  int s_len;
  int len;
  char *s_buffer_start = s_buffer; // used to adjust the input buffer when the s_len is not zero
  int packet_len = PACKET_SIZE;

  int bytes_read;

  const int compressing = strcmp(argv[1], "c") == 0;

  while((bytes_read = fread(s_buffer_start, 1, BUFFER_SIZE - s_len, s_file)) > 0 || s_len > 0) {
    s_len += bytes_read;
    const int original_s_len = s_len;

    if(PACKET_SIZE == 0) { // disable packeting
      packet_len = BUFFER_SIZE;
    }

    if(compressing) {
      len = compress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, &s_len, packet_len);
      packet_len -= len;
    } else {
      len = decompress(&dictionary, d_buffer, BUFFER_SIZE, s_buffer, &s_len, packet_len);
      packet_len -= original_s_len - s_len;
    }

    fwrite(d_buffer, 1, len, d_file);

    textcount += bytes_read;
    codecount += len;

    //printf("bytes_read: %3d, original_s_len: %3d, s_len: %3d, packet_len: %4d\n", bytes_read, original_s_len, s_len, packet_len);
    //printf("bytes_read: %3d, s_len: %3d, packet_len: %4d\n", bytes_read, s_len, packet_len);
    assert(packet_len >= 0);

    if(packet_len == 0) {
      packet_len = PACKET_SIZE;
      dictionray_init(&dictionary);
    }

    if(s_len != 0) { // some bytes are unused; copy them to be decompressed next.
      memmove(s_buffer, s_buffer + original_s_len - s_len, s_len);
      s_buffer_start = s_buffer + s_len;
    } else {
      // reset src just in case if it was assigned before
      s_buffer_start = s_buffer;
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


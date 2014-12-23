//#include "neurobat.h"
#include "logger.h"

extern "C"
{
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lzjb.c"
}

//CppUTest includes should be after your and system includes
#include "CppUTest/TestHarness.h"


const char * text =
    "[0x0004][N] MAX_TFLOW: 53.89"
    "[0x8026][O] OFT: 10.17 C"
    "[0xFFFF][L] ???@250 FF FF FF FF FF 0C"
    "[0x8029][O] Step time: 82720; Function calls: 3336; OTSE: 437.00 Ohm"
    "[0x8037][C] OTS1: 940.53 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.42; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.51 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.42; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.56 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.39; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.62 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.36; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.68 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.33; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.65 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.30; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.71 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.27; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.80 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.24; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.84 Ohm-5.15 C"
    "[0xFFFF][L] ???@248 FF FF FF FF FF FF FF 0C"
    "[0x8032][C] FTS: 27.21; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.89 Ohm-5.15 C"
    "[0x8032][C] FTS: 27.20; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.88 Ohm-5.15 C"
    "[0x8032][C] FTS: 27.17; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.85 Ohm-5.15 C"
    "[0x8032][C] FTS: 27.14; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.76 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.11; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.69 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.08; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x8037][C] OTS1: 940.57 Ohm-5.16 C"
    "[0x8032][C] FTS: 27.06; OTSE: 437.00 Ohm- 25.00 C; Hour 22"
    "[0x801B][O] 1001905 seconds since last (re)boot"
    "[0x8005][U] Arena: 809504, fordblks: 692612, maxfree: 677356"
    "[0x8028][O] Schedule detected: 000000001111111111111100"
    "[0x801F][O] Mode: Comfort"
    "[0x802E][O] 24-hour presence prediction 111111111111111111111111"
    "[0x8036][O] OTS1: 5.16 C; RTS: 22.90 C; FTS: 27.05 C; SRS: 0.00 W-m2; RFTS: 25.34 C"
    "[0x000E][N] Building model prediction: 22.82 C; Error: 0.08 C"
    "[0xFFFF][L] ???@252 FF FF FF 0C"
    ;



TEST_GROUP(LZJB) {

  void setup() {
    int len = strlen(text);
    printf("%d\n", len);
  }

  void teardown() {
  }

};

#define LEMPEL_SIZE 256
uint16_t lempel_table[LEMPEL_SIZE];

TEST(LZJB, LZJB_Compress_CompressionRatio) {

  int len = strlen((const char*)text);
  const int MAX_BUFFER_SIZE = 1024 * 4;
  char * buffer = (char *) malloc(MAX_BUFFER_SIZE);

  char * temp = (char *) malloc(len * 4 + 50);
  strcpy(temp, text);
  strcpy(&temp[len], text);
  strcpy(&temp[len*2], text);

  printf("uncompressed: %d\n", (int) strlen(temp));

  int l;
  l = compress((uchar_t*)temp, (uchar_t*)buffer, strlen(temp), lempel_table, LEMPEL_SIZE);

  printf("compressed: %d\n", l);

  CHECK(l > 0);
}







#include "common.h"


inline int minimum(int a, int b) {
  return a <= b ? a : b;
}

/*
 * copy minimum(n, max) bytes from source to destination.
 * add a null character to destination if there is room for it and n is positive.
 * return the number of bytes copied excluding the null character.
 */
int memnmcpy(char *dst, const char *src, int n, int max) {
  int l = minimum(n, max);
  n = l;

  while(l > 0) {
    *dst ++ = *src ++;
    -- l;
  }
  if(max > n && n > 0) {
    *dst = 0;
  }

  return n >= 0 ? n : 0;
}


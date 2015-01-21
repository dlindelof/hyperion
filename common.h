#ifndef COMMON_H_
#define COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

int minimum(int a, int b);
int memnmcpy(char *dst, const char *src, int n, int max);

#ifdef __cplusplus
}
#endif

#endif // COMMON_H_

#pragma once
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

  // NOTE The following functions are not available in the OFW, so we need to reimplement them as a polyfill.

  char *strcat_(char *dest, const char *src);
  char *rawmemchr_(char *s, char c);
  char *strpbrk_(char *s1, const char *s2);
  char *strtok_(char *s, const char *delim);

#ifdef __cplusplus
}
#endif

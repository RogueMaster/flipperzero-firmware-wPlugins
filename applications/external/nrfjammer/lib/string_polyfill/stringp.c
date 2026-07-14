#include <stringp.h>

char *strcat_(char *dest, const char *src) {
  int len = strlen(dest);
  strcpy(dest + len, src);
  return dest;
}

char *rawmemchr_(char *s, char c) {
  int len = strlen(s);
  return memchr(s, c, len);
}

char *strpbrk_(char *s1, const char *s2) {
  if((s1 == NULL) || (s2 == NULL))
    return NULL;

  while(*s1) {
    // Return s1 char position if found in s2
    if(strchr(s2, *s1)) {
      return s1;
    } else {
      s1++;
    }
  }
  return NULL;
}

char *strtok_(char *s, const char *delim) {
  static char *olds;
  char *token;

  if (s == NULL)
    s = olds;

  // Scan leading delimiters
  s += strspn(s, delim);
  if (*s == '\0') {
    olds = s;
    return NULL;
  }

  // Find the end of the token.
  token = s;
  s = strpbrk_(token, delim);
  if (s == NULL)
    // This token finishes the string.
    olds = rawmemchr_(token, '\0');
  else {
    // Terminate the token and make OLDS point past it.
    *s = '\0';
    olds = s + 1;
  }
  return token;
}

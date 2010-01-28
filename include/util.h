#ifndef __UTIL_H
#define __UTIL_H

int readline(int fd, char* buffer, unsigned int maxlen);
int strpos(const char* str, char c);
inline int max(int a, int b);
inline int min(int a, int b);
int fromHex(char digit);
char toHex(int digit);

#endif

/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include <string.h>
#include "fileio.h"

/// read a line from the specified file handle, up to maxlen characters long, into the buffer
int readline(int fd, char* buffer, unsigned int maxlen) {
	char ch = '\0';
	buffer[0] = '\0';
	int res = 1, n = 0;
	
	while ((ch != 13)) {
		res=fioRead(fd, &ch, 1);
		
		if (res != 1) {
			res = 0;
			break;
		}
		
		if(ch != 10 && ch != 13) {
			buffer[n++] = ch;
			buffer[n] = '\0';
		} else {
			break;
		}
		
		if (n == maxlen - 1)
			break;
	};

	buffer[n] = '\0';
	return res;
}

// as there seems to be no strpos in my libs (?)
int strpos(const char* str, char c) {
	int pos;
	
	for (pos = 0; *str; ++pos, ++str)
		if (*str == c)
			return pos;

	return -1;
}

// a simple maximum of two inline
inline int max(int a, int b) {
	return a > b ? a : b;
}

// a simple minimum of two inline
inline int min(int a, int b) {
	return a < b ? a : b;
}

// single digit from hex decode
int fromHex(char digit) {
	if ((digit >= '0') && (digit <= '9')) {
		return (digit - '0');
	} else if ( (digit >= 'A') && (digit <= 'F') ) {
		return (digit - 'A' + 10);
	} else if ( (digit >= 'a') && (digit <= 'f') ) {
		return (digit - 'a' + 10);	
	} else
		return -1;
}

static const char htab[16] = "0123456789ABCDEF";
char toHex(int digit) {
	return htab[digit & 0x0F];
}

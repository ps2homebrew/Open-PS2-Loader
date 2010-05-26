#ifndef __UTIL_H
#define __UTIL_H

#include <gsToolkit.h>

int getFileSize(int fd);
int openFile(char* path, int mode);
void* readFile(char* path, int align, int* size);
int listDir(char* path, char* separator, int maxElem,
		int (*readEntry)(int index, char *path, char* separator, char* name, unsigned int mode));

typedef struct {
	int fd;
	char* buffer;
	unsigned int size;
	unsigned int available;
	char* lastPtr;
	short allocResult;
} read_context_t;

read_context_t* openReadContext(char* fpath, short allocResult, unsigned int size);
int readLineContext(read_context_t* readContext, char** outBuf);
void closeReadContext(read_context_t* readContext);

inline int max(int a, int b);
inline int min(int a, int b);
int fromHex(char digit);
char toHex(int digit);

#endif

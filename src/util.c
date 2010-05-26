/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/util.h"
#include "include/ioman.h"
#include <io_common.h>
#include <string.h>
#include "malloc.h"
#include "fileio.h"


static int mcID = -1;

int getFileSize(int fd) {
	int size = fioLseek(fd, 0, SEEK_END);
	fioLseek(fd, 0, SEEK_SET);
	return size;
}

static int checkMC() {
	if (mcID == -1) {
		int fd = fioDopen("mc0:OPL");
		if(fd < 0) {
			fd = fioDopen("mc1:OPL");
			if(fd < 0) {
				// No base dir found on any MC, will create the folder
				if (fioMkdir("mc0:OPL") >= 0)
					mcID = 0x30;
				else if (fioMkdir("mc1:OPL") >= 0)
					mcID = 0x31;
			}
			else {
				fioDclose(fd);
				mcID = 0x31;
			}
		}
		else {
			fioDclose(fd);
			mcID = 0x30;
		}
	}
	return mcID;
}

static int checkFile(char* path, int mode) {
	// check if it is mc
	if (strncmp(path, "mc", 2) == 0) {

		// if user didn't explicitly asked for a MC (using '?' char)
		if (path[2] == 0x3F) {

			// Use default detected card
			if (checkMC() >= 0)
				path[2] = mcID;
			else
				return 0;
		}

		// in create mode, we check that the directory exist, or create it
		if (mode & O_CREAT) {
			char dirPath[255];
			char* pos = strrchr(path, '/');
			if (pos) {
				memcpy(dirPath, path, pos - path);
				dirPath[pos - path] = '\0';
				int fd = fioDopen(dirPath);
				if (fd < 0) {
					if (fioMkdir(dirPath) < 0)
						return 0;
				}
				else
					fioDclose(fd);
			}
		}
	}
	return 1;
}

int openFile(char* path, int mode) {
	if (checkFile(path, mode))
		return fioOpen(path, mode);
	else
		return -1;
}

void* readFile(char* path, int align, int* size) {
	void *buffer = NULL;

	int fd = openFile(path, O_RDONLY);
	if (fd >= 0) {
		unsigned int realSize = getFileSize(fd);

		if (*size > 0 && *size != realSize) {
			LOG("Invalid filesize, expected: %d, got: %d\n", *size, realSize);
			fioClose(fd);
			return NULL;
		}

		if (align > 0)
			buffer = memalign(128, realSize); // The allocation is aligned to aid the DMA transfers
		else
			buffer = malloc(realSize);

		fioRead(fd, buffer, realSize);
		fioClose(fd);
		*size = realSize;
	}
	return buffer;
}

int listDir(char* path, char* separator, int maxElem,
		int (*readEntry)(int index, char *path, char* separator, char* name, unsigned int mode)) {
	int fdDir, index = 0;
	if (checkFile(path, O_RDONLY)) {
		fio_dirent_t record;

		fdDir = fioDopen(path);
		if (fdDir > 0) {
			while (index < maxElem && fioDread(fdDir, &record) > 0)
				index = readEntry(index, path, separator, record.name, record.stat.mode);

			fioDclose(fdDir);
		}
	}
	return index;
}

/* size will be the maximum line size possible */
read_context_t* openReadContext(char* fpath, short allocResult, unsigned int size) {
	read_context_t* readContext = NULL;

	int fd = openFile(fpath, O_RDONLY);
	if (fd >= 0) {
		readContext = (read_context_t*) malloc(sizeof(read_context_t));
		readContext->size = size;
		readContext->available = 0;
		readContext->buffer = (char*) malloc(size * sizeof(char));
		readContext->lastPtr = NULL;
		readContext->allocResult = allocResult;
		readContext->fd = fd;
	}

	return readContext;
}

int readLineContext(read_context_t* readContext, char** outBuf) {
	int lineSize = 0, read, length;
	char* posLF = NULL;

	while (1) {
		// if lastPtr is set, then we continue the read from this point as reference
		if (readContext->lastPtr) {
			// Calculate the remaining chars to the right of lastPtr
			lineSize = readContext->available - (readContext->lastPtr - readContext->buffer);
			/*LOG("##### Continue read, position: %X (total: %d) line size (\\0 not inc.): %d end: %x\n",
					readContext->lastPtr - readContext->buffer, readContext->available, lineSize, readContext->lastPtr[lineSize]);*/
			posLF = strchr(readContext->lastPtr, 0x0A);
		}

		if (!posLF) { // We can come here either when the buffer is empty, or if the remaining chars don't have a LF

			// if available, we shift the remaining chars to the left ...
			if (lineSize) {
				//LOG("##### LF not found, Shift %d characters from end to beginning\n", lineSize);
				memmove(readContext->buffer, readContext->lastPtr, lineSize);
			}

			// ... and complete the buffer if we're not at EOF
			if (readContext->fd >= 0) {

				// Load as many characters necessary to fill the buffer
				length = readContext->size - lineSize - 1;
				//LOG("##### Asking for %d characters to complete buffer\n", length);
				read = fioRead(readContext->fd, readContext->buffer + lineSize, length);
				readContext->buffer[lineSize + read] = '\0';

				// Search again (from the lastly added chars only), the result will be "analyzed" in next if
				posLF = strchr(readContext->buffer + lineSize, 0x0A);

				// Now update read context info
				lineSize = lineSize + read;
				//LOG("##### %d characters really read, line size now (\\0 not inc.): %d\n", read, lineSize);

				// If buffer not full it means we are at EOF
				if (readContext->size != lineSize + 1) {
					//LOG("##### Reached EOF\n");
					fioClose(readContext->fd);
					readContext->fd = -1;
				}
			}

			readContext->lastPtr = readContext->buffer;
			readContext->available = lineSize;
		}

		if(posLF)
			lineSize = posLF - readContext->lastPtr;

		// Check the previous char (on Windows there are CR/LF instead of single linux LF)
		if (lineSize)
			if (*(readContext->lastPtr + lineSize - 1) == 0x0D)
				lineSize--;

		readContext->lastPtr[lineSize] = '\0';
		*outBuf = readContext->lastPtr;

		//LOG("##### Result line is \"%s\" size: %d avail: %d pos: %d\n", readContext->lastPtr, lineSize, readContext->available, readContext->lastPtr - readContext->buffer);

		// If we are at EOF and no more chars available to scan, then we are finished
		if (!lineSize && !readContext->available && readContext->fd == -1)
			return 0;

		if (readContext->lastPtr[0] == 0x23) {// '#' for comment lines
			if (posLF)
				readContext->lastPtr = posLF + 1;
			else
				readContext->lastPtr = NULL;
			continue;
		}

		if (lineSize && readContext->allocResult) {
			*outBuf = (char*) malloc((lineSize + 1) * sizeof(char));
			memcpy(*outBuf, readContext->lastPtr,  lineSize + 1);
		}

		// Either move the pointer to next chars, or set it to null to force a whole buffer read (if possible)
		if (posLF)
			readContext->lastPtr = posLF + 1;
		else {
			readContext->lastPtr = NULL;
		}

		return 1;
	}
}

void closeReadContext(read_context_t* readContext) {
	if (readContext->fd >= 0)
		fioClose(readContext->fd);
	free(readContext->buffer);
	free(readContext);
	readContext = NULL;
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

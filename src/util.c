/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/util.h"
#include "include/ioman.h"
#include <io_common.h>
#include <string.h>
#include <malloc.h>
#include <fileio.h>
#include <osd_config.h>

extern void *icon_sys;
extern int size_icon_sys;
extern void *icon_icn;
extern int size_icon_icn;

static int mcID = -1;

int getFileSize(int fd) {
	int size = fioLseek(fd, 0, SEEK_END);
	fioLseek(fd, 0, SEEK_SET);
	return size;
}

static void writeMCIcon() {
	void** buffer = &icon_icn;
	file_buffer_t* fileBuffer = openFileBuffer("mc?:OPL/opl.icn", O_WRONLY | O_CREAT | O_TRUNC, 0, 0);
	if (fileBuffer) {
		writeFileBuffer(fileBuffer, (char *)buffer, size_icon_icn);
		closeFileBuffer(fileBuffer);
	}

	buffer = &icon_sys;
	fileBuffer = openFileBuffer("mc?:OPL/icon.sys", O_WRONLY | O_CREAT | O_TRUNC, 0, 0);
	if (fileBuffer) {
		writeFileBuffer(fileBuffer, (char *)buffer, size_icon_sys);
		closeFileBuffer(fileBuffer);
	}
}

static int checkMC() {
	if (mcID == -1) {
		int fd = fioDopen("mc0:OPL");
		if(fd < 0) {
			fd = fioDopen("mc1:OPL");
			if(fd < 0) {
				// No base dir found on any MC, will create the folder
				if (fioMkdir("mc0:OPL") >= 0) {
					mcID = 0x30;
					writeMCIcon();
				}
				else if (fioMkdir("mc1:OPL") >= 0) {
					mcID = 0x31;
					writeMCIcon();
				}
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
			char dirPath[256];
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

		if ((*size > 0) && (*size != realSize)) {
			LOG("UTIL Invalid filesize, expected: %d, got: %d\n", *size, realSize);
			fioClose(fd);
			return NULL;
		}

		if (align > 0)
			buffer = memalign(128, realSize); // The allocation is aligned to aid the DMA transfers
		else
			buffer = malloc(realSize);

		if (!buffer) {
			LOG("UTIL ReadFile: Failed allocation of %d bytes", realSize);
			*size = 0;
		} else {
			fioRead(fd, buffer, realSize);
			fioClose(fd);
			*size = realSize;
		}
	}
	return buffer;
}

void checkCreateDir(char* dirPath) {
	int fd = fioDopen(dirPath);
	if (fd < 0)
		fioMkdir(dirPath);
	else
		fioDclose(fd);
}

int listDir(char* path, const char* separator, int maxElem,
		int (*readEntry)(int index, const char *path, const char* separator, const char* name, unsigned int mode)) {
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
file_buffer_t* openFileBuffer(char* fpath, int mode, short allocResult, unsigned int size) {
	file_buffer_t* fileBuffer = NULL;

	int fd = openFile(fpath, mode);
	if (fd >= 0) {
		fileBuffer = (file_buffer_t*) malloc(sizeof(file_buffer_t));
		fileBuffer->size = size;
		fileBuffer->available = 0;
		fileBuffer->buffer = (char*) malloc(size * sizeof(char));
		if (mode == O_RDONLY)
			fileBuffer->lastPtr = NULL;
		else
			fileBuffer->lastPtr = fileBuffer->buffer;
		fileBuffer->allocResult = allocResult;
		fileBuffer->fd = fd;
		fileBuffer->mode = mode;
	}

	return fileBuffer;
}

int readFileBuffer(file_buffer_t* fileBuffer, char** outBuf) {
	int lineSize = 0, read, length;
	char* posLF = NULL;

	while (1) {
		// if lastPtr is set, then we continue the read from this point as reference
		if (fileBuffer->lastPtr) {
			// Calculate the remaining chars to the right of lastPtr
			lineSize = fileBuffer->available - (fileBuffer->lastPtr - fileBuffer->buffer);
			/*LOG("##### Continue read, position: %X (total: %d) line size (\\0 not inc.): %d end: %x\n",
					fileBuffer->lastPtr - fileBuffer->buffer, fileBuffer->available, lineSize, fileBuffer->lastPtr[lineSize]);*/
			posLF = strchr(fileBuffer->lastPtr, 0x0A);
		}

		if (!posLF) { // We can come here either when the buffer is empty, or if the remaining chars don't have a LF

			// if available, we shift the remaining chars to the left ...
			if (lineSize) {
				//LOG("##### LF not found, Shift %d characters from end to beginning\n", lineSize);
				memmove(fileBuffer->buffer, fileBuffer->lastPtr, lineSize);
			}

			// ... and complete the buffer if we're not at EOF
			if (fileBuffer->fd >= 0) {

				// Load as many characters necessary to fill the buffer
				length = fileBuffer->size - lineSize - 1;
				//LOG("##### Asking for %d characters to complete buffer\n", length);
				read = fioRead(fileBuffer->fd, fileBuffer->buffer + lineSize, length);
				fileBuffer->buffer[lineSize + read] = '\0';

				// Search again (from the lastly added chars only), the result will be "analyzed" in next if
				posLF = strchr(fileBuffer->buffer + lineSize, 0x0A);

				// Now update read context info
				lineSize = lineSize + read;
				//LOG("##### %d characters really read, line size now (\\0 not inc.): %d\n", read, lineSize);

				// If buffer not full it means we are at EOF
				if (fileBuffer->size != lineSize + 1) {
					//LOG("##### Reached EOF\n");
					fioClose(fileBuffer->fd);
					fileBuffer->fd = -1;
				}
			}

			fileBuffer->lastPtr = fileBuffer->buffer;
			fileBuffer->available = lineSize;
		}

		if(posLF)
			lineSize = posLF - fileBuffer->lastPtr;

		// Check the previous char (on Windows there are CR/LF instead of single linux LF)
		if (lineSize)
			if (*(fileBuffer->lastPtr + lineSize - 1) == 0x0D)
				lineSize--;

		fileBuffer->lastPtr[lineSize] = '\0';
		*outBuf = fileBuffer->lastPtr;

		//LOG("##### Result line is \"%s\" size: %d avail: %d pos: %d\n", fileBuffer->lastPtr, lineSize, fileBuffer->available, fileBuffer->lastPtr - fileBuffer->buffer);

		// If we are at EOF and no more chars available to scan, then we are finished
		if (!lineSize && !fileBuffer->available && fileBuffer->fd == -1)
			return 0;

		if (fileBuffer->lastPtr[0] == 0x23) {// '#' for comment lines
			if (posLF)
				fileBuffer->lastPtr = posLF + 1;
			else
				fileBuffer->lastPtr = NULL;
			continue;
		}

		if (lineSize && fileBuffer->allocResult) {
			*outBuf = (char*) malloc((lineSize + 1) * sizeof(char));
			memcpy(*outBuf, fileBuffer->lastPtr,  lineSize + 1);
		}

		// Either move the pointer to next chars, or set it to null to force a whole buffer read (if possible)
		if (posLF)
			fileBuffer->lastPtr = posLF + 1;
		else {
			fileBuffer->lastPtr = NULL;
		}

		return 1;
	}
}

void writeFileBuffer(file_buffer_t* fileBuffer, char* inBuf, int size) {
	//LOG("writeFileBuffer avail: %d size: %d\n", fileBuffer->available, size);
	if (fileBuffer->available && fileBuffer->available + size > fileBuffer->size) {
		//LOG("writeFileBuffer flushing: %d\n", fileBuffer->available);
		fioWrite(fileBuffer->fd, fileBuffer->buffer, fileBuffer->available);
		fileBuffer->lastPtr = fileBuffer->buffer;
		fileBuffer->available = 0;
	}

	if (size > fileBuffer->size) {
		//LOG("writeFileBuffer direct write: %d\n", size);
		fioWrite(fileBuffer->fd, inBuf, size);
	}
	else {
		memcpy(fileBuffer->lastPtr, inBuf, size);
		fileBuffer->lastPtr += size;
		fileBuffer->available += size;

		//LOG("writeFileBuffer lastPrt: %d\n", (fileBuffer->lastPtr - fileBuffer->buffer));
	}
}

void closeFileBuffer(file_buffer_t* fileBuffer) {
	if (fileBuffer->fd >= 0) {
		if (fileBuffer->mode != O_RDONLY && fileBuffer->available) {
			//LOG("writeFileBuffer final write: %d\n", fileBuffer->available);
			fioWrite(fileBuffer->fd, fileBuffer->buffer, fileBuffer->available);
		}
		fioClose(fileBuffer->fd);
	}
	free(fileBuffer->buffer);
	free(fileBuffer);
	fileBuffer = NULL;
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

enum CONSOLE_REGIONS{
	CONSOLE_REGION_INVALID	= -1,
	CONSOLE_REGION_JAPAN	= 0,
	CONSOLE_REGION_USA,	//USA and HK/SG.
	CONSOLE_REGION_EUROPE,
	CONSOLE_REGION_CHINA,

	CONSOLE_REGION_COUNT
};

static short int ConsoleRegion=CONSOLE_REGION_INVALID;
static char SystemDataFolderPath[]="BRDATA-SYSTEM";
static char SystemFolderLetter='R';

static void UpdateSystemPaths(void){
	char regions[CONSOLE_REGION_COUNT]={'I', 'A', 'E', 'C'};

	SystemFolderLetter=regions[ConsoleRegion];
	SystemDataFolderPath[1]=SystemFolderLetter;
}

int InitConsoleRegionData(void){
	int result;
	char romver[16];

	if((result=ConsoleRegion)<0){
		GetRomName(romver);

		switch(romver[4]){
			case 'C':
				ConsoleRegion=CONSOLE_REGION_CHINA;
				break;
			case 'E':
				ConsoleRegion=CONSOLE_REGION_EUROPE;
				break;
			case 'H':
			case 'A':
				ConsoleRegion=CONSOLE_REGION_USA;
				break;
			default:	//For Japanese and unidentified consoles.
				ConsoleRegion=CONSOLE_REGION_JAPAN;
		}

		result=ConsoleRegion;

		UpdateSystemPaths();
	}

	return result;
}

const char *GetSystemDataPath(void){
	return SystemDataFolderPath;
}

char GetSystemFolderLetter(void){
	return SystemFolderLetter;
}

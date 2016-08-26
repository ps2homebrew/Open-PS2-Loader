ifndef CC
CC = gcc
endif

CFLAGS = -std=gnu99 -Wall -pedantic -I/usr/include -I/usr/local/include -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifeq ($(_WIN32),1)
	CFLAGS += -D_WIN32
endif

all: bin/opl2iso

clean:
	rm -f -r bin
	rm -f src/*.o

rebuild: clean all
	
bin/opl2iso: src/opl2iso.o
	@mkdir -p bin
	$(CC) $(CFLAGS) src/opl2iso.c -o bin/opl2iso

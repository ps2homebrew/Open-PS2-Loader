ifndef CC
CC = gcc
endif

CFLAGS = -std=gnu99 -Wall -pedantic -I/usr/include -I/usr/local/include -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
#CFLAGS += -DDEBUG

ifeq ($(_WIN32),1)
	CFLAGS += -D_WIN32
endif


all: bin/iso2opl

clean:
	rm -f -r bin
	rm -f src/*.o

rebuild: clean all
	
bin/iso2opl: src/iso2opl.o
	@mkdir -p bin
	$(CC) $(CFLAGS) src/iso2opl.c src/isofs.c -o bin/iso2opl

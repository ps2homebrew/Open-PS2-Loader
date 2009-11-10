ifndef CC
CC = gcc
endif

CFLAGS = -std=gnu99 -Wall -pedantic -I/usr/include -I/usr/local/include -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

all: bin/iso2usbld

clean:
	rm -f bin/*iso2usbld* src/*.o

bin/iso2usbld: src/iso2usbld.o
	@mkdir -p bin
	$(CC) $(CFLAGS) src/iso2usbld.c src/isofs.c -o bin/iso2usbld

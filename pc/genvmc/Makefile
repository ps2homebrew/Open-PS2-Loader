ifndef CC
CC = gcc
endif

CFLAGS = -std=gnu99 -Wall -pedantic -I/usr/include -I/usr/local/include
#CFLAGS += -DDEBUG

ifeq ($(_WIN32),1)
	CFLAGS += -D_WIN32
endif


all: bin/genvmc

clean:
	rm -f -r bin
	rm -f src/*.o

rebuild: clean all

bin/genvmc: src/genvmc.o
	@mkdir -p bin
	$(CC) $(CFLAGS) src/genvmc.c -o bin/genvmc

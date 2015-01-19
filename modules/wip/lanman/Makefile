IOP_BIN  = lanman.irx

IOP_INCS +=
IOP_CFLAGS += -Wall -fno-builtin-printf
IOP_CFLAGS += -DUDPTTY
IOP_LDFLAGS += -s

IOP_OBJS = lanman.o smap.o arp.o tcp.o inet.o imports.o
IOP_OBJS += udptty.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make

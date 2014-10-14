IOP_BIN  = ps2atad.irx

IOP_INCS +=
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s

IOP_OBJS = ps2atad.o exports.o imports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make

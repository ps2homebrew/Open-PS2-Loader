IOP_BIN  = ps2dev9.irx
IOP_OBJS = ps2dev9.o exports.o imports.o
IOP_LIBS =

IOP_INCS +=
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s


all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include Rules.make

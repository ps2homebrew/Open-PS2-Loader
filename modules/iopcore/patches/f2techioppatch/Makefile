IOP_BIN  = f2techioppatch.irx
IOP_OBJS = main.o imports.o

IOP_INCS +=
IOP_CFLAGS += -Os -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../../../Rules.make

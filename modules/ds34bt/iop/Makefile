IOP_BIN = ds34bt.irx

IOP_INCS +=
IOP_CFLAGS += -Wall -fno-builtin
IOP_LDFLAGS += -s

IOP_OBJS = ds34bt.o smsutils.o imports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

include $(PS2SDK)/Defs.make
include ../../Rules.make

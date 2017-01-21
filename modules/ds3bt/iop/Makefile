IOP_BIN = ds3bt.irx

IOP_INCS +=
IOP_CFLAGS += -Wall
IOP_LDFLAGS += -s

IOP_OBJS = ds3bt.o smsutils.o imports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal

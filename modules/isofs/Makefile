IOP_BIN  = isofs.irx

IOP_INCS +=
IOP_CFLAGS += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s

IOP_OBJS = isofs.o imports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

include $(PS2SDK)/Defs.make
include ../Rules.make

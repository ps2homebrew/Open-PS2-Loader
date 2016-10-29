IOP_BIN = nbns.irx
IOP_OBJS = main.o nbns.o imports.o exports.o

IOP_INCS += -I../../../include
IOP_CFLAGS += -Wall -fno-builtin -D_IOP -mno-gpopt -G0
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include ../../Rules.make

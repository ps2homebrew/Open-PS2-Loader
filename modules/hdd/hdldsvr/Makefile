IOP_BIN = hdldsvr.irx
IOP_OBJS = hdldsvr.o imports.o exports.o

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

include $(PS2SDK)/Defs.make
include ../../Rules.bin.make
include ../../Rules.make

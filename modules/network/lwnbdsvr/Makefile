IOP_BIN = lwnbdsvr.irx
IOP_OBJS = lwnbdsvr.o imports.o exports.o drivers/atad.o lwNBD/nbd_server.o

IOP_CFLAGS += -DPS2SDK
IOP_INCS += -I../../iopcore/common   -I../../../include/

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG -DNBD_DEBUG
endif

include $(PS2SDK)/Defs.make
include ../../Rules.bin.make
include ../../Rules.make

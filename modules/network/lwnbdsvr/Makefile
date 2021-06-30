IOP_BIN = lwnbdsvr.irx
IOP_OBJS = lwnbdsvr.o imports.o exports.o drivers/atad.o lwNBD/nbd_server.o

IOP_CFLAGS += -DPS2SDK
IOP_INCS += -I../../iopcore/common -I../../../include/

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG -DNBD_DEBUG
endif

include $(PS2SDK)/Defs.make
include ../../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal

ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
# Due to usage of 64 bit integers, support code for __ashldi3 and __lshrdi3 is needed
IOP_LIBS += -lgcc
endif
endif

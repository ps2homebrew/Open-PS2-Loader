
IOP_BIN = hidusb.irx
IOP_OBJS = hidusb.o imports.o
IOP_OBJS_DIR = obj.hidusb/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make

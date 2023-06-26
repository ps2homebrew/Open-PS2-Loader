
IOP_BIN = ds3bt.irx
IOP_OBJS = ds3bt.o imports.o
IOP_OBJS_DIR = obj.ds3bt/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make

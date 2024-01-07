
IOP_BIN = ds4bt.irx
IOP_OBJS = ds4bt.o imports.o
IOP_OBJS_DIR = obj.ds4bt/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make

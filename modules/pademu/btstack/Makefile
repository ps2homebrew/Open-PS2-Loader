
IOP_BIN = btstack.irx
IOP_OBJS = btstack.o imports.o exports.o
IOP_OBJS_DIR = obj.btstack/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make
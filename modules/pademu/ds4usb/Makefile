
IOP_BIN = ds4usb.irx
IOP_OBJS = ds4usb.o imports.o
IOP_OBJS_DIR = obj.ds4usb/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make

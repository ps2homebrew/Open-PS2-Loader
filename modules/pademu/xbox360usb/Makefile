
IOP_BIN = xbox360usb.irx
IOP_OBJS = xbox360usb.o imports.o
IOP_OBJS_DIR = obj.xbox360usb/

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include ../../Rules.bin.make

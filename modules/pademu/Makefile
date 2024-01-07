
IOP_BIN = pademu.irx
IOP_OBJS = pademu.o sys_utils.o padmacro.o imports.o exports.o
IOP_OBJS_DIR = obj.pademu/

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_CFLAGS += -DUSE_SMSUTILS
IOP_INCS += -I../../include/

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal

IOP_OBJS = pademu_common.o pademu.o sys_utils.o imports.o exports.o padmacro.o

IOP_BIN = pademu.irx

ifeq ($(USE_DS3USB),1)
IOP_CFLAGS += -DDS3USB
IOP_OBJS += ds3usb.o
endif

ifeq ($(USE_DS3BT),1)
IOP_CFLAGS += -DDS3BT
IOP_OBJS += ds3bt.o
endif

ifeq ($(USE_DS4USB),1)
IOP_CFLAGS += -DDS4USB
IOP_OBJS += ds4usb.o
endif

ifeq ($(USE_DS4BT),1)
IOP_CFLAGS += -DDS4BT
IOP_OBJS += ds4bt.o
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_CFLAGS += -DUSE_SMSUTILS
IOP_INCS += -I../../include/

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal
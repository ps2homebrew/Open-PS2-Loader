IOP_OBJS = pademu.o sys_utils.o imports.o exports.o

ifeq ($(USE_USB),1)
IOP_BIN = usb_pademu.irx
IOP_CFLAGS += -DUSB
IOP_OBJS_DIR = obj.usb/
IOP_OBJS += ds34usb.o
endif

ifeq ($(USE_BT),1)
IOP_BIN = bt_pademu.irx
IOP_CFLAGS += -DBT
IOP_OBJS_DIR = obj.bt/
IOP_OBJS += ds34bt.o
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_CFLAGS += -DUSE_SMSUTILS

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal

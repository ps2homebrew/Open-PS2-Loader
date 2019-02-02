
IOP_OBJS = pademu.o sys_utils.o imports.o exports.o

ifeq ($(USE_USB),1)
IOP_BIN = usb_pademu.irx
IOP_CFLAGS += -DUSB
IOP_OBJS_DIR = obj.usb/
IOP_OBJS += ds34usb.o
endif

ifeq ($(USE_BT),1)
IOP_BIN = bt_pademu.irx
IOP_CFLAGS += -DBT -DUSE_THREAD
IOP_OBJS_DIR = obj.bt/
IOP_OBJS += ds34bt.o
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJS_DIR)%)

IOP_CFLAGS += -Wall -fno-builtin -DUSE_SMSUTILS
IOP_LDFLAGS += -s

all: OBJ_DIR $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

OBJ_DIR:
	mkdir -p $(IOP_OBJS_DIR)

include $(PS2SDK)/Defs.make
include Rules.make

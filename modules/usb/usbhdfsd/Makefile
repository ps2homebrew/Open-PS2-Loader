# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: $

IOP_BIN  = usbhdfsd.irx
IOP_OBJS = usb_driver.o fs_driver.o part_driver.o fat_driver.o scache.o usbhdfsd.o imports.o exports.o
IOP_LIBS =

IOP_INCS += -I../../../include/
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s

WRITE_SUPPORT=1

ifeq ($(WRITE_SUPPORT),1)
IOP_CFLAGS += -DWRITE_SUPPORT
IOP_OBJS += fat_write.o
endif

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make

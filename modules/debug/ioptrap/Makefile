# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: Makefile 597 2004-09-21 16:15:05Z pixel $


IOP_BIN  = ioptrap.irx

IOP_INCS += -I$(PS2SDK)/common/include
IOP_CFLAGS += -Wall -fno-builtin-printf #-DTEST_TRAP
IOP_LDFLAGS += -s

IOP_OBJS = ioptrap.o handler.o breakpoint.o imports.o exports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make


# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: $

IOP_BIN  = ps2dev9.irx

IOP_INCS +=
IOP_CFLAGS  += -Wall -fno-builtin-printf -DCHECK_LOADED -DDEV9X_DEV -DPOWEROFF -DPCMCIA #-DDEBUG
IOP_LDFLAGS += -s

IOP_OBJS = ps2dev9.o exports.o imports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../Rules.make

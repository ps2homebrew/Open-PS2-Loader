# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#

IOP_BIN = xhdd.irx
IOP_OBJS = xhdd.o xatad.o hdpro.o imports.o

IOP_CFLAGS += -Wall -fno-builtin $(DEBUG_FLAGS)
IOP_LDFLAGS += -s
IOP_INCS += -I../common

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

rebuild: clean all

include $(PS2SDK)/Defs.make
include ../../Rules.make

#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#                           (C)2003, adresd     ( adresd_ps2dev@yahoo.com )
#  ------------------------------------------------------------------------


IOP_BIN = smap.irx
IOP_OBJS = main.o smap.o xfer.o imports.o

IOP_INCS += -I$(PS2SDK)/iop/include -I../../iopcore/common -I../common
IOP_CFLAGS += -Wall -fno-builtin -D_IOP -Os -mgpopt -G8192
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include ../../Rules.make

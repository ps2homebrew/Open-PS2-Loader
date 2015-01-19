IOP_BIN  = udnl.irx
IOP_OBJS = udnl.o udnl_asm.o imports.o IOPRP_img.o

IOP_CFLAGS  += -Wall -fno-builtin -Os -mno-check-zero-division
IOP_LDFLAGS += -s
IOP_INCS += -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include -I./include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) IOPRP_img.c

IOPRP_img.c:
	$(BIN2C) IOPRP.img IOPRP_img.c IOPRP_img

include $(PS2SDK)/Defs.make
include ../../Rules.make

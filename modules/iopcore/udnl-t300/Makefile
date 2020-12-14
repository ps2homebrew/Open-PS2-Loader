IOP_BIN  = udnl.irx
IOP_OBJS = udnl.o udnl_asm.o imports.o IOPRP_img.o

IOP_INCS += -I./include

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) IOPRP_img.c

IOPRP_img.c:
	$(BIN2C) IOPRP.img IOPRP_img.c IOPRP_img

include $(PS2SDK)/Defs.make
include ../../Rules.make

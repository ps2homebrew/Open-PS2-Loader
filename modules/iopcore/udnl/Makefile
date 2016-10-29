IOP_BIN  = udnl.irx
IOP_OBJS = udnl.o udnl_asm.o imports.o

IOP_CFLAGS  += -Wall -fno-builtin -Os -mno-check-zero-division
IOP_LDFLAGS += -s
IOP_INCS += -I./include
ifeq ($(DECI2_DEBUG),1)
IOP_CFLAGS += -D__DECI2_DEBUG
IOP_OBJS += IOPRP_DECI2_img.o
else
IOP_OBJS += IOPRP_img.o
endif

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) IOPRP_img.c IOPRP_DECI2_img.c

IOPRP_img.c:
	$(BIN2C) IOPRP.img IOPRP_img.c IOPRP_img

IOPRP_DECI2_img.c:
	$(BIN2C) IOPRP_DECI2.img IOPRP_DECI2_img.c IOPRP_img

include $(PS2SDK)/Defs.make
include ../../Rules.make

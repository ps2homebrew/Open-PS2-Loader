IOP_BIN      = SMSUTILS.irx
IOP_OBJS     = SMSUTILS.o
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS  = -Wl,-s

all: $(IOP_BIN)

clean:
	rm -f *.elf *.o *.a *.irx

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release

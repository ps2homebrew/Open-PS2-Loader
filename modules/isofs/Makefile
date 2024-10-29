LZ4 := lz4

IOP_BIN  = isofs.irx
IOP_INCS += -Ilz4/
IOP_OBJS = isofs.o zso.o $(LZ4)/lz4.o imports.o
IOP_CFLAGS += $(IOP_INCS)

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal

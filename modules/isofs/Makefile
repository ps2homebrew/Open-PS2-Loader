#  _____     ___ ____
#   ____|   |    ____|     
#  |     ___|   |____      
#                          
#  ------------------------------------------------------------------------

IOP_BIN = isofs.irx
IOP_OBJS = isofs.o dummy_irx.o imports.o exports.o

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_CFLAGS += -Wall -fno-builtin $(DEBUG_FLAGS)
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	$(MAKE) -C dummy_irx clean
	-rm -f *.o *.s *.irx

dummy_irx.s:
	$(MAKE) -C dummy_irx
	bin2s dummy_irx/dummy.irx dummy_irx.s dummy_irx
	
	
	
include $(PS2SDK)/Defs.make
include Rules.make


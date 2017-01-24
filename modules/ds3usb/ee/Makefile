EE_LIB  = libds3usb.a

EE_OBJS = libds3usb.o
EE_LIBS = -lc
EE_CFLAGS = -Wall

all: $(EE_LIB)

clean:
	rm -f -r $(EE_OBJS) $(EE_LIB)

rebuild: clean all	

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

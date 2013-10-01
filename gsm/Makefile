EE_BIN = gsm.elf
EE_OBJS = engine.o gsm.o 
MAPFILE = gsm.map

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -Iinclude -I.
EE_CFLAGS = -D_EE -O2 -G0 -Wall $(EE_INCS)
ifeq ($(EESIO_DEBUG),1) 
EE_CFLAGS += -D__EESIO_DEBUG
endif
ifeq ($(LOAD_DEBUG_MODULES),1)
EE_CFLAGS += -D__LOAD_DEBUG_MODULES
endif
ifeq ($(VMC),1)
EE_CFLAGS += -DVMC
endif

EE_LDFLAGS = -nostartfiles -Tlinkfile -L$(PS2SDK)/ee/lib -L$(PS2SDK)/sbv/lib -s
EE_LIBS += -lpatches

ifeq ($(EESIO_DEBUG),1) 
EE_LIBS += -lc
endif

EE_LIBS += -lkernel


%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS) -Xlinker -Map -Xlinker $(MAPFILE)

clean:
	rm -f *.o *.a *.map *.elf

clean_all:
	rm -f *.o *.a *.map *.elf

include $(PS2SDK)/samples/Makefile.pref


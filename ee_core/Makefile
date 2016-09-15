# Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
# Copyright 2006-2008 Polo
# Licenced under Academic Free License version 3.0
# Review OPNPS2LD README & LICENSE files for further details.
#
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Standard Makefile

EE_BIN = ee_core.elf

GSMCORE_EE_OBJS = obj/gsm_engine.o obj/gsm_api.o
IGSCORE_EE_OBJS = obj/igs_api.o
CHEATCORE_EE_OBJS = obj/cheat_engine.o obj/cheat_api.o

EE_OBJS = obj/main.o obj/syshook.o obj/iopmgr.o obj/modmgr.o obj/util.o obj/patches.o obj/patches_asm.o \
	  obj/padhook.o obj/spu.o obj/tlb.o obj/asm.o obj/crt0.o
MAPFILE = ee_core.map

EE_SRC_DIR = src/
EE_OBJS_DIR = obj/

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -Iinclude -I.
EE_CFLAGS = -D_EE -Os -G0 -Wall $(EE_INCS)

ifeq ($(EESIO_DEBUG),1) 
EE_CFLAGS += -D__EESIO_DEBUG
endif

ifeq ($(DTL_T10000),1)
	EE_CFLAGS += -D_DTL_T10000
endif

ifeq ($(LOAD_DEBUG_MODULES),1)
EE_CFLAGS += -D__LOAD_DEBUG_MODULES
	ifeq ($(DECI2_DEBUG),1)
	EE_CFLAGS += -D__DECI2_DEBUG
	endif
endif

ifeq ($(VMC),1)
EE_CFLAGS += -DVMC
endif

ifeq ($(GSM),1)
EE_OBJS += $(GSMCORE_EE_OBJS)
EE_CFLAGS += -DGSM
endif

ifeq ($(IGS),1)
EE_OBJS += $(IGSCORE_EE_OBJS)
EE_CFLAGS += -DIGS
EE_LIBS += -lmc
endif

ifeq ($(CHEAT),1)
EE_OBJS += $(CHEATCORE_EE_OBJS)
EE_CFLAGS += -DCHEAT
endif

ifeq ($(IGS),1)
EE_LDFLAGS = -nostartfiles -Tlinkfile.igs -L$(PS2SDK)/ee/lib -s -Wl,-Map,$(MAPFILE)
else
EE_LDFLAGS = -nostartfiles -Tlinkfile -L$(PS2SDK)/ee/lib -s -Wl,-Map,$(MAPFILE)
endif
EE_LIBS += -lpatches

ifeq ($(EESIO_DEBUG),1) 
EE_LIBS += -lc
endif

EE_LIBS += -lkernel


$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	@mkdir -p obj
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.S
	@mkdir -p obj
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.s
	@mkdir -p obj
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS)

all:
	$(EE_BIN)

clean:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)

clean_all:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)


include $(PS2SDK)/samples/Makefile.pref

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
# Loader section begin at 0x000e7000
# g_buf is located at 0x00088000

EE_BIN = ee_core.elf
GSMCORE_EE_OBJS = obj/gsm_engine.o obj/gsm_api.o
EE_OBJS = obj/main.o obj/syshook.o obj/iopmgr.o obj/modmgr.o obj/util.o obj/patches.o \
	  obj/padhook.o obj/spu.o obj/smbauth.o obj/des.o obj/md4.o obj/tlb.o obj/asm.o obj/crt0.o
MAPFILE = ee_core.map

EE_SRC_DIR = src/
EE_OBJS_DIR = obj/

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
ifeq ($(GSM),1)
EE_OBJS += $(GSMCORE_EE_OBJS)
EE_CFLAGS += -DGSM
endif

EE_LDFLAGS = -nostartfiles -Tlinkfile -L$(PS2SDK)/ee/lib -L$(PS2SDK)/sbv/lib -s
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
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS) -Xlinker -Map -Xlinker $(MAPFILE)


all:
	$(EE_BIN)

clean:
	rm -f $(EE_OBJS_DIR)*.* $(EE_BIN) $(MAPFILE)

clean_all:
	rm -f $(EE_OBJS_DIR)*.* $(EE_BIN) $(MAPFILE)


include $(PS2SDK)/samples/Makefile.pref

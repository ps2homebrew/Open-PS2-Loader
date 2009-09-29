.SILENT:

GSKIT = $(PS2DEV)/gsKit

EE_BIN = main.elf
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_OBJS = obj/main.o obj/pad.o obj/gfx.o obj/system.o obj/config.o obj/loader.o obj/imgdrv.o obj/eesync.o obj/cdvdman.o obj/usbd.o obj/usbhdfsd.o obj/isofs.o obj/font.o obj/exit_icon.o obj/config_icon.o obj/games_icon.o obj/disc_icon.o obj/theme_icon.o obj/language_icon.o
EE_LIBS = $(GSKIT)/lib/libgskit.a $(GSKIT)/lib/libdmakit.a $(GSKIT)/lib/libgskit_toolkit.a -ldebug -lpatches -lpad -lm -lc
EE_INCS += -I$(GSKIT)/include -I$(GSKIT)/ee/dma/include -I$(GSKIT)/ee/gs/include -I$(GSKIT)/ee/toolkit/include

all:
	@mkdir -p obj
	@mkdir -p asm
	$(MAKE) $(EE_BIN)
	$(PS2DEV)/bin/ps2-packer/ps2-packer main.elf OPNUSBLD.ELF
	$(MAKE) -C pc

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C modules/imgdrv clean
	$(MAKE) -C modules/eesync clean
	$(MAKE) -C modules/cdvdman clean
	$(MAKE) -C modules/usbhdfsd clean	
	$(MAKE) -C modules/isofs clean
	$(MAKE) -C pc clean
	rm -f $(EE_BIN) asm/*.* obj/*.*

rebuild: clean all

loader.s:
	$(MAKE) -C loader
	bin2s loader/loader.elf asm/loader.s loader_elf

imgdrv.s:
	$(MAKE) -C modules/imgdrv
	bin2s modules/imgdrv/imgdrv.irx asm/imgdrv.s imgdrv_irx

eesync.s:
	$(MAKE) -C modules/eesync
	bin2s modules/eesync/eesync.irx asm/eesync.s eesync_irx

cdvdman.s:
	$(MAKE) -C modules/cdvdman
	bin2s modules/cdvdman/cdvdman.irx asm/cdvdman.s cdvdman_irx

usbd.s:
	#bin2s $(PS2SDK)/iop/irx/usbd.irx asm/usbd.s usbd_irx
	bin2s modules/usbd/usbd.irx asm/usbd.s usbd_irx
	
usbhdfsd.s:
	$(MAKE) -C modules/usbhdfsd
	bin2s modules/usbhdfsd/bin/usbhdfsd.irx asm/usbhdfsd.s usbhdfsd_irx
	
isofs.s:
	$(MAKE) -C modules/isofs
	bin2s modules/isofs/isofs.irx asm/isofs.s isofs_irx
	
font.s:
	bin2s gfx/font.raw asm/font.s font_raw

exit_icon.s:
	bin2s gfx/exit.raw asm/exit_icon.s exit_raw
	
config_icon.s:
	bin2s gfx/config.raw asm/config_icon.s config_raw
	
games_icon.s:
	bin2s gfx/games.raw asm/games_icon.s games_raw

disc_icon.s:
	bin2s gfx/disc.raw asm/disc_icon.s disc_raw
	
theme_icon.s:
	bin2s gfx/theme.raw asm/theme_icon.s theme_raw
	
language_icon.s:
	bin2s gfx/language.raw asm/language_icon.s language_raw

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
	
$(EE_OBJS_DIR)%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $(EE_ASM_DIR)$< -o $@
			
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

.SILENT:

GSKIT = $(PS2DEV)/gsKit
PS2ETH = $(PS2DEV)/ps2eth

EE_BIN = main.elf
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_OBJS = obj/main.o obj/pad.o obj/gfx.o obj/system.o obj/lang.o obj/config.o obj/loader.o obj/imgdrv.o obj/eesync.o \
		  obj/cdvdman.o obj/usbd_ps2.o obj/usbd_ps3.o obj/usbhdfsd.o obj/ingame_usbhdfsd.o obj/isofs.o \
		  obj/ps2dev9.o obj/ps2ip.o obj/ps2smap.o obj/netlog.o obj/smbman.o obj/dummy.o \
		  obj/font.o obj/exit_icon.o obj/config_icon.o obj/games_icon.o obj/disc_icon.o obj/theme_icon.o obj/language_icon.o \
		  obj/apps_icon.o obj/menu_icon.o obj/scroll_icon.o obj/usb_icon.o obj/save_icon.o obj/netconfig_icon.o obj/network_icon.o
EE_LIBS = $(GSKIT)/lib/libgskit.a $(GSKIT)/lib/libdmakit.a $(GSKIT)/lib/libgskit_toolkit.a -ldebug -lpatches -lpad -lm -lmc -lc
EE_INCS += -I$(GSKIT)/include -I$(GSKIT)/ee/dma/include -I$(GSKIT)/ee/gs/include -I$(GSKIT)/ee/toolkit/include

all:
	@mkdir -p obj
	@mkdir -p asm
	echo "Building Open USB Loader..."
	echo "    * Interface"
	$(MAKE) $(EE_BIN)
	echo "Compressing..."
	$(PS2DEV)/bin/ps2-packer/ps2-packer main.elf OPNUSBLD.ELF > /dev/null
	echo "Building iso2usbld..."
	$(MAKE) -C pc

clean:
	echo "Cleaning..."
	echo "    * Interface"
	rm -f $(EE_BIN) OPNUSBLD.ELF asm/*.* obj/*.*
	echo "    * Loader"
	$(MAKE) -C loader clean
	echo "    * imgdrv.irx"
	$(MAKE) -C modules/imgdrv clean
	echo "    * eesync.irx"
	$(MAKE) -C modules/eesync clean
	echo "    * cdvdman.irx"
	$(MAKE) -C modules/cdvdman clean
	echo "    * usbhdfsd.irx"
	$(MAKE) -C modules/usbhdfsd clean
	echo "    * isofs.irx"
	$(MAKE) -C modules/isofs clean
	echo "    * ps2dev9.irx"
	$(MAKE) -C modules/dev9 clean	
	echo "    * netlog.irx"
	$(MAKE) -C modules/netlog clean	
	echo "    * smbman.irx"
	$(MAKE) -C modules/smbman clean
	echo "    * dummy.irx"
	$(MAKE) -C modules/dummy_irx clean	
	echo "    * iso2usbld"
	$(MAKE) -C pc clean

rebuild: clean all

loader.s:
	echo "    * Loader"
	$(MAKE) -C loader
	bin2s loader/loader.elf asm/loader.s loader_elf

imgdrv.s:
	echo "    * imgdrv.irx"
	$(MAKE) -C modules/imgdrv
	bin2s modules/imgdrv/imgdrv.irx asm/imgdrv.s imgdrv_irx

eesync.s:
	echo "    * eesync.irx"
	$(MAKE) -C modules/eesync
	bin2s modules/eesync/eesync.irx asm/eesync.s eesync_irx

cdvdman.s:
	echo "    * cdvdman.irx"
	$(MAKE) -C modules/cdvdman
	bin2s modules/cdvdman/cdvdman.irx asm/cdvdman.s cdvdman_irx

usbd_ps2.s:
	bin2s $(PS2SDK)/iop/irx/usbd.irx asm/usbd_ps2.s usbd_ps2_irx

usbd_ps3.s:
	bin2s modules/usbd/usbd.irx asm/usbd_ps3.s usbd_ps3_irx

usbhdfsd.s:
	echo "    * usbhdfsd.irx"
	$(MAKE) -C modules/usbhdfsd
	bin2s modules/usbhdfsd/bin/usbhdfsd.irx asm/usbhdfsd.s usbhdfsd_irx

ingame_usbhdfsd.s:
	echo "    * ingame_usbhdfsd.irx"
	$(MAKE) -C modules/usbhdfsd clean
	$(MAKE) -C modules/usbhdfsd -f Makefile.readonly
	bin2s modules/usbhdfsd/bin/usbhdfsd.irx asm/ingame_usbhdfsd.s ingame_usbhdfsd_irx

isofs.s:
	echo "    * isofs.irx"
	$(MAKE) -C modules/isofs
	bin2s modules/isofs/isofs.irx asm/isofs.s isofs_irx

ps2dev9.s:
	echo "    * ps2dev9.irx"
	$(MAKE) -C modules/dev9
	bin2s modules/dev9/ps2dev9.irx asm/ps2dev9.s ps2dev9_irx
	
ps2ip.s:
	bin2s $(PS2SDK)/iop/irx/ps2ip.irx asm/ps2ip.s ps2ip_irx
	
ps2smap.s:
	bin2s $(PS2ETH)/smap/ps2smap.irx asm/ps2smap.s ps2smap_irx

netlog.s:
	echo "    * netlog.irx"
	$(MAKE) -C modules/netlog
	bin2s modules/netlog/netlog.irx asm/netlog.s netlog_irx

smbman.s:
	echo "    * smbman.irx"
	$(MAKE) -C modules/smbman
	bin2s modules/smbman/smbman.irx asm/smbman.s smbman_irx
	
dummy.s:
	echo "    * dummy.irx"
	$(MAKE) -C modules/dummy_irx
	bin2s modules/dummy_irx/dummy.irx asm/dummy.s dummy_irx
			
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

apps_icon.s:
	bin2s gfx/apps.raw asm/apps_icon.s apps_raw

  
menu_icon.s:
	bin2s gfx/menu.raw asm/menu_icon.s menu_raw

  
scroll_icon.s:
	bin2s gfx/scroll.raw asm/scroll_icon.s scroll_raw

  
usb_icon.s:
	bin2s gfx/usb.raw asm/usb_icon.s usb_raw

save_icon.s:
	bin2s gfx/save.raw asm/save_icon.s save_raw

netconfig_icon.s:
	bin2s gfx/netconfig.raw asm/netconfig_icon.s netconfig_raw

network_icon.s:
	bin2s gfx/network.raw asm/network_icon.s network_raw

  
$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
	
$(EE_OBJS_DIR)%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $(EE_ASM_DIR)$< -o $@
			
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

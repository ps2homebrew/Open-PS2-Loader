
EE_BIN = genvmclab.elf
EE_OBJS = genvmclab.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o udptty.o ioptrap.o ps2link.o iomanx.o filexio.o usbd.o usbhdfsd.o mcman.o genvmc.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

BIN2S = $(PS2SDK)/bin/bin2s

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/network/SMSUTILS clean
	$(MAKE) -C ../../modules/network/SMSTCPIP clean
	$(MAKE) -C ../../modules/network/SMSMAP clean
	$(MAKE) -C ../../modules/debug/udptty clean
	$(MAKE) -C ../../modules/debug/ioptrap clean
	$(MAKE) -C ../../modules/debug/ps2link clean
	$(MAKE) -C ../../modules/usb/usbhdfsd clean
	$(MAKE) -C ../../modules/vmc/genvmc clean
	$(MAKE) -C ../../modules/vmc/mcman clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.s:
	$(BIN2S) $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

ps2dev9.s:
	$(MAKE) -C ../../modules/dev9
	$(BIN2S) ../../modules/dev9/ps2dev9.irx ps2dev9.s ps2dev9_irx

smsutils.s:
	$(MAKE) -C ../../modules/network/SMSUTILS
	$(BIN2S) ../../modules/network/SMSUTILS/SMSUTILS.irx smsutils.s smsutils_irx

smstcpip.s:
	$(MAKE) -C ../../modules/network/SMSTCPIP
	$(BIN2S) ../../modules/network/SMSTCPIP/SMSTCPIP.irx smstcpip.s smstcpip_irx

smsmap.s:
	$(MAKE) -C ../../modules/network/SMSMAP
	$(BIN2S) ../../modules/network/SMSMAP/SMSMAP.irx smsmap.s smsmap_irx

iomanx.s:
	$(BIN2S) $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	$(BIN2S) $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

usbd.s:
	$(BIN2S) $(PS2SDK)/iop/irx/usbd.irx usbd.s usbd_irx

usbhdfsd.s:
	$(MAKE) -C ../../modules/usb/usbhdfsd
	$(BIN2S) ../../modules/usb/usbhdfsd/usbhdfsd.irx usbhdfsd.s usbhdfsd_irx

udptty.s:
	$(MAKE) -C ../../modules/debug/udptty
	$(BIN2S) ../../modules/debug/udptty/udptty.irx udptty.s udptty_irx

ioptrap.s:
	$(MAKE) -C ../../modules/debug/ioptrap
	$(BIN2S) ../../modules/debug/ioptrap/ioptrap.irx ioptrap.s ioptrap_irx

ps2link.s:
	$(MAKE) -C ../../modules/debug/ps2link
	$(BIN2S) ../../modules/debug/ps2link/ps2link.irx ps2link.s ps2link_irx

mcman.s:
	$(MAKE) -C ../../modules/vmc/mcman
	$(BIN2S) ../../modules/vmc/mcman/mcman.irx mcman.s mcman_irx

genvmc.s:
	$(MAKE) -C ../../modules/vmc/genvmc
	$(BIN2S) ../../modules/vmc/genvmc/genvmc.irx genvmc.s genvmc_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

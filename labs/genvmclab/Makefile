
EE_BIN = genvmclab.elf
EE_OBJS = genvmclab.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o udptty.o ioptrap.o ps2link.o iomanx.o filexio.o usbd.o usbhdfsd.o mcman.o genvmc.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/SMSUTILS clean
	$(MAKE) -C ../../modules/SMSTCPIP clean
	$(MAKE) -C ../../modules/SMSMAP clean
	$(MAKE) -C ../../modules/udptty clean
	$(MAKE) -C ../../modules/ioptrap clean
	$(MAKE) -C ../../modules/ps2link clean
	$(MAKE) -C ../../modules/usbhdfsd clean
	$(MAKE) -C ../../modules/genvmc clean
	$(MAKE) -C ../../modules/mcman clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

ps2dev9.s:
	$(MAKE) -C ../../modules/dev9
	bin2s ../../modules/dev9/ps2dev9.irx ps2dev9.s ps2dev9_irx

smsutils.s:
	$(MAKE) -C ../../modules/SMSUTILS
	bin2s ../../modules/SMSUTILS/SMSUTILS.irx smsutils.s smsutils_irx

smstcpip.s:
	$(MAKE) -C ../../modules/SMSTCPIP
	bin2s ../../modules/SMSTCPIP/SMSTCPIP.irx smstcpip.s smstcpip_irx
		
smsmap.s:
	$(MAKE) -C ../../modules/SMSMAP
	bin2s ../../modules/SMSMAP/SMSMAP.irx smsmap.s smsmap_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

usbd.s:
	bin2s $(PS2SDK)/iop/irx/usbd.irx usbd.s usbd_irx

usbhdfsd.s:
	$(MAKE) -C ../../modules/usbhdfsd
	bin2s ../../modules/usbhdfsd/usbhdfsd.irx usbhdfsd.s usbhdfsd_irx

udptty.s:
	$(MAKE) -C ../../modules/udptty
	bin2s ../../modules/udptty/udptty.irx udptty.s udptty_irx

ioptrap.s:
	$(MAKE) -C ../../modules/ioptrap
	bin2s ../../modules/ioptrap/ioptrap.irx ioptrap.s ioptrap_irx

ps2link.s:
	$(MAKE) -C ../../modules/ps2link
	bin2s ../../modules/ps2link/ps2link.irx ps2link.s ps2link_irx

usbhdfsd.s:
	$(MAKE) -C ../../modules/usbhdfsd
	bin2s ../../modules/usbhdfsd/usbhdfsd.irx usbhdfsd.s usbhdfsd_irx

mcman.s:
	$(MAKE) -C ../../modules/mcman
	bin2s ../../modules/mcman/mcman.irx mcman.s mcman_irx

genvmc.s:
	$(MAKE) -C ../../modules/genvmc
	bin2s ../../modules/genvmc/genvmc.irx genvmc.s genvmc_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

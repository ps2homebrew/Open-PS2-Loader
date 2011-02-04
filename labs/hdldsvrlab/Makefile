
EE_BIN = hdldsvrlab.elf
EE_OBJS = hdldsvrlab.o discid.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o ps2atad.o ps2hdd.o hdldsvr.o iomanx.o filexio.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/cdvd/discID clean
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/network/SMSUTILS clean
	$(MAKE) -C ../../modules/network/SMSTCPIP -f Makefile clean
	$(MAKE) -C ../../modules/network/SMSMAP clean
	$(MAKE) -C ../../modules/hdd/atad clean
	$(MAKE) -C ../../modules/hdd/ps2hdd clean
	$(MAKE) -C ../../modules/hdd/hdldsvr clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

discid.s:
	$(MAKE) -C ../../modules/cdvd/discID
	bin2s ../../modules/cdvd/discID/discID.irx discid.s discid_irx

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

ps2dev9.s:
	$(MAKE) -C ../../modules/dev9
	bin2s ../../modules/dev9/ps2dev9.irx ps2dev9.s ps2dev9_irx

smsutils.s:
	$(MAKE) -C ../../modules/network/SMSUTILS
	bin2s ../../modules/network/SMSUTILS/SMSUTILS.irx smsutils.s smsutils_irx

smstcpip.s:
	$(MAKE) -C ../../modules/network/SMSTCPIP -f Makefile rebuild
	bin2s ../../modules/network/SMSTCPIP/SMSTCPIP.irx smstcpip.s smstcpip_irx
		
smsmap.s:
	$(MAKE) -C ../../modules/network/SMSMAP
	bin2s ../../modules/network/SMSMAP/SMSMAP.irx smsmap.s smsmap_irx

ps2atad.s:
	$(MAKE) -C ../../modules/hdd/atad
	bin2s ../../modules/hdd/atad/ps2atad.irx ps2atad.s ps2atad_irx

ps2hdd.s:
	$(MAKE) -C ../../modules/hdd/ps2hdd
	bin2s ../../modules/hdd/ps2hdd/ps2hdd.irx ps2hdd.s ps2hdd_irx

hdldsvr.s:
	$(MAKE) -C ../../modules/hdd/hdldsvr
	bin2s ../../modules/hdd/hdldsvr/hdldsvr.irx hdldsvr.s hdldsvr_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

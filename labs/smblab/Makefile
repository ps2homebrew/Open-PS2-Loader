
EE_BIN = smblab.elf
EE_OBJS = smblab.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o smbman.o iomanx.o filexio.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../modules/dev9 clean
	$(MAKE) -C ../modules/SMSUTILS clean
	$(MAKE) -C ../modules/SMSTCPIP clean
	$(MAKE) -C ../modules/SMSMAP clean
	$(MAKE) -C ../modules/smbman clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

ps2dev9.s:
	$(MAKE) -C ../modules/dev9
	bin2s ../modules/dev9/ps2dev9.irx ps2dev9.s ps2dev9_irx

smsutils.s:
	$(MAKE) -C ../modules/SMSUTILS
	bin2s ../modules/SMSUTILS/SMSUTILS.irx smsutils.s smsutils_irx

smstcpip.s:
	$(MAKE) -C ../modules/SMSTCPIP
	bin2s ../modules/SMSTCPIP/SMSTCPIP.irx smstcpip.s smstcpip_irx
		
smsmap.s:
	$(MAKE) -C ../modules/SMSMAP
	bin2s ../modules/SMSMAP/SMSMAP.irx smsmap.s smsmap_irx

smbman.s:
	$(MAKE) -C ../modules/smbman
	bin2s ../modules/smbman/smbman.irx smbman.s smbman_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

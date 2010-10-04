
EE_BIN = lanlab.elf
EE_OBJS = lanlab.o poweroff.o ps2dev9.o smsutils.o lanman.o iomanx.o filexio.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/network/SMSUTILS clean
	$(MAKE) -C ../../modules/wip/lanman clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx poweroff.s poweroff_irx

ps2dev9.s:
	$(MAKE) -C ../../modules/dev9
	bin2s ../../modules/dev9/ps2dev9.irx ps2dev9.s ps2dev9_irx

smsutils.s:
	$(MAKE) -C ../../modules/network/SMSUTILS
	bin2s ../../modules/network/SMSUTILS/SMSUTILS.irx smsutils.s smsutils_irx

lanman.s:
	$(MAKE) -C ../../modules/wip/lanman
	bin2s ../../modules/wip/lanman/lanman.irx lanman.s lanman_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx filexio.s filexio_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

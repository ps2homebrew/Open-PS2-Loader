/*
  Copyright 2011, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  HD Pro Kit Checker module
*/

#include <loadcore.h>
#include <intrman.h>
#include <thbase.h>
#include <stdio.h>
#include <sifman.h>

// HD Pro Kit is mapping the 1st word in ROM0 seg as a main ATA controller,
// The pseudo ATA controller registers are accessed (input/ouput) by writing
// an id to the main ATA controller
#define HDPROreg_IO8	      (*(volatile unsigned char *)0xBFC00000) 
#define HDPROreg_IO32	      (*(volatile unsigned int  *)0xBFC00000)

#define CDVDreg_STATUS        (*(volatile unsigned char *)0xBF40200A) 

static int hdpro_io_active = 0;
static int intr_suspended = 0;
static int intr_state;

static void suspend_intr(void)
{
	if (!intr_suspended) {
		CpuSuspendIntr(&intr_state);

		intr_suspended = 1;
	}
}

static void resume_intr(void)
{
	if (intr_suspended) {
		CpuResumeIntr(intr_state);

		intr_suspended = 0;
	}
}

static int hdpro_io_start(void)
{
	if (hdpro_io_active)
		return 0;

	hdpro_io_active = 0;

	suspend_intr();

	// HD Pro IO start commands sequence
	HDPROreg_IO8 = 0x72;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x34;
	CDVDreg_STATUS = 0;
	HDPROreg_IO8 = 0x61;
	CDVDreg_STATUS = 0;
	u32 res = HDPROreg_IO8;
	CDVDreg_STATUS = 0;

	resume_intr();

	// check result
	if ((res & 0xff) == 0xe7)
		hdpro_io_active = 1;

	return hdpro_io_active;
}

static int hdpro_io_finish(void)
{
	if (!hdpro_io_active)
		return 0;

	suspend_intr();

	// HD Pro IO finish commands sequence
	HDPROreg_IO8 = 0xf3;
	CDVDreg_STATUS = 0;

	resume_intr();

	DelayThread(200);

	if (HDPROreg_IO32 == 0x401a7800) // check the 1st in ROM0 seg get
		hdpro_io_active = 0;	 // back to it's original state

	return hdpro_io_active ^ 1;
}

int _start(int argc, char *argv[])
{
	int res = hdpro_io_start();

	HDPROreg_IO8 = 0xe3;
	CDVDreg_STATUS = 0;

	hdpro_io_finish();

	if (res) {
		SifDmaTransfer_t dmat;
		int oldstate, id;
		u32 val = 0xdeadfeed;

		dmat.dest = (void *)0x200c0000;
		dmat.size = sizeof(u32);
		dmat.src = (void *)&val;
		dmat.attr = 0;

		id = 0;
		while (!id) {
			CpuSuspendIntr(&oldstate);
			id = sceSifSetDma(&dmat, 1);
			CpuResumeIntr(oldstate);
		}
		while (sceSifDmaStat(id) >= 0);
	}

	return MODULE_NO_RESIDENT_END;
}


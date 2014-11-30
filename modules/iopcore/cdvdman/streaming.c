#include "smsutils.h"
#include "ioplib_util.h"
#include "cdvdman.h"
#include "cdvd_config.h"
#include "internal.h"

#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <errno.h>

#if 0
#define DPRINTF(args...) printf(args)
#define iDPRINTF(args...) Kprintf(args)
#else
#define DPRINTF(args...)
#define iDPRINTF(args...)
#endif

extern cdvdman_status_t cdvdman_stat;

static int AllocBank(void **pointer);
static int ReadSectors(int maxcount, void *buffer);
static int StFillStreamBuffer(void);
static void StStartFillStreamBuffer(void);

static unsigned int StmScheduleCb(void *arg){
	return((StFillStreamBuffer()>=0)?0:0x00704000);
}

static void StmCallback(void){
	int OldState;
	iop_sys_clock_t StmScheduleClock;

	CpuSuspendIntr(&OldState);
	cdvdman_stat.StreamingData.Stlsn += cdvdman_stat.StreamingData.StBanksize;
	cdvdman_stat.StreamingData.StStreamed += cdvdman_stat.StreamingData.StBanksize;
	cdvdman_stat.StreamingData.StWritePtr += cdvdman_stat.StreamingData.StBanksize;
	if(cdvdman_stat.StreamingData.StWritePtr >= cdvdman_stat.StreamingData.StBufmax) cdvdman_stat.StreamingData.StWritePtr = 0;
	cdvdman_stat.StreamingData.StIsReading = 0;
	CpuResumeIntr(OldState);

	DPRINTF("StmCallback: %08lx, wr: %u, rd: %u, streamed: %u\n", cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

	//As a thread cannot wake itself up, set a callback.
	StmScheduleClock.lo = 0;
	StmScheduleClock.hi = 0;
	SetAlarm(&StmScheduleClock, &StmScheduleCb, &cdvdman_stat.StreamingData);
}

static void StReset(void){
	cdvdman_stat.StreamingData.StWritePtr = 0;
	cdvdman_stat.StreamingData.StReadPtr = 0;
	cdvdman_stat.StreamingData.StStreamed = 0;
	cdvdman_stat.StreamingData.StIsReading = 0;
}

// 0 = OK. <0 = error in sceCdRead. >0 = full buffer.
static int StFillStreamBuffer(void){
	int result, OldState;
	void *ptr;

	CpuSuspendIntr(&OldState);

	if(cdvdman_stat.StreamingData.StIsReading){
		CpuResumeIntr(OldState);
		return 0;
	}

	CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);
	cdvdman_stat.StreamingData.StIsReading = 1;

	//Determine how much more to read.
	result = AllocBank(&ptr);

	CpuResumeIntr(OldState);

	if(result==0){
//		iDPRINTF("Stream fill buffer: Stream lsn 0x%08x - %u sectors:%p\n", cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StBanksize, ptr);
		if(cdvdman_AsyncRead(cdvdman_stat.StreamingData.Stlsn, cdvdman_stat.StreamingData.StBanksize, ptr)==0){
			//Failed to start reading.
			cdvdman_stat.StreamingData.StIsReading = 0;
			result = -1;
		}else{
			result = 0;
		}
	}else{
		iDPRINTF("Stream fill buffer: Stream full.\n");
		//Nothing else to read.
		cdvdman_stat.StreamingData.StIsReading = 0;
		result = 1;
	}

	return result;
}

static void StStartFillStreamBuffer(void){
	iop_sys_clock_t StmScheduleClock;

	if(StFillStreamBuffer() < 0){
		DPRINTF("StmCallback: Rescheduling read.\n");
		StmScheduleClock.lo = 0x00704000;
		StmScheduleClock.hi = 0;
		SetAlarm(&StmScheduleClock, &StmScheduleCb, &cdvdman_stat.StreamingData);
	}
}

int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr){
	int OldState;

	cdvdman_stat.err = CDVD_ERR_NO;

	CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

	CpuSuspendIntr(&OldState);
	cdvdman_stat.StreamingData.StBankmax = bankmax;
	cdvdman_stat.StreamingData.StBanksize = bufmax/bankmax;
	cdvdman_stat.StreamingData.StBufmax = cdvdman_stat.StreamingData.StBanksize * cdvdman_stat.StreamingData.StBankmax;
	cdvdman_stat.StreamingData.StIOP_bufaddr = iop_bufaddr;
	StReset();

	CpuResumeIntr(OldState);

	DPRINTF("sceCdStInit bufmax: %u (%lu), bankmax: %lu, banksize: %u, buffer: %p\n", cdvdman_stat.StreamingData.StBufmax, bufmax, bankmax, cdvdman_stat.StreamingData.StBanksize, iop_bufaddr);

	return 1;
}

//Must be called from an interrupt-disabled state.
static int AllocBank(void **pointer){
	int result;

	if(cdvdman_stat.StreamingData.StBufmax - cdvdman_stat.StreamingData.StStreamed >= cdvdman_stat.StreamingData.StBanksize){
		*pointer = cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StWritePtr*2048;
		result = 0;
	}else{
		*pointer = NULL;
		result = -ENOMEM;
	}

//	iDPRINTF("AllocBank: wrptr: %u, rdptr: %u, streamed: %u\n", cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

	return result;
}

static int ReadSectors(int maxcount, void *buffer){
	int OldState, result;
	unsigned short int SectorsToCopy;
	void *ptr;

//	DPRINTF("ReadSectors: wr: %u, rd: %u, streamed: %u\n", cdvdman_stat.StreamingData.StWritePtr, cdvdman_stat.StreamingData.StReadPtr, cdvdman_stat.StreamingData.StStreamed);

	result = 0;
	ptr=buffer;
	CpuSuspendIntr(&OldState);

	if(cdvdman_stat.StreamingData.StWritePtr<=cdvdman_stat.StreamingData.StReadPtr && cdvdman_stat.StreamingData.StStreamed>0){
		SectorsToCopy = cdvdman_stat.StreamingData.StBufmax - cdvdman_stat.StreamingData.StReadPtr;
		if(SectorsToCopy > maxcount) SectorsToCopy = maxcount;
		if(SectorsToCopy > 0){
			mips_memcpy(buffer, cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StReadPtr*2048, SectorsToCopy*2048);
			ptr += SectorsToCopy*2048;
			cdvdman_stat.StreamingData.StReadPtr += SectorsToCopy;
			if(cdvdman_stat.StreamingData.StReadPtr>=cdvdman_stat.StreamingData.StBufmax) cdvdman_stat.StreamingData.StReadPtr = 0;
			cdvdman_stat.StreamingData.StStreamed -= SectorsToCopy;
			result += SectorsToCopy;
		}
	}
	if(result<maxcount && cdvdman_stat.StreamingData.StReadPtr<cdvdman_stat.StreamingData.StWritePtr){
		SectorsToCopy = cdvdman_stat.StreamingData.StWritePtr-cdvdman_stat.StreamingData.StReadPtr;
		if(SectorsToCopy > maxcount-result) SectorsToCopy = maxcount-result;
		if(SectorsToCopy > 0){
			mips_memcpy(ptr, cdvdman_stat.StreamingData.StIOP_bufaddr + cdvdman_stat.StreamingData.StReadPtr*2048, SectorsToCopy*2048);
			cdvdman_stat.StreamingData.StReadPtr += SectorsToCopy;
			if(cdvdman_stat.StreamingData.StReadPtr>=cdvdman_stat.StreamingData.StBufmax) cdvdman_stat.StreamingData.StReadPtr = 0;
			cdvdman_stat.StreamingData.StStreamed -= SectorsToCopy;
			result += SectorsToCopy;
		}
	}

	CpuResumeIntr(OldState);

	return result;
}

int sceCdStStart(u32 lsn, cd_read_mode_t *mode){
	int OldState;

	DPRINTF("StStart called. lsn: 0x%08lx\n", lsn);

	sceCdStStop();

	CpuSuspendIntr(&OldState);

	cdvdman_stat.StreamingData.Stlsn = lsn;
	cdvdman_stat.StreamingData.StStat = 1;
	StReset();
	SetStm0Callback(&StmCallback);
	CpuResumeIntr(OldState);

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	StStartFillStreamBuffer();

	return 1;
}

int sceCdStStat(void){
	DPRINTF("StStat called: %u\n", cdvdman_stat.StreamingData.StStreamed);
	cdvdman_stat.err = CDVD_ERR_NO;
	return cdvdman_stat.StreamingData.StStreamed;
}

int sceCdStStop(void){
	int OldState;

	DPRINTF("StStop called. Stat: 0x%x\n", cdvdman_stat.StreamingData.StStat);

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	if(cdvdman_stat.StreamingData.StStat){
		CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

		CpuSuspendIntr(&OldState);

		//Stop.
		SetStm0Callback(NULL);
		cdvdman_stat.StreamingData.StStreamed = 0;
		cdvdman_stat.StreamingData.StStat = 0;
		StReset();

		CpuResumeIntr(OldState);

		sceCdSync(0);
	}

	return 1;
}

int sceCdStPause(void){
	int OldState;

	DPRINTF("StPause called. Stat: 0x%x\n", cdvdman_stat.StreamingData.StStat);

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	if(cdvdman_stat.StreamingData.StStat){
		CancelAlarm(&StmScheduleCb, &cdvdman_stat.StreamingData);

		CpuSuspendIntr(&OldState);
		//Pause.
		SetStm0Callback(NULL);
		CpuResumeIntr(OldState);

		sceCdSync(0);

		return 1;
	}else{
		return 0;
	}
}

int sceCdStResume(void){
	int OldState;

	DPRINTF("StResume called. Stat: 0x%x\n", cdvdman_stat.StreamingData.StStat);

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	if(cdvdman_stat.StreamingData.StStat){
		CpuSuspendIntr(&OldState);
		//Resume
		SetStm0Callback(&StmCallback);
		CpuResumeIntr(OldState);

		StStartFillStreamBuffer();

		return 1;
	}else{
		return 0;
	}
}

int sceCdStSeek(u32 lsn){
	DPRINTF("StSeek: %lu\n", lsn);

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	if(cdvdman_stat.StreamingData.StStat){
		return sceCdStStart(lsn, NULL);
	}else{
		return 0;
	}
}

int sceCdStRead(u32 sectors, void *buffer, u32 mode, u32 *error){
	int SectorsRead, SectorsToRead, result;
	void *ptr;

	DPRINTF("StRead called: sectors %lu:%p, mode: %lu, stat: %u,%u\n", sectors, buffer, mode, cdvdman_stat.StreamingData.StStat, cdvdman_stat.StreamingData.StIsReading);

	cdvdman_stat.err = CDVD_ERR_NO;
	if(cdvdman_stat.StreamingData.StStat){
		SetEventFlag(cdvdman_stat.intr_ef, 8);
		for(SectorsToRead=sectors,result=0,SectorsRead=0,ptr=buffer; result < sectors; SectorsToRead = sectors - SectorsRead,ptr += SectorsRead*2048){
			WaitEventFlag(cdvdman_stat.intr_ef, 8, WEF_AND, NULL);

//			DPRINTF("Sectors: %u:%p, mode: %lu", SectorsToRead, ptr, mode);
			SectorsRead=ReadSectors(SectorsToRead, ptr);
//			DPRINTF(", Read: %u\n", SectorsRead);
			ClearEventFlag(cdvdman_stat.intr_ef, ~8);

			if(SectorsRead == 0) DPRINTF("StRead: buffer underrun. %u/%lu read.\n", result, sectors);

			result+=SectorsRead;
			//if(mode == STMNBLK) break;
			if(mode==0) break;
		}
		*error = sceCdGetError();

		StStartFillStreamBuffer();
	}else{
		result = 0;
	}

	return result;
}

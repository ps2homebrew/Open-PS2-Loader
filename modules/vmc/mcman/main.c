/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "mcman.h"

IRX_ID(MODNAME, 1, 1);

struct modInfo_t mcman_modInfo = { "mcman_cex", 0x20b }; 

char sio2man_modname[8] = "sio2man\0";
int sio2man_type = SIO2MAN;

char SUPERBLOCK_MAGIC[] = "Sony PS2 Memory Card Format ";
char SUPERBLOCK_VERSION[] = "1.2.0.0";

int mcman_wr_port = -1;
int mcman_wr_slot = -1;
int mcman_wr_block = -1;
int mcman_wr_flag3 = -10;
int mcman_curdircluster = -1;

u32 DOT    = 0x0000002e;
u32 DOTDOT = 0x00002e2e;

int timer_ID;
int PS1CardFlag = 1;


// mcman xor table
u8 mcman_xortable[256] = {
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
	0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
	0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
	0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
	0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
	0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
	0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
	0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
	0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
	0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
	0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
	0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
	0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
	0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
	0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
	0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
	0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00
};

//-------------------------------------------------------------- 
void long_multiply(u32 v1, u32 v2, u32 *HI, u32 *LO)
{
	register long a, b, c, d;
	register long x, y;

	a = (v1 >> 16) & 0xffff;
	b = v1 & 0xffff;
	c = (v2 >> 16) & 0xffff;
	d = v2 & 0xffff;

	*LO = b * d;   
	x = a * d + c * b;
	y = ((*LO >> 16) & 0xffff) + x;

	*LO = (*LO & 0xffff) | ((y & 0xffff) << 16);
	*HI = (y >> 16) & 0xffff;

	*HI += a * c;     
}

//-------------------------------------------------------------- 
int mcman_chrpos(char *str, int chr)
{
	char *p;

	p = str;
	if (*str) {
		do {
			if (*p == (chr & 0xff))
				break;
			p++;	
		} while (*p);	
	}
	if (*p != (chr & 0xff))
		return -1;
	
	return p - str;	
}
//-------------------------------------------------------------- 
int _start(int argc, const char **argv)
{
	iop_library_table_t *libtable;
	iop_library_t *libptr;
	register int i, sio2man_loaded;
	void **export_tab;

#ifdef SIO_DEBUG
	sio_init(38400, 0, 0, 0, 0);
#endif
#ifdef DEBUG
	DPRINTF("mcman: _start...\n"); 		
#endif		
		
	// Get sio2man lib ptr
	sio2man_loaded = 0;
	libtable = GetLibraryEntryTable();
	libptr = libtable->tail;
	while (libptr != 0) {
		for (i=0; i<8; i++) {
			if (libptr->name[i] != sio2man_modname[i])
				break;
		} 
		if (i == 8) {
			sio2man_loaded = 1;
			break;
		}
		libptr = libptr->prev;	
	}
	
	if (!sio2man_loaded) {
#ifdef DEBUG	
		DPRINTF("mcman: sio2man module is not loaded...\n"); 		
#endif		
		return MODULE_NO_RESIDENT_END;
	}

#ifdef DEBUG
	DPRINTF("mcman: sio2man version=0x%03x\n", libptr->version); 		
#endif			
	if (libptr->version > 0x101)
		sio2man_type = XSIO2MAN;
	
	// Get sio2man export table	 
	export_tab = (void **)(((struct irx_export_table *)libptr)->fptrs);
		
	// Set functions pointers to match SIO2MAN exports
	sio2_mc_transfer_init = export_tab[24];
	sio2_transfer = export_tab[25];
	// set internals function pointers for MCMAN
	mcman_sio2transfer = (void *)mcsio2_transfer;
	mc_detectcard = (void *)McDetectCard2;	
		
	if (sio2man_type == XSIO2MAN) {
		// Set functions pointers to match XSIO2MAN exports
		sio2_transfer_reset = export_tab[26];
		sio2_func1 = export_tab[55];
		
		// set internals function pointers for XMCMAN
		mcman_sio2transfer = (void *)mcsio2_transfer2;
		mc_detectcard = (void *)mcman_detectcard;
		
		// Modify mcman export ver
		_exp_mcman.version = 0x203;
		// Get mcman export table	 
		export_tab = (void **)(struct irx_export_table *)&_exp_mcman.fptrs;
		export_tab[17] = (void *)McEraseBlock2;
		export_tab[21] = (void *)McDetectCard2;
		export_tab[22] = (void *)McGetFormat;
		export_tab[23] = (void *)McGetEntSpace;
		export_tab[24] = (void *)mcman_replacebadblock;
		export_tab[25] = (void *)McCloseAll;
		export_tab[42] = (void *)McGetModuleInfo;
		export_tab[43] = (void *)McGetCardSpec;
		export_tab[44] = (void *)mcman_getFATentry;
		export_tab[45] = (void *)McCheckBlock;
		export_tab[46] = (void *)mcman_setFATentry;
		export_tab[47] = (void *)mcman_readdirentry;
		export_tab[48] = (void *)mcman_1stcacheEntsetwrflagoff;
		export_tab[49] = (void *)mcman_createDirentry;
		export_tab[50] = (void *)mcman_readcluster;
		export_tab[51] = (void *)mcman_flushmccache;
		export_tab[52] = (void *)mcman_setdirentrystate;
	}

#ifdef DEBUG
	DPRINTF("mcman: registering exports...\n"); 		
#endif
	if (RegisterLibraryEntries(&_exp_mcman) != 0)
		return MODULE_NO_RESIDENT_END;
	
	CpuEnableIntr();	

#ifdef DEBUG
	DPRINTF("mcman: initPS2com...\n"); 		
#endif
	mcman_initPS2com();

#ifdef DEBUG
	DPRINTF("mcman: initPS1PDAcom...\n"); 		
#endif
	mcman_initPS1PDAcom();

#ifdef DEBUG
	DPRINTF("mcman: initcache...\n"); 		
#endif
	mcman_initcache();

#ifdef DEBUG
	DPRINTF("mcman: initdev...\n");
#endif
	mcman_initdev();
	
	timer_ID = ReferHardTimer(1, 32, 0, 0x6309);	
		
	if (timer_ID != -150) 
		return MODULE_RESIDENT_END;
		
	timer_ID = AllocHardTimer(1, 32, 1);
	
	if (timer_ID > 0)
		SetTimerMode(timer_ID, 0);

#ifdef DEBUG
	DPRINTF("mcman: _start returns MODULE_RESIDENT_END...\n"); 		
#endif		

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------- 
int McGetFormat(int port, int slot) // Export #22 XMCMAN only
{
#ifdef DEBUG	
	DPRINTF("mcman: McGetFormat port%d slot%d\n", port, slot);
#endif
	return mcman_devinfos[port][slot].cardform;
}

//-------------------------------------------------------------- 
int McGetMcType(int port, int slot) // Export #39
{
#ifdef DEBUG		
	DPRINTF("mcman: McGetMcType port%d slot%d\n", port, slot);
#endif	
	return mcman_devinfos[port][slot].cardtype;
}

//-------------------------------------------------------------- 
struct modInfo_t *McGetModuleInfo(void) // Export #42 XMCMAN only
{
#ifdef DEBUG		
	DPRINTF("mcman: McGetModuleInfo\n");
#endif	
	return (struct modInfo_t *)&mcman_modInfo;
}

//-------------------------------------------------------------- 
void McSetPS1CardFlag(int flag) // Export #40
{
#ifdef DEBUG
	DPRINTF("mcman: McSetPS1CardFlag flag %x\n", flag);
#endif	
	PS1CardFlag = flag;
}

//-------------------------------------------------------------- 
int McGetFreeClusters(int port, int slot) // Export #38
{
	register int r, mcfree;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];

#ifdef DEBUG	
	DPRINTF("mcman: McGetFreeClusters port%d slot%d\n", port, slot);
#endif	

	mcfree = 0;
	if (mcdi->cardform)	{
		switch (mcdi->cardtype) {
   			case sceMcTypePS2:
   				mcfree = mcman_findfree2(port, slot, 0);
   				break;
   			case sceMcTypePS1:
   			case sceMcTypePDA:
  				mcfree = mcman_findfree1(port, slot, 0);
   				break;
   			case sceMcTypeNoCard:
   				mcfree = 0;
   				break;
		}
		if (mcfree == sceMcResFullDevice)
			mcfree = 0;
			
		r = mcfree;
	}
	else
		return 0;

	return r;
}

//-------------------------------------------------------------- 
void mcman_wmemset(void *buf, int size, int value)
{
	int *p = buf; 
	size = (size >> 2) - 1;
	
	if (size > -1) {
		do {
			*p++ = value;
		} while (--size > -1);
	}
}

//-------------------------------------------------------------- 
int mcman_calcEDC(void *buf, int size)
{
	register u32 checksum;
	register int i;
	u8 *p = (u8 *)buf; 

	checksum = 0;
		
	if (size > 0) {
		size--;
		i = 0;
		while (size-- != -1) {
			checksum ^= p[i];
			i++;
		}
	}
	return checksum & 0xff;
}

//-------------------------------------------------------------- 
int mcman_checkpath(char *str) // check that a string do not contain special chars ( chr<32, ?, *)
{
	register int i;
	u8 *p = (u8 *)str; 
	
	i = 0;	
	while (p[i]) {	
		if (((p[i] & 0xff) == '?') || ((p[i] & 0xff) == '*'))
			return 0;
		if ((p[i] & 0xff) < 32)
			return 0;			
		i++;	
	} 
	return 1;
}

//-------------------------------------------------------------- 
int mcman_checkdirpath(char *str1, char *str2)
{
	register int r, pos1, pos2, pos;
	u8 *p1 = (u8 *)str1;
	u8 *p2 = (u8 *)str2;
	
	do {
		pos1 = mcman_chrpos(p2, '?');
		pos2 = mcman_chrpos(p2, '*');
	
		if ((pos1 < 0) && (pos2 < 0)) {
			if (!strcmp(p2, p1))
				return 1;
			return 0;	
		}
		pos = pos2;
		if (pos1 >= 0) {
			pos = pos1;
			if (pos2 >= 0) {
				pos = pos2;
				if (pos1 < pos2) {
					pos = pos1;
				}
			}
		}
		if (strncmp(p2, p1, pos) != 0)
			return 0;
			
		p2 += pos;
		p1 += pos;
		
		while (p2[0] == '?') {
			if (p1[0] == 0) 
				return 1;
			p2++;	
			p1++;
		}
	} while (p2[0] != '*');	
			
	while((p2[0] == '*') || (p2[0] == '?'))
		p2++;

	if (p2[0] != 0) {
		do {	
			pos = mcman_chrpos(p1, (u8)p2[0]);
			p1 += pos;
			if (pos < 0)
				return 0;			
			r = mcman_checkdirpath(p1, p2);			
			p1++;
		} while (r == 0);
	}
	return 1;	
}

//--------------------------------------------------------------
void mcman_invhandles(int port, int slot)
{
	register int i = 0;
	register MC_FHANDLE *fh = (MC_FHANDLE *)&mcman_fdhandles;
	
	do {
		if ((fh->port == port) && (fh->slot == slot))
			fh->status = 0;
		fh++;
	} while (++i < MAX_FDHANDLES);
}

//--------------------------------------------------------------
int McCloseAll(void) // Export #25 XMCMAN only
{
	register int fd = 0, rv = 0;

	do {
		if (mcman_fdhandles[fd].status) {
			register int rc;

			rc = McClose(fd);
			if (rc < rv)
				rv = rc;   
		}
		fd++;
		
	} while (fd < MAX_FDHANDLES);

	return rv;
}
 
//--------------------------------------------------------------
int McDetectCard(int port, int slot) // Export #5
{	
	return mc_detectcard(port, slot);
}

//--------------------------------------------------------------
int mcman_detectcard(int port, int slot)
{
	register int r;
	register MCDevInfo *mcdi;

#ifdef DEBUG	
	DPRINTF("mcman: mcman_detectcard port%d slot%d\n", port, slot);
#endif	
	mcdi = (MCDevInfo *)&mcman_devinfos[port][slot];
	
	if ((mcdi->cardtype == sceMcTypeNoCard) || (mcdi->cardtype == sceMcTypePS2)) {
		r = mcman_probePS2Card2(port, slot);
		if (r < -9) {
			r = mcman_probePS1Card2(port, slot);
			if (!(r < -9)) {
				if (mcman_probePDACard(port, slot)) {
					mcdi->cardtype = sceMcTypePS1;
					return (!PS1CardFlag) ? sceMcResDeniedPS1Permit : r;
				}
				else {
					mcdi->cardtype = sceMcTypePDA;
					return r;
				}
			}
		}
		else {
			mcdi->cardtype = sceMcTypePS2;
			return r;
		}  
	}
	else {
		r = mcman_probePS1Card2(port, slot);
		if (r) {
			if ((r < -9) || (r >= 0)) {
				r = mcman_probePS2Card2(port, slot);
				if (!(r < -9)) {
					mcdi->cardtype = sceMcTypePS2;
					return r;
				}
			}
			else {
				if (mcman_probePDACard(port, slot)) {
					mcdi->cardtype = sceMcTypePS1;
					return (!PS1CardFlag) ? sceMcResDeniedPS1Permit : r;
				}
				else {
					mcdi->cardtype = sceMcTypePDA;
					return r;
				}
			}
		}
		else {
			if (PS1CardFlag)
				return sceMcResSucceed;
			if (mcdi->cardtype == sceMcTypePS1)
				return sceMcResDeniedPS1Permit;
			return sceMcResSucceed;
		}
	}

	mcdi->cardtype = 0;
	mcdi->cardform = 0;
	mcman_invhandles(port, slot);
	mcman_clearcache(port, slot);

	return r;
}

//--------------------------------------------------------------
int McDetectCard2(int port, int slot) // Export #21 XMCMAN only
{
	register int r;
	register MCDevInfo *mcdi;
	
#ifdef DEBUG	
	DPRINTF("mcman: McDetectCard2 port%d slot%d\n", port, slot);
#endif
	
	mcdi = (MCDevInfo *)&mcman_devinfos[port][slot];
	
	if ((mcdi->cardtype == sceMcTypeNoCard) || (mcdi->cardtype == sceMcTypePS2)) {
		r = mcman_probePS2Card(port, slot);
		if (r < -9) {
			r = mcman_probePS1Card(port, slot);
			if (!(r < -9)) {
				if (mcman_probePDACard(port, slot)) {
					mcdi->cardtype = sceMcTypePS1;
					return (!PS1CardFlag) ? sceMcResDeniedPS1Permit : r;
				}
				else {
					mcdi->cardtype = sceMcTypePDA;
					return r;
				}
			}
		}
		else {
			mcdi->cardtype = sceMcTypePS2;
			return r;
		}  
	}
	else {
		r = mcman_probePS1Card(port, slot);
		if (r) {
			if ((r < -9) || (r >= 0)) {
				r = mcman_probePS2Card(port, slot);
				if (!(r < -9)) {
					mcdi->cardtype = sceMcTypePS2;
					return r;
				}
			}
			else {
				if (mcman_probePDACard(port, slot)) {
					mcdi->cardtype = sceMcTypePS1;
					return (!PS1CardFlag) ? sceMcResDeniedPS1Permit : r;
				}
				else {
					mcdi->cardtype = sceMcTypePDA;
					return r;
				}
			}
		}
		else {
			if (PS1CardFlag)
				return sceMcResSucceed;
			if (mcdi->cardtype == sceMcTypePS1)
				return sceMcResDeniedPS1Permit;
			return sceMcResSucceed;
		}
	}

	mcdi->cardtype = 0;
	mcdi->cardform = 0;
	mcman_invhandles(port, slot);
	mcman_clearcache(port, slot);

	return r;
}

//--------------------------------------------------------------
int McOpen(int port, int slot, char *filename, int flag) // Export #6
{
	register int r;
	
	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed) 
		return r;
		
	if (!PS1CardFlag)
		flag &= 0xFFFFDFFF; // disables FRCOM flag OR what is it

	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_open2(port, slot, filename, flag);
	else
		r = mcman_open1(port, slot, filename, flag); 
		
	if (r < -9)	{
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
		
	return r;
}

//--------------------------------------------------------------
int McClose(int fd) // Export #7
{
	register MC_FHANDLE *fh;
	register MCDevInfo *mcdi;
	register int r;
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;	
	
	fh->status = 0;
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;
	
	r = mcman_flushmccache(fh->port, fh->slot);	
	if (r < -9)	{
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
	
	if (r != sceMcResSucceed)
		return r;
		
	if (fh->unknown2 != 0) {
		fh->unknown2 = 0;
		mcdi = (MCDevInfo *)&mcman_devinfos[fh->port][fh->slot];
		if (mcdi->cardtype == sceMcTypePS2)
			r = mcman_close2(fd);
		else 
			r = mcman_close1(fd);
			
		if (r < -9)	{
			mcman_invhandles(fh->port, fh->slot);
			mcman_clearcache(fh->port, fh->slot);
		}
		
		if (r != sceMcResSucceed)
			return r;
	}	
		
	r = mcman_flushmccache(fh->port, fh->slot);	
	if (r < -9)	{
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}

	return r;
}

//--------------------------------------------------------------
int McFlush(int fd) // Export #14
{
	register int r;
	register MC_FHANDLE *fh;
	register MCDevInfo *mcdi;
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;	
	
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;

	r = mcman_flushmccache(fh->port, fh->slot);
	if (r < -9) {
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
	
	if (r != sceMcResSucceed) {	
		fh->status = 0;	
		return r;
	}

	if (fh->unknown2 != 0) {
		fh->unknown2 = 0;
		mcdi = (MCDevInfo *)&mcman_devinfos[fh->port][fh->slot];
		if (mcdi->cardtype == sceMcTypePS2)
			r = mcman_close2(fd);
		else 
			r = mcman_close1(fd);
			
		if (r < -9)	{
			mcman_invhandles(fh->port, fh->slot);
			mcman_clearcache(fh->port, fh->slot);
		}
		
		if (r != sceMcResSucceed) {	
			fh->status = 0;	
			return r;
		}
	}
	
	r = mcman_flushmccache(fh->port, fh->slot);
	if (r < 0)
		fh->status = 0;	
	
	if (r < -9) {
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
			
	return r;
}

//--------------------------------------------------------------
int McSeek(int fd, int offset, int origin) // Export #10
{
	register int r;
	register MC_FHANDLE *fh;
	
#ifdef DEBUG	
	DPRINTF("mcman: McSeek fd %d offset %d origin %d\n", fd, offset, origin);
#endif	
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;	
	
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;

	switch (origin) {
		default:		
		case SEEK_CUR:
			r = fh->position + offset;
			break;
		case SEEK_SET:	
			r = offset;
			break;			
		case SEEK_END:	
			r = fh->filesize + offset;		
			break;			
	}

	return fh->position = (r < 0) ? 0 : r;
}

//--------------------------------------------------------------
int McRead(int fd, void *buf, int length) // Export #8
{
	register int r;
	register MC_FHANDLE *fh;
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;
	
	if (!fh->rdflag) 	
		return sceMcResDeniedPermit;
				
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;
	
	if (mcman_devinfos[fh->port][fh->slot].cardtype == sceMcTypePS2)
		r = mcman_read2(fd, buf, length);
	else
		r = mcman_read1(fd, buf, length);		
		
    if (r < 0)
    	fh->status = 0;

	if (r < -9) {
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
 	    	
	return r;
}

//--------------------------------------------------------------
int McWrite(int fd, void *buf, int length) // Export #9
{
	register int r;
	register MC_FHANDLE *fh;
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;
	
	if (!fh->wrflag) 	
		return sceMcResDeniedPermit;
	
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;
	
	if (mcman_devinfos[fh->port][fh->slot].cardtype == sceMcTypePS2)
		r = mcman_write2(fd, buf, length);
	else
		r = mcman_write1(fd, buf, length);		
		
    if (r < 0)
    	fh->status = 0;
    	
	if (r < -9) {
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
    	
	return r;
}

//--------------------------------------------------------------
int McGetEntSpace(int port, int slot, char *dirname) // Export #23 XMCMAN only
{
	register int r;
	
	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;
	
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2) {
		r = mcman_getentspace(port, slot, dirname);
	}
	
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	return r;
}

//--------------------------------------------------------------
int McGetDir(int port, int slot, char *dirname, int flags, int maxent, sceMcTblGetDir *info) // Export #12
{
	register int r;

	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;
	
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_getdir2(port, slot, dirname, flags & 0xFFFF, maxent, info);	
	else
		r = mcman_getdir1(port, slot, dirname, flags & 0xFFFF, maxent, info);	
	
	if (r < -9) {	
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}

	return r;		
}

//--------------------------------------------------------------
int mcman_dread(int fd, fio_dirent_t *dirent)
{
	register int r;
	register MC_FHANDLE *fh;
	
	if (!((u32)fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[fd];	
	if (!fh->status) 
		return sceMcResDeniedPermit;
		
	if (!fh->drdflag) 	
		return sceMcResDeniedPermit;
		
	r = McDetectCard(fh->port, fh->slot);
	if (r != sceMcResSucceed)
		return r;

	if (mcman_devinfos[fh->port][fh->slot].cardtype == sceMcTypePS2)
		r = mcman_dread2(fd, dirent);
	else
		r = mcman_dread1(fd, dirent);
			
    if (r < 0)
    	fh->status = 0;
    	
	if (r < -9) {
		mcman_invhandles(fh->port, fh->slot);
		mcman_clearcache(fh->port, fh->slot);
	}
    	
	return r;
}

//--------------------------------------------------------------
int mcman_getstat(int port, int slot, char *filename, fio_stat_t *stat)
{
	register int r;

	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;
	
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_getstat2(port, slot, filename, stat);
	else	
		r = mcman_getstat1(port, slot, filename, stat);
	
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
		
	return r;	
}

//--------------------------------------------------------------
int McSetFileInfo(int port, int slot, char *filename, sceMcTblGetDir *info, int flags) // Export #16
{
	register int r;

	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;
		
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_setinfo2(port, slot, filename, info, flags);
	else
		r = mcman_setinfo1(port, slot, filename, info, flags);
	
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	if (r == sceMcResSucceed) {
		r = mcman_flushmccache(port, slot);
		if (r < -9) {
			mcman_invhandles(port, slot);
			mcman_clearcache(port, slot);
		}
	}
	
	return r;
}

//--------------------------------------------------------------
int McChDir(int port, int slot, char *newdir, char *currentdir) // Export #15
{
	register int r;

	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;

	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_chdir(port, slot, newdir, currentdir);
	else {	
		currentdir[0] = 0;
		r = sceMcResSucceed;
	}
	
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	return r;
}

//--------------------------------------------------------------
int McDelete(int port, int slot, char *filename, int flags) // Export #13
{
	register int r;

	r = McDetectCard(port, slot);
	if (r != sceMcResSucceed)
		return r;

	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_delete2(port, slot, filename, flags);
	else
		r = mcman_delete1(port, slot, filename, flags);
		
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	return r;
}

//--------------------------------------------------------------
int McFormat(int port, int slot) // Export #11
{
	register int r;
	
	mcman_invhandles(port, slot);
		
	r = McDetectCard(port, slot);
	if (r < -2)
		return r;
	
	mcman_clearcache(port, slot);
	
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_format2(port, slot);
	else	
		r = mcman_format1(port, slot);
		
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	return r;
}

//--------------------------------------------------------------
int McUnformat(int port, int slot) // Export #36
{
	register int r;
	
	mcman_invhandles(port, slot);
		
	r = McDetectCard(port, slot);
	if (r < -2)
		return r;
	
	mcman_clearcache(port, slot);
	
	if (mcman_devinfos[port][slot].cardtype == sceMcTypePS2)
		r = mcman_unformat2(port, slot);
	else	
		r = mcman_unformat1(port, slot);
	
	mcman_devinfos[port][slot].cardform = 0;
	
	if (r < -9) {
		mcman_invhandles(port, slot);
		mcman_clearcache(port, slot);
	}
	
	return r;		
}

//--------------------------------------------------------------
int mcman_getmcrtime(sceMcStDateTime *time)
{
	register int retries;
	cd_clock_t cdtime;
	
	retries = 64;
	
	do {
		if (sceCdRC(&cdtime))
			break;
	} while (--retries > 0);
	
	if (cdtime.stat & 128) {
		*((u16 *)&cdtime.month) = 0x7d0;
		cdtime.day = 3;
		cdtime.week = 4;
		cdtime.hour = 0;
		cdtime.minute = 0;
		cdtime.second = 0;
		cdtime.stat = 0;
	}
	
	time->Resv2 = 0;
	time->Sec = ((((cdtime.second >> 4) << 2) + (cdtime.second >> 4)) << 1) + (cdtime.second & 0xf);
	time->Min = ((((cdtime.minute >> 4) << 2) + (cdtime.minute >> 4)) << 1) + (cdtime.minute & 0xf);
	time->Hour = ((((cdtime.hour >> 4) << 2) + (cdtime.hour >> 4)) << 1) + (cdtime.hour & 0xf);
	time->Day = ((((cdtime.day >> 4) << 2) + (cdtime.day >> 4)) << 1) + (cdtime.day & 0xf);
	
	if ((cdtime.month & 0x10) != 0)
		time->Month = (cdtime.month & 0xf) + 0xa;
	else	
		time->Month = cdtime.month & 0xf;
	
	time->Year = ((((cdtime.year >> 4) << 2) + (cdtime.year >> 4)) << 1) + ((cdtime.year & 0xf) | 0x7d0);

	return 0;	
}

//--------------------------------------------------------------
int McEraseBlock(int port, int block, void **pagebuf, void *eccbuf) // Export #17 in MCMAN
{
	return mcman_eraseblock(port, 0, block, (void **)pagebuf, eccbuf);
}

//--------------------------------------------------------------
int McEraseBlock2(int port, int slot, int block, void **pagebuf, void *eccbuf) // Export #17 in XMCMAN
{
	return mcman_eraseblock(port, slot, block, (void **)pagebuf, eccbuf);
}

//--------------------------------------------------------------
int McReadPage(int port, int slot, int page, void *buf) // Export #18
{
	register int r, index, ecres, retries, count, erase_byte;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	u8 eccbuf[32];
	u8 *pdata, *peccb;
 
	count = (mcdi->pagesize + 127) >> 7;
	erase_byte = (mcdi->cardflags & CF_ERASE_ZEROES) ? 0x0 : 0xFF;

	retries = 0;
	ecres = sceMcResSucceed;
	do {
		if (!mcman_readpage(port, slot, page, buf, eccbuf)) {
			if (mcdi->cardflags & CF_USE_ECC) { // checking ECC from spare data block
   				// check for erased page (last byte of spare data set to 0xFF or 0x0)/
    			if (eccbuf[mcman_sparesize(port, slot) - 1] == erase_byte)
    				break;

    			index = 0;

    			if (count > 0) {
     				peccb = (u8 *)eccbuf;
     				pdata = (u8 *)buf;

     				do {
      					r = mcman_correctdata(pdata, peccb);
      					if (r < ecres)
      						ecres = r;

      					peccb += 3;
      					pdata += 128;
					} while (++index < count);
				}

				if (ecres == sceMcResSucceed)
					break;
					
				if ((retries == 4) && (!(ecres < sceMcResNoFormat)))
					break;
			}
		}
	} while (++retries < 5);

	if (retries < 5)
		return sceMcResSucceed;

	return (ecres != sceMcResSucceed) ? sceMcResNoFormat : sceMcResChangedCard;	
}

//--------------------------------------------------------------
void McDataChecksum(void *buf, void *ecc) // Export #20
{
	register u8 *p, *p_ecc;
	register int i, a2, a3, v, t0;

	p = buf;
	i = 0;
	a2 = 0;
	a3 = 0;
	t0 = 0;

	do {
		v = mcman_xortable[*p++];
		a2 ^= v;
		if (v & 0x80) {
			a3 ^= ~i;
			t0 ^= i;
		}
	} while (++i < 0x80);
	
	p_ecc = ecc;
	p_ecc[0] = ~a2 & 0x77;
	p_ecc[1] = ~a3 & 0x7F;
	p_ecc[2] = ~t0 & 0x7F;
}

//--------------------------------------------------------------
int mcman_getcnum(int port, int slot)
{
	return ((port & 1) << 3) + slot;
}

//--------------------------------------------------------------
int mcman_correctdata(void *buf, void *ecc)
{
	register int xor0, xor1, xor2, xor3, xor4;
	u8 eccbuf[12];
	u8 *p = (u8 *)ecc;

	McDataChecksum(buf, eccbuf);

	xor0 = p[0] ^ eccbuf[0];		
	xor1 = p[1] ^ eccbuf[1];
	xor2 = p[2] ^ eccbuf[2];
	
	xor3 = xor1 ^ xor2;
	xor4 = (xor0 & 0xf) ^ (xor0 >> 4);
	
	if (!xor0 && !xor1 && !xor2)
		return 0;

	if ((xor3 == 0x7f) && (xor4 == 0x7)) {
		p[xor2] ^= 1 << (xor0 >> 4);
		return -1;
	}

	xor0 = 0;
	xor2 = 7;
	do {
		if ((xor3 & 1))
			xor0++;
		xor2--;	
		xor3 = xor3 >> 1;
	} while (xor2 >= 0);
	
	xor2 = 3;
	do {	
		if ((xor4 & 1))
			xor0++;
		xor2--;	
		xor4 = xor4 >> 1;
	} while (xor2 >= 0);	

	if (xor0 == 1)
		return -2;
		
	return -3;	
}

//--------------------------------------------------------------
int mcman_sparesize(int port, int slot)
{ // Get ps2 mc spare size by dividing pagesize / 32
	return (mcman_devinfos[port][slot].pagesize + 0x1F) >> 5;
}

//--------------------------------------------------------------
int mcman_setdevspec(int port, int slot)
{
	int cardsize;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];

#ifdef DEBUG		
	DPRINTF("mcman: mcman_setdevspec port%d, slot%d\n", port, slot);	
#endif
	
	if (McGetCardSpec(port, slot, &mcdi->pagesize, &mcdi->blocksize, &cardsize, &mcdi->cardflags) != sceMcResSucceed)
		return sceMcResFullDevice;
		
	mcdi->pages_per_cluster = MCMAN_CLUSTERSIZE / mcdi->pagesize;
	mcdi->cluster_size = MCMAN_CLUSTERSIZE;
	mcdi->unknown1 = 0;
	mcdi->unknown2 = 0;
	mcdi->unused = 0xff00;
	mcdi->FATentries_per_cluster = MCMAN_CLUSTERFATENTRIES;
	mcdi->unknown5 = -1;
	mcdi->rootdir_cluster2 = mcdi->rootdir_cluster;
	mcdi->clusters_per_block = mcdi->blocksize / mcdi->pages_per_cluster;
	mcdi->clusters_per_card = (cardsize / mcdi->blocksize) * (mcdi->blocksize / mcdi->pages_per_cluster);
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_setdevinfos(int port, int slot)
{
	register int r, allocatable_clusters_per_card, iscluster_valid, current_allocatable_cluster, cluster_cnt;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	McFsEntry *pfse;
	
#ifdef DEBUG	
	DPRINTF("mcman: mcman_setdevinfos port%d slot%d\n", port, slot);
#endif	
	
	mcman_wmemset((void *)mcdi, sizeof(MCDevInfo), 0);
	
	r = mcman_setdevspec(port, slot);
	if (r != sceMcResSucceed)
		return -49;
	
	r = McReadPage(port, slot, 0, mcman_pagebuf);	
	if (r == sceMcResNoFormat)
		return sceMcResNoFormat; // should rebuild a valid superblock here
	if (r != sceMcResSucceed)
		return -48;
			
	if (strncmp(SUPERBLOCK_MAGIC, mcman_pagebuf, 28) != 0) {
#ifdef DEBUG		
		DPRINTF("mcman: mcman_setdevinfos No card format !!!\n");
#endif		
		return sceMcResNoFormat;
	}
	
	if (((mcman_pagebuf[28] - 48) == 1) && ((mcman_pagebuf[30] - 48) == 0))  // check ver major & minor	
		return sceMcResNoFormat;
			
	u8 *p = (u8 *)mcdi;
	for (r=0; r<0x150; r++) 
		p[r] = mcman_pagebuf[r];

	mcdi->cardtype = sceMcTypePS2; // <--
	
	r = mcman_checkBackupBlocks(port, slot);
	if (r != sceMcResSucceed)
		return -47;
		
	r = mcman_readdirentry(port, slot, 0, 0, &pfse);	
	if (r != sceMcResSucceed)
		return sceMcResNoFormat; // -46
		
	if (strcmp(pfse->name, ".") != 0)		
		return sceMcResNoFormat;	
	
	if (mcman_readdirentry(port, slot, 0, 1, &pfse) != sceMcResSucceed)
		return -45;
		
	if (strcmp(pfse->name, "..") != 0)		
		return sceMcResNoFormat;	
			
	mcdi->cardform = 1;	
//	mcdi->cardtype = sceMcTypePS2;
	
	if (((mcman_pagebuf[28] - 48) == 1) && ((mcman_pagebuf[30] - 48) == 1)) {  // check ver major & minor	
		if ((mcdi->clusters_per_block * mcdi->backup_block2) == mcdi->alloc_end)
			mcdi->alloc_end = (mcdi->clusters_per_block * mcdi->backup_block2) - mcdi->alloc_offset;
	}
		
	u32 hi, lo, temp;
	
	long_multiply(mcdi->clusters_per_card, 0x10624dd3, &hi, &lo);
	temp = (hi >> 6) - (mcdi->clusters_per_card >> 31);
	allocatable_clusters_per_card = (((((temp << 5) - temp) << 2) + temp) << 3) + 1;
	iscluster_valid = 0;
	cluster_cnt = 0;
	current_allocatable_cluster = mcdi->alloc_offset;
	
	while (cluster_cnt < allocatable_clusters_per_card) {
		if (current_allocatable_cluster >= mcdi->clusters_per_card)
			break;

		if (((current_allocatable_cluster % mcdi->clusters_per_block) == 0) \
			|| (mcdi->alloc_offset == current_allocatable_cluster)) {
			iscluster_valid = 1;
			for (r=0; r<16; r++) {
				if ((current_allocatable_cluster / mcdi->clusters_per_block) == mcdi->bad_block_list[r])
					iscluster_valid = 0;
			}				
		}
		if (iscluster_valid == 1) 
			cluster_cnt++;
		current_allocatable_cluster++;
	}	
	  
	mcdi->max_allocatable_clusters = current_allocatable_cluster - mcdi->alloc_offset;
	
	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_reportBadBlocks(int port, int slot)
{
	register int r, i, block, bad_blocks, page, erase_byte, err_cnt, err_limit; 
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	u8 *p;

#ifdef DEBUG	
	DPRINTF("mcman: mcman_reportBadBlocks port%d, slot%d\n", port, slot);	
#endif	
	
	mcman_wmemset((void *)mcdi->bad_block_list, 128, -1);	
	
	if ((mcdi->cardflags & CF_BAD_BLOCK) == 0)
		return sceMcResSucceed;	
	
	err_limit = (((mcdi->pagesize << 16) >> 16) + ((mcdi->pagesize << 16) >> 31)) >> 1; //s7
		
	erase_byte = 0;	
	if ((mcdi->cardflags & CF_ERASE_ZEROES) != 0)	
		erase_byte = 0xff;
				
	bad_blocks = 0; // s2
	
	if ((mcdi->clusters_per_card / mcdi->clusters_per_block) > 0) {
			
		block = 0; // s1
	
		do {
			if (bad_blocks >= 16)
				break;
		
			err_cnt = 0; 	//s4
			page = 0; //s3		
			do {
				
				r = McReadPage(port, slot, (block * mcdi->blocksize) + page, mcman_pagebuf);
				if (r == sceMcResNoFormat) {
					mcdi->bad_block_list[bad_blocks] = block;
					bad_blocks++;
					break;
				}
				if (r != sceMcResSucceed)
					return r;
				
				if ((mcdi->cardflags & CF_USE_ECC) == 0) {
					p = (u8 *)&mcman_pagebuf;
					for (i = 0; i < mcdi->pagesize; i++) {
						// check if the content of page is clean
						if (*p++ != erase_byte)
							err_cnt++;
							
						if (err_cnt >= err_limit) {
							mcdi->bad_block_list[bad_blocks] = block;
							bad_blocks++;
							break;
						}
					}
				}
			} while (++page < 2);

		} while (++block < (mcdi->clusters_per_card / mcdi->clusters_per_block));
	}
		
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_createDirentry(int port, int slot, int parent_cluster, int num_entries, int cluster, sceMcStDateTime *ctime)
{
	register int r;	
	McCacheEntry *mce;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McFsEntry *mfe, *mfe_next, *pfse;

#ifdef DEBUG	
	DPRINTF("mcman: mcman_createDirentry port%d slot%d parent_cluster %x num_entries %d cluster %x\n", port, slot, parent_cluster, num_entries, cluster);
#endif	
	
	r = mcman_readcluster(port, slot, mcdi->alloc_offset + cluster, &mce);
	if (r != sceMcResSucceed)
		return r;
		
	mcman_wmemset(mce->cl_data, MCMAN_CLUSTERSIZE, 0);

	mfe = (McFsEntry*)mce->cl_data;
	mfe_next = (McFsEntry*)(mce->cl_data + sizeof (McFsEntry));	
	
	mfe->mode = sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable \
			  | sceMcFileAttrSubdir | sceMcFile0400 | sceMcFileAttrExists; // 0x8427

	if (ctime == NULL)
		mcman_getmcrtime(&mfe->created);
	else
		mfe->created = *ctime;

	mfe->modified =	mfe->created;
	
	mfe->length = 0;
	mfe->dir_entry = num_entries;
	mfe->cluster = parent_cluster;
	*(u16 *)&mfe->name = *((u16 *)&DOT);
	
	if ((parent_cluster == 0) && (num_entries == 0)) {
		// entry is root directory
		mfe_next->created = mfe->created;
		mfe->length = 2;
		mfe++;

		mfe->mode = sceMcFileAttrWriteable | sceMcFileAttrExecutable | sceMcFileAttrSubdir \
				  | sceMcFile0400 | sceMcFileAttrExists | sceMcFileAttrHidden; // 0xa426
		mfe->dir_entry = 0;
		mfe->cluster = 0;
	}
	else {
		// entry is normal "." / ".."
		mcman_readdirentry(port, slot, parent_cluster, 0, &pfse);

		mfe_next->created = pfse->created;
		mfe++;

		mfe->mode = sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable \
				  | sceMcFileAttrSubdir | sceMcFile0400 | sceMcFileAttrExists; // 0x8427
		mfe->dir_entry = pfse->dir_entry;
		
		mfe->cluster = pfse->cluster;
	}

	mfe->modified = mfe->created;
	mfe->length = 0;
	
	*(u16 *)&mfe->name = *(u16 *)&DOTDOT;
	*(u8 *)&mfe->name[2] = *((u8 *)&DOTDOT+2);

	mce->wr_flag = 1;

	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_fatRseek(int fd)
{
	register int r, entries_to_read, fat_index;
	register MC_FHANDLE *fh = (MC_FHANDLE *)&mcman_fdhandles[fd]; //s1
	register MCDevInfo *mcdi = &mcman_devinfos[fh->port][fh->slot];	//s2
	int fat_entry;
	
	entries_to_read = fh->position / mcdi->cluster_size; //s0
	
	//s5 = 0
	
	if (entries_to_read < fh->clust_offset) //v1 = fh->fh->clust_offset
		fat_index = fh->freeclink;
	else {
		fat_index = fh->clink; // a2
		entries_to_read -= fh->clust_offset; 		
	}
	
	if (entries_to_read == 0) {
		if (fat_index >= 0)
			return fat_index + mcdi->alloc_offset;
	
		return sceMcResFullDevice;			
	}
	
	do {
		r = mcman_getFATentry(fh->port, fh->slot, fat_index, &fat_entry);
		if (r != sceMcResSucceed)
			return r;
		
		fat_index = fat_entry;
			
		if (fat_index >= -1)
			return sceMcResFullDevice;
	
		entries_to_read--;
	
		fat_index &= 0x7fffffff;
		fh->clink = fat_index;
		fh->clust_offset = (fh->position / mcdi->cluster_size) - entries_to_read;
		
	} while (entries_to_read > 0);	
		
	return fat_index + mcdi->alloc_offset;
}

//--------------------------------------------------------------
int mcman_fatWseek(int fd) // modify FAT to hold new content for a file
{
	register int r, entries_to_write, fat_index;
	register MC_FHANDLE *fh = (MC_FHANDLE *)&mcman_fdhandles[fd];
	register MCDevInfo *mcdi = &mcman_devinfos[fh->port][fh->slot];	
	register McCacheEntry *mce;	
	int fat_entry;
	
	entries_to_write = fh->position / mcdi->cluster_size;
	
	if ((fh->clust_offset == 0) || (entries_to_write < fh->clust_offset)) {
		fat_index = fh->freeclink;
		
		if (fat_index < 0) {
			fat_index = mcman_findfree2(fh->port, fh->slot, 1);
			
			if (fat_index < 0)
				return sceMcResFullDevice;
				
			mce = (McCacheEntry *)mcman_get1stcacheEntp();
			fh->freeclink = fat_index;
			
			r = mcman_close2(fd);
			if (r != sceMcResSucceed)
				return r;
				
			mcman_addcacheentry(mce);
			mcman_flushmccache(fh->port, fh->slot);
		}
	}
	else {
		fat_index = fh->clink;
		entries_to_write -= fh->clust_offset;
	}
	
	if (entries_to_write != 0) {
		do {
			r = mcman_getFATentry(fh->port, fh->slot, fat_index, &fat_entry);
			if (r != sceMcResSucceed)
				return r;
			
			if (fat_entry >= 0xffffffff) {
				r = mcman_findfree2(fh->port, fh->slot, 1);
				if (r < 0)
						return r;
				fat_entry = r;
				fat_entry |= 0x80000000;						
				
				mce = (McCacheEntry *)mcman_get1stcacheEntp();
				
				r = mcman_setFATentry(fh->port, fh->slot, fat_index, fat_entry);
				if (r != sceMcResSucceed)
					return r;
					
				mcman_addcacheentry(mce);	
			}
			
			entries_to_write--;
			fat_index = fat_entry & 0x7fffffff;
		} while (entries_to_write > 0);
	}
	
	fh->clink = fat_index;
	fh->clust_offset = fh->position / mcdi->cluster_size;
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_findfree2(int port, int slot, int reserve)
{
	register int r, rfree, indirect_index, ifc_index, fat_offset, indirect_offset, fat_index, block;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McCacheEntry *mce1, *mce2;	
	
#ifdef DEBUG	
	DPRINTF("mcman: mcman_findfree2 port%d slot%d reserve%d\n", port, slot, reserve);
#endif	
	
	fat_index = mcdi->unknown2;
	rfree = 0;

	for (fat_index = mcdi->unknown2; fat_index < mcdi->max_allocatable_clusters; fat_index++) {
	
		indirect_index = fat_index / mcdi->FATentries_per_cluster;
		fat_offset = fat_index % mcdi->FATentries_per_cluster;
					
		if ((fat_offset == 0) || (fat_index == mcdi->unknown2)) {
				
			ifc_index = indirect_index / mcdi->FATentries_per_cluster;
			r = mcman_readcluster(port, slot, mcdi->ifc_list[ifc_index], &mce1);
			if (r != sceMcResSucceed)
				return r;
		//}
		//if ((fat_offset == 0) || (fat_index == mcdi->unknown2)) {
			indirect_offset = indirect_index % mcdi->FATentries_per_cluster;
			McFatCluster *fc = (McFatCluster *)mce1->cl_data;
			r = mcman_readcluster(port, slot, fc->entry[indirect_offset], &mce2);
			if (r != sceMcResSucceed)
				return r;
		}

		McFatCluster *fc = (McFatCluster *)mce2->cl_data;
		
		if (fc->entry[fat_offset] >= 0) {
			block = (mcdi->alloc_offset + fat_offset) / mcdi->clusters_per_block;
			if (block != mcman_badblock) {
				if (reserve) {
					fc->entry[fat_offset] = 0xffffffff;
					mce2->wr_flag = 1;
					mcdi->unknown2 = fat_index;
					return fat_index;
				}
				rfree++;
			}
		}
	}
	
	if (reserve) 
		return sceMcResFullDevice;
		
	return (rfree) ? rfree : sceMcResFullDevice;	
}

//-------------------------------------------------------------- 
int mcman_getentspace(int port, int slot, char *dirname)
{
	register int r, i, entspace;
	McCacheDir cacheDir;
	McFsEntry *fse;
	McFsEntry mfe;
	u8 *pfsentry, *pmfe, *pfseend;

#ifdef DEBUG		
	DPRINTF("mcman: mcman_getentspace port%d slot%d dirname %s\n", port, slot, dirname);
#endif	
		
	r = mcman_cachedirentry(port, slot, dirname, &cacheDir, &fse, 1);
	if (r > 0)
		return sceMcResNoEntry;
	if (r < 0)
		return r;
			
	pfsentry = (u8 *)fse;	
	pmfe = (u8 *)&mfe;	
	pfseend = (u8 *)(pfsentry + sizeof (McFsEntry));
	
	do {
		*((u32 *)pmfe  ) = *((u32 *)pfsentry  );
		*((u32 *)pmfe+1) = *((u32 *)pfsentry+1);
		*((u32 *)pmfe+2) = *((u32 *)pfsentry+2);
		*((u32 *)pmfe+3) = *((u32 *)pfsentry+3);	
		pfsentry += 16;
		pmfe += 16;
	} while (pfsentry < pfseend);
	
	entspace = mfe.length & 1;
	
	for (i = 0; i < mfe.length; i++) {
		
		r = mcman_readdirentry(port, slot, mfe.cluster, i, &fse);
		if (r != sceMcResSucceed)
			return r;
		if ((fse->mode & sceMcFileAttrExists) == 0)
			entspace++;
	}
				
	return entspace;
}

//-------------------------------------------------------------- 
int mcman_cachedirentry(int port, int slot, char *filename, McCacheDir *pcacheDir, McFsEntry **pfse, int unknown_flag)
{
	register int r, fsindex, cluster, fmode;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McFsEntry *fse;
	McCacheDir cacheDir;
	u8 *p, *pfsentry, *pcache, *pfseend;

#ifdef DEBUG		
	DPRINTF("mcman: mcman_cachedirentry port%d slot%d name %s\n", port, slot, filename);
#endif	
	
	if (pcacheDir == NULL) {
		pcacheDir = &cacheDir;
		pcacheDir->maxent = -1;
	}
	
	p = (u8 *)filename;
	if (*p == '/') {
		p++;
		cluster = 0;
		fsindex = 0;
	}
	else {
		cluster = mcdi->rootdir_cluster2;
		fsindex = mcdi->unknown1;
	}
	
	r = mcman_readdirentry(port, slot, cluster, fsindex, &fse);
	if (r != sceMcResSucceed)
		return r;
		
	if (*p == 0) {
		if (!(fse->mode & sceMcFileAttrExists))
			return 2;
				
		if (pcacheDir == NULL) {
			*pfse = (McFsEntry *)fse;	
			return sceMcResSucceed;
		}
	
		pfsentry = (u8 *)fse;	
		pcache = (u8 *)&mcman_dircache[0];	
		pfseend = (u8 *)(pfsentry + sizeof (McFsEntry));
	
		do {
			*((u32 *)pcache  ) = *((u32 *)pfsentry  );
			*((u32 *)pcache+1) = *((u32 *)pfsentry+1);
			*((u32 *)pcache+2) = *((u32 *)pfsentry+2);
			*((u32 *)pcache+3) = *((u32 *)pfsentry+3);	
			pfsentry += 16;
			pcache += 16;
		} while (pfsentry < pfseend);
		
		r = mcman_getdirinfo(port, slot, (McFsEntry *)&mcman_dircache[0], ".", pcacheDir, unknown_flag);	
		
		mcman_readdirentry(port, slot, pcacheDir->cluster, pcacheDir->fsindex, pfse);
	
		if (r > 0)
			return 2;
		 
		return r;	
		
	} else {
		
		do {
			fmode = sceMcFileAttrReadable | sceMcFileAttrExecutable;
			if ((fse->mode & fmode) != fmode)
				return sceMcResDeniedPermit;
		
			if (mcman_chrpos(p, '/') < 0) 	
				strlen(p);
			
			pfsentry = (u8 *)fse;	
			pcache = (u8 *)&mcman_dircache[0];	
			pfseend = (u8 *)(pfsentry + sizeof(McFsEntry));
	
			do {
				*((u32 *)pcache  ) = *((u32 *)pfsentry  );
				*((u32 *)pcache+1) = *((u32 *)pfsentry+1);
				*((u32 *)pcache+2) = *((u32 *)pfsentry+2);
				*((u32 *)pcache+3) = *((u32 *)pfsentry+3);	
				pfsentry += 16;
				pcache += 16;
			} while (pfsentry < pfseend);

			r = mcman_getdirinfo(port, slot, (McFsEntry *)&mcman_dircache[0], p, pcacheDir, unknown_flag);				

			if (r > 0) {
				if (mcman_chrpos(p, '/') >= 0)
					return 2;
				
				pcacheDir->cluster = cluster;
				pcacheDir->fsindex = fsindex;
		
				return 1;
			}
		
			r = mcman_chrpos(p, '/');
			if ((r >= 0) && (p[r + 1] != 0)) {
				p += mcman_chrpos(p, '/') + 1;
				cluster = pcacheDir->cluster;
				fsindex = pcacheDir->fsindex;
				
				mcman_readdirentry(port, slot, cluster, fsindex, &fse);
			}
			else {
				mcman_readdirentry(port, slot, pcacheDir->cluster, pcacheDir->fsindex, pfse);
				
				return sceMcResSucceed;
			}
		} while (*p != 0);		
	}	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_getdirinfo(int port, int slot, McFsEntry *pfse, char *filename, McCacheDir *pcd, int unknown_flag)
{
	register int i, r, ret, len, pos;
	McFsEntry *fse;
	u8 *pfsentry, *pfsee, *pfseend;
	
#ifdef DEBUG	
	DPRINTF("mcman: mcman_getdirinfo port%d slot%d name %s\n", port, slot, filename);
#endif	
	
	pos = mcman_chrpos(filename, '/');
	if (pos < 0)
		pos = strlen(filename);
				
	ret = 0;
	if ((pos == 2) && (!strncmp(filename, "..", 2))) {
		
		r = mcman_readdirentry(port, slot, pfse->cluster, 0, &fse);
		if (r != sceMcResSucceed)
			return r;
			
		r = mcman_readdirentry(port, slot, fse->cluster, 0, &fse);		
		if (r != sceMcResSucceed)
			return r;

		if (pcd) {
			pcd->cluster = fse->cluster;
			pcd->fsindex = fse->dir_entry;
		}

		r = mcman_readdirentry(port, slot, fse->cluster, fse->dir_entry, &fse);		
		if (r != sceMcResSucceed)
			return r;
				
		pfsentry = (u8 *)fse;	
		pfsee = (u8 *)pfse;	
		pfseend = (u8 *)(pfsentry + sizeof(McFsEntry));
	
		do {
			*((u32 *)pfsee  ) = *((u32 *)pfsentry  );
			*((u32 *)pfsee+1) = *((u32 *)pfsentry+1);
			*((u32 *)pfsee+2) = *((u32 *)pfsentry+2);
			*((u32 *)pfsee+3) = *((u32 *)pfsentry+3);	
			pfsentry += 16;
			pfsee += 16;
		} while (pfsentry < pfseend);
			
		if ((fse->mode & sceMcFileAttrHidden) != 0) {
			ret = 1;
			if (!PS1CardFlag) {
				ret = 2;
				if ((pcd == NULL) || (pcd->maxent < 0))
					return 3;	
			}
		}
		
		if ((pcd == NULL) || (pcd->maxent < 0))
			return sceMcResSucceed;
	}
	else {
		if ((pos == 1) && (!strncmp(filename, ".", 1))) {
			
			r = mcman_readdirentry(port, slot, pfse->cluster, 0, &fse);
			if (r != sceMcResSucceed)
				return r;
		
			if (pcd) {
				pcd->cluster = fse->cluster;
				pcd->fsindex = fse->dir_entry;
			}
			
			if ((fse->mode & sceMcFileAttrHidden) != 0) {
				ret = 1;
				if (!PS1CardFlag) {
					ret = 2;
					if ((pcd == NULL) || (pcd->maxent < 0))
						return 3;	
				}
				else {
					if ((pcd == NULL) || (pcd->maxent < 0))
						return sceMcResSucceed;
				}
			}
			else {
				ret = 1;
				if ((pcd == NULL) || (pcd->maxent < 0))
					return sceMcResSucceed;				
			}	
		}
	}
	
	if ((pcd) && (pcd->maxent >= 0))
		pcd->maxent = pfse->length;
		
	if (pfse->length > 0) {
		
		i = 0;
		do {
			r = mcman_readdirentry(port, slot, pfse->cluster, i, &fse);
			if (r != sceMcResSucceed)
				return r;
				
			if (((fse->mode & sceMcFileAttrExists) == 0) && (pcd) && (i < pcd->maxent))
				 pcd->maxent = i;
		
			if (unknown_flag) {
				if ((fse->mode & sceMcFileAttrExists) == 0)
					continue;
			}
			else {
				if ((fse->mode & sceMcFileAttrExists) != 0)
					continue;
			} 
		
			if (ret != 0)	
				continue;
				
			if ((pos >= 11) && (!strncmp(&filename[10], &fse->name[10], pos-10))) {
				len = pos;
				if (strlen(fse->name) >= pos) 
					len = strlen(fse->name);
				
				if (!strncmp(filename, fse->name, len))
					goto continue_check; 
			}	
		
			if (strlen(fse->name) >= pos)
				len = strlen(fse->name);
			else
				len = pos;
			
			if (strncmp(filename, fse->name, len))
				continue;	
				
continue_check:
			ret = 1;
			
			if ((fse->mode & sceMcFileAttrHidden) != 0) {
				if (!PS1CardFlag)
					ret = 2;
			}

			if (pcd == NULL)
				break;	
			
			pcd->fsindex = i;
			pcd->cluster = pfse->cluster;
			
			if (pcd->maxent < 0)
				break;
			
		} while (++i < pfse->length);	
	}
	
	if (ret == 2)
		return 2;
			
	return ((ret < 1) ? 1 : 0);
}

//-------------------------------------------------------------- 
int mcman_writecluster(int port, int slot, int cluster, int flag)
{
	register int r, i, j, page, block, pageword_cnt;
	register u32 erase_value;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	
	block = cluster / mcdi->clusters_per_block;
	
	if ((mcman_wr_port == port) && (mcman_wr_slot == slot) && (mcman_wr_block == block))
		return mcman_wr_flag3;
		
	mcman_wr_port = port;
	mcman_wr_slot = slot;
	mcman_wr_block = block;
	mcman_wr_flag3 = -9;
	
	for (i = 0; i < 16; i++) { // check only 16 bad blocks ?
		if (mcdi->bad_block_list[i] < 0)
			break;
		if (mcdi->bad_block_list[i] == block) {
			mcman_wr_flag3 = 0;
			return sceMcResSucceed;
		}
	}
	
	if (flag) {
		for (i = 1; i < mcdi->blocksize; i++)
			mcman_pagedata[i] = 0;
			
		mcman_pagedata[0] = &mcman_pagebuf;
		
		pageword_cnt = mcdi->pagesize >> 2;
		page = block * mcdi->blocksize;
		
		if (mcdi->cardflags & CF_ERASE_ZEROES)
			erase_value = 0xffffffff;
		else
			erase_value = 0x00000000;	
			
		for (i = 0; i < pageword_cnt; i++)
			*((u32 *)&mcman_pagebuf + i) = erase_value;
	
		r = mcman_eraseblock(port, slot, block, (void **)mcman_pagedata, (void *)mcman_eccdata);
		if (r == sceMcResFailReplace)
			return sceMcResSucceed;
		if (r != sceMcResSucceed)
			return sceMcResChangedCard;
		
		for (i = 1; i < mcdi->blocksize; i++) {
			r = McWritePage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
			if (r == sceMcResFailReplace)
				return sceMcResSucceed;
			if (r != sceMcResSucceed)
				return sceMcResNoFormat;
		}

		for (i = 1; i < mcdi->blocksize; i++) {
			r = McReadPage(port, slot, page + i, mcman_pagebuf);
			if (r == sceMcResNoFormat)
				return sceMcResSucceed;
			if (r != sceMcResSucceed)
				return sceMcResFullDevice;

			for (j = 0; j < pageword_cnt; j++) {
				if (*((u32 *)&mcman_pagebuf + j) != erase_value) {
					mcman_wr_flag3 = 0;
					return sceMcResSucceed;
				}
			}
		}
			
		r = mcman_eraseblock(port, slot, block, NULL, NULL);
		if (r != sceMcResSucceed)
			return sceMcResChangedCard;
		
		r = McWritePage(port, slot, page, mcman_pagebuf, mcman_eccdata);
		if (r == sceMcResFailReplace)
			return sceMcResSucceed;
		if (r != sceMcResSucceed) 
			return sceMcResNoFormat;

		r = McReadPage(port, slot, page, mcman_pagebuf);
		if (r == sceMcResNoFormat)
			return sceMcResSucceed;
		if (r != sceMcResSucceed)
			return sceMcResFullDevice;
								
		for (j = 0; j < pageword_cnt; j++) {
			if (*((u32 *)&mcman_pagebuf + j) != erase_value) {
				mcman_wr_flag3 = 0;
				return sceMcResSucceed;
			}
		}
					
		r = mcman_eraseblock(port, slot, block, NULL, NULL);
		if (r == sceMcResFailReplace)
			return sceMcResSucceed;
		if (r != sceMcResSucceed)
			return sceMcResFullDevice;

		erase_value = ~erase_value;
									
		for (i = 0; i < mcdi->blocksize; i++) {
			r = McReadPage(port, slot, page + i, mcman_pagebuf);
			if (r != sceMcResSucceed)
				return sceMcResDeniedPermit;
				
			for (j = 0; j < pageword_cnt; j++) {
				if (*((u32 *)&mcman_pagebuf + j) != erase_value) {
					mcman_wr_flag3 = 0;  						
					return sceMcResSucceed;  			
				}  										
			}
		}
	}
	mcman_wr_flag3 = 1;

	return mcman_wr_flag3; 
}

//-------------------------------------------------------------- 
int mcman_setdirentrystate(int port, int slot, int cluster, int fsindex, int flags)
{
	register int r, i, fat_index;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	McFsEntry *fse;
	int fat_entry;
	
	r = mcman_readdirentry(port, slot, cluster, fsindex, &fse);
	if (r != sceMcResSucceed)
		return r;
		
	if (fse->name[0] == '.') {
		if ((fse->name[1] == 0) || (fse->name[1] == '.'))
			return sceMcResNoEntry;
	}
	
	i = 0;
	do {	
		if (mcman_fdhandles[i].status == 0)
			continue;
			
		if ((mcman_fdhandles[i].port != port) || (mcman_fdhandles[i].slot != slot))
			continue;		

		if (mcman_fdhandles[i].field_20 != cluster)
			continue;	
			
		if (mcman_fdhandles[i].field_24 == fsindex)
			return sceMcResDeniedPermit;	
						
	} while (++i < MAX_FDHANDLES);	
	
	if (flags == 0)
		fse->mode = fse->mode & (sceMcFileAttrExists - 1);
	else
		fse->mode = fse->mode | sceMcFileAttrExists;
		
	mcman_1stcacheEntsetwrflagoff();

	fat_index = fse->cluster;	
	
	if (fat_index >= 0) {
		if (fat_index < mcdi->unknown2)
			mcdi->unknown2 = fat_index;
		mcdi->unknown5 = -1;	
		
		do {	
			r = mcman_getFATentry(port, slot, fat_index, &fat_entry);
			if (r != sceMcResSucceed)
				return r;
	
			if (flags == 0)	{
				fat_entry &= 0x7fffffff;
				if (fat_index < mcdi->unknown2)
					mcdi->unknown2 = fat_entry;
			}
			else
				fat_entry |= 0x80000000;
			
			r = mcman_setFATentry(port, slot, fat_index, fat_entry);	
			if (r != sceMcResSucceed)
				return r;
			
			fat_index = fat_entry & 0x7fffffff;	
			
		} while (fat_index != 0x7fffffff);
	}
		
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_checkBackupBlocks(int port, int slot)
{
	register int r1, r2, r, value1, value2, eccsize;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McCacheEntry *mce;
	int *pagebuf = (int *)&mcman_pagebuf;	
	
	// First check backup block2 to see if it's in erased state 	
	r1 = McReadPage(port, slot, mcdi->backup_block2 * mcdi->blocksize, mcman_pagebuf); //s1
	
	value1 = *pagebuf; //s3
	if (((mcdi->cardflags & CF_ERASE_ZEROES) != 0) && (value1 == 0))
		value1 = -1;
	if (value1 != -1) 
		value1 = value1 & 0x7fffffff;
	
	r2 = McReadPage(port, slot, (mcdi->backup_block2 * mcdi->blocksize) + 1, mcman_pagebuf); //a0
	
	value2 = *pagebuf; //s0
	if (((mcdi->cardflags & CF_ERASE_ZEROES) != 0) && (value2 == 0))
		value2 = -1;
	if (value2 != -1) 
		value2 = value2 & 0x7fffffff;
			
	if ((value1 != -1) && (value2 == -1))		
		goto check_done;	
	if ((r1 < 0) || (r2 < 0))
		goto check_done;		

	if ((value1 == -1) && (value1 == value2))
		return sceMcResSucceed;
		
	// bachup block2 is not erased, so programming is assumed to have not been completed
	// reads content of backup block1
	for (r1 = 0; r1 < mcdi->clusters_per_block; r1++) {
		
		mcman_readcluster(port, slot, (mcdi->backup_block1 * mcdi->clusters_per_block) + r1, &mce);
		mce->rd_flag = 1;
		
		for (r2 = 0; r2 < mcdi->pages_per_cluster; r2++) {
			mcman_pagedata[(r1 * ((mcdi->pages_per_cluster << 16) >> 16)) + r2] = \
				(void *)(mce->cl_data + (r2 * mcdi->pagesize));
		}
	}
	
	// Erase the block where data must be written
	r = mcman_eraseblock(port, slot, value1, (void **)mcman_pagedata, (void *)mcman_eccdata);
	if (r != sceMcResSucceed)
		return r;
	
	// Write the block	
	for (r1 = 0; r1 < mcdi->blocksize; r1++) {	
		
		eccsize = mcdi->pagesize;
		if (eccsize < 0)
			eccsize += 0x1f;
		eccsize = eccsize >> 5;

		r = McWritePage(port, slot, (value1 * ((mcdi->blocksize << 16) >> 16)) + r1, \
			mcman_pagedata[r1], (void *)(mcman_eccdata + (eccsize * r1))); 	
		
		if (r != sceMcResSucceed)
			return r;
	}

	for (r1 = 0; r1 < mcdi->clusters_per_block; r1++)
		mcman_freecluster(port, slot, (mcdi->backup_block1 * mcdi->clusters_per_block) + r1);
		
check_done:
	// Finally erase backup block2
	return mcman_eraseblock(port, slot, mcdi->backup_block2, NULL, NULL);
}

//-------------------------------------------------------------- 
int McCheckBlock(int port, int slot, int block)
{
	register int r, i, j, page, ecc_count, pageword_cnt, flag, erase_value;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	u8 *p_page, *p_ecc;
	
#ifdef DEBUG
	DPRINTF("mcman: McCheckBlock port%d slot%d block 0x%x\n", port, slot, block);
#endif
	
	// sp50 = block;
	page = block * mcdi->blocksize; //sp18
	pageword_cnt = mcdi->pagesize >> 2; //s6
	
	ecc_count = mcdi->pagesize;	
	if (mcdi->pagesize < 0) 
		ecc_count += 127;
	ecc_count = ecc_count >> 7; // s7
	
	flag = 0; // s4
			
	if (mcdi->cardform > 0) {
		for (i = 0; i < 16; i++) {
			if (mcdi->bad_block_list[i] <= 0)
				break;
			if (mcdi->bad_block_list[i] == block)	
				goto lbl_8764;
		}
	}

	flag = 16; // s4
	
	for (i = 0; i < mcdi->blocksize; i++) {
		r = mcman_readpage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed)
			return -45;
				
		if ((mcdi->cardflags & CF_USE_ECC) != 0) {
			if (mcman_eccdata[mcman_sparesize(port, slot) - 1] != 0xff) {
				p_page = (void *)mcman_pagebuf; //s1
				p_ecc = (void *)mcman_eccdata; //s2
				
				for (j = 0; j < ecc_count; j++) {
					r = mcman_correctdata(p_page, p_ecc);
					if (r != sceMcResSucceed) {
						flag = -1;
						goto lbl_8764;
					}
					p_ecc += 3;
					p_page += 128;
				}
			}
		}
	} 
	
	for (j = 0; j < pageword_cnt; j++)
		*((u32 *)&mcman_pagebuf + j) = 0x5a5aa5a5;
	
	for (j = 0; j < 128; j++)
		*((u32 *)&mcman_eccdata + j) = 0x5a5aa5a5;
		
	r = mcman_eraseblock(port, slot, block, NULL, NULL);
	if (r != sceMcResSucceed) {
		r = mcman_probePS2Card2(port, slot);
		if (r != sceMcResSucceed)
			return -45;
		flag = -1;
		goto lbl_8764;
	}

	for (i = 0; i < mcdi->blocksize; i++) {
		r = McWritePage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed) {
			r = mcman_probePS2Card2(port, slot);
			if (r != sceMcResSucceed)
				return -44;
			flag = -1;
			goto lbl_8764;
		}
	}
	
	for (i = 0; i < mcdi->blocksize; i++) {
		r = mcman_readpage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed)
			return -45;
		
		for (j = 0; j < pageword_cnt; j++) {	
			if (*((u32 *)&mcman_pagebuf + j) != 0x5a5aa5a5) {
				flag = -1;
				goto lbl_8764;
			}
		}
		
		for (j = 0; j < 128; j++) {	
			if (*((u32 *)&mcman_eccdata + j) != 0x5a5aa5a5) {
				flag = -1;
				goto lbl_8764;
			}
		}
	}
	
	for (j = 0; j < pageword_cnt; j++) 
		*((u32 *)&mcman_pagebuf + j) = 0x05a55a5a;
	
	for (j = 0; j < 128; j++)
		*((u32 *)&mcman_eccdata + j) = 0x05a55a5a;
		
	r = mcman_eraseblock(port, slot, block, NULL, NULL);
	if (r != sceMcResSucceed) {
		r = mcman_probePS2Card2(port, slot);
		if (r != sceMcResSucceed)
			return -42;
		flag = -1;
		goto lbl_8764;
	}
	
	for (i = 0; i < mcdi->blocksize; i++) {
		r = McWritePage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed) {
			r = mcman_probePS2Card2(port, slot);
			if (r != sceMcResSucceed)
				return -46;
			flag = -1;
			goto lbl_8764;
		}
	}
	
	for (i = 0; i < mcdi->blocksize; i++) {
		r = mcman_readpage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed)
			return -45;
		
		for (j = 0; j < pageword_cnt; j++) {	
			if (*((u32 *)&mcman_pagebuf + j) != 0x05a55a5a) {
				flag = -1;
				goto lbl_8764;
			}
		}
		
		for (j = 0; j < 128; j++) {	
			if (*((u32 *)&mcman_eccdata + j) != 0x05a55a5a) {
				flag = -1;
				goto lbl_8764;
			}
		}
	}
	
lbl_8764:		
	if (flag == 16) {
		mcman_eraseblock(port, slot, block, NULL, NULL);
		return sceMcResSucceed;
	}
	
	erase_value = 0x00000000;
	if ((mcdi->cardflags & CF_ERASE_ZEROES) != 0)
		erase_value = 0xffffffff;

	for (j = 0; j < pageword_cnt; j++) 
		*((u32 *)&mcman_pagebuf + j) = erase_value;
	
	for (j = 0; j < 128; j++)
		*((u32 *)&mcman_eccdata + j) = erase_value;
		
	for (i = 0; i < mcdi->blocksize; i++) {
		r = McWritePage(port, slot, page + i, mcman_pagebuf, mcman_eccdata);
		if (r != sceMcResSucceed) {
			r = mcman_probePS2Card2(port, slot);
			if (r != sceMcResSucceed)
				return -48;
		}
	}
		
	return 1;
}

//-------------------------------------------------------------- 
int mcman_setPS1devinfos(int port, int slot)
{
	register int r, i;
	MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_setPS1devinfos port%d slot%d\n", port, slot);
#endif	
	
	memset((void *)mcdi, 0, sizeof (MCDevInfo));
	memset((void *)&mcdi->bad_block_list[0], -1, 128);
	
	mcdi->pagesize = 128;
	mcdi->blocksize = 128;
	mcdi->pages_per_cluster = 64;
	mcdi->unused = 0xff00;
	mcdi->cluster_size = 8192;
	mcdi->FATentries_per_cluster = 2048;
	mcdi->clusters_per_card = 16;
	mcdi->clusters_per_block = 0;
	mcdi->cardform = 0;
	mcdi->cardtype = 1;
	
	r = McReadPS1PDACard(port, slot, 0, mcman_PS1PDApagebuf);
	if (r < 0)
		return -14;
	
	if (mcman_sio2outbufs_PS1PDA[1] != 0)
		return -15;
		
	if (mcman_PS1PDApagebuf[0] != 0x4d)
		return sceMcResNoFormat;
	
	if (mcman_PS1PDApagebuf[1] != 0x43)
		return sceMcResNoFormat;
		
	for (i = 0; i < 20; i++) {
		r = McReadPS1PDACard(port, slot, i + 16, mcman_PS1PDApagebuf);
		if (r != sceMcResSucceed)
			return -43;
		
		if (mcman_PS1PDApagebuf[127] == (mcman_calcEDC(mcman_PS1PDApagebuf, 127) & 0xff)) {
			mcdi->bad_block_list[i] = *((u32 *)mcman_PS1PDApagebuf);
		}
	}
		
	r = mcman_cachePS1dirs(port, slot);

	if (r != sceMcResSucceed)
		return r;
		
	mcdi->cardform = 1;
		
	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_getPS1direntry(int port, int slot, char *filename, McFsEntryPS1 **pfse, int flag)
{
	register int r, i;
	char *p = filename;

#ifdef DEBUG		
	DPRINTF("mcman: mcman_getPS1direntry port%d slot%d file %s flag %x\n", port, slot, filename, flag);
#endif	
	
	if (*p == '/')
		p++;
				
	i = 0;	
	do {
		r = mcman_readdirentryPS1(port, slot, i, pfse);
		if (r != sceMcResSucceed)
			return r;
			
		if (flag != 0) {	
			if (pfse[0]->mode != 0x51)
				continue;
		}
		else {
			if (pfse[0]->mode != 0xa1)
				continue;
		}
			
		if (!strcmp(p, pfse[0]->name))
			return i;
				
	} while (++i < 15);
		
	return sceMcResNoEntry;
}

//-------------------------------------------------------------- 
int mcman_clearPS1direntry(int port, int slot, int cluster, int flags)
{
	register int r, i, temp;
	McFsEntryPS1 *fse;
	MC_FHANDLE *fh;
	McCacheEntry *mce;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_clearPS1direntry port%d slot%d cluster %x flags %x\n", port, slot, cluster, flags);
#endif	

	r = mcman_readdirentryPS1(port, slot, cluster, &fse);
	if (r != sceMcResSucceed)
		return r;
			
	fh = (MC_FHANDLE *)&mcman_fdhandles[0];
		
	for (i = 0; i < MAX_FDHANDLES; i++) {
		if ((fh->status != 0) && (fh->port == port) && (fh->slot == slot)) {
			if (fh->freeclink == cluster)
				return sceMcResDeniedPermit;
				
		}
		fh++;		
	}
		
	if (!flags) {
		if (fse->mode != 0x51)
			return sceMcResNoEntry;
	}
	else {
		if (fse->mode != 0xa1)
			return sceMcResNoEntry;
	}
	
	do {
lbl0:		
		mce = mcman_get1stcacheEntp();
	
		if (cluster + 1 < 0)	
			temp = cluster + 8;
		else
			temp = cluster + 1;
		
		mce->wr_flag |= 1 << ((cluster + 1) - ((temp >> 3) << 3));	
	
		if (flags == 0) 
			temp = (fse->mode & 0xf) | 0xa0;
		else 
			temp = (fse->mode & 0xf) | 0x50;	
		
		fse->mode = temp;	
		fse->edc = mcman_calcEDC((void *)fse, 127);
		
		if (fse->linked_block < 0) {
			//cluster = 0;
			goto lbl1;
		}
		
		cluster = fse->linked_block;
		
		r = mcman_readdirentryPS1(port, slot, cluster, &fse);
		if (r != sceMcResSucceed)
			return r;
			
		if (flags == 0) {
			if ((fse->mode & 0xf0) != 0x50)
				break;
			goto lbl0;	
		}
		
	} while ((fse->mode & 0xf0) == 0xa0);	
	
	r = mcman_flushmccache(port, slot);
	if (r != sceMcResSucceed)
		return r;
				
	return sceMcResNoEntry;
	
lbl1:		
	r = mcman_flushmccache(port, slot);
	if (r != sceMcResSucceed)
		return r;
				
	return sceMcResSucceed;	
}

//-------------------------------------------------------------- 
int mcman_findfree1(int port, int slot, int reserve)
{
	register int r, i, free;
	McFsEntryPS1 *fse;
		
#ifdef DEBUG
	DPRINTF("mcman: mcman_findfree1 port%d slot%d reserve %d\n", port, slot, reserve);
#endif	
	
	free = 0; 
	i = 0;	  
	
	do {
		r = mcman_readdirentryPS1(port, slot, i, &fse);
		if (r != sceMcResSucceed)
			return -37;
		
		if ((fse->mode & 0xf0) == 0xa0) {
			if (reserve)
				return i;
			free++;	
		}
	} while (++i < 15);
	
	if (reserve)
		return sceMcResFullDevice;
		
	if (free)
		return free;	
	
	return sceMcResFullDevice;	
}

//-------------------------------------------------------------- 
int mcman_fatRseekPS1(int fd)
{
	register int r, rpos, numclust, clust;
	MC_FHANDLE *fh = (MC_FHANDLE *)&mcman_fdhandles[fd];
	MCDevInfo *mcdi = &mcman_devinfos[fh->port][fh->slot];
	McFsEntryPS1 *fse;
	
	numclust = fh->position / mcdi->cluster_size;	
	numclust--;
	clust = fh->freeclink;
		
	if (numclust > -1) {
		do {
			if (clust < 0) 
				return sceMcResFullDevice;
		
			r = mcman_readdirentryPS1(fh->port, fh->slot, clust, &fse);
			if (r != sceMcResSucceed)
				return r;
			clust = fse->linked_block;
			
		} while (--numclust > -1);
	}
	
	if (clust < 0)
		return sceMcResFullDevice;
			
	rpos = fh->position % mcdi->cluster_size;		
		
	if (rpos < 0)	
		rpos += 1023;	
		
	return ((clust + 1) << 6) + ((rpos >> 10) * (1024 / mcdi->pagesize));
}

//-------------------------------------------------------------- 
int mcman_fatWseekPS1(int fd)
{
	register int r, numclust, clust;
	MC_FHANDLE *fh = (MC_FHANDLE *)&mcman_fdhandles[fd];
	MCDevInfo *mcdi = &mcman_devinfos[fh->port][fh->slot];
	McFsEntryPS1 *fse;
	
	numclust = fh->position / mcdi->cluster_size;
	
	if (numclust > 0) {
		clust = fh->freeclink;
		do {
			r = mcman_readdirentryPS1(fh->port, fh->slot, clust, &fse); 
			if (r != sceMcResSucceed)
				return r;
				
			clust = fse->linked_block;	

			if (clust < 0) {
				r = mcman_FNC8ca4(fh->port, fh->slot, (MC_FHANDLE *)fh);
				if (r < 0) 
					return r;
				clust = r;	
			}
		} while (--numclust);
	}
	r = mcman_flushmccache(fh->port, fh->slot);
		
	return r;
}

//-------------------------------------------------------------- 
int mcman_FNC8ca4(int port, int slot, MC_FHANDLE *fh)
{
	register int r, i, j, mcfree, cluster_size, pages_per_FATclust;
	register int temp;
	MCDevInfo *mcdi = &mcman_devinfos[fh->port][fh->slot];
	McFsEntryPS1 *fse1, *fse2, *fse3;
	McCacheEntry *mce;
	u8 *pfsentry, *pfsee, *pfseend; 

#ifdef DEBUG		
	DPRINTF("mcman: mcman_FNC8ca4 port%d slot%d\n", port, slot); 
#endif	
	
	if (mcdi->cluster_size < 0)
		cluster_size = mcdi->cluster_size + 1023;
	else	
		cluster_size = mcdi->cluster_size;
		
	cluster_size = cluster_size >> 10;
	
	pages_per_FATclust = 1024 / mcdi->pagesize;
	
	r = mcman_findfree1(port, slot, 1); 
	if (r < 0)
		return r;
		
	i = fh->freeclink;		
	mcfree = r;
	j = -1;
	
	if (i >= 0) {
		do {
			if (mcfree < i) {
				if (cluster_size > 0) {
					i = 0;
					
					do {
						mcman_freecluster(port, slot, ((mcfree * cluster_size) + i) * pages_per_FATclust);
					
						r = mcman_readclusterPS1(port, slot, ((i * cluster_size) + i) * pages_per_FATclust, &mce);
						if (r != sceMcResSucceed)
							return r;
						
						mce->wr_flag = -1;
						mce->cluster -= (i - mcfree) * cluster_size;
					
					} while (++i < cluster_size);
				}
			
				r = mcman_readdirentryPS1(port, slot, i, &fse1);
				if (r != sceMcResSucceed)
					return r;
				
				r = mcman_readdirentryPS1(port, slot, mcfree, &fse2);
				if (r != sceMcResSucceed)
					return r;
	
				mce = mcman_get1stcacheEntp();		
			
				if (mcfree + 1 < 0)	
					temp = mcfree + 8;
				else	
					temp = mcfree + 1;
				
				mce->wr_flag |= 1 << ((mcfree + 1) - ((temp >> 3) << 3));	
			
				pfsentry = (u8 *)fse1;	
				pfsee = (u8 *)fse2;	
				pfseend = (u8 *)(pfsentry + sizeof(McFsEntryPS1));
	
				do {
					*((u32 *)pfsee  ) = *((u32 *)pfsentry  );
					*((u32 *)pfsee+1) = *((u32 *)pfsentry+1);
					*((u32 *)pfsee+2) = *((u32 *)pfsentry+2);
					*((u32 *)pfsee+3) = *((u32 *)pfsentry+3);						
					pfsentry += 16;
					pfsee += 16;
				} while (pfsentry < pfseend);
				
				if (j >= 0) {
					r = mcman_readdirentryPS1(port, slot, j, &fse3);
					if (r != sceMcResSucceed)
						return r;
					
					mce = mcman_get1stcacheEntp();
					
					if ((j + 1) < 0)
						temp = j + 8;
					else
						temp = j + 1;
						
					mce->wr_flag |= 1 << ((j + 1) - ((temp >> 3) << 3));
					fse3->linked_block = mcfree;
					fse3->edc = mcman_calcEDC((void *)fse3, 127);
				}
				else {
					fh->freeclink = mcfree;
				}
				j = mcfree;
				mcfree = i;
			
			} 
			else {
				j = i;
				r = mcman_readdirentryPS1(port, slot, j, &fse1);
				if (r != sceMcResSucceed)
					return r;
			}
			i = fse1->linked_block;
		} while (i >= 0);
	}
	
	r = mcman_readdirentryPS1(port, slot, mcfree, &fse2);
	if (r != sceMcResSucceed)
		return r;
		
	mce = mcman_get1stcacheEntp();
	
	if (mcfree + 1 < 0)	
		temp = mcfree + 8;
	else	
		temp = mcfree + 1;
				
	mce->wr_flag |= 1 << ((mcfree + 1) - ((temp >> 3) << 3));	
		
	mcman_wmemset((void *)fse2, sizeof(McFsEntryPS1), 0);	
	
	fse2->mode = 0x53;
	fse2->linked_block = -1;
	fse2->edc = mcman_calcEDC((void *)fse2, 127);
	
	r = mcman_readdirentryPS1(port, slot, j, &fse3);
	if (r != sceMcResSucceed)
		return r;
		
	mce = mcman_get1stcacheEntp();
	
	if ((j + 1) < 0)
		temp = j + 8;
	else
		temp = j + 1;
					
	mce->wr_flag |= 1 << ((j + 1) - ((temp >> 3) << 3));
		
	if (fse3->mode == 0x53)
		fse3->mode = 0x52;
	
	fse3->linked_block =	mcfree;
	fse3->edc = mcman_calcEDC((void *)fse3, 127);		
	
	return mcfree;
}

//-------------------------------------------------------------- 
int mcman_PS1pagetest(int port, int slot, int page)
{
	register int r, i;

	for (i = 0; i < 32; i++)
		*((u32 *)&mcman_PS1PDApagebuf+i) = 0xffffffff;
	
	r = McWritePS1PDACard(port, slot, page, mcman_PS1PDApagebuf);
	if (r != sceMcResSucceed)
		return sceMcResDeniedPermit;
		
	r = McReadPS1PDACard(port, slot, page, mcman_PS1PDApagebuf);
	if (r != sceMcResSucceed)
		return sceMcResNotEmpty;
		 	
	for (i = 0; i < 32; i++) {
		if (*((u32 *)&mcman_PS1PDApagebuf + i) != 0xffffffff) {
			return 0;
		}		
	}	
	
	for (i = 0; i < 32; i++)
		*((u32 *)&mcman_PS1PDApagebuf + i) = 0;
	
	r = McWritePS1PDACard(port, slot, page, mcman_PS1PDApagebuf);
	if (r != sceMcResSucceed)
		return sceMcResDeniedPermit;
		
	r = McReadPS1PDACard(port, slot, page, mcman_PS1PDApagebuf);
	if (r != sceMcResSucceed)
		return sceMcResNotEmpty;

	for (i = 0; i < 32; i++) {
		if (*((u32 *)&mcman_PS1PDApagebuf + i) != 0) {
			return 0;
		}
	}
	
	return 1;
}

//-------------------------------------------------------------- 
int mcman_cachePS1dirs(int port, int slot)
{
	register int r, i, j, temp1, temp2, index, linked_block;
	McFsEntryPS1 *fs_t[15];
	McCacheEntry *mce[2];
	int cluster_t[15];

#ifdef DEBUG
	DPRINTF("mcman: mcman_cachePS1dirs port%d slot%d\n", port, slot);
#endif	
	
	i = 0;
	do {
		r = mcman_readdirentryPS1(port, slot, i, &fs_t[i]);		
		if (r != sceMcResSucceed)
			return r;	
		
		fs_t[i]->field_38 = fs_t[i]->length; /// <-- Special fix for saves from a real PS1		
			
		if (i == 0) {
			mce[0] = mcman_get1stcacheEntp();
		}	
		else {
			if (i == 7)
				mce[1] = mcman_get1stcacheEntp();
		}
	} while (++i < 15);
	
	memset((void *)cluster_t, -1, sizeof(cluster_t));
	
	i = 0;
	do {
		temp1 = fs_t[i]->mode & 0xf0;	
		
		if ((fs_t[i]->mode & 0xf) != 1)
			continue;
				
		cluster_t[i] = i;
		
		linked_block = fs_t[i]->linked_block; 
		if (linked_block >= 0) {
			do {
				if ((fs_t[linked_block]->mode & 0xf0) != temp1)
					temp1 = 0;
					
				if (fs_t[linked_block]->mode == 0xa0)
					break;
					
				if (cluster_t[linked_block] != -1)
					break;
						
				cluster_t[linked_block] = i;
				linked_block = fs_t[linked_block]->linked_block;
				
			} while (linked_block >= 0);
			
			if ((linked_block < 0) && (temp1 != 0)) 
				continue;
		}
		else {
			if (temp1 != 0)
				continue;
		} 
		
		j = 0;
		do {
			if (cluster_t[j] != i)
				continue;
				
			memset((void *)fs_t[j], 0, sizeof (McFsEntryPS1));
			
			fs_t[j]->mode = 0xa0;
			fs_t[j]->linked_block = -1;
			fs_t[j]->edc = mcman_calcEDC((void *)fs_t[j], 127);
			
			temp2 = j + 1;
			
			if (j < 7)
				index = 0;
			else 
				index = 1;
			
			if (temp2 < 0)
			 	temp1 = j + 8;
			else
			 	temp1 = temp2;
			 	
			mce[index]->wr_flag |= (1 << (temp2 - ((temp1 >> 3) << 3)));
				
		} while (++j < 15);
			
	} while (++i < 15);
	
	i = 0;
	do {
		if ((cluster_t[i] != -1) || (fs_t[i]->mode == 0xa0))
			continue;
			
		memset((void *)fs_t[i], 0, sizeof (McFsEntryPS1));
		
		fs_t[i]->mode = 0xa0;
		fs_t[i]->linked_block = cluster_t[i];
		fs_t[i]->edc = mcman_calcEDC((void *)fs_t[i], 127);
		
		temp2 = i + 1;
			
		if (i < 7)
			index = 0;
		else 
			index = 1;
			
		if (temp2 < 0)
		 	temp1 = i + 8;
		else
		 	temp1 = temp2;
		
		mce[index]->wr_flag |= (1 << (temp2 - ((temp1 >> 3) << 3))); 		
		
	} while (++i < 15);
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_fillPS1backuparea(int port, int slot, int block)
{
	register int r, i, curpage;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	
	memset(mcman_PS1PDApagebuf, 0, 128);

	curpage = 16;
	i = 0;
	do {
		if (mcdi->bad_block_list[i] == block) {
			mcdi->bad_block_list[i] = block | 0x01000000;
			*((u32 *)&mcman_PS1PDApagebuf) = block | 0x01000000;
			mcman_PS1PDApagebuf[127] = mcman_calcEDC(mcman_PS1PDApagebuf, 127);
			r = McWritePS1PDACard(port, slot, curpage, mcman_PS1PDApagebuf);
			if (r != sceMcResSucceed)
				return -43;
		}
		else {
			if (mcdi->bad_block_list[i] < 0) {
				*((u32 *)&mcman_PS1PDApagebuf) = block;
				mcman_PS1PDApagebuf[127] = mcman_calcEDC(mcman_PS1PDApagebuf, 127);
				r = McWritePS1PDACard(port, slot, curpage, mcman_PS1PDApagebuf);
				if (r != sceMcResSucceed)
					return -43;
				mcdi->bad_block_list[i] = block;
				break;
			}
		}
		curpage++;
	} while (++i < 20);
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
void mcman_initcache(void)
{
	register int i, j;
	u8 *p;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_initcache\n");
#endif	
	
	j = MAX_CACHEENTRY - 1;
	p = (u8 *)mcman_cachebuf; 
	
	for (i = 0; i < MAX_CACHEENTRY; i++) {	
		mcman_entrycache[i].cl_data = (u8 *)p;	
		mcman_mccache[i] = (McCacheEntry *)&mcman_entrycache[j - i];
		mcman_entrycache[i].cluster = -1;
		p += MCMAN_CLUSTERSIZE;
	}

	pmcman_entrycache = (McCacheEntry *)mcman_entrycache;
	pmcman_mccache = (McCacheEntry **)mcman_mccache;
	
	for (i = 0; i < MCMAN_MAXSLOT; i++) {
		mcman_devinfos[0][i].unknown3 = -1;
		mcman_devinfos[0][i].unknown4 = -1;
		mcman_devinfos[0][i].unknown5 = -1;
		mcman_devinfos[1][i].unknown3 = -1;
		mcman_devinfos[1][i].unknown4 = -1;
		mcman_devinfos[1][i].unknown5 = -1;
	}
		
	memset((void *)mcman_fatcache, -1, sizeof (mcman_fatcache));

	for (i = 0; i < MCMAN_MAXSLOT; i++) {
		mcman_fatcache[0][i].entry[0] = 0;
		mcman_fatcache[1][i].entry[0] = 0;
	}
}

//-------------------------------------------------------------- 
int McRetOnly(int fd) // Export #37
{
#ifdef DEBUG	
	DPRINTF("mcman: McRetOnly param %x\n", fd);
#endif	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_clearcache(int port, int slot)
{
	register int i, j;
	McCacheEntry **pmce = (McCacheEntry **)pmcman_mccache;
	McCacheEntry *mce, *mce_save;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_clearcache port%d, slot%d\n", port, slot);	
#endif	

	for (i = MAX_CACHEENTRY - 1; i >= 0; i--) { 
		mce = (McCacheEntry *)pmce[i];
		if ((mce->mc_port == port) && (mce->mc_slot == slot) && (mce->cluster >= 0)) {
			mce->mc_port = -1;
			mce->mc_slot = -1;
			mce->wr_flag = 0;
			mce->cluster = -1;
		}
	}
	
	for (i = 0; i < (MAX_CACHEENTRY - 1); i++) { 
		mce = (McCacheEntry *)pmce[i];	
		mce_save = (McCacheEntry *)pmce[i];
		if (mce->cluster < 0) {
			for (j = i+1; j < MAX_CACHEENTRY; j++) {  
				mce = (McCacheEntry *)pmce[j];
				if (mce->cluster >= 0)
					break; 
			}
			if (j == MAX_CACHEENTRY)
				break;
			
			pmce[i] = (McCacheEntry *)pmce[j];
			pmce[j] = (McCacheEntry *)mce_save;
		}
	}	
	
	memset((void *)&mcman_fatcache[port][slot], -1, sizeof (McFatCache));
	
	mcman_fatcache[port][slot].entry[0] = 0;
		
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
McCacheEntry *mcman_getcacheentry(int port, int slot, int cluster)
{
	register int i;
	McCacheEntry *mce = (McCacheEntry *)pmcman_entrycache;
	
	//DPRINTF("mcman: mcman_getcacheentry port%d slot%d cluster %x\n", port, slot, cluster);		
	
	for (i = 0; i < MAX_CACHEENTRY; i++) { 		
		if ((mce->mc_port == port) && (mce->mc_slot == slot) && (mce->cluster == cluster))
			return mce;
		mce++;
	}
	
	return NULL;
}

//-------------------------------------------------------------- 
void mcman_freecluster(int port, int slot, int cluster) // release cluster from entrycache
{
	register int i;
	McCacheEntry *mce = (McCacheEntry *)pmcman_entrycache;
	
	for (i = 0; i < MAX_CACHEENTRY; i++) { 		
		if ((mce->mc_port == port) && (mce->mc_slot == slot) && (mce->cluster == cluster)) {
			mce->cluster = -1;
			mce->wr_flag = 0;
		}
		mce++;
	}
}

//-------------------------------------------------------------- 
int mcman_getFATindex(int port, int slot, int num)
{
	return mcman_fatcache[port][slot].entry[num];
}

//-------------------------------------------------------------- 
void mcman_1stcacheEntsetwrflagoff(void)
{
	McCacheEntry *mce = (McCacheEntry *)*pmcman_mccache;
	
	mce->wr_flag = -1;
}

//-------------------------------------------------------------- 
McCacheEntry *mcman_get1stcacheEntp(void)
{
	return *pmcman_mccache;
}

//-------------------------------------------------------------- 
void mcman_addcacheentry(McCacheEntry *mce) 
{
	register int i;
	McCacheEntry **pmce = (McCacheEntry **)pmcman_mccache;
		
	i = MAX_CACHEENTRY - 1;
	
	if (i < 0)
		goto lbl1;
		
	do {
		if (pmce[i] == mce)
			break;
	} while (--i >= 0);

	if (i != 0) {
lbl1:		
		do {
			pmce[i] = pmce[i-1];
		} while (--i != 0);	
	}
			
	pmce[0] = (McCacheEntry *)mce;
}

//-------------------------------------------------------------- 
int mcman_flushmccache(int port, int slot)
{
	register int i, r;
	McCacheEntry **pmce = (McCacheEntry **)pmcman_mccache;
	McCacheEntry *mce;

#ifdef DEBUG		
	DPRINTF("mcman: mcman_flushmccache port%d slot%d\n", port, slot);
#endif	
	
	i = MAX_CACHEENTRY - 1;

	if (i >= 0) {
		do {
			mce = (McCacheEntry *)pmce[i];
			if ((mce->mc_port == port) && (mce->mc_slot == slot) && (mce->wr_flag != 0)) {
				r = mcman_flushcacheentry((McCacheEntry *)mce);
				if (r != sceMcResSucceed)
					return r;
			}
			i--;
		} while (i >= 0);
	}
		
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_flushcacheentry(McCacheEntry *mce)
{
	register int r, i, j, ecc_count;
	register int temp1, temp2, offset, pageindex; 
	static int clusters_per_block, blocksize, cardtype, pagesize, sparesize, flag, cluster, block, pages_per_fatclust;
	McCacheEntry *pmce[16]; // sp18	
	register MCDevInfo *mcdi;	
	McCacheEntry *mcee;
	static u8 eccbuf[32];
	void *p_page, *p_ecc;
		
#ifdef DEBUG	
	DPRINTF("mcman: mcman_flushcacheentry mce %x cluster %x\n", (int)mce, (int)mce->cluster);
#endif	
	
	if (mce->wr_flag == 0)
		return sceMcResSucceed; 
		
	mcdi = (MCDevInfo *)&mcman_devinfos[mce->mc_port][mce->mc_slot];	

	//mcdi->pagesize = sp84
	pagesize = mcdi->pagesize; //sp84
	cardtype = mcdi->cardtype;
	
	if (cardtype == 0) {
		mce->wr_flag = 0;
		return sceMcResSucceed; 
	}
		
	if ((cardtype == sceMcTypePS1) || (cardtype == sceMcTypePDA)) {
		pages_per_fatclust = MCMAN_CLUSTERSIZE / pagesize;
		i = mce->cluster; //s0
		j = 0; //s1
		r = 0;
				
		if (pages_per_fatclust > 0) {
lbl0:

			do {
				if (((mce->wr_flag >> j) & 1) != 0) {
					//DPRINTF("flushing ent %x cl_data %x to page %d offset %d\n", mce, mce->cl_data, i, (pagesize * j));
					r = McWritePS1PDACard(mce->mc_port, mce->mc_slot, i, (void *)(mce->cl_data + (pagesize * j)));
					if (r == sceMcResFailReplace)
						break;
					if (r != sceMcResSucceed)
						return -50;
					
				}
				i++;
			} while (++j < pages_per_fatclust);
		}
		
		if (j == pages_per_fatclust) {
			r = mcman_probePS1Card2(mce->mc_port, mce->mc_slot);
			if (r == -14)
				r = sceMcResFailReplace;
		}
		if (cardtype != sceMcTypePS1) {
			if (r == sceMcResFailReplace)
				return r;
				
			mce->wr_flag = 0;
			return sceMcResSucceed; 
		}
		if (r != 8) {
			mce->wr_flag = 0;
			return sceMcResSucceed;
		}
		
		if ((mce->wr_flag & ~(-1 << j)) == 0) {
			mce->wr_flag = 0;
			return sceMcResSucceed;
		}
		
		if (j-- == 0) {
			mce->wr_flag = 0;
			return sceMcResSucceed;
		} 
		
		i--;
		
		do {
			if (((mce->wr_flag >> j) & 1) != 0) {
				r = mcman_fillPS1backuparea(mce->mc_port, mce->mc_slot, i);
				if (r != sceMcResSucceed)
					return sceMcResFailReplace;
				
				goto lbl0;
			}
		} while (j++);
		
		mce->wr_flag = 0;
		return sceMcResSucceed;
	}
			
	clusters_per_block = mcdi->clusters_per_block; //sp7c
	block = mce->cluster / mcdi->clusters_per_block; //sp78
	blocksize = mcdi->blocksize;  //sp80
	sparesize = mcman_sparesize(mce->mc_port, mce->mc_slot); //sp84
	flag = 0; //sp88
		
	memset((void *)pmce, 0, 64);
	
	i = 0; //s1		
	if (MAX_CACHEENTRY > 0) {
		mcee = (McCacheEntry *)pmcman_entrycache;
		do {
			if ((*((u32 *)&mcee->mc_slot) & 0xff00ffff) == (*((u32 *)&mce->mc_slot) & 0xff00ffff)) {
				temp1 = mcee->cluster / clusters_per_block;
				temp2 = mcee->cluster % clusters_per_block; 
					
				if (temp1 == block) {
					pmce[temp2] = (McCacheEntry *)mcee;
					if (mcee->rd_flag == 0)
						flag = 1;
				}
			}
			mcee++;
		} while (++i < MAX_CACHEENTRY);
	}
		
	if (clusters_per_block > 0) {
		i = 0; //s1
		pageindex = 0; //s5
		cluster = block * clusters_per_block; // sp8c
			
		do {
			if (pmce[i] != 0) {
				j = 0; // s0
				offset = 0; //a0
				for (j = 0; j < mcdi->pages_per_cluster; j++) {
					mcman_pagedata[pageindex + j] = (void *)(pmce[i]->cl_data + offset);
					offset += pagesize;
				}
			}
			else {
				//s3 = s5
				// s2 = (cluster + i) * mcdi->pages_per_cluster
				j = 0; //s0
				do {	
					offset = (pageindex + j) * pagesize; // t0
					mcman_pagedata[pageindex + j] = (void *)(mcman_backupbuf + offset);
					
					r = McReadPage(mce->mc_port, mce->mc_slot, \
						((cluster + i) * mcdi->pages_per_cluster) + j, \
							mcman_backupbuf + offset);
					if (r != sceMcResSucceed)
						return -51;
						
				} while (++j < mcdi->pages_per_cluster);	
			}
				
			pageindex += mcdi->pages_per_cluster;
		} while (++i < clusters_per_block);
	}
		
lbl1:	
	if ((flag != 0) && (mcman_badblock <= 0)) {
		r = mcman_eraseblock(mce->mc_port, mce->mc_slot, mcdi->backup_block1, (void**)mcman_pagedata, mcman_eccdata);
		if (r == sceMcResFailReplace) {
lbl2:			
			r = mcman_replaceBackupBlock(mce->mc_port, mce->mc_slot, mcdi->backup_block1);
			mcdi->backup_block1 = r;
			goto lbl1;
		} 
		if (r != sceMcResSucceed)
			return -52;
							
		*((u32 *)&mcman_pagebuf) = block | 0x80000000;
		p_page = (void *)mcman_pagebuf; //s0
		p_ecc = (void *)eccbuf; //s2 = sp58

		i = 0;	//s1					
		do {
			if (pagesize < 0)
				ecc_count = (pagesize + 0x7f) >> 7;
			else
				ecc_count = pagesize >> 7;	
				
			if (i >= ecc_count)
				break;
				
			McDataChecksum(p_page, p_ecc);
			
			p_ecc += 3;
			p_page += 128;
			i++;
		} while (1); 
		
		
		r = McWritePage(mce->mc_port, mce->mc_slot, mcdi->backup_block2 * blocksize, mcman_pagebuf, eccbuf);
		if (r == sceMcResFailReplace)
			goto lbl3;
		if (r != sceMcResSucceed)
			return -53;
			
		if (r < mcdi->blocksize) {
			i = 0; //s0
			p_ecc = (void *)mcman_eccdata; 
								
			do {
				r = McWritePage(mce->mc_port, mce->mc_slot, (mcdi->backup_block1 * blocksize) + i, mcman_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace)
					goto lbl2;
				if (r != sceMcResSucceed)
					return -54;
				p_ecc += sparesize;					
			} while (++i < mcdi->blocksize);
		}
		
		r = McWritePage(mce->mc_port, mce->mc_slot, (mcdi->backup_block2 * blocksize) + 1, mcman_pagebuf, eccbuf);
		if (r == sceMcResFailReplace)
			goto lbl3;
		if (r != sceMcResSucceed)
			return -55;
	}

	r = mcman_eraseblock(mce->mc_port, mce->mc_slot, block, (void**)mcman_pagedata, mcman_eccdata);
	//if (block == 1) /////
	//	r = sceMcResFailReplace; /////
	if (r == sceMcResFailReplace) {
		r = mcman_fillbackupblock1(mce->mc_port, mce->mc_slot, block, (void**)mcman_pagedata, mcman_eccdata);
		for (i = 0; i < clusters_per_block; i++) {
			if (pmce[i] != 0)
				pmce[i]->wr_flag = 0;
		}
		if (r == sceMcResFailReplace)
			return r;
		return -58;	
	}
	if (r != sceMcResSucceed)
		return -57;

	if (mcdi->blocksize > 0) {
		i = 0; //s0
		p_ecc = (void *)mcman_eccdata; 		
		
		do {
			if (pmce[i / mcdi->pages_per_cluster] == 0) {
				r = McWritePage(mce->mc_port, mce->mc_slot, (block * blocksize) + i, mcman_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace) {
					r = mcman_fillbackupblock1(mce->mc_port, mce->mc_slot, block, (void**)mcman_pagedata, mcman_eccdata);
					for (i = 0; i < clusters_per_block; i++) {
						if (pmce[i] != 0)
							pmce[i]->wr_flag = 0;
					}
					if (r == sceMcResFailReplace)
						return r;
					return -58;	
				}
				if (r != sceMcResSucceed)
					return -57;
			}
			p_ecc += sparesize;					
		} while (++i < mcdi->blocksize);
	}
	
	if (mcdi->blocksize > 0) {
		i = 0; //s0
		p_ecc = (void *)mcman_eccdata; 		
		
		do {
			if (pmce[i / mcdi->pages_per_cluster] != 0) {
				r = McWritePage(mce->mc_port, mce->mc_slot, (block * blocksize) + i, mcman_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace) {
					r = mcman_fillbackupblock1(mce->mc_port, mce->mc_slot, block, (void**)mcman_pagedata, mcman_eccdata);
					for (i = 0; i < clusters_per_block; i++) {
						if (pmce[i] != 0)
							pmce[i]->wr_flag = 0;
					}
					if (r == sceMcResFailReplace)
						return r;
					return -58;	
				}
				if (r != sceMcResSucceed)
					return -57;
			}
			p_ecc += sparesize;					
		} while (++i < mcdi->blocksize);
	}
	
	if (clusters_per_block > 0) {
		i = 0;
		do {
			if (pmce[i] != 0)
				pmce[i]->wr_flag = 0;
		} while (++i < clusters_per_block);
	}
	
	if ((flag != 0) && (mcman_badblock <= 0)) {
		r = mcman_eraseblock(mce->mc_port, mce->mc_slot, mcdi->backup_block2, NULL, NULL);
		if (r == sceMcResFailReplace) {
			goto lbl3;
		} 
		if (r != sceMcResSucceed)
			return -58;
	}
	goto lbl_exit;
						
lbl3:
	r = mcman_replaceBackupBlock(mce->mc_port, mce->mc_slot, mcdi->backup_block2);
	mcdi->backup_block2 = r;
	goto lbl1;
	
lbl_exit:
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_readcluster(int port, int slot, int cluster, McCacheEntry **pmce)
{
	register int r, i, block, block_offset;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McCacheEntry *mce;
	 
	if (mcman_badblock > 0) {
		block = cluster / mcdi->clusters_per_block;
		block_offset = cluster % mcdi->clusters_per_block;
		
		if ((block == mcman_badblock) && (mcman_badblock_port == port) && (mcman_badblock_slot == slot)) {
			cluster = (mcdi->backup_block1 * mcdi->clusters_per_block) + block_offset;
		}
		else {
			if (mcman_badblock > 0) {
				for (i = 0; i < mcdi->clusters_per_block; i++) {
					if ((mcman_replacementcluster[i] != 0) && (mcman_replacementcluster[i] == cluster)) {
						block_offset = i % mcdi->clusters_per_block;
						cluster = (mcdi->backup_block1 * mcdi->clusters_per_block) + block_offset;
					}
				}
			}
		}
	}
		
	mce = mcman_getcacheentry(port, slot, cluster);
	if (mce == NULL) {
		mce = pmcman_mccache[MAX_CACHEENTRY - 1];
		
		if (mce->wr_flag != 0) {
			r = mcman_flushcacheentry((McCacheEntry *)mce);
			if (r != sceMcResSucceed)
				return r;
		}
		
		mce->mc_port = port;
		mce->mc_slot = slot;
		mce->cluster = cluster;
		mce->rd_flag = 0;
		//s3 = (cluster * mcdi->pages_per_cluster);
		
		for (i = 0; i < mcdi->pages_per_cluster; i++) {
			r = McReadPage(port, slot, (cluster * mcdi->pages_per_cluster) + i, \
					(void *)(mce->cl_data + (i * mcdi->pagesize)));
			if (r != sceMcResSucceed)
				return -21;
				
		}
	}
	mcman_addcacheentry(mce);
	*pmce = (McCacheEntry *)mce;
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_readdirentry(int port, int slot, int cluster, int fsindex, McFsEntry **pfse) // Export #47 XMCMAN only 
{
	register int r, i;
	static int maxent, index, clust;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	register McFatCache *fci = &mcman_fatcache[port][slot];
	McCacheEntry *mce;
	
#ifdef DEBUG	
	DPRINTF("mcman: mcman_readdirentry port%d slot%d cluster %d fsindex %d\n", port, slot, cluster, fsindex);
#endif	
	
	maxent = 0x402 / (mcdi->cluster_size >> 9); //a1
	index = fsindex / (mcdi->cluster_size >> 9);//s2
		
	clust = cluster;
	i = 0; // s0
	if ((cluster == 0) && (index != 0)) {
		if (index < maxent) {
			i = index;
			if ((fci->entry[index]) >= 0 )
				clust = fci->entry[index];
		}
		i = 0;
		if (index > 0) {
			do {
				if (i >= maxent)
					break; 
				if (fci->entry[i] < 0)
					break;
				clust = fci->entry[i];
			} while (++i < index);
		}
		i--;
	}
	
	if (i < index) {
		do {
			r = mcman_getFATentry(port, slot, clust, &clust);
			if (r != sceMcResSucceed)
				return -70;
						
			if (clust == 0xffffffff)
				return sceMcResNoEntry;
			clust &= 0x7fffffff; 		
			
			i++;
			if (cluster == 0) {
				if (i < maxent)
					fci->entry[i] = clust;
			}
		} while (i < index);
	}
		
	r = mcman_readcluster(port ,slot, mcdi->alloc_offset + clust, &mce);
	if (r != sceMcResSucceed)
		return -71;
	
	*pfse = (McFsEntry *)(mce->cl_data + ((fsindex % (mcdi->cluster_size >> 9)) << 9));
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_readdirentryPS1(int port, int slot, int cluster, McFsEntryPS1 **pfse)
{
	register int r, offset, index, pages_per_fatclust;
	McCacheEntry *mce;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
		
	if (cluster >= 15)
		return -73;
	
	pages_per_fatclust = MCMAN_CLUSTERSIZE / mcdi->pagesize;
		
	cluster++;
	index = cluster / pages_per_fatclust;
	offset = cluster % pages_per_fatclust;
	
	r = mcman_readclusterPS1(port, slot, index * pages_per_fatclust, &mce);
	if (r != sceMcResSucceed)
		return -74;	
		
	*pfse = (void *)&mce->cl_data[offset << 7];	
	
	if (sio2man_type == XSIO2MAN) {
		McFsEntryPS1 *fse = (McFsEntryPS1 *)*pfse; // <--- XMCMAN seems to work with this
		fse->field_7d = 0;						   // 
	}
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_setFATentry(int port, int slot, int fat_index, int fat_entry) // Export #46 XMCMAN only
{
	register int r, ifc_index, indirect_index, indirect_offset, fat_offset;
	McCacheEntry *mce;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	
	//DPRINTF("mcman: mcman_setFATentry port%d slot%d fat_index %x fat_entry %x\n", port, slot, fat_index, fat_entry);	
	
	indirect_index = fat_index / mcdi->FATentries_per_cluster;
	fat_offset = fat_index % mcdi->FATentries_per_cluster;
	
	ifc_index = indirect_index / mcdi->FATentries_per_cluster;
	indirect_offset = indirect_index % mcdi->FATentries_per_cluster;
	
	r = mcman_readcluster(port, slot, mcdi->ifc_list[ifc_index], &mce);
	if (r != sceMcResSucceed)
		return -75;

	McFatCluster *fc = (McFatCluster *)mce->cl_data;
		
	r = mcman_readcluster(port, slot, fc->entry[indirect_offset], &mce);
	if (r != sceMcResSucceed)
		return -76;
 		
	fc = (McFatCluster *)mce->cl_data;
		
 	fc->entry[fat_offset] = fat_entry;
 	mce->wr_flag = 1;

	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_getFATentry(int port, int slot, int fat_index, int *fat_entry) // Export #44 XMCMAN only
{
	register int r, ifc_index, indirect_index, indirect_offset, fat_offset;
	McCacheEntry *mce;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	
	indirect_index = fat_index / mcdi->FATentries_per_cluster;
	fat_offset = fat_index % mcdi->FATentries_per_cluster;
	
	ifc_index = indirect_index / mcdi->FATentries_per_cluster;
	indirect_offset = indirect_index % mcdi->FATentries_per_cluster;
	
	r = mcman_readcluster(port, slot, mcdi->ifc_list[ifc_index], &mce);
	if (r != sceMcResSucceed)
		return -78;
	
	McFatCluster *fc = (McFatCluster *)mce->cl_data;
	
	r = mcman_readcluster(port, slot, fc->entry[indirect_offset], &mce);
	if (r != sceMcResSucceed)
		return -79;

	fc = (McFatCluster *)mce->cl_data;
		
	*fat_entry = fc->entry[fat_offset];
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_readclusterPS1(int port, int slot, int cluster, McCacheEntry **pmce)
{
	register int r, i, pages_per_fatclust;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McCacheEntry *mce;
		
	mce = mcman_getcacheentry(port, slot, cluster);
	if (mce == NULL) {
		mce = pmcman_mccache[MAX_CACHEENTRY - 1];
		
		if (mce->wr_flag != 0) {
			r = mcman_flushcacheentry((McCacheEntry *)mce);
			if (r != sceMcResSucceed)
				return r;
		}
		
		mce->mc_port = port;
		mce->mc_slot = slot;
		mce->cluster = cluster;

		pages_per_fatclust = MCMAN_CLUSTERSIZE / mcdi->pagesize;
		
		for (i = 0; i < pages_per_fatclust; i++) {
			r = McReadPS1PDACard(port, slot, cluster + i, (void *)(mce->cl_data + (i * mcdi->pagesize)));
			if (r != sceMcResSucceed)
				return -21;
		}
	}
	
	mcman_addcacheentry(mce);
	*pmce = (McCacheEntry *)mce;
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_replaceBackupBlock(int port, int slot, int block)
{
	register int i;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	
	if (mcman_badblock > 0)
		return sceMcResFailReplace;
		
	for (i = 0; i < 16; i++) {
		if (mcdi->bad_block_list[i] == -1)
			break;
	}
	
	if (i < 16) {
		if ((mcdi->alloc_end - mcdi->max_allocatable_clusters) < 8)
			return sceMcResFullDevice;
			
		mcdi->alloc_end -= 8;
		mcdi->bad_block_list[i] = block;	
		mcman_badblock_port = port;
		mcman_badblock_slot = slot;
		mcman_badblock = -1;
		
		return (mcdi->alloc_offset + mcdi->alloc_end) / mcdi->clusters_per_block;
	}
	
	return sceMcResFullDevice;
}

//-------------------------------------------------------------- 
int mcman_fillbackupblock1(int port, int slot, int block, void **pagedata, void *eccdata)
{
	register int r, i, sparesize, page_offset;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	register u8 *p_ecc;
	
#ifdef DEBUG	
	DPRINTF("mcman: mcman_fillbackupblock1 port%d slot%d block %x mcman_badblock %x\n", port, slot, block, mcman_badblock);
#endif	
	
	sparesize = mcman_sparesize(port, slot);
	
	if (mcman_badblock != 0) {
		if ((mcman_badblock != block) || (mcman_badblock_port != port) || (mcman_badblock_slot != slot))
			return sceMcResFailReplace;
	}		
	
	if ((mcdi->alloc_offset / mcdi->clusters_per_block) == block) // Appparently this refuse to take care of a bad rootdir cluster
		return sceMcResFailReplace;
			
	r = mcman_eraseblock(port, slot, mcdi->backup_block2, NULL, NULL);	
	if (r != sceMcResSucceed) 
		return r;

	r = mcman_eraseblock(port, slot, mcdi->backup_block1, NULL, NULL);	
	if (r != sceMcResSucceed) 
		return r;
						
	for (i = 0; i < 16; i++) {
		if (mcdi->bad_block_list[i] == -1)
			break;
	}
	
	if (i >= 16)
		return sceMcResFailReplace;
	
	page_offset = mcdi->backup_block1 * mcdi->blocksize;	
	p_ecc = (u8 *)eccdata;
	
	for (i = 0; i < mcdi->blocksize; i++) {
		r = McWritePage(port, slot, page_offset + i, pagedata[i], p_ecc);
		if (r != sceMcResSucceed) 
			return r;
		p_ecc += sparesize;
	}

	mcman_badblock_port = port;
	mcman_badblock_slot = slot;
	mcman_badblock = block;
	
	i = 15;
	do {
		mcman_replacementcluster[i] = 0;
	} while (--i >= 0); 
				
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int mcman_replacebadblock(void)
{
	register int r, i, curentry, clust, index, offset, numifc, fat_length, temp, length;
	register int value, value2, cluster, index2, offset2, s3;
	register MCDevInfo *mcdi = &mcman_devinfos[mcman_badblock_port][mcman_badblock_slot];	
	int fat_entry[16];
	int fat_entry2;
	McCacheEntry *mce;
	McFsEntry *fse;

#ifdef DEBUG	
	DPRINTF("mcman: mcman_replacebadblock mcman_badblock_port%d mcman_badblock_slot%d mcman_badblock %x\n", mcman_badblock_port, mcman_badblock_slot, mcman_badblock);
#endif	
	
	if (mcman_badblock == 0)
		return sceMcResSucceed;
		
	if (mcman_badblock >= 0) {
		mcman_flushmccache(mcman_badblock_port, mcman_badblock_slot);
		mcman_clearcache(mcman_badblock_port, mcman_badblock_slot);
		
		for (i = 0; i <16; i++) {
			if (mcdi->bad_block_list[i] == -1)
				break;
		}
		if (i >= 16)
			goto lbl_e168;
			
		if (mcdi->alloc_end >= (mcdi->max_allocatable_clusters + 8)) {
			mcdi->max_allocatable_clusters += 8;
			mcdi->bad_block_list[i] = mcman_badblock;
		}	
		
		fat_length = (((mcdi->clusters_per_card << 2) - 1) / mcdi->cluster_size) + 1;
		numifc = (((fat_length << 2) - 1) / mcdi->cluster_size) + 1;
		
		if (numifc > 32) {
			numifc = 32;
			fat_length = mcdi->FATentries_per_cluster << 5;
		}
		
		i = 0; //sp5c
		value = ~(-1 << mcdi->clusters_per_block);
		do {
			clust = (mcman_badblock * mcdi->clusters_per_block) + i;
			if (clust < mcdi->alloc_offset) {
				fat_entry[i] = 0;
			}
			else {
				r = mcman_getFATentry(mcman_badblock_port, mcman_badblock_slot, clust - mcdi->alloc_offset, &fat_entry[i]);
				if (r != sceMcResSucceed)
					goto lbl_e168;
					
				if (((fat_entry[i] & 0x80000000) == 0) || ((fat_entry[i] < -1) && (fat_entry[i] >= -9)))
					value &= ~(1 << i);
			}
		} while (++i < mcdi->clusters_per_block);
		
		if (mcdi->clusters_per_block > 0) {
			i = 0;
			do {
				if ((value & (1 << i)) != 0) {
					r = mcman_findfree2(mcman_badblock_port, mcman_badblock_slot, 1);
					if (r < 0) {
						mcman_replacementcluster[i] = r;
						goto lbl_e168;
					}
					r += mcdi->alloc_offset;
					mcman_replacementcluster[i] = r;	
				}
			} while (++i < mcdi->clusters_per_block);
		}

		if (mcdi->clusters_per_block > 0) {
			i = 0;
			do {
				if ((value & (1 << i)) != 0) {
					if (fat_entry[i] != 0) {
						index = ((fat_entry[i] & 0x7fffffff) + mcdi->alloc_offset) / mcdi->clusters_per_block;
						offset = ((fat_entry[i] & 0x7fffffff) + mcdi->alloc_offset) % mcdi->clusters_per_block;
						if (index == mcman_badblock) {
							fat_entry[i] = (mcman_replacementcluster[offset] - mcdi->alloc_offset) | 0x80000000;
						}
					}
				}
			} while (++i < mcdi->clusters_per_block);			
		}
		
		if (mcdi->clusters_per_block > 0) {
			i = 0;
			do {
				if ((mcman_replacementcluster[i] != 0) && (fat_entry[i] != 0)) {
					r = mcman_setFATentry(mcman_badblock_port, mcman_badblock_slot, mcman_replacementcluster[i] + mcdi->alloc_offset, fat_entry[i]);
					if (r != sceMcResSucceed)
						goto lbl_e168;
				}
			} while (++i < mcdi->clusters_per_block);			
		}
		
		for (i = 0; i < numifc; i++) {
			index = mcdi->ifc_list[i] / mcdi->clusters_per_block;
			offset = mcdi->ifc_list[i] % mcdi->clusters_per_block;
			
			if (index == mcman_badblock) {
				value &= ~(1 << offset);
				mcdi->ifc_list[i] = mcman_replacementcluster[i];
			}
		}
		
		if (value == 0)
			goto lbl_e030;
			 
		for (i = 0; i < fat_length; i++) {
			index = i / mcdi->FATentries_per_cluster;
			offset = i % mcdi->FATentries_per_cluster;
			
			if (offset == 0) {
				r = mcman_readcluster(mcman_badblock_port, mcman_badblock_slot, mcdi->ifc_list[index], &mce);
				if (r != sceMcResSucceed)
					goto lbl_e168;
			}
			offset = i % mcdi->FATentries_per_cluster;
			index2 = *((u32 *)&mce->cl_data[offset]) / mcdi->clusters_per_block;
			offset2 = *((u32 *)&mce->cl_data[offset]) % mcdi->clusters_per_block;
				
			if (index2 == mcman_badblock) {
				value &= ~(1 << offset2);
				*((u32 *)&mce->cl_data[offset]) = mcman_replacementcluster[offset2];
				mce->wr_flag = 1;
			}
		}
			
		mcman_flushmccache(mcman_badblock_port, mcman_badblock_slot);
		mcman_clearcache(mcman_badblock_port, mcman_badblock_slot);

		if (value == 0)
			goto lbl_e030;
								
		value2 = value;	
			
		for (i = 0; i < mcdi->alloc_end; i++) {
			r = mcman_getFATentry(mcman_badblock_port, mcman_badblock_slot, i, &fat_entry2);
			if (r != sceMcResSucceed)
				goto lbl_e168;
		
			index = (u32)(((fat_entry2 & 0x7fffffff) + mcdi->alloc_offset) / mcdi->clusters_per_block);	
			offset = (u32)(((fat_entry2 & 0x7fffffff) + mcdi->alloc_offset) % mcdi->clusters_per_block);	
			
			if (index == mcman_badblock) {
				value &= ~(1 << offset);
				r = mcman_setFATentry(mcman_badblock_port, mcman_badblock_slot, i, (mcman_replacementcluster[offset] - mcdi->alloc_offset) | 0x80000000);
				if (r != sceMcResSucceed)
					goto lbl_e168;
			}
		}	
		
		if (value2 != value)
			mcman_flushmccache(mcman_badblock_port, mcman_badblock_slot);
		
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, 0, 0, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
		
		curentry = 2;
		s3 = 0;
		length = fse->length;
		
lbl_dd8c:			
		if (curentry >= length)
			goto lbl_ded0;
					 
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, s3, curentry, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
				
		cluster = fse->cluster;	
		index = (fse->cluster + mcdi->alloc_offset) / mcdi->clusters_per_block;	
		offset = (fse->cluster + mcdi->alloc_offset) % mcdi->clusters_per_block;	
			
		temp = fse->length;
			
		if (index == mcman_badblock) {
			fse->cluster = mcman_replacementcluster[offset] - mcdi->alloc_offset;
			mcman_1stcacheEntsetwrflagoff();
		}
			
		if ((fse->mode & sceMcFileAttrSubdir) == 0) {
			curentry++;
			goto lbl_dd8c;
		}
				
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, cluster, 0, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
			
		if ((fse->mode & sceMcFileAttrSubdir) == 0) {
			curentry++;
			goto lbl_dd8c;
		}
			
		if (fse->cluster != 0) {
			curentry++;
			goto lbl_dd8c;
		}
			
		if (fse->dir_entry != curentry) {
			curentry++;
			goto lbl_dd8c;
		}
								
		curentry++;
			
		if (strcmp(fse->name, "."))
			goto lbl_dd8c;
		
		s3 = cluster;
		curentry = 2;
		length = temp;
		goto lbl_dd8c;
	
lbl_ded0:
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, s3, 1, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
		
		index = (fse->cluster + mcdi->alloc_offset) / mcdi->clusters_per_block;		
		offset = (fse->cluster + mcdi->alloc_offset) % mcdi->clusters_per_block;		
		
		if (index == mcman_badblock) {
			fse->cluster = mcman_replacementcluster[offset] - mcdi->alloc_offset;
			mcman_1stcacheEntsetwrflagoff();
		}
		
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, fse->cluster, fse->dir_entry, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
			
		length = fse->length;
			
		r = mcman_readdirentry(mcman_badblock_port, mcman_badblock_slot, s3, 0, &fse);
		if (r != sceMcResSucceed)
			goto lbl_e168;
			
		s3 = fse->cluster;	
		index = (fse->cluster + mcdi->alloc_offset) / mcdi->clusters_per_block;		
		offset = (fse->cluster + mcdi->alloc_offset) % mcdi->clusters_per_block;		
		curentry = fse->dir_entry + 1;

		if (index == mcman_badblock) {
			fse->cluster = mcman_replacementcluster[offset] - mcdi->alloc_offset;
			mcman_1stcacheEntsetwrflagoff();
		}
		
		if (s3 != 0) 
			goto lbl_dd8c;
			
		if (curentry != 1)	
			goto lbl_dd8c;	
		
lbl_e030:		
		if (mcdi->clusters_per_block > 0) {
			i = 0;
			do {
				clust = (mcman_badblock * mcdi->clusters_per_block) + i;
				if (clust >= mcdi->alloc_offset) {
					r = mcman_setFATentry(mcman_badblock_port, mcman_badblock_slot, clust - mcdi->alloc_offset, 0xfffffffd);
					if (r != sceMcResSucceed)
						goto lbl_e168;
				}			
			} while (++i < mcdi->clusters_per_block);			
		}				
		
		mcman_flushmccache(mcman_badblock_port, mcman_badblock_slot);
		mcman_clearcache(mcman_badblock_port, mcman_badblock_slot);
		
		mcman_badblock = 0;
		
		if (mcdi->clusters_per_block > 0) {
			i = 0;
			do {
				if (mcman_replacementcluster[i] != 0) {
					r = mcman_readcluster(mcman_badblock_port, mcman_badblock_slot, (mcdi->backup_block1 * mcdi->clusters_per_block) + i, &mce);
					if (r != sceMcResSucceed)
						goto lbl_e168;
					
					mce->cluster = mcman_replacementcluster[i];
					mce->wr_flag = 1;
				}
			} while (++i < mcdi->clusters_per_block);			
		}				
		
		mcman_flushmccache(mcman_badblock_port, mcman_badblock_slot);
	}
	else {
		mcman_badblock = 0;
	}
		
	r = mcman_clearsuperblock(mcman_badblock_port, mcman_badblock_slot);
	return r;	
	
lbl_e168:	
	mcman_badblock = 0;
	return sceMcResFailReplace;
}

//-------------------------------------------------------------- 
int mcman_clearsuperblock(int port, int slot)
{
	register int r, i, size, temp;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	McCacheEntry *mce;
	
	// set superblock magic & version
	memset((void *)&mcdi->magic, 0, sizeof (mcdi->magic) + sizeof (mcdi->version));
	*((u32 *)&mcdi->magic    ) = *((u32 *)&SUPERBLOCK_MAGIC    );
	*((u32 *)&mcdi->magic + 1) = *((u32 *)&SUPERBLOCK_MAGIC + 1);
	*((u32 *)&mcdi->magic + 2) = *((u32 *)&SUPERBLOCK_MAGIC + 2);
	*((u32 *)&mcdi->magic + 3) = *((u32 *)&SUPERBLOCK_MAGIC + 3);
	*((u32 *)&mcdi->magic + 4) = *((u32 *)&SUPERBLOCK_MAGIC + 4);
	*((u32 *)&mcdi->magic + 5) = *((u32 *)&SUPERBLOCK_MAGIC + 5);
	*((u32 *)&mcdi->magic + 6) = *((u32 *)&SUPERBLOCK_MAGIC + 6);
	*((u8 *)&mcdi->magic + 28) = *((u8 *)&SUPERBLOCK_MAGIC + 28);
	
	strcat((char *)&mcdi->magic, SUPERBLOCK_VERSION);
	
	for (i = 0; (u32)(i < sizeof(MCDevInfo)); i += 1024) {
		temp = i;
		if (i < 0)
			temp = i + 1023;
		r = mcman_readcluster(port, slot, temp >> 10, &mce);	
		if (r != sceMcResSucceed)
			return -48;
		mce->wr_flag = 1;
		size = 1024;
		temp = sizeof(MCDevInfo) - i;
		
		if (temp <= 1024) 
			size = temp;
		
		memcpy((void *)mce->cl_data, (void *)mcdi, size);	
	}
	
	r = mcman_flushmccache(port, slot);
	
	return r;
}

//-------------------------------------------------------------- 

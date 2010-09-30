/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "mcman.h"

extern int timer_ID;

// mcman cmd table used by sio2packet_add fnc
u8 mcman_cmdtable[36] = {
	0x11, 0x04, 0x12, 0x04, 0x21, 0x09, 0x22, 0x09, 
	0x23, 0x09, 0x24, 0x86, 0x25, 0x04, 0x26, 0x0d,
	0x27, 0x05, 0x28, 0x05, 0x42, 0x86, 0x43, 0x86, 
	0x81, 0x04, 0x82, 0x04, 0x42, 0x06, 0x43, 0x06, 
	0xbf, 0x05, 0xf3, 0x05
};

// sio2packet_add child functions
void (*sio2packet_add_func)(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_wdma_u32(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_pagedata_out(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_pagedata_in(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_ecc_in(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_ecc_out(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_wdma_5a(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_wdma_00(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_wdma_u8(int port, int slot, int cmd, u8 *buf, int pos);
void sio2packet_add_do_nothing(int port, int slot, int cmd, u8 *buf, int pos);

// functions pointer array to handle sio2packet_add inner functions calls
void *sio2packet_add_funcs_array[16] = {
    (void *)sio2packet_add_wdma_00,		// commands that needs to clear in_dma.addr[2]
    (void *)sio2packet_add_wdma_u32,	// commands that needs to copy an int to in_dma.addr[2..5] (an int* can be passed as arg into buf) 
    (void *)sio2packet_add_wdma_u32,	// ...
    (void *)sio2packet_add_wdma_u32,	// ...
    (void *)sio2packet_add_wdma_u8,		// commands that needs to copy an u8 value to in_dma.addr[2]
    (void *)sio2packet_add_do_nothing,	// do nothing	
    (void *)sio2packet_add_do_nothing,	// ...
    (void *)sio2packet_add_wdma_5a,		// commands that needs to set in_dma.addr[2] to value 0x5a, used to set termination code 	
    (void *)sio2packet_add_do_nothing,	// do nothing		
    (void *)sio2packet_add_pagedata_in,	// commands that needs to copy pagedata buf into in_dma.addr 	
    (void *)sio2packet_add_pagedata_out,// commands that needs to set in_dma.addr[2] to value 128 for pagedata out		
    (void *)sio2packet_add_do_nothing,	// do nothing		
    (void *)sio2packet_add_do_nothing,	// ...
    (void *)sio2packet_add_ecc_in,		// commands that needs to copy sparedata buf into in_dma.addr 	
    (void *)sio2packet_add_ecc_out,		// commands that needs to prepare regdata and in_dma.addr to receive sparedata
    (void *)sio2packet_add_wdma_u8		// commands that needs to copy an u8 value to in_dma.addr[2]
};

//--------------------------------------------------------------
void sio2packet_add(int port, int slot, int cmd, u8 *buf)
{	// Used to build the sio2packets for all mc commands
	register u32 regdata;
	register int pos;
	u8 *p;
	
	if (cmd == 0xffffffff) {
		mcman_sio2packet.in_dma.count = 0;
		return;
	}
	
	if (mcman_sio2packet.in_dma.count < 0xb) {
		
		if (cmd == 0xfffffffe) {
			mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count] = 0;
			mcman_sio2packet.out_dma.count = mcman_sio2packet.in_dma.count;
			return;
		}
		
		regdata = (((port & 1) + 2) & 3) | 0x70;
		regdata |= mcman_cmdtable[(cmd << 1) + 1] << 18;
		regdata |= mcman_cmdtable[(cmd << 1) + 1] << 8;

		mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count] = regdata;
						
		pos = ((mcman_sio2packet.in_dma.count + (mcman_sio2packet.in_dma.count << 3)) << 4);

		mcman_sio2packet.in_dma.count++;
	
		p = mcman_sio2packet.in_dma.addr + pos;			
		p[0] = 0x81;
		p[1] = mcman_cmdtable[cmd << 1];
			
		if (((cmd - 1) >= 16) || ((cmd - 1) < 0))
			return;
				
		sio2packet_add_func = (void*)sio2packet_add_funcs_array[(cmd - 1)];
		(sio2packet_add_func)(port, slot, cmd, buf, pos); // call to the needed child func	
	}
}

//----
// sio2packet child funcs

void sio2packet_add_wdma_u32(int port, int slot, int cmd, u8 *buf, int pos)
{
	u8 *p = mcman_sio2packet.in_dma.addr + pos;
	p[2] = buf[0];	
	p[3] = buf[1];
	p[4] = buf[2];
	p[5] = buf[3];
	p[6] = mcman_calcEDC(&p[2], 4);
}

//--

void sio2packet_add_pagedata_out(int port, int slot, int cmd, u8 *buf, int pos)
{
	// used for sio2 command 0x81 0x43 to read page data
	u8 *p = mcman_sio2packet.in_dma.addr + pos;	
	p[2] = 128;
}

//--

void sio2packet_add_pagedata_in(int port, int slot, int cmd, u8 *buf, int pos)
{
	// used for sio2 command 0x81 0x42 to write page data
	register int i;
	u8 *p = mcman_sio2packet.in_dma.addr + pos;
	p[2] = 128;
	
	for (i=0; i<128; i++)
		p[3 + i] = buf[i];
	
	p[3 + 128] = mcman_calcEDC(&buf[0], 128);
}

//--

void sio2packet_add_ecc_in(int port, int slot, int cmd, u8 *buf, int pos)
{
	// used for sio2 command 0x81 0x42 to write page ecc
	register u32 regdata;
	int i;
	u8 *p = mcman_sio2packet.in_dma.addr + pos;
		
	p[2] = mcman_sparesize(port, slot);
	
	for (i=0; i<(p[2] & 0xff); i++)	
		p[3 + i] = buf[i];
	
	p[3 + p[2]] = mcman_calcEDC(&buf[0], p[2]);
		
	regdata  = (p[2] + mcman_cmdtable[(cmd << 1) + 1]) << 18;
	regdata |= mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count-1] & 0xf803ffff;
	regdata &= 0xfffe00ff;
	regdata |= (p[2] + mcman_cmdtable[(cmd << 1) + 1]) << 8;
	
	mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count-1] = regdata;				
}

//--

void sio2packet_add_ecc_out(int port, int slot, int cmd, u8 *buf, int pos)
{
	// used for sio2 command 0x81 0x43 to read page ecc
	register u32 regdata;
	u8 *p = mcman_sio2packet.in_dma.addr + pos;	
		
	p[2] = mcman_sparesize(port, slot);
	
	regdata  = (p[2] + mcman_cmdtable[(cmd << 1) + 1]) << 18;
	regdata |= mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count-1] & 0xf803ffff;
	regdata &= 0xfffe00ff;
	regdata |= (p[2] + mcman_cmdtable[(cmd << 1) + 1]) << 8;
	
	mcman_sio2packet.regdata[mcman_sio2packet.in_dma.count-1] = regdata;				
}

//--

void sio2packet_add_wdma_5a(int port, int slot, int cmd, u8 *buf, int pos)
{
	// used for sio2 command 0x81 0x27 to set a termination code
	u8 *p = mcman_sio2packet.in_dma.addr + pos;	
	p[2] = 0x5a;
}

//--

void sio2packet_add_wdma_00(int port, int slot, int cmd, u8 *buf, int pos)
{
	u8 *p = mcman_sio2packet.in_dma.addr + pos;			
	p[2] = 0x00;
	
}

//--

void sio2packet_add_wdma_u8(int port, int slot, int cmd, u8 *buf, int pos)
{
	u8 *p = mcman_sio2packet.in_dma.addr + pos;		
	p[2] = buf[0];
}

//--

void sio2packet_add_do_nothing(int port, int slot, int cmd, u8 *buf, int pos)
{
	// do nothing
}

//-------------------------------------------------------------- 
int mcsio2_transfer(int port, int slot, sio2_transfer_data_t *sio2data)
{
	register int r;

#ifdef DEBUG
	u8 *p = (u8 *)(sio2data->in_dma.addr);
	if (p)
		DPRINTF("mcman: mcsio2_transfer port%d slot%d cmd = %02x %02x %02x ", port, slot, p[0], p[1], p[2]);
	else {
		p = (u8 *)(sio2data->in);
		DPRINTF("mcman: mcsio2_transfer for secrman port%d slot%d cmd = %02x %02x %02x ", port, slot, p[0], p[1], p[2]);
	}
#endif

	// SIO2 transfer for MCMAN
	sio2_mc_transfer_init();
	r = sio2_transfer(sio2data);

#ifdef DEBUG	
	DPRINTF("returns %d\n", r);
#endif

	return r;
}

//-------------------------------------------------------------- 
int mcsio2_transfer2(int port, int slot, sio2_transfer_data_t *sio2data)
{
	// SIO2 transfer for XMCMAN
	register int r;
	int port_ctrl[8];	

#ifdef DEBUG
	DPRINTF("mcman: mcsio2_transfer2 port%d slot%d\n", port, slot);		
#endif
	
	port_ctrl[0] = -1; 
	port_ctrl[1] = -1; 
	port_ctrl[2] = -1; 
	port_ctrl[3] = -1; 

	port_ctrl[(port & 1) + 2] = slot;
	
	sio2_mc_transfer_init();        
	sio2_func1(&port_ctrl);   
	r = sio2_transfer(sio2data);
	sio2_transfer_reset();

#ifdef DEBUG
	DPRINTF("mcman: mcsio2_transfer2 returns %d\n", r);
#endif
	
	return r;
}

//--------------------------------------------------------------
void mcman_initPS2com(void)
{
	mcman_wmemset((void *)&mcman_sio2packet, sizeof (mcman_sio2packet), 0);
	
	mcman_sio2packet.port_ctrl1[2] = 0xff020405;
	mcman_sio2packet.port_ctrl1[3] = 0xff020405;
	mcman_sio2packet.port_ctrl2[2] = 0x0005ffff & 0xfcffffff;
	mcman_sio2packet.port_ctrl2[3] = 0x0005ffff & 0xfcffffff;

	mcman_sio2packet.in_dma.addr = &mcman_wdmabufs;
	mcman_sio2packet.in_dma.size = 0x24;
		
	mcman_sio2packet.out_dma.addr = &mcman_rdmabufs;
	mcman_sio2packet.out_dma.size = 0x24;

#ifdef DEBUG
	DPRINTF("mcman: mcman_initPS2com registering secrman_mc_command callback\n");
#endif			
	SetMcCommandCallback((void *)secrman_mc_command);

#ifdef DEBUG
	DPRINTF("mcman: mcman_initPS2com registering mcman_getcnum callback\n");
#endif			

	SetMcDevIDCallback((void *)mcman_getcnum);
}

//-------------------------------------------------------------- 
void mcman_initPS1PDAcom(void)
{
	memset((void *)&mcman_sio2packet_PS1PDA, 0, sizeof (mcman_sio2packet_PS1PDA));

	mcman_sio2packet_PS1PDA.port_ctrl1[0] = 0xffc00505;
	mcman_sio2packet_PS1PDA.port_ctrl1[1] = 0xffc00505;
	mcman_sio2packet_PS1PDA.port_ctrl1[2] = 0xffc00505;
	mcman_sio2packet_PS1PDA.port_ctrl1[3] = 0xffc00505;

	mcman_sio2packet_PS1PDA.port_ctrl2[0] = 0x000201f4 & 0xfcffffff;
	mcman_sio2packet_PS1PDA.port_ctrl2[1] = 0x000201f4 & 0xfcffffff;
	mcman_sio2packet_PS1PDA.port_ctrl2[2] = 0x000201f4 & 0xfcffffff;
	mcman_sio2packet_PS1PDA.port_ctrl2[3] = 0x000201f4 & 0xfcffffff;

	mcman_sio2packet_PS1PDA.in = (u8 *)&mcman_sio2inbufs_PS1PDA;
	mcman_sio2packet_PS1PDA.out = (u8 *)&mcman_sio2outbufs_PS1PDA;
}

//--------------------------------------------------------------
int secrman_mc_command(int port, int slot, sio2_transfer_data_t *sio2data)
{
	register int r;

#ifdef DEBUG
	DPRINTF("mcman: secrman_mc_command port%d slot%d\n", port, slot);
#endif			
					
	r = mcman_sio2transfer(port, slot, sio2data);
		
	return r;
}

//--------------------------------------------------------------
int mcman_cardchanged(int port, int slot)
{
	register int retries;
	u8 *p = mcman_sio2packet.out_dma.addr;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_cardchanged sio2cmd port%d slot%d\n", port, slot);		
#endif
	
	sio2packet_add(port, slot, 0xffffffff, NULL);
	sio2packet_add(port, slot, 0, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);
  
	retries = 0;
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if (((mcman_sio2packet.stat6c & 0xF000) == 0x1000) && (p[3] != 0x66))
			break;
			
	} while (++retries < 5);

	if (retries >= 5) {
#ifdef DEBUG
		DPRINTF("mcman: mcman_cardchanged sio2cmd card changed!\n");		
#endif
		return sceMcResChangedCard; 
	}

#ifdef DEBUG		
	DPRINTF("mcman: mcman_cardchanged sio2cmd succeeded\n");
#endif	
	
	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_eraseblock(int port, int slot, int block, void **pagebuf, void *eccbuf)
{		
	register int retries, size, ecc_offset;
	int page;
	u8 *p = mcman_sio2packet.out_dma.addr;
	void *p_ecc;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	
	page = block * mcdi->blocksize;
	
	sio2packet_add(port, slot, 0xffffffff, NULL);    
	sio2packet_add(port, slot, 0x02, (u8 *)&page);
	sio2packet_add(port, slot, 0x0d, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);
	
	retries = 0;
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
			
		if (((mcman_sio2packet.stat6c & 0xF000) != 0x1000) || (p[8] != 0x5a))
			continue;
			
		if (p[0x93] == p[8])
			break;
	} while (++retries < 5);

	if (retries >= 5)
		return sceMcResChangedCard;
		
	if (pagebuf && eccbuf) { // This part leave the first ecc byte of each block page in eccbuf 
		mcman_wmemset(eccbuf, 32, 0);
		
		page = 0;
		while (page < mcdi->blocksize) {
			ecc_offset = page * mcdi->pagesize;
			if (ecc_offset < 0)
				ecc_offset += 0x1f;
			ecc_offset = ecc_offset >> 5;
			p_ecc = (void *)(eccbuf + ecc_offset);	
			size = 0;
			while (size < mcdi->pagesize)	{
				if (*pagebuf) 
					McDataChecksum((void *)(*pagebuf + size), p_ecc);
				size += 128;
				p_ecc += 3;
			} 
			pagebuf++; 
			page++;
		}
	}

	sio2packet_add(port, slot, 0xffffffff, NULL);
	sio2packet_add(port, slot, 0x1, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);
	
	retries = 0;
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if (((mcman_sio2packet.stat6c & 0xF000) != 0x1000))
			continue;
			
		if (p[3] == 0x5a)
			return sceMcResSucceed;
	} while (++retries < 100);
	
	if (p[3] == 0x66)
		return sceMcResFailReplace;
	
	return sceMcResNoFormat;
}

//--------------------------------------------------------------
int McWritePage(int port, int slot, int page, void *pagebuf, void *eccbuf) // Export #19
{
	register int index, count, retries;
	u8 *p_pagebuf = (u8 *)pagebuf;
	u8 *p = mcman_sio2packet.out_dma.addr;
 	
	count = (mcman_devinfos[port][slot].pagesize + 127) >> 7;
	
	retries = 0;
	
	do {
		if (retries > 0)
			mcman_cardchanged(port, slot);
 		
 		sio2packet_add(port, slot, 0xffffffff, NULL);
 		sio2packet_add(port, slot, 0x03, (u8 *)&page);
 		
 		index = 0;
 		while (index < count) {
     		sio2packet_add(port, slot, 0x0a, (u8 *)&p_pagebuf[index << 7]);
			index++;
 		}

   		if (mcman_devinfos[port][slot].cardflags & CF_USE_ECC) { 
     		// if memcard have ECC support 
   			sio2packet_add(port, slot, 0x0e, eccbuf);
		}
 		sio2packet_add(port, slot, 0xfffffffe, NULL);
  		 		 		
		mcman_sio2transfer(port, slot, &mcman_sio2packet);

		if (((mcman_sio2packet.stat6c & 0xF000) != 0x1000) || (p[8] != 0x5a))
			continue; 		
		
		index = 0;
 		while (index < count) {
			if (p[0x94 + 128 + 1 + ((index + (index << 3)) << 4)] != 0x5a)
				break;
			index++;
   		}	
   		
	   	if (index < count)
	   		continue;
	   			
	   	if (mcman_devinfos[port][slot].cardflags & CF_USE_ECC) {
     		// if memcard have ECC support 
			index++;
     		if (p[5 + ((index + (index << 3)) << 4) + mcman_sparesize(port, slot)] != 0x5a)
     			continue;
		}
		
		sio2packet_add(port, slot, 0xffffffff, NULL);
		sio2packet_add(port, slot, 0x0c, NULL);
		sio2packet_add(port, slot, 0xfffffffe, NULL);
			
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
  		
		if (((mcman_sio2packet.stat6c & 0xF000) != 0x1000) || (p[3] != 0x5a))
			continue;

		return sceMcResSucceed;	
			
	} while (++retries < 5);			
	
	if (p[3] == 0x66)
		return sceMcResFailReplace;
	
	return sceMcResNoFormat;
}

//--------------------------------------------------------------
int mcman_readpage(int port, int slot, int page, void *buf, void *eccbuf)
{
	register int index, count, retries, r, i;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];
	u8 *pbuf = (u8 *)buf;
	u8 *pecc = (u8 *)eccbuf;
	u8 *p = mcman_sio2packet.out_dma.addr;
			
	count = (mcdi->pagesize + 127) >> 7;
	
	retries = 0;
		
	do {
		if (retries > 0)
			mcman_cardchanged(port, slot);
 		
 		sio2packet_add(port, slot, 0xffffffff, NULL);
 		sio2packet_add(port, slot, 0x04, (u8 *)&page);
 		
 		if (count > 0) {
	 		index = 0;
     		while (index < count) {
	     		sio2packet_add(port, slot, 0x0b, NULL);
	     		index++;
     		}
 		}

   		if (mcdi->cardflags & CF_USE_ECC) // if memcard have ECC support 
   			sio2packet_add(port, slot, 0x0f, NULL);
	 	
 		sio2packet_add(port, slot, 0x0c, NULL);
 		sio2packet_add(port, slot, 0xfffffffe, NULL);
 		
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if (((mcman_sio2packet.stat6c & 0xF000) != 0x1000)
			|| (p[8] != 0x5a)
				|| (p[0x94 + 0x2cf] != 0x5a)) 
			continue; 		
		
		if (count > 0) {
	 		index = 0;
	 		while (index < count) {
   				// checking EDC
   				r = mcman_calcEDC(&p[0x94 + ((index + (index << 3)) << 4)], 128) & 0xFF;
   				if (r != p[0x94 + 128 + ((index + (index << 3)) << 4)])
   					break;
	     		index++;   					
	   		}
	   		
	   		if (index < count)
		   		continue;
			
	 		index = 0;
	 		while (index < count) {
	     		for (i=0; i<128; i++)
	     			pbuf[(index << 7) + i] = p[0x94 + ((index + (index << 3)) << 4) + i];
	     		index++;
   			}	
		}

		memcpy(pecc, &p[0x94 + ((count + (count << 3)) << 4)] , mcman_sparesize(port, slot));
		break;
 		
	} while (++retries < 5);			
	
	if (retries < 5) 
		return sceMcResSucceed;
		
	return sceMcResChangedCard;
}

//--------------------------------------------------------------
int McGetCardSpec(int port, int slot, u16 *pagesize, u16 *blocksize, int *cardsize, u8 *flags)
{
	register int retries, r;
	u8 *p = mcman_sio2packet.out_dma.addr;
	
#ifdef DEBUG	
	DPRINTF("mcman: McGetCardSpec sio2cmd port%d slot%d\n", port, slot);		
#endif
	
	sio2packet_add(port, slot, 0xffffffff, NULL);
	sio2packet_add(port, slot, 0x07, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);
  
	retries = 0;
 
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);

		if (((mcman_sio2packet.stat6c & 0xF000) == 0x1000) && (p[12] == 0x5a)) {
   			// checking EDC
   			r = mcman_calcEDC(&p[3], 8) & 0xFF;
   			if (r == p[11])
   				break;
		}
	} while (++retries < 5);
 
	if (retries >= 5)
		return sceMcResChangedCard; 

	*pagesize = (p[4] << 8) + p[3];
	*blocksize = (p[6] << 8) + p[5];
	*cardsize = (p[8] << 8) + p[7] + (p[9] << 16) + (p[10] << 24);
	*flags = p[2];

#ifdef DEBUG	
	DPRINTF("mcman: McGetCardSpec sio2cmd pagesize=%d blocksize=%d cardsize=%d flags%x\n", *pagesize, *blocksize, *cardsize, *flags);
#endif	
	
	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_resetauth(int port, int slot)
{
	register int retries;

#ifdef DEBUG		
	DPRINTF("mcman: mcman_resetauth sio2cmd port%d slot%d\n", port, slot);
#endif	
	
	sio2packet_add(port, slot, 0xffffffff, NULL);
	sio2packet_add(port, slot, 0x11, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);

	retries = 0;
 
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if ((mcman_sio2packet.stat6c & 0xF000) == 0x1000)
			break;
		
	} while (++retries < 5);
	
	if (retries >= 5) {
#ifdef DEBUG		
		DPRINTF("mcman: mcman_resetauth sio2cmd card changed!\n");
#endif	

		return sceMcResChangedCard;
	}

#ifdef DEBUG		
	DPRINTF("mcman: mcman_resetauth sio2cmd succeeded\n");
#endif	
 		
	return sceMcResSucceed;
}

//--------------------------------------------------------------
int mcman_probePS2Card2(int port, int slot)
{
	register int retries, r;
	u8 *p = mcman_sio2packet.out_dma.addr;

#ifdef DEBUG
	DPRINTF("mcman: mcman_probePS2Card2 sio2cmd port%d slot%d\n", port, slot);
#endif	
		
	retries = 0;
	do {
		sio2packet_add(port, slot, 0xffffffff, NULL);
		sio2packet_add(port, slot, 0x09, NULL);
		sio2packet_add(port, slot, 0xfffffffe, NULL);

		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if (((mcman_sio2packet.stat6c & 0xF000) == 0x1000) && (p[4] != 0x66))
			break;
	} while (++retries < 5);
	
	if (retries >= 5)
		return sceMcResFailDetect;
		
	if (p[3] == 0x5a) {
		r = McGetFormat(port, slot);
		if (r > 0) {
#ifdef DEBUG
			DPRINTF("mcman: mcman_probePS2Card2 succeeded\n");
#endif	

			return sceMcResSucceed;
		}
			
		r = McGetFormat(port, slot);
		if (r < 0) {
#ifdef DEBUG
			DPRINTF("mcman: mcman_probePS2Card2 sio2cmd failed (no format)\n");
#endif

			return sceMcResNoFormat;
		}
	}

#ifdef DEBUG
	DPRINTF("mcman: mcman_probePS2Card2 sio2cmd failed (mc detection failed)\n");
#endif

	return sceMcResFailDetect2;
}

//--------------------------------------------------------------
int mcman_probePS2Card(int port, int slot) //2
{
	register int retries, r;
	register MCDevInfo *mcdi;
	u8 *p = mcman_sio2packet.out_dma.addr;

#ifdef DEBUG	
	DPRINTF("mcman: mcman_probePS2Card sio2cmd port%d slot%d\n", port, slot);
#endif	
	
	r = mcman_cardchanged(port, slot);
	if (r == sceMcResSucceed) {
		r = McGetFormat(port, slot);
		if (r != 0) {
#ifdef DEBUG	
			DPRINTF("mcman: mcman_probePS2Card sio2cmd succeeded\n");
#endif	

			return sceMcResSucceed;
		}
	}
	
	if (mcman_resetauth(port, slot) != sceMcResSucceed) {
#ifdef DEBUG	
		DPRINTF("mcman: mcman_probePS2Card sio2cmd failed (auth reset failed)\n");
#endif	

		return sceMcResFailResetAuth;
	}
	
	if (SecrAuthCard(port + 2, slot, mcman_getcnum(port, slot)) == 0) {
#ifdef DEBUG	
		DPRINTF("mcman: mcman_probePS2Card sio2cmd failed (auth failed)\n");
#endif	

		return sceMcResFailAuth;
	}
			
	retries = 0;
	do {
		sio2packet_add(port, slot, 0xffffffff, NULL);
		sio2packet_add(port, slot, 0x09, NULL);
		sio2packet_add(port, slot, 0xfffffffe, NULL);

		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
		if (((mcman_sio2packet.stat6c & 0xF000) == 0x1000) && (p[4] != 0x66))
			break;
	} while (++retries < 5);
	
	if (retries >= 5) {
#ifdef DEBUG
		DPRINTF("mcman: mcman_probePS2Card sio2cmd failed (mc detection failed)\n");
#endif
		return sceMcResFailDetect;
	}
	
	mcman_clearcache(port, slot);

	sio2packet_add(port, slot, 0xffffffff, NULL);
	sio2packet_add(port, slot, 0x08, NULL);
	sio2packet_add(port, slot, 0xfffffffe, NULL);

	retries = 0;
    do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet);
		
    	if ((mcman_sio2packet.stat6c & 0xF000) != 0x1000)
    		continue;
    	
    	if (p[4] == 0x5a)
    		break;
	} while (++retries < 5);

	if (retries >= 5) {
#ifdef DEBUG
		DPRINTF("mcman: mcman_probePS2Card sio2cmd failed (mc detection failed)\n");
#endif

		return sceMcResFailDetect2;
	}
	
	r = mcman_setdevinfos(port, slot);
	if (r == 0) {
#ifdef DEBUG
		DPRINTF("mcman: mcman_probePS2Card sio2cmd card changed!\n");
#endif
		return sceMcResChangedCard;
	}
	if (r != sceMcResNoFormat) {
#ifdef DEBUG
		DPRINTF("mcman: mcman_probePS2Card sio2cmd failed (mc detection failed)\n");
#endif
		return sceMcResFailDetect2;
	}
		
	mcdi = &mcman_devinfos[port][slot];	
	mcdi->cardform = r;	

#ifdef DEBUG	
	DPRINTF("mcman: mcman_probePS2Card sio2cmd succeeded\n");
#endif	
	
	return r;
}

//-------------------------------------------------------------- 
int mcman_probePS1Card2(int port, int slot)
{
	register int retries;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_probePS1Card2 port%d slot%d\n", port, slot);
#endif	
	
	mcman_sio2packet_PS1PDA.regdata[0] = 0;
	mcman_sio2packet_PS1PDA.regdata[0] = (((port & 1) + 2)) | 0x000c0340;
	mcman_sio2packet_PS1PDA.in_size = 3;
	mcman_sio2packet_PS1PDA.out_size = 3;
	
	mcman_sio2packet_PS1PDA.regdata[1] = 0;
	mcman_sio2inbufs_PS1PDA[0] = 0x81;
	mcman_sio2inbufs_PS1PDA[1] = 0x52;
	
	retries = 0;
	do {
		mcman_timercount = (u32)(GetTimerCounter(timer_ID) - mcman_timercount);

		u32 hi, lo;
		long_multiply(mcman_timercount, 0x3e0f83e1, &hi, &lo);
		
		if ((u32)((hi >> 3) < mcman_timerthick))
			DelayThread(mcman_timerthick - (hi >> 3));
			
		mcman_sio2transfer(port, slot, &mcman_sio2packet_PS1PDA);
				
		mcman_timercount = GetTimerCounter(timer_ID);
		mcman_timerthick = 0;	
		
		if (((mcman_sio2packet_PS1PDA.stat6c & 0xf000) == 0x1000) \
				&& (mcman_sio2outbufs_PS1PDA[2] == 0x5a)) {
			break;
		}
	} while (++retries < 5);

	if (retries >= 5)	
		return -12;

	if (mcman_sio2outbufs_PS1PDA[1] == 0) {
		if (mcdi->cardform > 0)
			return sceMcResSucceed;
		if (mcdi->cardform < 0)
			return sceMcResNoFormat;
	}
	else if (mcman_sio2outbufs_PS1PDA[1] != 8) {
		return -14;
	}
	
	return -13;
}

//-------------------------------------------------------------- 
int mcman_probePS1Card(int port, int slot)
{
	register int i, r, retries;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	u32 *p;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_probePS1Card port%d slot%d\n", port, slot);
#endif	
	
	mcman_sio2packet_PS1PDA.regdata[0] = 0;
	mcman_sio2packet_PS1PDA.regdata[0] = (((port & 1) + 2)) | 0x000c0340;
	mcman_sio2packet_PS1PDA.in_size = 3;
	mcman_sio2packet_PS1PDA.out_size = 3;
	
	mcman_sio2packet_PS1PDA.regdata[1] = 0;
	mcman_sio2inbufs_PS1PDA[0] = 0x81;
	mcman_sio2inbufs_PS1PDA[1] = 0x52;
	
	retries = 0;
	do {
		mcman_timercount = (u32)(GetTimerCounter(timer_ID) - mcman_timercount);

		u32 hi, lo;
		long_multiply(mcman_timercount, 0x3e0f83e1, &hi, &lo);
		
		if ((u32)((hi >> 3) < mcman_timerthick))
			DelayThread(mcman_timerthick - (hi >> 3));
			
		mcman_sio2transfer(port, slot, &mcman_sio2packet_PS1PDA);
				
		mcman_timercount = GetTimerCounter(timer_ID);
		mcman_timerthick = 0;	
		
		if (((mcman_sio2packet_PS1PDA.stat6c & 0xf000) == 0x1000) \
				&& (mcman_sio2outbufs_PS1PDA[2] == 0x5a)) {
			break;
		}
	} while (++retries < 5);

	if (retries >= 5)	
		return -11;

	if (mcman_sio2outbufs_PS1PDA[1] == 0) {
		if (mcdi->cardform != 0)
			return sceMcResSucceed;
	}
	else if (mcman_sio2outbufs_PS1PDA[1] != 8) {
		return -12;
	}
	
	mcman_clearcache(port, slot);
	
	p = (u32 *)&mcman_sio2outbufs_PS1PDA[124];
	for (i = 31; i >= 0; i--)
		*p-- = 0;
	
	r = McWritePS1PDACard(port, slot, 63, mcman_sio2outbufs_PS1PDA);
	if (r < 0) 
		return -13;	
	
	r = mcman_setPS1devinfos(port, slot);
	if (r == 0) 
		return sceMcResChangedCard;	
	
	mcdi->cardform = r; 	
		
	return r;
}

//-------------------------------------------------------------- 
int mcman_probePDACard(int port, int slot)
{
	register int retries;
	
#ifdef DEBUG
	DPRINTF("mcman: mcman_probePDACard port%d slot%d\n", port, slot);
#endif	
	
	mcman_sio2packet_PS1PDA.regdata[0] = 0;
	mcman_sio2packet_PS1PDA.regdata[0] = (port & 3) | 0x00140540;
	mcman_sio2packet_PS1PDA.in_size = 5;
	mcman_sio2packet_PS1PDA.out_size = 5;
	
	mcman_sio2packet_PS1PDA.regdata[1] = 0;
	mcman_sio2inbufs_PS1PDA[0] = 0x81;
	mcman_sio2inbufs_PS1PDA[1] = 0x58;
	
	retries = 0;
	do {
		mcman_sio2transfer(port, slot, &mcman_sio2packet_PS1PDA);
				
		if ((mcman_sio2packet_PS1PDA.stat6c & 0xf000) == 0x1000)
				break;
				
	} while (++retries < 5);

	if (retries >= 5)	
		return -11;
		
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int McWritePS1PDACard(int port, int slot, int page, void *buf) // Export #30
{
	register int i, retries;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	u8 *p;
	
#ifdef DEBUG
	//DPRINTF("mcman: McWritePS1PDACard port%d slot%d page %x\n", port, slot, page);
#endif	
	
	mcman_sio2packet_PS1PDA.regdata[0] = 0;
	mcman_sio2packet_PS1PDA.regdata[0] = (((port & 1) + 2) & 3) | 0x02288a40;
	mcman_sio2packet_PS1PDA.in_size = 138;
	mcman_sio2packet_PS1PDA.out_size = 138;
	mcman_sio2packet_PS1PDA.regdata[1] = 0;
		
	for (i = 0; i < 20; i++) {
		if (mcdi->bad_block_list[i] == page)
			page += 36;
	}
	
	mcman_sio2inbufs_PS1PDA[0] = 0x81;
	mcman_sio2inbufs_PS1PDA[1] = 0x57;
	mcman_sio2inbufs_PS1PDA[4] = (u8)(page >> 8);
	mcman_sio2inbufs_PS1PDA[5] = (u8)page;
	
	p = (u8 *)buf;
	for (i = 0; i < 128; i++)
		mcman_sio2inbufs_PS1PDA[6 + i] = p[i];
	
	mcman_sio2inbufs_PS1PDA[4 + 130] = mcman_calcEDC(&mcman_sio2inbufs_PS1PDA[4], 130);	

	retries = 0;
	do {
		mcman_timercount = (u32)(GetTimerCounter(timer_ID) - mcman_timercount);
				
		u32 hi, lo;
		long_multiply(mcman_timercount, 0x3e0f83e1, &hi, &lo);
		
		if ((u32)((hi >> 3) < mcman_timerthick))
			DelayThread(mcman_timerthick - (hi >> 3));
			
		mcman_sio2transfer(port, slot, &mcman_sio2packet_PS1PDA);
		
		mcman_timercount = GetTimerCounter(timer_ID);
		mcman_timerthick = 20000;		
		
		if (((mcman_sio2packet_PS1PDA.stat6c & 0xf000) == 0x1000) \
				&& (mcman_sio2outbufs_PS1PDA[2] == 0x5a) \
					&& (mcman_sio2outbufs_PS1PDA[3] == 0x5d) \
						&& (mcman_sio2outbufs_PS1PDA[137] == 0x47)) {
			break;				
		}
	} while (++retries < 5);

	if (retries >= 5)	
		return sceMcResNoEntry;
		
	if ((mcman_sio2outbufs_PS1PDA[1] != 0) && (mcman_sio2outbufs_PS1PDA[1] != 8))
		return sceMcResFullDevice;	
	
	return sceMcResSucceed;
}

//-------------------------------------------------------------- 
int McReadPS1PDACard(int port, int slot, int page, void *buf) // Export #29
{
	register int i, retries;
	register MCDevInfo *mcdi = &mcman_devinfos[port][slot];	
	u8 *p;
	
#ifdef DEBUG
	//DPRINTF("mcman: McReadPS1PDACard port%d slot%d page %x\n", port, slot, page);
#endif	
	
	mcman_sio2packet_PS1PDA.regdata[0] = 0;
	mcman_sio2packet_PS1PDA.regdata[0] = (((port & 1) + 2) & 3) | 0x02308c40;
	mcman_sio2packet_PS1PDA.in_size = 140;
	mcman_sio2packet_PS1PDA.out_size = 140;
	mcman_sio2packet_PS1PDA.regdata[1] = 0;
	
	for (i = 0; i < 20; i++) {
		if (mcdi->bad_block_list[i] == page)
			page += 36;
	}
	
	mcman_sio2inbufs_PS1PDA[0] = 0x81;
	mcman_sio2inbufs_PS1PDA[1] = 0x52;
	mcman_sio2inbufs_PS1PDA[4] = (u8)(page >> 8);
	mcman_sio2inbufs_PS1PDA[5] = (u8)page;
	
	retries = 0;
	do {
		mcman_timercount = (u32)(GetTimerCounter(timer_ID) - mcman_timercount);
		
		u32 hi, lo;
		long_multiply(mcman_timercount, 0x3e0f83e1, &hi, &lo);
		
		if ((u32)((hi >> 3) < mcman_timerthick))
			DelayThread(mcman_timerthick - (hi >> 3));
			
		mcman_sio2transfer(port, slot, &mcman_sio2packet_PS1PDA);
				
		mcman_timercount = GetTimerCounter(timer_ID);
		mcman_timerthick = 10000;
		
		if (((mcman_sio2packet_PS1PDA.stat6c & 0xf000) == 0x1000) \
				&& (mcman_sio2outbufs_PS1PDA[2] == 0x5a) \
					&& (mcman_sio2outbufs_PS1PDA[3] == 0x5d) \
						&& (mcman_sio2outbufs_PS1PDA[4] == 0x00) \
							&& (mcman_sio2outbufs_PS1PDA[6] == 0x5c) \
								&& (mcman_sio2outbufs_PS1PDA[7] == 0x5d) \
									&& (mcman_sio2outbufs_PS1PDA[139] == 0x47)) {
										
			if (mcman_sio2outbufs_PS1PDA[138] == (mcman_calcEDC(&mcman_sio2outbufs_PS1PDA[8], 130) & 0xff))
				break;
		}
	} while (++retries < 5);

	if (retries >= 5)	
		return sceMcResNoEntry;

	p = (u8 *)buf;
	for (i = 0; i < 128; i++)
		p[i] = mcman_sio2outbufs_PS1PDA[10 + i];
					
	if ((mcman_sio2outbufs_PS1PDA[1] != 0) && (mcman_sio2outbufs_PS1PDA[1] != 8))
		return sceMcResDeniedPermit;	
		
	return sceMcResSucceed;
}



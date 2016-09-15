/*
 * PS2 IGS (InGame Screenshot)
 *
 * Copyright (C) 2010-2014 maximus32
 * Copyright (C) 2014,2015,2016 doctorxyz
 *
 * This file is part of PS2 IGS Feature.
 *
 * PS2 IGS Feature is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2 IGS Feature is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#include "include/igs_api.h"

_IGS_ENGINE_ void D1_CHCR_Init(void) {
	*D1_CHCR = 0x00000000;
	*D1_MADR = 0x00000000;
	*D1_SIZE = 0x00000000;
	*D1_TADR = 0x00000000;
	*D1_ASR0 = 0x00000000;
	*D1_ASR1 = 0x00000000;
	*D1_SADR = 0x00000000;
}

_IGS_ENGINE_ void D2_CHCR_Init(void) {
	*D2_CHCR = 0x00000000;
	*D2_MADR = 0x00000000;
	*D2_SIZE  = 0x00000000; 
	*D2_TADR = 0x00000000;
	*D2_ASR0 = 0x00000000;
	*D2_ASR1 = 0x00000000;
	*D2_SADR = 0x00000000;
}

 _IGS_ENGINE_ void BlinkError(u8 x) {
	u8 i;
	while(1){
		for(i=1;i<=x;i++){
			delay(1);
			GS_BGCOLOUR=0x0000FF;
			delay(2);
			GS_BGCOLOUR=0;
		}
	delay(4);
	GS_BGCOLOUR=0x0000FF;
	}
}

_IGS_ENGINE_ int GsSetDefStoreImage(GsStoreImage *sp, u16 sbp, u16 sbw, u16 spsm, u16 width, u16 height) {
	sp->vifcode[0] = VIF1_NOP(0);
	sp->vifcode[1] = VIF1_MSKPATH3(0x8000, 0);
	sp->vifcode[2] = VIF1_FLUSHA(0);
	sp->vifcode[3] = VIF1_DIRECT(6, 0);

	GIF_CLEAR_TAG(&sp->giftag);
	sp->giftag.NLOOP = 5;
	sp->giftag.EOP = 1;
	sp->giftag.NREG = 1;
	sp->giftag.REGS0 = 0xe; // GIF Packed A+D (Address+Data) GS Packet

	*(u64 *)&sp->bitbltbuf = GS_SET_BITBLTBUF(sbp, sbw, spsm, 0, 0, 0);
	sp->bitbltbufaddr = (u64)GS_BITBLTBUF;

	*(u64 *)&sp->trxpos = GS_SET_TRXPOS(0, 0, 0, 0, 0);
	sp->trxposaddr = (u64)GS_TRXPOS;

	*(u64 *)&sp->trxreg = GS_SET_TRXREG(width, height);
	sp->trxregaddr = (u64)GS_TRXREG;

	*(u64 *)&sp->finish = (u64) 0;
	sp->finishaddr = (u64)GS_FINISH;

	*(u64 *)&sp->trxdir = GS_SET_TRXDIR(1);
	sp->trxdiraddr = (u64)GS_TRXDIR;

	return 0;
}

_IGS_ENGINE_ int GsExecStoreImage(GsStoreImage *sp, u128 *buffer) {
	static	u32 ReinitPATH3Transfers[4] __attribute__((aligned(16))) = {0x06000000, 0x00000000, 0x00000000, 0x00000000 };

	u32 width;
	u32 height;
	u32 currwaittime = 0;
	u32 qwords = 0;
	u32 qwordslices = 0;
	u32 image_size = 0;
	u32 pixel_size = 0;
	u8 i;

	width = sp->trxreg.RRW;
	height = sp->trxreg.RRH;

	switch(sp->bitbltbuf.SPSM)
	{
		case GS_PSM_CT32:
		{
			pixel_size = 4;
			break;
		}
		case GS_PSM_CT24:
		{
			pixel_size = 3;
			break;
		}
		case GS_PSM_CT16:
		{
			pixel_size = 2;
			break;
		}
		case GS_PSM_CT16S:
		{
			pixel_size = 2;
			break;
		}
		default:
		{
			BlinkError(1);//return -1;
		}
	}

	image_size = width * height * pixel_size;

	//qwords per transfer - Take empirically
	//Example: SLES_554.74 (Persona 4) - 640x512 32bpp
	//640 x 512 x 4 = 1.310.720 / 16 = 81.920 = 0x14000 qwords needed
	//640 x 101 x 4 + 256 * 1 * 4 = 258.560 + 1.024 = 259.584 / 16 = 16.224 = 0x3F60 max qwords processed per time (stop after this)
	//qwordslices = 81.920 / 14336 = 5
	qwordslices = 1;
	qwords = (image_size>>4);
	if (qwords > 16224) {
		qwordslices = (qwords/14336) + 1;
		qwords = 14336;
	}
	//Disable Interrupts
	//__asm__ __volatile__(" di;");

	delay(2);GS_BGCOLOUR = 0xE5FFCC;	//Light Green
	//Init DMA Channel #1 (VIF1)
	D1_CHCR_Init();
	
	//Init DMA Channel #2 (GIF)
	delay(2);GS_BGCOLOUR = 0x00FF00;	//Green
	D2_CHCR_Init();
	
	delay(2);GS_BGCOLOUR = 0x006400;	//Dark Green
	// Wait DMA to be ready (STR=0)
	FlushCache(0);
	currwaittime = 0;
	while((*D1_CHCR) & 0x0100){
		// Delay for a little while.
		asm __volatile__( "nop;nop;nop;nop;nop;nop;nop;nop;sync.l;sync.p;" );
		if((currwaittime++) > MAXWAITTIME){
			// DMA Ch.1 does not terminate!
			BlinkError(2);//return -1;
		}
	}

	delay(2);GS_BGCOLOUR = 0xCCFFFF;	//Light Yellow
	// Setup and start DMA transfer to VIF1
	FlushCache(0);
	DPUT_D1_SIZE(7);
	DPUT_D1_MADR(((u32)sp & 0x0fffffff));
	DPUT_D1_CHCR((1 | (1<<8)));

/*
	delay(2);GS_BGCOLOUR = 0x00FFFF;	//Yellow
	// Wait DMA to be ready (STR=0)
	FlushCache(0);
	currwaittime = 0;
	while((*D1_CHCR) & 0x0100){
		// Delay for a little while.
		asm __volatile__( "nop;nop;nop;nop;nop;nop;nop;nop;sync.l;sync.p;" );
		if((currwaittime++) > MAXWAITTIME){
			// DMA Ch.1 does not terminate!
			BlinkError(3);//return -1;
		}
	}
*/
	
	delay(2);GS_BGCOLOUR = 0xCCE5FF;	//Light Orange
	FlushCache(0);
	// Change to the VIF1->Memory direction
	*VIF1_STAT = 0x00800000;
	// Change VIF1 FIF0 Local->Host direction
	*GS_BUSDIR = 1;
	
	for(i = 0; i < qwordslices;i ++) {

		delay(2);GS_BGCOLOUR = 0xFFCCFF;	//Light Magenta
		// Setup and start DMA transfer from VIF1
		FlushCache(0);
		DPUT_D1_SIZE(qwords);
		DPUT_D1_MADR(((u32)buffer & 0x0FFFFFFF));	// Ensure use of Cached RAM for safe writing
		DPUT_D1_CHCR(1<<8);

/*
		delay(2);GS_BGCOLOUR = 0xFF00FF;	//Magenta
		// Wait DMA to be ready (STR=0)
		FlushCache(0);
		currwaittime = 0;
		while((*D1_CHCR) & 0x0100){
			// Delay for a little while.
		asm __volatile__( "nop;nop;nop;nop;nop;nop;nop;nop;sync.l;sync.p;" );
			if((currwaittime++) > MAXWAITTIME){
				// DMA Ch.1 does not terminate!
				BlinkError(4);//return -1;
			}
		}
*/

/*
		delay(2);GS_BGCOLOUR = 0x0000FF;	//Red
		// Wait VIF1-FIFO to empty and become idle
		FlushCache(0);
		currwaittime = 0;
		while((*VIF1_STAT) & 0x1F000003){
			// Delay for a little while.
		asm __volatile__( "nop;nop;nop;nop;nop;nop;nop;nop;sync.l;sync.p;" );
			if((currwaittime++) > MAXWAITTIME){
				// DMA Ch.1 does not terminate!
				BlinkError(5);//return -1;
			}
		}
*/
		delay(2);GS_BGCOLOUR = 0x0008B;	//Dark Red
		buffer += qwords;

	}

	delay(2);GS_BGCOLOUR = 0xFFFFCC;	//Light Blue
	// Restore VIF1 and VIF1 FIFO to normal direction
	FlushCache(0);
	*VIF1_STAT = 0;
	*GS_BUSDIR = 0;

	delay(2);GS_BGCOLOUR = 0xFF0000;	//Blue
	// Reinit PATH3 Transfers
	FlushCache(0);
	DPUT_VIF1_FIFO(*(u128 *)ReinitPATH3Transfers);
	FlushCache(0);

	return 0;
}

_IGS_ENGINE_ int takeIGS(u16 sbp, u8 sbw, u32 height, u8 spsm, void *buffer) {

	u32 width;
	u32 pixel_size = 0;

	GsStoreImage gs_simage;
	
	switch(spsm)
	{
		case GS_PSM_CT32:
		{
			pixel_size = 4;
			break;
		}
		case GS_PSM_CT24:
		{
			pixel_size = 3;
			break;
		}
		case GS_PSM_CT16:
		{
			pixel_size = 2;
			break;
		}
		case GS_PSM_CT16S:
		{
			pixel_size = 2;
			break;
		}
		default:
		{
			BlinkError(6);//return -1;
		}
	}

	width = sbw * 64;

	if((width < 64) || (width > 1920))
		BlinkError(7);//return -1;

	if((height < 64) || (height > 1526))	//Sega Genesis Collection (SLUS_215.42) has DH = 1525!
		BlinkError(8);//return -1;

	// Transmit image from local buffer (GS VRAM) to host buffer (EE RAM)
	GsSetDefStoreImage(&gs_simage, sbp, sbw, spsm, width, height);
	GsExecStoreImage(&gs_simage, buffer);

	return 0;
}

_IGS_ENGINE_ int saveIGS_file(u16 sbp, u8 sbw, u32 height, u8 spsm, void *buffer, u8 DOUBLE_HEIGHT_adaptation) {
	//  0000000001111111111222222
	//  1234567890123456789012345
	// "mc1:/XXXX_yyy.zz_SCR.bmp"
	char PathFilenameExtension[64];

	u32 i;
	u8 swap_color;
	s32 file_handle;
	u32 address;
	u32 width;
	u32 pixel_size = 0;
	u32 image_size;
	u32 file_size;
	u32 bits_per_pixel;
	char id[2] = "BM";

	u8 *u8_buffer;
	u16 *u16_buffer;
	u32 *u32_buffer;

	u8_buffer = buffer;
	u16_buffer = buffer;
	u32_buffer = buffer;

	u32 size;
	void *addr;

	BmpHeader *bh;

	delay(2);GS_BGCOLOUR = 0xEE82EE;	//Violet
	switch(spsm)
	{
		case GS_PSM_CT32:
		{
			pixel_size = 4;
			break;
		}
		case GS_PSM_CT24:
		{
			pixel_size = 3;
			break;
		}
		case GS_PSM_CT16:
		{
			pixel_size = 2;
			break;
		}
		case GS_PSM_CT16S:
		{
			pixel_size = 2;
			break;
		}
		default:
		{
			BlinkError(9);//return -1;
		}
	}

	address = sbp * 64;
	width = sbw * 64;

	image_size = width * height * pixel_size;
	file_size = image_size + 54;
	bits_per_pixel = pixel_size * 8;

	delay(2);GS_BGCOLOUR = 0x800080;	//Purple
	switch(spsm)
	{
		case GS_PSM_CT32:
		{
 			//32-bit PS2 BGR to BMP RGB conversion 
			for(i = 0; i < (image_size/4); i++) {
				*(u32_buffer+i) = BGR2RGB32(*(u32_buffer+i));
			}
			break;
		}

		case GS_PSM_CT24:
		{
			//24-bit PS2 BGR to BMP RGB conversion 
			for(i = 0; i < image_size; i += 3) {
				swap_color = *(u8_buffer+i);
				*(u8_buffer+i) = *(u8_buffer+i+2);
				*(u8_buffer+i+2) = swap_color;
			}
			break;
		}
		case GS_PSM_CT16:
		{
			//16-bit PS2 BGR to BMP RGB conversion 
			for(i = 0; i < (image_size/2); i++) {
				*(u16_buffer+i) = BGR2RGB16(*(u16_buffer+i));
			}
			break;
		}
		case GS_PSM_CT16S:
		{
			//16-bit PS2 BGR to BMP RGB conversion 
			for(i = 0; i < (image_size/2); i++) {
				*(u16_buffer+i) = BGR2RGB16(*(u16_buffer+i));
			}
			break;
		}
		default:
		{
			BlinkError(10);//return -1;
		}
	}

	_strcpy(PathFilenameExtension, "mc1:/");
	_strcat(PathFilenameExtension, GameID);
	_strcat(PathFilenameExtension, "_SCR.bmp");
	
	delay(2);GS_BGCOLOUR = 0x000080;	//Maroon
	// Open file
	file_handle = fioOpen(PathFilenameExtension, O_CREAT|O_WRONLY);
	if(file_handle < 0)
			BlinkError(11);//return -1;

	delay(2);GS_BGCOLOUR = 0xC0C0C0;	//Silver
	// Write Bitmap Header
	//(first, write the BMP Header ID ouside of struct, due to alignment issues...)
	fioWrite(file_handle, id, 2);
	//(...then, write the remaining info!)
	bh = buffer + image_size;
	bh->filesize		= (u32)file_size;
	bh->reserved		= (u32)0;
	bh->headersize		= (u32)54;
	bh->infoSize		= (u32)40;
	bh->width			= (u32)width;
	bh->depth			= (u32)height;
	bh->biPlanes		= (u16)1;
	bh->bits			= (u16)bits_per_pixel;
	bh->biCompression	= (u32)0;
	bh->biSizeImage		= (u32)image_size;
	bh->biXPelsPerMeter	= (u32)0;
	bh->biYPelsPerMeter	= (u32)0;
	bh->biClrUsed		= (u32)0;
	bh->biClrImportant	= (u32)0;
	// Wait until the preceding loads are completed
	__asm__ __volatile__(" sync.l; sync.p;");
	
	delay(2);GS_BGCOLOUR = 0xFFFF00;	//Cyan
	fioWrite(file_handle, bh, 52);
	
	delay(2);GS_BGCOLOUR = 0xFF00FF;	//Magenta
	//Write image in reverse order (since BMP is written Left to Right, Bottom to Top)
	size = width * pixel_size;
	for(i = 0; i < height; i++) {
		addr = buffer + (height - i - 1) * width * pixel_size;
		fioWrite(file_handle, addr, size);
		if (DOUBLE_HEIGHT_adaptation ==2) fioWrite(file_handle, addr, size);
		GS_BGCOLOUR = (0xFFFFFF - i * 0x10101);
	}

	delay(2);GS_BGCOLOUR = 0x000000;	//Black
	// Close file
	fioClose(file_handle);

	return 0;
}

_IGS_ENGINE_ int save_gs_dump_file(void) {
	//  000000000111111111122222
	//  123456789012345678901234
	// "mc1:/XXXX_yyy.zz_GS.dmp"
	char PathFilenameExtension[24];
	s32 file_handle;

	_strcpy(PathFilenameExtension, "mc1:/");
	_strcat(PathFilenameExtension, GameID);
	_strcat(PathFilenameExtension, "_GS.dmp");

	delay(2);GS_BGCOLOUR = 0x00FFFF;	//Yellow
	// Open file
	file_handle = fioOpen(PathFilenameExtension, O_CREAT|O_WRONLY);
	if(file_handle < 0)
			BlinkError(12);//return -1;
        
	fioWrite(file_handle, (u8 *)&Source_VModeSettings, 12);

	fioWrite(file_handle, (u8 *)&Source_GSRegisterValues, 56);

	delay(2);GS_BGCOLOUR = 0xFFFF00;	//Cyan
	// Close file
	fioClose(file_handle);

	delay(2);GS_BGCOLOUR = 0x000000;	//Black

	return 0;
}

 _IGS_ENGINE_ int InGameScreenshot(void) {

	delay(2);GS_BGCOLOUR = 0x0000FF;	//Red
	delay(2);GS_BGCOLOUR = 0x00FF00;	//Green
	delay(2);GS_BGCOLOUR = 0xFF0000;	//Blue

	u64 pmode;
	u64 smode2;
	u64 dispfb1;
	u64 display1;
	u64 dispfb2;
	u64 display2;

	u32 fbp;
	u8 fbw;	// = sbw
	u8 psm;	// = spsm
	u16 sbp;
	u32 dh;
	u8 DOUBLE_HEIGHT_adaptation;
	
	int ret;
	
	// Init RPC & CMD
	SifInitRpc(0);

	// Apply Sbv patches
	sbv_patch_disable_prefix_check();

	LoadModule("rom0:SIO2MAN", 0, NULL);
	LoadModule("rom0:MCMAN", 0, NULL);
	LoadModule("rom0:MCSERV", 0, NULL);

#ifdef _DTL_T10000
	mcInit(MC_TYPE_XMC);
#else
	mcInit(MC_TYPE_MC);
#endif
	
	//Save GS source VModes/Registers Dump File (for debugging purposes)
	ret = save_gs_dump_file();

	pmode = Source_GSRegisterValues.pmode;
	smode2 = Source_GSRegisterValues.smode2;
	dispfb1 = Source_GSRegisterValues.dispfb1;
	display1 = Source_GSRegisterValues.display1;
	dispfb2 = Source_GSRegisterValues.dispfb2;
	display2 = Source_GSRegisterValues.display2;

	if((GET_PMODE_EN1(pmode) == 1) && (dispfb1 != 0) && (display1 != 0)){
		fbp = GET_DISPFB_FBP(dispfb1);
		fbw = GET_DISPFB_FBW(dispfb1);
		psm = GET_DISPFB_PSM(dispfb1);
		DOUBLE_HEIGHT_adaptation = (((smode2&3)==3)?2:1);
		dh = GET_DISPLAY_DH(display1)/DOUBLE_HEIGHT_adaptation;	// = (height - 1) / DOUBLE_HEIGHT_adaptation
	} else {	//	if((GET_PMODE_EN2(pmode) == 1) && (dispfb2 != 0) && (display2 != 0)){
		fbp = GET_DISPFB_FBP(dispfb2);
		fbw = GET_DISPFB_FBW(dispfb2);
		psm = GET_DISPFB_PSM(dispfb2);
		DOUBLE_HEIGHT_adaptation = (((smode2&3)==3)?2:1);
		dh = GET_DISPLAY_DH(display2)/DOUBLE_HEIGHT_adaptation;	// = (height - 1) / DOUBLE_HEIGHT_adaptation
	} 
	//Convert DISPFB.FBP to BITBLTBUF.SBP
	sbp = (fbp * 2048) / 64;

	//Take IGS
	ret = takeIGS( sbp, fbw, dh+1, psm, ((void *)0x00100000) );

	//Save IGS to file
	ret = saveIGS_file( sbp, fbw, dh+1, psm, ((void *)0x00100000), DOUBLE_HEIGHT_adaptation );

	// Exit services
	fioExit();
	LoadFileExit();
	SifExitIopHeap();
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	delay(2);GS_BGCOLOUR = 0x000000;	//Black

	return 0;
}

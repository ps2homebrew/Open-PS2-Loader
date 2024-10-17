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
 * along with PS2 IGS.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#include "include/igs_api.h"
#define NEWLIB_PORT_AWARE
#include "fileio.h"
#include "coreconfig.h"

static void FastDelay(int count)
{
    int i, ret;

    for (i = 0; i < count; i++) {
        ret = 0x0300;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }
}

static u32 FindEmptyArea(u32 start, u32 end, u32 emptysize)
{
    u128 *addr = (u128 *)start;
    u32 counter = 0;
    u32 result = 0;
    emptysize = (((emptysize + 16) >> 4) << 4); // It must be 16-bytes aligned

    while ((u32)addr < (end - emptysize)) {
        if (*(addr) == 0)
            counter += 16;
        else
            counter = 0;
        if (counter == emptysize)
            break;
        addr++;
    }

    if (counter == emptysize) {
        result = (u32)addr - emptysize;
        result = (((result) >> 4) << 4);                                // It must be 16-bytes aligned
        DBGCOL_BLNK(2, 0x00FF00, false, IGS, "FindEmptyArea(): Found"); // FOUND => Double Green :-)
    } else {
        // result = 0x00100000;                                // 1st. possible workaround: Use the ingame area (Typically starts on "0x00100000"). It must be 16-bytes aligned
        result = ((0x01F32568 - emptysize - 16) >> 4) << 4;                 // 2nd. possible workaround: Himem area ("0x01F32568" taken from  PS2LINK hi-mem).   It must be 16-bytes aligned
        DBGCOL_BLNK(2, 0x0066FF, false, IGS, "FindEmptyArea(): Not found"); // Double Orange :-(
    }

    return result;
}

static void ClearBuffer(u8 *buffer, u32 size)
{
    u32 i;
    for (i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

static void u8todecstr(u8 input, char *output, u8 digits)
{
    u8 i = digits; // Number of digits (output)
    do {
        i--;
        output[i] = "0123456789"[input % 10];
        input = input / 10;
    } while (input > 0);
    while (i > 0) {
        i--;
        output[i] = '0';
    }
    output[digits] = 0;
}

static void u16todecstr(u16 input, char *output, u8 digits)
{
    u8 i = digits; // Number of digits (output)
    do {
        i--;
        output[i] = "0123456789"[input % 10];
        input = input / 10;
    } while (input > 0);
    while (i > 0) {
        i--;
        output[i] = '0';
    }
    output[digits] = 0;
}

static void u32todecstr(u32 input, char *output, u8 digits)
{
    u8 i = digits; // Number of digits (output)
    do {
        i--;
        output[i] = "0123456789"[input % 10];
        input = input / 10;
    } while (input > 0);
    while (i > 0) {
        i--;
        output[i] = '0';
    }
    output[digits] = 0;
}

static void u32tohexstr(u32 input, char *output, u8 digits)
{
    u8 i = digits; // Number of digits (output)
    do {
        i--;
        output[i] = "0123456789ABCDEF"[input % 16];
        input = input / 16;
    } while (input > 0);
    while (i > 0) {
        i--;
        output[i] = '0';
    }
    output[digits] = 0;
}

static void u64tohexstr(u64 input, char *output, u8 digits)
{
    u8 i = digits; // Number of digits (output)
    do {
        i--;
        output[i] = "0123456789ABCDEF"[input % 16];
        input = input / 16;
    } while (input > 0);
    while (i > 0) {
        i--;
        output[i] = '0';
    }
    output[digits] = 0;
}

static u8 PixelSize(u8 spsm)
{
    u8 result = 0;
    if (spsm == GS_PSM_CT32)
        result = 4;
    else if (spsm == GS_PSM_CT24)
        result = 3;
    else if ((spsm == GS_PSM_CT16) || (spsm == GS_PSM_CT16S))
        result = 2;
    else
        DBGCOL_BLNK(1, 0x0000FF, true, IGS, "PixelSize(): Unknown?"); // Red
    return result;
}

static void Screenshot(u16 sbp, u8 sbw, u8 spsm, u16 width, u16 height, u32 dimensions, u8 pixel_size, u32 image_size, u128 *buffer)
{

    delay(1);
    DBGCOL(0xCCFFFF, IGS, "Screenshot() begins");

    static u128 EnableGIFPATH3;

    u32 *EnableGIFPATH3_32 = (u32 *)&EnableGIFPATH3;
    EnableGIFPATH3_32[0] = GS_VIF1_MSKPATH3(0);
    EnableGIFPATH3_32[1] = GS_VIF1_NOP;
    EnableGIFPATH3_32[2] = GS_VIF1_NOP;
    EnableGIFPATH3_32[3] = GS_VIF1_NOP;

    u32 DMAChain[20 * 2] ALIGNED(16);
    u32 *DMA32Packet = (u32 *)&DMAChain;
    u64 *DMA64Packet = (u64 *)(DMA32Packet + 4);
    u32 IMRState;
    u32 CHCRState;

    u32 qwords;
    u16 slices;
    u32 remainder;
    u8 i;

    if ((sbw < 1) || (sbw > 32))
        DBGCOL_BLNK(2, 0x0000FF, true, IGS, "Screenshot(): sbw out of range [1-32]"); // Red

    if ((height < 64) || (height > 1080))
        DBGCOL_BLNK(3, 0x0000FF, true, IGS, "Screenshot(): height out of range [64-1080]"); // Red

    // Number of qwords (each qword = 2^4 bytes = 16 bytes = 128 bits)
    qwords = image_size >> 4;

    u32 addr = (u32)buffer;

    // +--------------------------------------------------------------------+
    // | Transmit image from local buffer (GS VRAM) to host buffer (EE RAM) |
    // +--------------------------------------------------------------------+

    // Wait for VIF FIFO to become empty
    while ((*GS_VIF1_STAT & (0x1f000000)))
        ;

    // Ensure that the VIF and transfer paths are idle before starting the download process
    DMA32Packet[0] = GS_VIF1_NOP;
    DMA32Packet[1] = GS_VIF1_MSKPATH3(0x8000); // Disable transfer processing to the GIF via PATH3 after any current transfer
    DMA32Packet[2] = GS_VIF1_FLUSHA;           // This is the only flush code that will wait for a PATH3 transfer to complete
    DMA32Packet[3] = GS_VIF1_DIRECT(6);        // Transfer the following 6 qwords of data directly to the GIF

    // Setup the transmission parameters for the BLIT operation (copy of a pixels rectangular area)
    DMA64Packet[0] = GS_GIFTAG(5, 1, 0, 0, 0, 1); // GIFTAG(NLOOP, EOP, PRE, PRIM, FLG, NREG)
    DMA64Packet[1] = GS_GIF_AD;
    DMA64Packet[2] = GS_GSBITBLTBUF_SET(sbp, sbw, spsm, 0, 0, spsm);
    DMA64Packet[3] = GS_GSBITBLTBUF;
    DMA64Packet[4] = GS_GSTRXPOS_SET(0, 0, 0, 0, 0); // SSAX, SSAY, DSAX, DSAY, DIR
    DMA64Packet[5] = GS_GSTRXPOS;
    DMA64Packet[6] = GS_GSTRXREG_SET(width, height); // RRW, RRh
    DMA64Packet[7] = GS_GSTRXREG;
    DMA64Packet[8] = 0;
    DMA64Packet[9] = GS_GSFINISH;
    DMA64Packet[10] = GS_GSTRXDIR_SET(1); // XDIR
    DMA64Packet[11] = GS_GSTRXDIR;

    // Store current IMR status
    IMRState = GsPutIMR(GsGetIMR() | 0x0200);
    // Store current CHCR status
    CHCRState = *GS_D1_CHCR;

    // Wait for DMA to complete (STR = 0)
    while (*GS_D1_CHCR & 0x0100)
        ;

    // Set FINISH event
    *GS_CSR = GS_CSR_FINISH;

    // Ensure that all the data has indeed been written back to main memory
    FlushCache(GS_WRITEBACK_DCACHE);

    // Start DMA transfer
    *GS_D1_QWC = 0x7;
    *GS_D1_MADR = (u32)DMA32Packet;
    *GS_D1_CHCR = 0x101;
    // Ensure that store is complete and transfer has begun
    asm volatile("sync.l");

    // Wait DMA to complete (STR = 0)
    while (*GS_D1_CHCR & 0x0100)
        ;

    // Wait for the FINISH event (CSR.FINISH = 1)
    while ((*GS_CSR & GS_CSR_FINISH) == 0)
        ;

    // Wait for VIF FIFO to become empty
    while ((*GS_VIF1_STAT & (0x1f000000)))
        ;

    // Reverse VIF1-FIFO
    *GS_VIF1_STAT = GS_VIF1_STAT_FDR;

    // Reverse BUSDIR
    *GS_BUSDIR = (u64)0x00000001;

    // Number of slices: qwords / 8192
    // qwords per slice (They have been taken empirically. For some reason GS don't work if we download all qwords once).
    // slice size: 8192 = 2^13 qwords
    slices = qwords >> 13;

    // qwords = slices * (qwords per slice) + remainder => remainder = qwords - slices * (qwords per slice)
    remainder = 0;
    if (slices > 0)
        remainder = qwords - (slices << 13);

    i = 0;
    do {

        // Ensure that all the data has indeed been written back to main memory
        FlushCache(GS_WRITEBACK_DCACHE);

        // Transfer image to host
        *GS_D1_QWC = (8192 + ((i + 1 == slices) ? remainder : 0));
        *GS_D1_MADR = addr;
        *GS_D1_CHCR = 0x0100;

        addr += 8192 << 4;
        i++;

        // Ensure that store is complete and transfer has begun
        asm volatile("sync.l");

        // Wait for DMA to complete (STR = 0)
        while (*GS_D1_CHCR & 0x0100)
            ;

        delay(1);
        BGCOLND((0x00FFFF - (255 * i / slices) * 0x000101)); // Yellow Fade Out Effect

    } while (i < slices);


    // Ensure that all the data has indeed been written back to main memory
    FlushCache(GS_WRITEBACK_DCACHE);

    // Restore previous CHCR state
    *GS_D1_CHCR = CHCRState;
    // Ensure that store is complete and transfer has begun
    asm volatile("sync.l");
    *GS_VIF1_STAT = 0;

    // Restore previous BUSDIR state
    *GS_BUSDIR = (u64)0;

    // Restore IMR state
    IMRState = GsPutIMR(IMRState);

    // Set FINISH event
    *GS_CSR = GS_CSR_FINISH;

    // Renable PATH3
    *GS_VIF1_FIFO = EnableGIFPATH3;
}

static void ConvertColors32(u32 *buffer, u32 dimensions)
{
    u32 i;
    u32 x32;
    for (i = 0; i < dimensions; i++) {

        x32 = buffer[i];
        buffer[i] = ((x32 >> 16) & 0xFF) | ((x32 << 16) & 0xFF0000) | (x32 & 0xFF00FF00);

        FastDelay(1);
        BGCOLND((0x0000FF - (255 * (i + 1) / dimensions) * 0x000001)); // Red Fade Out Effect
    }
}

static void ConvertColors24(u8 *buffer, u32 image_size)
{
    u32 i;
    u32 x32 __attribute__((aligned(16)));
    for (i = 0; i < image_size; i += 3) {

        x32 = (((u32)(buffer[i])) << 16) | (((u32)(buffer[i + 1])) << 8) | (((u32)(buffer[i + 2])) << 0);
        buffer[i] = (u8)((x32 >> 0) & 0xFF);
        buffer[i + 1] = (u8)((x32 >> 8) & 0xFF);
        buffer[i + 2] = (u8)((x32 >> 16) & 0xFF);

        FastDelay(1);
        BGCOLND((0x00FF00 - (255 * (i + 1) / image_size) * 0x000100)); // Green Fade Out Effect
    }
}

static void ConvertColors16(u16 *buffer, u32 dimensions)
{
    u32 i;
    u16 x16;
    for (i = 0; i < dimensions; i++) {

        x16 = buffer[i];
        buffer[i] = (x16 & 0x8000) | ((x16 << 10) & 0x7C00) | (x16 & 0x3E0) | ((x16 >> 10) & 0x1F);

        FastDelay(1);
        BGCOLND((0xFF0000 - (255 * (i + 1) / dimensions) * 0x010000)); // Blue Fade Out Effect
    }
}

static void SaveTextFile(u32 buffer, u16 width, u16 height, u8 pixel_size, u32 image_size, u8 Number)
{
    USE_LOCAL_EECORE_CONFIG;

    delay(1);
    DBGCOL(0x0099FF, IGS, "SaveTextFile() begins");

    //  0000000001111111111222222222
    //  1234567890123456789012345678
    // "mc1:/XXXX_yyy.zz_GS(nnn).txt"
    char PathFilenameExtension[64];
    s32 file_handle;

    char text[1024];
    char u64text[20 + 1];
    char u32text[10 + 1];
    char u16text[6 + 1];
    char u8text[3 + 1];

    u32 i = 0;
    u64 pmode = GSMSourceGSRegs.pmode;
    u64 smode2 = GSMSourceGSRegs.smode2;
    u64 dispfb1 = GSMSourceGSRegs.dispfb1;
    u64 display1 = GSMSourceGSRegs.display1;
    u64 dispfb2 = GSMSourceGSRegs.dispfb2;
    u64 display2 = GSMSourceGSRegs.display2;

    // Sequential number, inherited from Bitmap File
    _strcpy(PathFilenameExtension, "mc1:/");
    _strcat(PathFilenameExtension, config->GameID);
    _strcat(PathFilenameExtension, "_GS(");
    u8todecstr(Number, u8text, 3);
    _strcat(PathFilenameExtension, u8text);
    _strcat(PathFilenameExtension, ").txt");

    // Create file
    file_handle = fioOpen(PathFilenameExtension, O_CREAT | O_WRONLY);
    if (file_handle < 0)
        DBGCOL_BLNK(4, 0x0000FF, true, IGS, "SaveTextFile(): FILEIO Open Error"); // Red

    _strcpy(text, "PS2 IGS (InGame Screenshot)\n---------------------------\n");
    _strcat(text, "\n\nGame ID=");
    _strcat(text, config->GameID);
    _strcat(text, "\nResolution=");
    u16todecstr(width, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "x");
    u16todecstr(height, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, ", Pixel Size=");
    u8todecstr(pixel_size, u8text, 3);
    _strcat(text, u8text);
    _strcat(text, ", Image Size=");
    u32todecstr(image_size, u32text, 10);
    _strcat(text, u32text);
    _strcat(text, "\n\nGS Registers\n------------");
    _strcat(text, "\nPMODE    0x");
    u64tohexstr(pmode, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " ALP=");
    u16todecstr((pmode >> 8) & 0xFF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " SLBG=");
    u16todecstr((pmode >> 7) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " AMOD=");
    u16todecstr((pmode >> 6) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " MMOD=");
    u16todecstr((pmode >> 5) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " CRTMD=");
    u16todecstr((pmode >> 2) & 0x7, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " EN2=");
    u16todecstr((pmode >> 1) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " EN1=");
    u16todecstr((pmode >> 0) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "\nSMODE2   0x");
    u64tohexstr(smode2, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " INT=");
    u16todecstr((smode2 >> 0) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " FFMD=");
    u16todecstr((smode2 >> 1) & 0x1, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DPMS=");
    u16todecstr((smode2 >> 2) & 0x3, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "\nDISPFB1  0x");
    u64tohexstr(dispfb1, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " DBY=");
    u16todecstr((dispfb1 >> 43) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DBX=");
    u16todecstr((dispfb1 >> 32) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " PSM=");
    u16todecstr((dispfb1 >> 15) & 0x1F, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " FBW=");
    u16todecstr((dispfb1 >> 9) & 0x3F, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " FBP=");
    u16todecstr((dispfb1 >> 0) & 0x1FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "  DISPLAY1 0x");
    u64tohexstr(display1, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " DH=");
    u16todecstr((display1 >> 44) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DW=");
    u16todecstr((display1 >> 32) & 0xFFF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " MAGV=");
    u16todecstr((display1 >> 27) & 0x3, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " MAGH=");
    u16todecstr((display1 >> 23) & 0xF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DY=");
    u16todecstr((display1 >> 12) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DX=");
    u16todecstr((display1 >> 0) & 0xFFF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "\nDISPFB2  0x");
    u64tohexstr(dispfb2, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " DBY=");
    u16todecstr((dispfb2 >> 43) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DBX=");
    u16todecstr((dispfb2 >> 32) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " PSM=");
    u16todecstr((dispfb2 >> 15) & 0x1F, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " FBW=");
    u16todecstr((dispfb2 >> 9) & 0x3F, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " FBP=");
    u16todecstr((dispfb2 >> 0) & 0x1FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "  DISPLAY2 0x");
    u64tohexstr(display2, u64text, 16);
    _strcat(text, u64text);
    _strcat(text, " DH=");
    u16todecstr((display2 >> 44) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DW=");
    u16todecstr((display2 >> 32) & 0xFFF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " MAGV=");
    u16todecstr((display2 >> 27) & 0x3, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " MAGH=");
    u16todecstr((display2 >> 23) & 0xF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DY=");
    u16todecstr((display2 >> 12) & 0x7FF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, " DX=");
    u16todecstr((display2 >> 0) & 0xFFF, u16text, 5);
    _strcat(text, u16text);
    _strcat(text, "\n\nHost buffer (EE RAM) address=0x");
    u32tohexstr(buffer, u32text, 8);
    _strcat(text, u32text);

    while (text[i] != '\0') {
        fioPutc(file_handle, text[i++]);
    }

    // Close file
    fioClose(file_handle);

    delay(1);
    BGCOLND(0x000000); // Black
}

static u8 SaveBitmapFile(u16 width, u16 height, u8 pixel_size, void *buffer, u8 intffmd)
{
    USE_LOCAL_EECORE_CONFIG;

    delay(1);
    BGCOLND(0x990066); // Purple Violet

    //  00000000011111111112222222222
    //  12345678901234567890123456789
    // "mc1:/XXXX_yyy.zz_IGS(nnn).bmp"
    char PathFilenameExtension[64];

    u32 i;
    s32 file_handle;
    int ret;
    u32 dimensions;
    u16 lenght;
    u32 image_size;
    u32 file_size;
    u8 bpp;
    char id[2] = "BM";

    void *addr;

    char u8text[3 + 1];

    BmpHeader *bh;

    u8 Number = 0; // 255 screenshots per-game should be enough? lol

    bpp = (pixel_size << 3);

    lenght = width * pixel_size;
    dimensions = width * height;
    image_size = dimensions * pixel_size;
    file_size = image_size + 54;

    // Sequential numbering feature
    while (1) {
        if (Number == 255)                                                          // 255 screenshots per-game should be enough? lol
            DBGCOL_BLNK(6, 0x0000FF, true, IGS, "SaveBitmapFile(): Number == 255"); // Red
        Number++;
        _strcpy(PathFilenameExtension, "mc1:/");
        _strcat(PathFilenameExtension, config->GameID);
        _strcat(PathFilenameExtension, "_GS(");
        u8todecstr(Number, u8text, 3);
        _strcat(PathFilenameExtension, u8text);
        _strcat(PathFilenameExtension, ").bmp");
        file_handle = fioOpen(PathFilenameExtension, O_RDONLY);
        if (file_handle < 0) // There is no file with this Sequential Number; that's time for creating a new one.
            break;
        if (fioLseek(file_handle, 0, SEEK_END) == 0) { // There is a file with this Sequential Number but it's empty, so let's overwrite it.
            fioClose(file_handle);
            break;
        }
        fioClose(file_handle);
    }
    // Create file
    file_handle = fioOpen(PathFilenameExtension, O_CREAT | O_WRONLY);
    if (file_handle < 0)
        DBGCOL_BLNK(4, 0x0000FF, true, IGS, "SaveBitmapFile(): FILEIO error"); // Red

    // Write Bitmap Header
    //(first, write the BMP Header ID ouside of struct, due to alignment issues...)
    ret = fioWrite(file_handle, id, 2);
    if (ret != 2)
        DBGCOL_BLNK(5, 0x0000FF, true, IGS, "SaveBitmapFile(): FILEIO write err"); // Red
    //(...then, write the remaining info!)
    bh = (void *)((u8 *)buffer + image_size);
    bh->filesize = (u32)file_size;
    bh->reserved = (u32)0;
    bh->headersize = (u32)54;
    bh->infoSize = (u32)40;
    bh->width = (u32)width;
    bh->depth = (u32)height;
    bh->biPlanes = (u16)1;
    bh->bits = (u16)bpp;
    bh->biCompression = (u32)0;
    bh->biSizeImage = (u32)image_size;
    bh->biXPelsPerMeter = (u32)0;
    bh->biYPelsPerMeter = (u32)0;
    bh->biClrUsed = (u32)0;
    bh->biClrImportant = (u32)0;
    // Wait until the preceding loads are completed
    asm volatile("sync.l;sync.p;");

    ret = fioWrite(file_handle, bh, 52);
    if (ret != 52)
        DBGCOL_BLNK(5, 0x0000FF, true, IGS, "SaveBitmapFile(): FILEIO write error"); // Red

    // Write image in reverse order (since BMP is written Left to Right, Bottom to Top)
    if (intffmd == 3) // Interlace Mode, FRAME Mode (Read every line)
        height >>= 1;
    for (i = 1; i <= height; i++) {
        addr = (void *)((u8 *)buffer + ((height - i) * lenght));

        // Ensure that all the data has indeed been written back to main memory
        FlushCache(GS_WRITEBACK_DCACHE);

        ret = fioWrite(file_handle, addr, lenght);
        if (ret != lenght)
            DBGCOL_BLNK(5, 0x0000FF, true, IGS, "SaveBitmapFile(): FILEIO write error"); // Red
        if (intffmd == 3) {                                                              // Interlace Mode, FRAME Mode (Read every line)
            ret = fioWrite(file_handle, addr, lenght);
            if (ret != lenght)
                DBGCOL_BLNK(5, 0x0000FF, true, IGS, "SaveBitmapFile(): FILEIO write error"); // Red
        }
        BGCOLND((0xFFFFFF - (255 * i / height) * 0x010101)); // Gray Fade Out Effect
    }

    // Close file
    fioClose(file_handle);

    delay(1);
    BGCOLND(0x000000); // Black

    // Return the Sequential Number to the related Text File
    return Number;
}

int InGameScreenshot(void)
{

    DI();

    u64 pmode;
    u64 smode2;
    u64 dispfb1;
    u64 display1;
    u64 dispfb2;
    u64 display2;

    u16 sbp;
    u8 sbw;
    u8 spsm;
    u8 intffmd;

    u16 width;
    u16 height;
    u32 dimensions;
    u8 pixel_size;
    u32 image_size;

    void *buffer = NULL;
    u32 *buffer32;
    u8 *buffer8;
    u16 *buffer16;

    u8 Number;

    pmode = GSMSourceGSRegs.pmode;
    smode2 = GSMSourceGSRegs.smode2;
    dispfb1 = GSMSourceGSRegs.dispfb1;
    display1 = GSMSourceGSRegs.display1;
    dispfb2 = GSMSourceGSRegs.dispfb2;
    display2 = GSMSourceGSRegs.display2;

    if (GET_PMODE_EN2(pmode)) {
        sbp = (GET_DISPFB_FBP(dispfb2) << 5);    // BITBLTBUF.SBP*64 = DISPFB1.FBP*2048 <-> BITBLTBUF.SBP = DISPFB2.FBP*2048/64 <-> BITBLTBUF.SBP = DISPFB2.FBP*32
        sbw = GET_DISPFB_FBW(dispfb2);           // BITBLTBUF.SBW = DISPFB2.FBW
        spsm = GET_DISPFB_PSM(dispfb2);          // BITBLTBUF.SPSM = DISPFB2.PSM
        height = (GET_DISPLAY_DH(display2) + 1); // height = DH+1
    } else {
        sbp = (GET_DISPFB_FBP(dispfb1) << 5);    // BITBLTBUF.SBP*64 = DISPFB1.FBP*2048 <-> BITBLTBUF.SBP = DISPFB1.FBP*2048/64 <-> BITBLTBUF.SBP = DISPFB1.FBP*32
        sbw = GET_DISPFB_FBW(dispfb1);           // BITBLTBUF.SBW = DISPFB1.FBW
        spsm = GET_DISPFB_PSM(dispfb1);          // BITBLTBUF.SPSM = DISPFB1.PSM
        height = (GET_DISPLAY_DH(display1) + 1); // height = DH+1
    }

    width = (u16)sbw << 6;
    dimensions = width * height;
    pixel_size = PixelSize(spsm);
    image_size = dimensions * pixel_size;

    // Try to find a empty (zeroed) area in EE RAM
    buffer = (void *)FindEmptyArea(0x00100000, 0x02000000, image_size + 52); // image size + Bitmap Header (52 bytes)

    // Take IGS
    Screenshot(sbp, sbw, spsm, width, height, dimensions, pixel_size, image_size, buffer);

    // Color Space conversion from BGR to RGB (i.e. little endian => big endian)
    switch (spsm) {
        case GS_PSM_CT32:
            buffer32 = (u32 *)UNCACHED_SEG(buffer);
            ConvertColors32(buffer32, dimensions);
            break; // 32-bit PS2 BGR  (PSMCT32  =  0, pixel_size=4) to BMP RGB conversion
        case GS_PSM_CT24:
            buffer8 = (u8 *)UNCACHED_SEG(buffer);
            ConvertColors24(buffer8, image_size);
            break; // 24-bit PS2 BGR  (PSMCT24  =  1, pixel_size=3) to BMP RGB conversion
        case GS_PSM_CT16:
            buffer16 = (u16 *)UNCACHED_SEG(buffer);
            ConvertColors16(buffer16, dimensions);
            break; // 16-bit PS2 BGR  (PSMCT16  =  2, pixel_size=2) to BMP RGB conversion
        case GS_PSM_CT16S:
            buffer16 = (u16 *)UNCACHED_SEG(buffer);
            ConvertColors16(buffer16, dimensions);
            break; // 16-bit PS2 BGRA (PSMCT16S = 10, pixel_size=2) to BMP RGB conversion
    }

    EI();

    delay(1);
    BGCOLND(0x660033); // Midnight Blue

    // Load modules.
    LoadFileInit();
    LoadModule("rom0:SIO2MAN", 0, NULL);
    LoadModule("rom0:MCMAN", 0, NULL);

    // Save IGS Bitmap File first, since it's the bigger file)
    intffmd = GET_SMODE2_INTFFMD(smode2);
    Number = SaveBitmapFile(width, height, pixel_size, buffer, intffmd);

    // The Number used on Bitmap File is returned to the related Text File, in order their both Sequential Numbers match
    SaveTextFile((u32)buffer, width, height, pixel_size, image_size, Number);

    // Clear buffer
    ClearBuffer(buffer, image_size + 52);

    // Exit services
    fioExit();
    LoadFileExit();
    SifExitRpc();
    FlushCache(0);
    FlushCache(2);

    DBGCOL_BLNK(1, 0xFF0000, false, IGS, "InGameScreenshot() end"); // Double Blue :-)

    return 0;
}

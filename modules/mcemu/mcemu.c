/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include "mcemu.h"

static int readyToGo = -1;
void StartNow(void *param);
#ifdef PADEMU
void no_pademu(Sio2Packet *sd, Sio2McProc sio2proc)
{
    sio2proc(sd);
}
void (*pademu_hookSio2man)(Sio2Packet *sd, Sio2McProc sio2proc) = no_pademu;
#endif

//---------------------------------------------------------------------------
// Shutdown callback
//---------------------------------------------------------------------------
static void mcemuShutdown(void)
{ // If necessary, implement some locking mechanism to prevent further requests from being made.
    DeviceShutdown();
}

//---------------------------------------------------------------------------
// Entry point
//---------------------------------------------------------------------------
int _start(int argc, char *argv[])
{
    int thid;
    iop_thread_t param;

    oplRegisterShutdownCallback(&mcemuShutdown);

    param.attr = TH_C;
    param.thread = StartNow;
    param.priority = 0x4f;
    param.stacksize = 0xB00;
    param.option = 0;
    thid = CreateThread(&param);
    StartThread(thid, 0);
    while (!((readyToGo == 1) || (readyToGo == 0))) {
        DelayThread(100 * 1000);
    }
    DeleteThread(thid);
    return readyToGo;
}
//------------------------------
// endfunc _start
//---------------------------------------------------------------------------
void StartNow(void *param)
{
    register int r;
    register void *exp;

    DPRINTF("mcemu starting\n");

    /* looking for LOADCORE's library entry table */
    exp = GetExportTable("loadcore", 0x100);
    if (exp == NULL) {
        DPRINTF("Unable to find loadcore exports.\n");
        readyToGo = MODULE_NO_RESIDENT_END;
        return;
    }

    /* configuring the virtual memory cards */
    if (!(r = mc_configure(memcards))) {
        (void)r;
        DPRINTF("mc_configure return %d.\n", r);
        readyToGo = MODULE_NO_RESIDENT_END;
        return;
    }

    /* hooking LOADCORE's RegisterLibraryEntires routine */
    pRegisterLibraryEntires = (PtrRegisterLibraryEntires)HookExportEntry(exp, 6, hookRegisterLibraryEntires);

    /* searching for a SIO2MAN export table */
    exp = GetExportTable("sio2man", 0x201);
    if (exp != NULL) {
        /* hooking SIO2MAN's routines */
        InstallSio2manHook(exp, 1);
    } else {
        DPRINTF("SIO2MAN exports not found.\n");
    }

    /* searching for a SECRMAN export table */
    exp = GetExportTable("secrman", 0x101);
    if (exp != NULL) {
        /* hooking SecrAuthCard entry */
        InstallSecrmanHook(exp);
    } else {
        DPRINTF("SECRMAN exports not found.\n");
    }

    readyToGo = MODULE_RESIDENT_END;
}
//------------------------------
// endfunc _start
//---------------------------------------------------------------------------
/* Installs SecrAuthCard handler for the enabled virtual memory cards */
void InstallSecrmanHook(void *exp)
{
    register int i;
    register void *old;
    register MemoryCard *mcds;

    /* hooking SecrAuthCard entry */
    old = HookExportEntry(exp, 6, hookSecrAuthCard);
    if (old == NULL)
        old = DummySecrAuthCard;

    for (i = 0, mcds = memcards; i < MCEMU_PORTS; i++, mcds++) {
        pSecrAuthCard[i] = (mcds->mcnum != -1) ? DummySecrAuthCard : (PtrSecrAuthCard)old;
    }
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Installs handlers for SIO2MAN's routine for enabled virtual memory cards */
void InstallSio2manHook(void *exp, int ver)
{
    psio2_mc_transfer_init = GetExportEntry(exp, 24);
    psio2_transfer_reset = GetExportEntry(exp, 26);

    /* hooking SIO2MAN entry #25 (used by MCMAN and old PADMAN) */
    pSio2man25 = HookExportEntry(exp, 25, hookSio2man25);
    /* hooking SIO2MAN entry #51 (used by MC2_* modules and PADMAN) */
    pSio2man51 = HookExportEntry(exp, 49 + (ver * 2), hookSio2man51);
    pSio2man67 = HookExportEntry(exp, 67, hookSio2man67);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Install hooks for MCMAN's sceMcReadFast & sceMcWriteFast */
void InstallMcmanHook(void *exp)
{
    register void *mcman63, *mcman68;

    /* getting MCMAN's sceMcRead & sceMcWrite routines */
    pMcRead = GetExportEntry(exp, 8);
    pMcWrite = GetExportEntry(exp, 9);
    /* hooking MCMAN's library entry #62 */
    HookExportEntry(exp, 62, hookMcman62);
    /* and choosing internal routines for sceMcReadFast & sceMcWriteFast */
    mcman63 = hookMcman63;
    mcman68 = hookMcman68;

    /* hooking sceMcReadFast entry */
    HookExportEntry(exp, 63, mcman63);
    /* hooking sceMcWriteFast entry */
    HookExportEntry(exp, 68, mcman68);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
int hookDmac_request(u32 channel, void *addr, u32 size, u32 count, int dir);
void hookDmac_transfer(u32 channel);

static int CheckPatchMc2_s1()
{
    modinfo_t info;
    ModuleInfo_t *mod_table = GetLoadcoreInternalData()->image_info;
    const char modname[8] = "mc2_s1";
    u32 start, end;

    getModInfo("modload\0", &info);
    if (info.version < 0x102)
        goto not_found; // I'm sure mc2_s1 won't work with this old iop kernel

    if (getModInfo("mcman", &info))
        goto not_found; // mcman and mc2_s1 are mutually exclusive

    while (mod_table) {
        int i;
        for (i = 0; i < 8; i++) {
            if (mod_table->name[i] != modname[i])
                break;
        }
        if (i == 8)
            break;
        mod_table = mod_table->next;
    }
    if (!mod_table)
        goto not_found_again; // mc2_s1 not found
    start = mod_table->text_start;
    end = start + mod_table->text_size + mod_table->data_size + mod_table->bss_size;

    getModInfo("dmacman\0", &info);

    // walk the import tables
    iop_library_t *lib = (iop_library_t *)((u32)info.exports - 0x14);
    struct irx_import_table *table = lib->caller;
    struct irx_import_stub *stub;
    while (table) {
        stub = (struct irx_import_stub *)table->stubs;
        if (((u32)stub > start) && ((u32)stub < end)) {
            DPRINTF("bad mc2_s1 found\n");
            while (stub->jump) {
                switch (stub->fno) {
                    case 0x1c: // dmac_request
                        stub->jump = 0x08000000 | (((u32)hookDmac_request << 4) >> 6);
                        break;
                    case 0x20: // dmac_transfer
                        stub->jump = 0x08000000 | (((u32)hookDmac_transfer << 4) >> 6);
                        break;
                    default:
                        break;
                }
                stub++;
            }
            FlushDcache();
            FlushIcache();
            return 1;
        }
        table = table->next;
    }
    DPRINTF("mc2_s1 found but no dmac imports\n");
    return 1;
not_found:
    DPRINTF("mc2_s1 not found\n");
    return 1;
not_found_again:
    DPRINTF("mc2_s1 and mcman not found, try again\n");
    return 0;
}

/* Returns "success" result to any SecrAuthCard calls to avoid real Magic Gate Authentication process */
int DummySecrAuthCard(int port, int slot, int cnum)
{
    // port; slot; cnum;
    DPRINTF("SecrAuthCard(0x%X, 0x%X, 0x%X)\n", port, slot, cnum);

    /*
    if (slot != 0)
        return 0;
    */
    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Hook for the LOADCORE's RegisterLibraryEntires call */
int hookRegisterLibraryEntires(iop_library_t *lib)
{
    register int ret;

    if (!strcmp(lib->name, "sio2man")) {
        ret = pRegisterLibraryEntires(lib);
        if (ret == 0) {
            ReleaseLibraryEntries((struct irx_export_table *)lib);
            /* hooking SIO2MAN's routines */
            InstallSio2manHook(&lib[1], GetExportTableSize(&lib[1]) >= 61);
        } else {
            DPRINTF("registering library %s failed, error %d\n", lib->name, ret);
            return ret;
        }
    } else if (!strcmp(lib->name, "secrman")) {
        ret = pRegisterLibraryEntires(lib);
        if (ret == 0) {
            ReleaseLibraryEntries((struct irx_export_table *)lib);
            /* hooking the SecrAuthCard() calls */
            InstallSecrmanHook(&lib[1]);
        } else {
            DPRINTF("registering library %s failed, error %d\n", lib->name, ret);
            return ret;
        }
    } else if (!strcmp(lib->name, "mcman")) {
        ret = pRegisterLibraryEntires(lib);
        if (ret == 0) {
            ReleaseLibraryEntries((struct irx_export_table *)lib);
            /* hooking MCMAN's sceMcReadFast() & sceMcWriteFast() calls */
            if (lib->version >= 0x208)
                InstallMcmanHook(&lib[1]);
        } else {
            DPRINTF("registering library %s failed, error %d\n", lib->name, ret);
            return ret;
        }
#ifdef PADEMU
    } else if (!strcmp(lib->name, "pademu")) {
        pademu_hookSio2man = GetExportEntry(&lib[1], 4);
#endif
    }

    DPRINTF("registering library %s\n", lib->name);

    return pRegisterLibraryEntires(lib);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Hook for SIO2MAN entry #25 (called by MCMAN) */
void hookSio2man25(Sio2Packet *sd)
{
    hookSio2man(sd, pSio2man25);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Hook for SIO2MAN entry #51 (called by MC2* and PADMAN) */
void hookSio2man51(Sio2Packet *sd)
{
    hookSio2man(sd, pSio2man51);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* SIO2MAN generic hook routine */
void hookSio2man(Sio2Packet *sd, Sio2McProc sio2proc)
{
    register u32 ctrl;

    /* getting first SIO2 control code */
    ctrl = sd->ctrl[0];

    /* checking if virtual memory card is active */
    if (memcards[ctrl & 0x1].mcnum != -1) {
        /* checking SIO2 transfer mode */
        switch (ctrl & 0xF0) {
            /* getting command code from first DMA buffer */
            case 0x70:
                ctrl = *(u8 *)sd->wrmaddr;
                break;
            /* getting command code from PIO buffer */
            case 0x40:
                ctrl = *(u8 *)sd->pwr_buf;
                break;
            /* unknown transfer mode, setting wrong command code */
            default:
                ctrl = 0;
                break;
        }

        /* checking SIO2 command code */
        if (ctrl == 0x81)
            sio2proc = Sio2McEmu;
    }

/* calling original SIO2MAN routine */
#ifdef PADEMU
    pademu_hookSio2man(sd, sio2proc);
#else
    sio2proc(sd);
#endif
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Hook for the SecrAuthCard call */
int hookSecrAuthCard(int port, int slot, int cnum)
{
    static int check_done = 0;
    // check for mc2_s1 once, secrauthcard should be called only after mc(2)man is loaded (I hope)
    // check_done may never bet set for those games which use only mc2_d like GH3, but other games
    // like GOW2 use both.
    if (!check_done)
        check_done = CheckPatchMc2_s1();
    return pSecrAuthCard[port & 0x1](port, slot, cnum);
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------

#define SIO2CTRL (*(volatile u32 *)(0xbf808268))
#define SIO2STAT (*(volatile u32 *)(0xbf80826c))
#define SIO2CMD  ((volatile u32 *)(0xbf808200))
static Sio2Packet *temp_packet = NULL;
static int skip_sema_wait = 0;

int hookDmac_request(u32 channel, void *addr, u32 size, u32 count, int dir)
{
    int i;
    int port = SIO2CMD[0] & 1;

    DPRINTF("hookDmac_request port = %d, channel = %ld\n", port, channel);
    if (memcards[port].mcnum == -1)
        return dmac_request(channel, addr, size, count, dir);

    switch (channel) {
        case 11: // sio2in
            // may have to copy the dma buffer, but isn't a problem right now
            temp_packet = _SysAlloc(sizeof(Sio2Packet));
            temp_packet->wrmaddr = addr;
            temp_packet->wrwords = size;
            temp_packet->wrcount = count;
            return 1;
        case 12: // sio2out
            if (temp_packet == NULL) {
                DPRINTF("sio2out request without sio2in\n");
                return 0;
            }
            temp_packet->rdmaddr = addr;
            temp_packet->rdwords = size;
            temp_packet->rdcount = count;
            for (i = 0; i < 16; i++)
                temp_packet->ctrl[i] = SIO2CMD[i];
            DPRINTF("hookDmac_request ctrl0 = %lX, cmd0 = %X, wrcount = %ld\n", temp_packet->ctrl[0], ((u8 *)temp_packet->wrmaddr)[1], temp_packet->wrcount);
            Sio2McEmu(temp_packet);
            SIO2CTRL |= 0x40; // reset it, pcsx2 suggests it's reset after every write
            _SysFree(temp_packet);
            temp_packet = NULL;
            skip_sema_wait = 1;
            return 1;
        default:
            DPRINTF("dmac_request invalid channel\n");
            return 0;
    }
}

void hookDmac_transfer(u32 channel)
{
    int port = SIO2CMD[0] & 1;
    if (memcards[port].mcnum == -1)
        return dmac_transfer(channel);

    switch (channel) {
        case 12:
            break;
        case 11:
            if (temp_packet != NULL)
                break;
        default:
            DPRINTF("dmac_transfer invalid transfer\n");
            break;
    }
    return;
}

u32 *hookSio2man67()
{
    static u32 fake_stat;
    if (skip_sema_wait) {
        if ((SIO2STAT & 0xf000) != 0x1000) { // uh oh
            u32 *ra;
            __asm__("move %0, $ra\n"
                    : "=r"(ra));
            if (ra[0]) {
                if ((ra[0] == 0x3c02bf80) && (ra[1] == 0x3442826c)) { // this really sucks
                    ra[0] = 0;
                    ra[1] = 0;

                } else if ((ra[0] == 0x8fa90050) && (ra[1] == 0) && (ra[2] == 0x8d220000)) {
                    ra[0] = 0;          // there must a better way
                    ra[2] = 0x8c420000; // lw v0,0(v0)
                } else
                    DPRINTF("hookSio2man67 failed to find stat6c check, mc access may fail\n");
                FlushDcache();
                FlushIcache();
            }
        }
        skip_sema_wait = 0;
        fake_stat = 0x1000;
        return (&fake_stat);
    }
    pSio2man67();
    fake_stat = SIO2STAT;
    return (&fake_stat);
}

/* SIO2 Command handler */
void Sio2McEmu(Sio2Packet *sd)
{
    /*
     * Unlock SIO2 access for MX4SIO
     * NOTE: we are assuming MC's are only accessed when LOCKED
     */
    psio2_transfer_reset();

    if ((sd->ctrl[0] & 0xF0) == 0x70) {
        register u32 ddi, *pctl, result, length;
        register u8 *wdma, *rdma;
        register MemoryCard *mcd;

        wdma = (u8 *)sd->wrmaddr; /* address of write buffers */
        rdma = (u8 *)sd->rdmaddr; /* address of read buffers */
        pctl = &sd->ctrl[0];      /* address of control codes */

        for (result = 1, ddi = 0; (ddi < sd->wrcount) && (ddi < 0x10) && (*pctl); ddi++, pctl++) {
            if (wdma[0] == 0x81) {
                mcd = &memcards[*pctl & 0x1];
                length = (*pctl >> 18) & 0x1FF;

                if (mcd->mcnum != -1) {
                    /* handling MC2 command code */
                    switch (wdma[1]) {
                        /* 0x81 0x12 - ? */
                        case 0x12: /* <- I/O status related */
                            SioResponse(mcd, rdma, length);
                            result = 1;
                            break;
                        /* 0x81 0x21 - Start erasing block of pages */
                        case 0x21: /* erase block */
                            SioResponse(mcd, rdma, length);
                            result = MceEraseBlock(mcd, GetInt(&wdma[2]));
                            break;
                        /* 0x81 0x22 - Start writing to a memory card */
                        case 0x22: /* write start */
                            result = MceStartWrite(mcd, GetInt(&wdma[2]));
                            SioResponse(mcd, rdma, length);
                            break;
                        /* 0x81 0x23 - Start reading from a memory card */
                        case 0x23: /* read start */
                            result = MceStartRead(mcd, GetInt(&wdma[2]));
                            SioResponse(mcd, rdma, length);
                            break;
                        /* 0x81 0x26 - Read memory card spec */
                        case 0x26:
                            rdma[0] = rdma[1] = 0xFF;
                            /* setting memory card flags */
                            rdma[2] = mcd->flags & 0xFF;
                            /* copying memory card spec */
                            mips_memcpy(&rdma[3], &mcd->cspec, sizeof(McSpec));
                            /* calculating EDC for memory card spec data */
                            rdma[11] = CalculateEDC(&rdma[3], sizeof(McSpec));
                            rdma[12] = mcd->tcode;
                            result = 1;
                            break;
                        /* 0x81 0x27 - Set new termination code */
                        case 0x27:
                            DPRINTF("0x81 0x27 - 0x%02X\n", wdma[2]);
                            mcd->tcode = wdma[2];
                            SioResponse(mcd, rdma, length);
                            result = 1;
                            break;
                        /* 0x81 0x28 - Probe card ? */
                        case 0x28:
                            SioResponse(mcd, rdma, 4);
                            rdma[4] = mcd->tcode;
                            result = 1;
                            break;
                        /* 0x81 0x42 - Write data to a memory card */
                        case 0x42:
                            SioResponse(mcd, rdma, length);
                            if (result)
                                result = MceWrite(mcd, &wdma[3], wdma[2]);
                            else
                                DPRINTF("skipping write command after I/O error.\n");
                            break;
                        /* 0x81 0x43 - Read data from a memory card */
                        case 0x43:
                            length = wdma[2]; /* length = (*pctl >> 18) & 0x1FF; */
                            SioResponse(mcd, rdma, 5);
                            if (result)
                                result = MceRead(mcd, &rdma[4], length);
                            else
                                DPRINTF("skipping read command after I/O error.\n");
                            if (!result)
                                mips_memset(&rdma[4], 0xFF, length);
                            rdma[length + 4] = CalculateEDC(&rdma[4], length);
                            rdma[length + 5] = mcd->tcode; /* <- 'f' should be set on error, probably */
                            break;
                        /* Dummy handling for standard commands */
                        /* 0x81 0x11 - ? */
                        case 0x11: /* <- primary card detection */
                                   /* 0x81 0x81 - ? */
                        case 0x81: /* <- read status ? */
                                   /* 0x81 0x82 - ? */
                        case 0x82: /* <- erase/write status ? */
                                   /* 0x81 0xBF - ?*/
                        case 0xBF:
                        /* 0x81 0xF3 - Reset Card Auth */
                        case 0xF3:
                            DPRINTF("mc2 command 0x81 0x%02X\n", wdma[1]);
                            SioResponse(mcd, rdma, length);
                            result = 1;
                            break;
                        /* Magic Gate command */
                        case 0xF0:
                            DPRINTF("Magic Gate command 0x81 0xF0 0x%02X\n", wdma[2]);
                            SioResponse(mcd, rdma, length);
                            result = 0;
                            break;
                        /* unknown commands */
                        default:
                            DPRINTF("unknown MC2 command 0x81 0x%02X\n", wdma[1]);
                            SioResponse(mcd, rdma, length);
                            result = 0;
                            break;
                    }
                } else {
                    mips_memset(rdma, 0xFF, sd->rdwords * 4);
                    result = 0;
                }
            } else {
                result = 0;
                DPRINTF("unsupported SIO2 command: 0x%02X\n", wdma[0]);
            }

            wdma = &wdma[sd->wrwords * 4]; /* set address of next dma write-buffer */
            rdma = &rdma[sd->rdwords * 4]; /* set address of next dma read-buffer */
        }

        sd->iostatus = (result > 0) ? 0x1000 : 0x1D000; /* <- set I/O flags */
        sd->iostatus |= (ddi & 0xF) << 8;
    } else {
        sd->iostatus = 0x1D100;
        /* DPRINTF("ctrl[0] = 0x%X\n", sd->ctrl[0]); */
    }

    /* DPRINTF("SIO2 status 0x%X\n", sd->iostatus); */

    /*
     * Lock SIO2 access again as the user expects it
     * NOTE: we are assuming MC's are only accessed when LOCKED
     */
    psio2_mc_transfer_init();
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Generates a "0xFF 0xFF ... 0xFF 0x2B 0x55" sequence */
void SioResponse(MemoryCard *mcd, void *buf, int length)
{
    register int i;
    register u8 *p;

    if (length < 2) {
        DPRINTF("invalid SIO2 data length.\n");
        return;
    }

    i = length - 2;
    p = (u8 *)buf;

    while (i--)
        *p++ = 0xFF;

    p[0] = 0x2B;
    p[1] = mcd->tcode;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* int cslogfd = -1;
   int prnflag = 0; */

/* Erases memory card block */
int MceEraseBlock(MemoryCard *mcd, int page)
{
    register int i, r;

    DPRINTF("erasing at 0x%X\n", page);

    /* creating clear buffer */
    r = (mcd->flags & 0x10) ? 0x0 : 0xFF;
    mips_memset(mcd->dbufp, r, mcd->cspec.PageSize);

    for (i = 0; i < mcd->cspec.BlockSize; i++) {
        r = DeviceWritePage(mcd->mcnum, mcd->dbufp, page + i);
        if (!r) {
            DPRINTF("erase error\n");
            return 0;
        }
    }

    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
static int do_read(MemoryCard *mcd)
{
    int r, i;
    r = (mcd->flags & 0x10) ? 0xFF : 0x0;
    mips_memset(mcd->cbufp, r, 0x10);

    r = DeviceReadPage(mcd->mcnum, mcd->dbufp, mcd->rpage);
    if (!r) {
        DPRINTF("read error\n");
        return 0;
    }
    for (r = 0, i = 0; r < mcd->cspec.PageSize; r += 128, i += 3)
        CalculateECC(&(mcd->dbufp[r]), &(mcd->cbufp[i]));
    return 1;
}

int MceStartRead(MemoryCard *mcd, int page)
{
    mcd->rpage = page;
    mcd->rdoff = 0;
    mcd->rcoff = 0;

    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Reads memory card page */
int MceRead(MemoryCard *mcd, void *buf, u32 size)
{
    u32 tot_size = size;
restart:
    DPRINTF("read sector %X size %ld\n", mcd->rpage, size);
    if (mcd->rcoff && !mcd->rdoff) {
        u32 csize = (size < 16) ? size : 16;
        mips_memcpy(buf, mcd->cbufp, csize);
        mcd->rcoff = (csize > 12) ? 0 : (mcd->rcoff - csize);
        buf = (void *)((u8 *)buf + csize);
        size -= csize;
        if (size <= 0)
            return 1;
    }
    if (mcd->rdoff < mcd->cspec.PageSize) {
        if (mcd->rdoff == 0)
            if (!do_read(mcd))
                return 0;

        if ((size + mcd->rdoff) > mcd->cspec.PageSize)
            size = mcd->cspec.PageSize - mcd->rdoff;

        mips_memcpy(buf, &mcd->dbufp[mcd->rdoff], size);
        mcd->rdoff += size;
        mcd->rcoff += 3;
    }
    if (mcd->rdoff == mcd->cspec.PageSize) {
        buf = (void *)((u8 *)buf + size);
        size = tot_size - size;
        mcd->rpage++;
        mcd->rdoff = 0;
        if (size > 0)
            goto restart;
    }

    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
int MceStartWrite(MemoryCard *mcd, int page)
{
    mcd->wpage = page;
    mcd->wroff = 0;
    mcd->wcoff = 0;

    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
/* Writes memory card page */
int MceWrite(MemoryCard *mcd, void *buf, u32 size)
{
    u32 tot_size = size;
restart:
    DPRINTF("write sector %X size %ld\n", mcd->wpage, size);
    if (mcd->wcoff && !mcd->wroff) {
        u32 csize = (size < 16) ? size : 16;
        mcd->wcoff = (csize > 12) ? 0 : (mcd->wcoff - csize);
        buf += csize;
        size -= csize;
        if (size <= 0)
            return 1;
    }
    if (mcd->wroff < mcd->cspec.PageSize) {
        if ((size + mcd->wroff) > mcd->cspec.PageSize)
            size = mcd->cspec.PageSize - mcd->wroff;

        mips_memcpy(&mcd->dbufp[mcd->wroff], buf, size);
        mcd->wroff += size;
        mcd->wcoff += 3;
    }
    if (mcd->wroff == mcd->cspec.PageSize) {
        register int r;

        buf += size;
        size = tot_size - size;
        mcd->wroff = 0;

        r = DeviceWritePage(mcd->mcnum, mcd->dbufp, mcd->wpage);
        if (!r) {
            DPRINTF("write error.\n");
            return 0;
        }

        mcd->wpage++;
        if (size > 0)
            goto restart;
    }

    return 1;
}
//------------------------------
// endfunc
//---------------------------------------------------------------------------
// End of file: mcemu.c
//---------------------------------------------------------------------------

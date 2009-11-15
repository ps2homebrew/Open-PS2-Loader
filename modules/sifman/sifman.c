/*
  Copyright 2009, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright 2002, [RO]man <roman_ps2dev@hotmail.com>
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
  
  Source code to generate a sifman module for IOPRP300.IMG
*/

#include <dmacman.h>
#include <intrman.h>
#include <loadcore.h>

#define MODNAME "IOP_SIF_manager"
IRX_ID(MODNAME, 2, 3);

#define CONFIG_1450	(*(volatile int*)0xBF801450)
#define BD0			(*(volatile int*)0xBD000000)	// mscom
#define BD1			(*(volatile int*)0xBD000010)	// smcom
#define BD2			(*(volatile int*)0xBD000020)	// msflag
#define BD3			(*(volatile int*)0xBD000030)	// smflag
#define BD4			(*(volatile int*)0xBD000040)
#define BD5			(*(volatile int*)0xBD000050)
#define BD6			(*(volatile int*)0xBD000060)
#define BD7			(*(volatile int*)0xBD000070)

#define IOP_DMAC_STAT		(*(volatile int*)0xBF801560)

typedef struct t_SifDmaTransfer
{
	void	*src;
	void	*dest;
	int		size;
	int		attr;
} SifDmaTransfer_t;

/* Modes for DMA transfers */
#define SIF_DMA_FROM_IOP	0x0
#define SIF_DMA_TO_IOP		0x1
#define SIF_DMA_FROM_EE		0x0
#define SIF_DMA_TO_EE		0x1

#define SIF_DMA_INT_I		0x2
#define SIF_DMA_INT_O		0x4
#define SIF_DMA_SPR		0x8
#define SIF_DMA_BSN		0x10 /* ? what is this? */
#define SIF_DMA_TAG		0x20

struct irx_export_table _exp_sifman;

static int sifDma2Init = 0;
static int sifInit = 0;

void sceSifSetDChain(void);
void RegisterSif0Handler(void);
u32 sceSifSetSubAddr(u32 addr);

struct sifData
{
	int	data,
		words,
		count,
		addr;
};

struct sifFn
{
	int 	(*fn)(void*);
	void 	*param;
};

struct sm_internal
{
	unsigned short	id,				//+000	some id...?!?
					res1;			//+002	not used
	int				index;			//+004	current position in dma buffer
	struct sifData	*crtbuf;		//+008	address of current used buffer
	struct sifData	Dbuf1[32];		//+00C	first buffer
	struct sifData	Dbuf2[32];		//+20C	second buffer
	struct sifFn	*fnbuf1;		//+40C
	struct sifFn	*fnbuf2;		//+410
	struct sifFn	Fbuf1[32];		//+414
	int 			res2;			//+514
	struct sifFn	Fbuf2[32];		//+518
	int 			res3;			//+618
	int 			(*fn)(void*);	//+61C
	void 			*param;			//+620
	int 			res4;			//+624	not used
} vars __attribute__((aligned(64)));

struct sifData	one;

/*-----------------------------------------------------------------------*/
int _start(int argc, char **argv)
{
	int PRId;

	asm volatile ("mfc0 %0, $15" : "=r" (PRId));

	if ((PRId >= 0x10) &&
		!(CONFIG_1450 & 8)&&
		((BD6 == 0x1D000060)||
		!(BD6  & 0xFFFFF000)))
		return ((u32)RegisterLibraryEntries(&_exp_sifman) > 0);

	return MODULE_NO_RESIDENT_END;	
}

/*-----------------------------------------------------------------------*/
int getBD2_loopchanging(void)
{
	register int a, b;

	b = BD2;
	do {
		a = b;
		b = BD2;
	}
	while (a != b);

	return a;
}

/*-----------------------------------------------------------------------*/
int getBD3_loopchanging(void)
{
    register int a, b;

	b = BD3;
	do {
		a = b;
		b = BD3;
	}
	while (a != b);

	return a;
}

/*-----------------------------------------------------------------------*/
void sceSifDma2Init(void)
{
	volatile int var;

	if (sifDma2Init == 0) {
		IOP_DMAC_SIF2_CHCR = 0;	// reset ch. 2
		IOP_DMAC_DPCR |= 0x800;	// enable dma ch. 2
		var = IOP_DMAC_DPCR;
		sifDma2Init	= 1;
	}	
}

/*-----------------------------------------------------------------------*/
void sceSifInit(void)
{
	volatile int var, status;
	int oldstate;

	if (sifInit == 0) {

		IOP_DMAC_DPCR2 |= 0x8800;	//enable dma ch. 9 and 10
		IOP_DMAC_SIF9_CHCR = 0;
		IOP_DMAC_SIFA_CHCR = 0;
		sceSifDma2Init();

		if (CONFIG_1450 & 0x10)
			CONFIG_1450 |= 0x10;
		CONFIG_1450 |= 0x1;

		RegisterSif0Handler();

		do {
			CpuSuspendIntr(&oldstate);
			status = getBD2_loopchanging();
			CpuResumeIntr(oldstate);
		} while (!(status & 0x10000));	//EE kernel sif ready

		sceSifSetDChain();
		sceSifSetSubAddr(0);	//sif1 receive buffer
		
		BD3 = 0x10000;			//IOPEE_sifman_init
		var = BD3;
		sifInit = 1;
	}
}

/*-----------------------------------------------------------------------*/
void shutdown(void)
{
	int x;

	DisableIntr(IOP_IRQ_DMA_SIF0, &x);
	ReleaseIntrHandler(IOP_IRQ_DMA_SIF0);
	IOP_DMAC_SIF9_CHCR = 0;
	IOP_DMAC_SIFA_CHCR = 0;

	if (CONFIG_1450 & 0x10)
		CONFIG_1450 |= 0x10;
}

/*-----------------------------------------------------------------------*/
int sceSifCheckInit(void)
{
	return sifInit;
}

/*-----------------------------------------------------------------------*/
void sceSifSetDChain(void)
{
	if ((BD4 & 0x40) == 0)
		BD4 = 0x40;
	IOP_DMAC_SIFA_CHCR = 0;
	IOP_DMAC_SIFA_BCR_size = 32;
	IOP_DMAC_SIFA_CHCR	= DMAC_CHCR_08|DMAC_CHCR_CO|DMAC_CHCR_TR|DMAC_CHCR_30;	//EE->IOP (to memory)
}

/*-----------------------------------------------------------------------*/
void sceSifSetDmaIntrHandler(int (*_fn)(void*), void *_param)
{
	vars.fn = _fn;
	vars.param = _param;
}

/*-----------------------------------------------------------------------*/
void sceSifResetDmaIntrHandler(void)
{
	vars.fn = NULL;
	vars.param = NULL;
}

/*-----------------------------------------------------------------------*/
int Sif0Handler(void *args)
{
	struct sm_internal *vars = (struct sm_internal *)args;
    volatile int var;
    register int i;
    
	if (vars->fn)
		vars->fn(vars->param);
	
	volatile int *res = (volatile int *)&vars->fnbuf1[32];			
	for (i = 0; i < *res; i++)
		vars->fnbuf1[i].fn(vars->fnbuf1[i].param);

	*res = 0;

	if (((IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR) == 0) && (vars->index > 0)) {
		IOP_DMAC_SIF9_CHCR = 0;
		IOP_DMAC_SIF9_TADR = (volatile int)vars->crtbuf;
		IOP_DMAC_SIF9_BCR_size = 32;
		if ((BD4 & 0x20) == 0)
			BD4 = 0x20;
		vars->id++;
		vars->index = 0;
		if (vars->crtbuf == vars->Dbuf1) {
			vars->crtbuf = vars->Dbuf2;
			vars->fnbuf2 = vars->Fbuf2;
			vars->fnbuf1 = vars->Fbuf1;
		} else {
			vars->fnbuf2 = vars->Fbuf1;
			vars->crtbuf = vars->Dbuf1;
			vars->fnbuf1 = vars->Fbuf2;
		}
		IOP_DMAC_SIF9_CHCR = DMAC_CHCR_DR|DMAC_CHCR_08|DMAC_CHCR_CO|DMAC_CHCR_LI|DMAC_CHCR_TR;	//IOP->EE (from memory)
		var = IOP_DMAC_SIF9_CHCR;
	}
	
	return 1;
}

/*-----------------------------------------------------------------------*/
void RegisterSif0Handler(void)
{
	int oldstate;
	
	vars.crtbuf = vars.Dbuf1;
	vars.fnbuf2 = vars.Fbuf1;
	vars.index = 0;
	vars.res2 = 0;
	vars.res3 = 0;
	vars.fnbuf1 = vars.Fbuf2;
	vars.fn = NULL;
	vars.param = NULL;
	
	CpuSuspendIntr(&oldstate);
	RegisterIntrHandler(IOP_IRQ_DMA_SIF0, 1, Sif0Handler, &vars);
	EnableIntr(IOP_IRQ_DMA_SIF0);
	CpuResumeIntr(oldstate);
}

/*-----------------------------------------------------------------------*/
void enqueue(SifDmaTransfer_t *dmat)
{
	register int nwords;
	
	vars.crtbuf[vars.index].data = (int)dmat->src & 0xffffff;	//16MB addressability

	if (dmat->attr & SIF_DMA_INT_I)
		vars.crtbuf[vars.index].data |= 0x40000000;
	
	nwords = (vu32)(dmat->size + 3) >> 2;
	vars.crtbuf[vars.index].words = nwords & 0xffffff;	
	vars.crtbuf[vars.index].count = (((vu32)(dmat->size + 3) >> 4) + (nwords & 3)) | 0x10000000;

	if (dmat->attr & SIF_DMA_INT_O)
		vars.crtbuf[vars.index].count |= 0x80000000;

	vars.crtbuf[vars.index].addr = (int)dmat->dest & 0x1fffffff;	//512MB addresability
	vars.index++;
}

/*-----------------------------------------------------------------------*/
int sifsetdma(SifDmaTransfer_t *dmat, int len, int (*_fn)(void*), void *_param)
{
	volatile int var;
    register int ret, i, index;

	if ((32 - vars.index) < len) // no place
		return 0;

	ret = vars.id;
	index = vars.index;
	
	if (index)
		vars.crtbuf[index-1].data &= 0x7fffffff;

	for (i = 0; i < len; i++)
		enqueue(&dmat[i]);

	vars.crtbuf[index-1].data |= 0x80000000;

	if (_fn) {
		volatile int *res = (volatile int *)&vars.fnbuf2[32];
		vars.fnbuf2[*res].fn = _fn;
		vars.fnbuf2[*res].param = _param;
		*res = *res + 1;
	}

	if ((IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR) == 0) {
		IOP_DMAC_SIF9_CHCR = 0;
		IOP_DMAC_SIF9_TADR = (volatile int)vars.crtbuf;

		if ((BD4 & 0x20) == 0)
			BD4 = 0x20;

		IOP_DMAC_SIF9_BCR_size = 32;

		vars.index = 0;
		vars.id++;
		
		if (vars.crtbuf == vars.Dbuf1) {
			vars.crtbuf = vars.Dbuf2;
			vars.fnbuf2 = vars.Fbuf2;
			vars.fnbuf1 = vars.Fbuf2;
		} else {
			vars.crtbuf = vars.Dbuf1;
			vars.fnbuf2 = vars.Fbuf1;
			vars.fnbuf1 = vars.Fbuf2;
		}

		IOP_DMAC_SIF9_CHCR = DMAC_CHCR_DR|DMAC_CHCR_08|DMAC_CHCR_CO|DMAC_CHCR_LI|DMAC_CHCR_TR;	//IOP->EE
		var = IOP_DMAC_SIF9_CHCR;
	}

	ret = ((vu32)ret << 16) | ((vu32)(index & 0xff) << 8) | (len & 0xff);

	return ret;
}

/*-----------------------------------------------------------------------*/
int sceSifSetDma(SifDmaTransfer_t *dmat, int count)
{
	return sifsetdma(dmat, count, NULL, NULL);
}

/*-----------------------------------------------------------------------*/
int sceSifSetDma2(SifDmaTransfer_t *dmat, int count, int (*_fn)(void*), void *_param)
{
	return sifsetdma(dmat, count, _fn, _param);
}

/*-----------------------------------------------------------------------*/
int sceSifDmaStat(int trid)
{
	if (IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR) {
		if (IOP_DMAC_STAT == 0)
			return -1;	//terminated
	}
		
	if (((vu32)trid >> 16) == vars.id)
		return 1;  //waiting in queue
			
	if (((((vu32)trid >> 16)+1) & 0xffff) == vars.id)
		return 0;  //running
	
	return -1;	//terminated
}

/*-----------------------------------------------------------------------*/
void sceSifSetOneDma(SifDmaTransfer_t dmat)
{
	volatile int var;
	
	dmat.size = ((vu32)dmat.size >> 2) + ((vu32)(dmat.size & 3) > 0);
	one.data = ((int)dmat.src & 0xffffff) | 0x80000000;
	one.words = (int)dmat.size & 0xffffff;

	if (dmat.attr & SIF_DMA_INT_I)
		one.data |= 0x40000000;

	one.count = (((vu32)dmat.size >> 2) + (dmat.size & 3)) | 0x10000000;

	if (dmat.attr & SIF_DMA_INT_O)
		one.count |= 0x80000000;

	one.addr = (int)dmat.dest & 0xfffffff;

	if ((BD4 & 0x20) == 0)
		BD4 = 0x20;

	IOP_DMAC_SIF9_CHCR = 0;
	IOP_DMAC_SIF9_TADR = (volatile int)&one;
	IOP_DMAC_SIF9_BCR_size = 32;
	IOP_DMAC_SIF9_CHCR = DMAC_CHCR_DR|DMAC_CHCR_08|DMAC_CHCR_CO|DMAC_CHCR_LI|DMAC_CHCR_TR; //IOP->EE
	var = IOP_DMAC_SIF9_CHCR;
}

/*-----------------------------------------------------------------------*/
void sceSifSendSync(void)
{
	while (IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
int sceSifIsSending(void)
{
	return (IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
void sceSifDma0Transfer(void *addr, int size)
{
    volatile int var;

	size = ((vu32)size >> 2) + ((vu32)(size & 3) > 0);

	if ((BD4 & 0x20) == 0)
		BD4 = 0x20;

	IOP_DMAC_SIF9_CHCR = 0;
	IOP_DMAC_SIF9_MADR = (volatile int)addr & 0xffffff;
	IOP_DMAC_SIF9_BCR_size = 32;
	IOP_DMAC_SIF9_BCR_count = ((vu32)size >> 5) + (size & 0x1f);
	IOP_DMAC_SIF9_CHCR = DMAC_CHCR_DR|DMAC_CHCR_CO|DMAC_CHCR_TR;
	var = IOP_DMAC_SIF9_CHCR;
}

/*-----------------------------------------------------------------------*/
void sceSifDma0Sync(void)
{
	while (IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
int sceSifDma0Sending(void)
{
	return (IOP_DMAC_SIF9_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
void sceSifDma1Transfer(void *addr, int size, int mode)
{
    volatile int var;

	size = ((vu32)size >> 2) + ((vu32)(size & 3) > 0);

	if ((BD4 & 0x40) == 0)
		BD4 = 0x40;

	IOP_DMAC_SIFA_CHCR = 0;
	IOP_DMAC_SIFA_MADR = (volatile int)addr & 0xffffff;
	IOP_DMAC_SIFA_BCR_size = 32;
	IOP_DMAC_SIFA_BCR_count = ((vu32)size >> 5) + (size & 0x1f);
	IOP_DMAC_SIFA_CHCR = DMAC_CHCR_CO|DMAC_CHCR_TR|(mode & SIF_DMA_BSN ? DMAC_CHCR_30 : 0);
	var = IOP_DMAC_SIFA_CHCR;
}

/*-----------------------------------------------------------------------*/
void sceSifDma1Sync(void)
{
	while (IOP_DMAC_SIFA_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
int sceSifDma1Sending(void)
{
	return (IOP_DMAC_SIFA_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
void sceSifDma2Transfer(void *addr, int size, int mode)
{
    volatile int var;

	size = ((vu32)size >> 2) + ((vu32)(size & 3) > 0);

	if ((BD4 & 0x80) == 0)
		BD4 = 0x80;

	IOP_DMAC_SIF2_CHCR = 0;
	IOP_DMAC_SIF2_MADR = (volatile int)addr & 0xffffff;
	IOP_DMAC_SIF2_BCR_size = (size < 33 ? size : 32);
	IOP_DMAC_SIF2_BCR_count = ((vu32)size >> 5) + (size & 0x1f);
	IOP_DMAC_SIF2_CHCR = DMAC_CHCR_CO|DMAC_CHCR_TR|(mode & SIF_DMA_TO_EE ? DMAC_CHCR_DR : (mode & SIF_DMA_BSN ? DMAC_CHCR_30 : 0));
	var = IOP_DMAC_SIF2_CHCR;
}

/*-----------------------------------------------------------------------*/
void sceSifDma2Sync(void)
{
	while (IOP_DMAC_SIF2_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
int sceSifDma2Sending(void)
{
	return (IOP_DMAC_SIF2_CHCR & DMAC_CHCR_TR);
}

/*-----------------------------------------------------------------------*/
u32 sceSifGetMSFlag(void)
{
	return getBD2_loopchanging();
}

/*-----------------------------------------------------------------------*/
u32 sceSifSetMSFlag(u32 val)
{
	BD2 = (volatile int)val;
	return getBD2_loopchanging();
}

/*-----------------------------------------------------------------------*/
u32 sceSifGetSMFlag(void)
{
	return getBD3_loopchanging();
}

/*-----------------------------------------------------------------------*/
u32 sceSifSetSMFlag(u32 val)
{
	BD3 = (volatile int)val;
	return getBD3_loopchanging();
}

/*-----------------------------------------------------------------------*/
u32 sceSifGetMainAddr(void) 
{
	return BD0;
}

/*-----------------------------------------------------------------------*/
u32 sceSifGetSubAddr(void)
{
	return BD1;
}

/*-----------------------------------------------------------------------*/
u32 sceSifSetSubAddr(u32 addr)
{
	BD1 = (volatile int)addr;
	return BD1;
}

/*-----------------------------------------------------------------------*/
void sceSifIntrMain(void)
{
    volatile int var;
    
	CONFIG_1450 |= 2;
	CONFIG_1450 &= 0xfffffffd;
	var = CONFIG_1450;
}

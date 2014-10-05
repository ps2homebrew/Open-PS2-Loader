/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "smsutils.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "dev9.h"
#include "smb.h"
#include "atad.h"
#include "ioplib_util.h"
#include "cdvdman.h"
#include "cdvd_config.h"

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <thmsgbx.h>
#include <errno.h>
#include <io_common.h>
#include <usbd.h>
#include <ps2ip.h>
#include "ioman_add.h"

#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...)	printf(args)
#else
#define DPRINTF(args...)	do { } while(0)
#endif

#ifdef HDD_DRIVER
#define MODNAME "dev9"
IRX_ID(MODNAME, 2, 8);
#else
#define MODNAME "cdvd_driver"
IRX_ID(MODNAME, 1, 1);
#endif

//------------------ Patch Zone ----------------------
#ifdef HDD_DRIVER
static struct cdvdman_settings_hdd cdvdman_settings={
	{
		0x69, 0x69,
		0x1234,
		0x39393939,
		"B00BS"
	},
	0x12345678
};
#elif SMB_DRIVER
struct cdvdman_settings_smb cdvdman_settings={
	{
		0x69, 0x69,
		0x1234,
		0x39393939,
		"B00BS"
	},
	"192.168.0.10",
	0x8510,
	"PS2SMB",
	"",
	"GUEST",
	"",
	"######  GAMETITLE SMB  ######",
	0,
	{
		{
			"EXTEN",
			"SLPM_550.52",
		}
	}
};
#elif USB_DRIVER
struct cdvdman_settings_usb cdvdman_settings={
	{
		0x69, 0x69,
		0x1234,
		0x39393939,
		"B00BS"
	},
	{1,2,3,4,5,6,7,8,9,10}
};
#else
	#error Unknown driver type. Please check the Makefile.
#endif

//----------------------------------------------------
struct irx_export_table _exp_cdvdman;
struct irx_export_table _exp_cdvdstm;
#ifdef SMB_DRIVER
struct irx_export_table _exp_dev9;
#endif
#ifdef HDD_DRIVER
struct irx_export_table _exp_dev9;
struct irx_export_table _exp_atad;
#endif
#ifdef USB_DRIVER
#ifdef __USE_DEV9
struct irx_export_table _exp_dev9;
#endif
#endif
struct irx_export_table _exp_smsutils;
#ifdef VMC_DRIVER
struct irx_export_table _exp_oplutils;
#endif

struct dirTocEntry {
	short	length;
	u32	fileLBA;			// 2
	u32	fileLBA_bigend;			// 6
	u32	fileSize;			// 10
	u32	fileSize_bigend;		// 14
	u8	dateStamp[6];			// 18
	u8	reserved1;			// 24
	u8	fileProperties;			// 25
	u8	reserved2[6];			// 26
	u8	filenameLength;			// 32
	char	filename[128];			// 33
} __attribute__((packed));

struct cdvdman_StreamingData{
	unsigned int lsn;
	unsigned int bufmax;
	int stat;
};

typedef struct {
	int err;
	int status;
	struct cdvdman_StreamingData StreamingData;
	int intr_ef;
	int disc_type_reg;
#ifdef ALT_READ_CORE
	int cdNCmd;
	int cddiskready;
#else
	u32 cdread_lba;
	u32 cdread_sectors;
	void *cdread_buf;
	cd_read_mode_t cdread_mode;
#endif
} cdvdman_status_t;

typedef struct {
	iop_file_t *f;
	u32 lsn;
	unsigned int filesize;
	unsigned int position;
} FHANDLE;

// internal functions prototypes
#ifdef USB_DRIVER
static void usbd_init(void);
#endif
#ifdef SMB_DRIVER
static void ps2ip_init(void);
#endif
static void fs_init(void);
static void cdvdman_init(void);
#ifdef ALT_READ_CORE
static void cdvdman_cdinit();
static int cdvdman_ReadSect(u32 lsn, u32 nsectors, void *buf);
#endif
static int cdvdman_readMechaconVersion(char *mname, u32 *stat);
static int cdvdman_readID(int mode, u8 *buf);
static FHANDLE *cdvdman_getfilefreeslot(void);
static void cdvdman_trimspaces(char* str);
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer);
static int cdvdman_findfile(cd_file_t *pcd_file, const char *name, int layer);
static int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
static int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size);
static int cdvdman_cb_event(int reason);
static unsigned int event_alarm_cb(void *args);
static void cdvdman_startThreads(void);
static void cdvdman_create_semaphores(void);
static void cdvdman_initdev(void);
#ifdef HDD_DRIVER
static void cdvdman_get_part_specs(u32 lsn);
#endif

#ifdef USB_DRIVER
// !!! usbd exports functions pointers !!!
int (*pUsbRegisterDriver)(UsbDriver *driver); 								// #4
void *(*pUsbGetDeviceStaticDescriptor)(int devId, void *data, u8 type); 				// #6
int (*pUsbSetDevicePrivateData)(int devId, void *data); 						// #7
int (*pUsbOpenEndpoint)(int devId, UsbEndpointDescriptor *desc); 					// #9
int (*pUsbCloseEndpoint)(int id); 									// #10
int (*pUsbTransfer)(int id, void *data, u32 len, void *option, UsbCallbackProc callback, void *cbArg); 	// #11
int (*pUsbOpenEndpointAligned)(int devId, UsbEndpointDescriptor *desc); 				// #12
#endif

#ifdef SMB_DRIVER
// !!! ps2ip exports functions pointers !!!
// Note: recvfrom() used here is not a standard recvfrom() function.
int (*plwip_close)(int s); 										// #6
int (*plwip_connect)(int s, struct sockaddr *name, socklen_t namelen); 					// #7
int (*plwip_recvfrom)(int s, void *header, int hlen, void *payload, int plen, unsigned int flags, struct sockaddr *from, socklen_t *fromlen);	// #10
int (*plwip_send)(int s, void *dataptr, int size, unsigned int flags); 					// #11
int (*plwip_socket)(int domain, int type, int protocol); 						// #13
u32 (*pinet_addr)(const char *cp); 									// #24
#endif

// for "cdrom" devctl
#define CDIOC_CMDBASE		0x430C

// for "cdrom" ioctl2
#define CIOCSTREAMPAUSE		0x630D
#define CIOCSTREAMRESUME	0x630E
#define CIOCSTREAMSTAT		0x630F

// driver ops protypes
static int cdrom_dummy(void);
static int cdrom_init(iop_device_t *dev);
static int cdrom_deinit(iop_device_t *dev);
static int cdrom_open(iop_file_t *f, const char *filename, int mode);
static int cdrom_close(iop_file_t *f);
static int cdrom_read(iop_file_t *f, void *buf, int size);
static int cdrom_lseek(iop_file_t *f, u32 offset, int where);
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat);
static int cdrom_dopen(iop_file_t *f, const char *dirname);
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent);
static int cdrom_dclose(iop_file_t *f);
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args);
static s64 cdrom_lseek64(iop_file_t *f, s64 pos, int where);
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen);
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen);

// driver ops func tab
static struct _iop_ext_device_ops cdrom_ops = {
	&cdrom_init,
	&cdrom_deinit,
	(void*)&cdrom_dummy,
	&cdrom_open,
	&cdrom_close,
	&cdrom_read,
	(void*)&cdrom_dummy,
	&cdrom_lseek,
	&cdrom_ioctl,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	&cdrom_dopen,
	&cdrom_dclose,
	&cdrom_dread,
	&cdrom_getstat,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	&cdrom_lseek64,
	(void*)&cdrom_devctl,
	(void*)&cdrom_dummy,
	(void*)&cdrom_dummy,
	&cdrom_ioctl2
};

// driver descriptor
static iop_ext_device_t cdrom_dev = {
	"cdrom",
	IOP_DT_FS | IOP_DT_FSEXT,
	1,
	"CD-ROM ",
	&cdrom_ops
};

#define MAX_FDHANDLES 		64
FHANDLE cdvdman_fdhandles[MAX_FDHANDLES];

typedef struct {
	u32 rootDirtocLBA;
	u32 rootDirtocLength;
} layer_info_t;

static layer_info_t layer_info[2];

static int cdvdman_cdinited = 0;
cdvdman_status_t cdvdman_stat;
static void *user_cb;

#ifndef HDD_DRIVER
static u32 cdvdman_layer1start = 0;
#endif

static int cdrom_io_sema;
static int cdvdman_cdreadsema;
static int cdvdman_searchfilesema;

#ifndef ALT_READ_CORE
static int cdvdman_lockreadsema;
static int sync_flag;
#endif

static char cdvdman_dirname[256];
static char cdvdman_filepath[256];
static char cdvdman_curdir[256];

// buffers
#define CDVDMAN_BUF_SECTORS 	2
static u8 cdvdman_buf[CDVDMAN_BUF_SECTORS*2048];

#define CDVDMAN_FS_BUFSIZE	CDVDMAN_FS_SECTORS * 2048
static u8 cdvdman_fs_buf[CDVDMAN_FS_BUFSIZE + 2*2048];

static int fs_inited = 0;

#ifdef HDD_DRIVER
int lba_48bit = 0;
int atad_inited = 0;
static hdl_apa_header apaHeader;

typedef struct {
	u32 part_offset; 	// in MB
	u32 data_start; 	// in sectors
	u32 part_size; 		// in KB
} cdvdman_partspecs_t;

static cdvdman_partspecs_t cdvdman_partspecs;
#endif

#define CDVDMAN_MODULE_VERSION 0x225
static int cdvdman_debug_print_flag = 0;

static int cdvdman_media_changed = 1;

static int cdvdman_cur_disc_type = 0;	/* real current disc type */
unsigned int ReadPos = 0;		/* Current buffer offset in 2048-byte sectors. */

static iop_sys_clock_t gCdvdCallback_SysClock;
#if (defined(HDD_DRIVER) && !defined(HD_PRO)) || defined(SMB_DRIVER)
static int POFFThreadID;
#endif

//-------------------------------------------------------------------------
#ifdef ALT_READ_CORE

enum NCMD_CMD{
	NCMD_INIT	= 0x00,
	NCMD_READ,
	NCMD_READCDDA,
	NCMD_SEEK,
	NCMD_STANDBY,
	NCMD_STOP,
	NCMD_PAUSE,

	NCMD_COUNT
};

static int cdvdman_NCmdlocksema;
static int cdvdman_NCmdsema;
static int cdvdman_NCmdsemacount = 0;
static int cdvdman_mbxid;

typedef struct {
	int ncmd;
	u8 nbuf[28];
} cdvdman_NCmd_t;

typedef struct NCmdMbx {
	iop_message_t iopmsg;
	cdvdman_NCmd_t NCmdmsg;
	struct NCmdMbx *next;
} NCmdMbx_t;

cdvdman_NCmd_t cdvdman_NCmd;

static void *cdvdman_pMbxnext;
static void *cdvdman_pMbxcur;
static void *cdvdman_pMbxbuf = NULL;

#define NCMD_NUMBER		16
static u8 cdvdman_Mbxbuf[NCMD_NUMBER*sizeof(NCmdMbx_t)];

static NCmdMbx_t *cdvdman_setNCmdMbx(void);
static void cdvdman_getNCmdMbx(NCmdMbx_t *mbxbuf);
static void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size);
static cdvdman_NCmd_t *cdvdman_receiveNCmdMbx(int mbxid);
static void cdvdman_startNCmdthread(void);
static void cdvdman_NCmdthread(void *args);
static int cdvdman_sendNCmd(u8 ncmd, void *ndata, int ndlen);
static void cdvdman_waitNCmdsema(void);
static void cdvdman_signalNCmdsema(void);
static void cdvdman_waitsignalNCmdsema(void);
static int cdvdman_getNCmdstate(void);
static void cdvdman_NCmdCall(u8 ncmd, void *ndata);
static void (*NCmd_fn)(void *ndata);
static void NCmd_cdInit(void *ndata);
static void NCmd_cdRead(void *ndata);
static void NCmd_cdReadCDDA(void *ndata);
static void NCmd_cdSeek(void *ndata);
static void NCmd_cdStandby(void *ndata);
static void NCmd_cdStop(void *ndata);
static void NCmd_cdPause(void *ndata);
static int (*cdSync_fn)(void);
static int cdSync_blk(void);
static int cdSync_noblk(void);

//--------------------------------------------------------------
static NCmdMbx_t *cdvdman_setNCmdMbx(void)
{
	int i, oldstate;
	NCmdMbx_t *pmbx;

	if (!cdvdman_pMbxbuf) {
		// Here we init the thread message buffer
		cdvdman_pMbxbuf = cdvdman_Mbxbuf;
		mips_memset(cdvdman_pMbxbuf, 0, sizeof(NCmdMbx_t) * NCMD_NUMBER);

		pmbx = (NCmdMbx_t *)cdvdman_pMbxbuf;

		for (i = (NCMD_NUMBER-2); i >= 0; i--) {
			pmbx->next = (NCmdMbx_t *)(pmbx + 1);
			pmbx++;
		}

		cdvdman_pMbxnext = cdvdman_pMbxbuf;
		pmbx->next = NULL;
	}

	if (!cdvdman_pMbxnext)
		return (NCmdMbx_t *)NULL;

	pmbx = (NCmdMbx_t *)cdvdman_pMbxnext;
	cdvdman_pMbxnext = pmbx->next;
	pmbx->next = (NCmdMbx_t *)cdvdman_pMbxcur;

	cdvdman_pMbxcur = (void *)pmbx;

	return (NCmdMbx_t *)pmbx;
}

//--------------------------------------------------------------
static void cdvdman_getNCmdMbx(NCmdMbx_t *pmbx)
{
	cdvdman_pMbxcur = (void *)pmbx->next;
	pmbx->next = (NCmdMbx_t *)cdvdman_pMbxnext;

	cdvdman_pMbxnext = (void *)pmbx;
}

//--------------------------------------------------------------
static void cdvdman_sendNCmdMbx(int mbxid, cdvdman_NCmd_t *NCmdmsg, int size)
{
	NCmdMbx_t *pmbx;

	pmbx = cdvdman_setNCmdMbx();

	if (size > sizeof(cdvdman_NCmd_t)) 	// 32
		size = sizeof(cdvdman_NCmd_t); 	// 32

	if (size)
		mips_memcpy((void *)&pmbx->NCmdmsg, (void *)NCmdmsg, size);

	if (QueryIntrContext())
		iSendMbx(mbxid, (void *)pmbx);
	else
		SendMbx(mbxid, (void *)pmbx);
}

//--------------------------------------------------------------
static cdvdman_NCmd_t *cdvdman_receiveNCmdMbx(int mbxid)
{
	NCmdMbx_t *pmbx;
	int r;

	r = ReceiveMbx((void **)&pmbx, mbxid);
	if (r < 0)
		return (cdvdman_NCmd_t *)NULL;

	cdvdman_getNCmdMbx(pmbx);

	return (cdvdman_NCmd_t *)(&pmbx->NCmdmsg);
}

//--------------------------------------------------------------
static int cdvdman_sendNCmd(u8 ncmd, void *ndata, int ndlen)
{
	if (cdvdman_NCmdlocksema) {
		if (cdvdman_stat.cdNCmd)
			return 0;

		WaitSema(cdvdman_NCmdsema);
	}

	cdvdman_stat.cdNCmd = 1;
	cdvdman_NCmd.ncmd = ncmd;

	if (ndlen)
		mips_memcpy(cdvdman_NCmd.nbuf, ndata, ndlen);

	cdvdman_sendNCmdMbx(cdvdman_mbxid, (cdvdman_NCmd_t *)&cdvdman_NCmd, sizeof(cdvdman_NCmd_t));

	return 1;
}

//--------------------------------------------------------------
static void cdvdman_waitNCmdsema(void)
{
	if (!cdvdman_NCmdsemacount) {
		cdvdman_NCmdlocksema = 0;
		WaitSema(cdvdman_NCmdsema);
	}

	cdvdman_NCmdsemacount++;
}

//--------------------------------------------------------------
static void cdvdman_signalNCmdsema(void)
{
	if (!cdvdman_NCmdsemacount)
		return;

	cdvdman_NCmdsemacount--;

	if (!cdvdman_NCmdsemacount) {

		SignalSema(cdvdman_NCmdsema);

		cdvdman_NCmdlocksema = 1;
	}
}

//--------------------------------------------------------------
static void cdvdman_waitsignalNCmdsema(void)
{
	WaitSema(cdvdman_NCmdsema);
	SignalSema(cdvdman_NCmdsema);
}

//--------------------------------------------------------------
static int cdvdman_getNCmdstate(void)
{
	return cdvdman_stat.cdNCmd;
}

//--------------------------------------------------------------
static void cdvdman_NCmdthread(void *args)
{
	cdvdman_NCmd_t *NCmd;

	while (1) {
		NCmd = cdvdman_receiveNCmdMbx(cdvdman_mbxid);

		cdvdman_NCmdCall(NCmd->ncmd, (void *)NCmd->nbuf);

		cdvdman_stat.cdNCmd = 0;

		if (cdvdman_NCmdlocksema)
			SignalSema(cdvdman_NCmdsema);

		//SetEventFlag(cdvdman_stat.cdvdman_intr_ef, 1);
	}
}

//--------------------------------------------------------------
static void cdvdman_startNCmdthread(void)
{
	int thid;
	iop_mbx_t mbx;
	iop_sema_t smp;
	iop_thread_t thread_param;

	mips_memset(&mbx, 0, sizeof(iop_mbx_t));
	cdvdman_mbxid = CreateMbx(&mbx);

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_NCmdsema = CreateSema(&smp);
	cdvdman_NCmdlocksema = 1;
	cdvdman_NCmdsemacount = 0;

	thread_param.thread = (void *)cdvdman_NCmdthread;
 	thread_param.stacksize = 0x400;
	thread_param.priority = 0x01;
	thread_param.attr = TH_C;
	thread_param.option = 0;

	thid = CreateThread(&thread_param);
	StartThread(thid, NULL);
}

//--------------------------------------------------------------
static void cdvdman_NCmdCall(u8 ncmd, void *ndata)
{
	switch(ncmd){
		case NCMD_INIT:
			NCmd_cdInit();
			break;
		case NCMD_READ:
			NCmd_cdRead();
			break;
		case NCMD_READCDDA:
			cdReadCDDA();
			break;
		case NCMD_SEEK:
			NCmd_cdSeek();
			break;
		case NCMD_STANDBY:
			NCmd_cdStandby();
			break;
		case NCMD_STOP:
			NCmd_cdStop();
		case NCMD_PAUSE:
			NCmd_cdPause();
			break;
	}
}

//--------------------------------------------------------------
static void NCmd_cdInit(void *ndata)
{
	cdvdman_cdinit();
}

//--------------------------------------------------------------
static void NCmd_cdRead(void *ndata)
{
	int r;
	u32 lsn, sectors;
	void *buf;
	u8 *wdbuf = (u8 *)ndata;

	cdvdman_stat.err = CDVD_ERR_NO;

	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = sceCdRead0(lsn, sectors, buf, NULL);
	if (r == 0)
		cdvdman_stat.err = CDVD_ERR_READ;

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncRead);
}

//--------------------------------------------------------------
static void NCmd_cdReadCDDA(void *ndata)
{
	int r;
	u32 lsn, sectors;
	void *buf;
	u8 *wdbuf = (u8 *)ndata;

	cdvdman_stat.err = CDVD_ERR_NO;

	lsn = *((u32 *)&wdbuf[0]);
	sectors = *((u32 *)&wdbuf[4]);
	buf = (void *)*((u32 *)&wdbuf[8]);

	r = sceCdRead0(lsn, sectors, buf, NULL);
	if (r == 0)
		cdvdman_stat.err = CDVD_ERR_READ;

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncReadCDDA);
}

//--------------------------------------------------------------
static void NCmd_cdSeek(void *ndata)
{
	u32 lsn;
	u8 *wdbuf = (u8 *)ndata;

	lsn = *((u32 *)&wdbuf[0]);

	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncSeek);
}

//--------------------------------------------------------------
static void NCmd_cdStandby(void *ndata)
{
	cdvdman_stat.status = CDVD_STAT_SPIN;

	cdvdman_cb_event(SCECdFuncStandby);
}

//--------------------------------------------------------------
static void NCmd_cdStop(void *ndata)
{
	cdvdman_stat.status = CDVD_STAT_STOP;

	cdvdman_cb_event(SCECdFuncStop);
}

//--------------------------------------------------------------
static void NCmd_cdPause(void *ndata)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncPause);
}

//--------------------------------------------------------------
static int cdSync_blk(void)
{
	cdvdman_waitsignalNCmdsema();

	return 0;
}

//--------------------------------------------------------------
static int cdSync_noblk(void)
{
	return cdvdman_getNCmdstate();
}

#endif // ALT_READ_CORE

//--------------------------------------------------------------
#ifdef USB_DRIVER
static void usbd_init(void)
{
	modinfo_t info;
	getModInfo("usbd\0\0\0\0", &info);

	// Set functions pointers here
	pUsbRegisterDriver = info.exports[4];
	pUsbGetDeviceStaticDescriptor = info.exports[6];
	pUsbSetDevicePrivateData = info.exports[7];
	pUsbOpenEndpoint = info.exports[9];
	pUsbCloseEndpoint = info.exports[10];
	pUsbTransfer = info.exports[11];
	pUsbOpenEndpointAligned = info.exports[12];
}
#endif
#ifdef SMB_DRIVER
static void ps2ip_init(void)
{
	modinfo_t info;
	getModInfo("ps2ip\0\0\0", &info);

	// Set functions pointers here
	plwip_close = info.exports[6];
	plwip_connect = info.exports[7];
	plwip_recvfrom = info.exports[10];
	plwip_send = info.exports[11];
	plwip_socket = info.exports[13];
	pinet_addr = info.exports[24];
}
#endif

//--------------------------------------------------------------
static void fs_init(void)
{
	if (fs_inited)
		return;

	DPRINTF("fs_init\n");

#ifdef USB_DRIVER
	// initialize usbd exports
	usbd_init();

	// initialize the mass driver
	mass_stor_init();

	// configure mass device
	while (mass_stor_configureDevice() <= 0) DelayThread(200);
#endif

#ifdef SMB_DRIVER
	register int i = 0;
	char tmp_str[256];

	ps2ip_init();

	// Open the Connection with SMB server
	smb_NegociateProtocol(cdvdman_settings.pc_ip, cdvdman_settings.pc_port, cdvdman_settings.smb_user, cdvdman_settings.smb_password);

	// open a session
	smb_SessionSetupAndX();

	// Then tree connect on the share resource
	sprintf(tmp_str, "\\\\%s\\%s", cdvdman_settings.pc_ip, cdvdman_settings.pc_share);
	smb_TreeConnectAndX(tmp_str);

	if (!(cdvdman_settings.common.flags&IOPCORE_SMB_FORMAT_USBLD)) {
		if (cdvdman_settings.pc_prefix[0]) {
			sprintf(tmp_str, "%s\\%s\\%s.%s%s", cdvdman_settings.pc_prefix, cdvdman_settings.common.media == 0x12?"CD":"DVD", cdvdman_settings.files.iso.startup, cdvdman_settings.files.iso.title, cdvdman_settings.files.iso.extension);
		} else {
			sprintf(tmp_str, "%s\\%s.%s%s", cdvdman_settings.common.media == 0x12?"CD":"DVD", cdvdman_settings.files.iso.startup, cdvdman_settings.files.iso.title, cdvdman_settings.files.iso.extension);
		}

		smb_OpenAndX(tmp_str, &cdvdman_settings.files.FIDs[i++], 0);
	} else {
		// Open all parts files
		for (i = 0; i < cdvdman_settings.common.NumParts; i++) {
			if (cdvdman_settings.pc_prefix[0])
				sprintf(tmp_str, "%s\\%s.%02x", cdvdman_settings.pc_prefix, cdvdman_settings.filename, i);
			else
				sprintf(tmp_str, "%s.%02x", cdvdman_settings.filename, i);

			smb_OpenAndX(tmp_str, &cdvdman_settings.files.FIDs[i], 0);
		}
	}
#endif

#ifdef HDD_DRIVER
	DPRINTF("fs_init: apa header LBA = %lu\n", cdvdman_settings.lba_start);

	int r = ata_device_sector_io(0, &apaHeader, cdvdman_settings.lba_start, 2, ATA_DIR_READ);
	if (r != 0)
		DPRINTF("fs_init: failed to read apa header %d\n", r);

	// checking HDL's deadfeed magic
	if (apaHeader.checksum != 0xdeadfeed)
		DPRINTF("fs_init: failed to find deadfeed magic\n");

	mips_memcpy(&cdvdman_partspecs, &apaHeader.part_specs[0], sizeof(cdvdman_partspecs));
#endif

	mips_memset(&cdvdman_fdhandles[0], 0, MAX_FDHANDLES * sizeof(FHANDLE));

	// Read the volume descriptor
	sceCdRead0(16, 1, cdvdman_buf, NULL);

	struct dirTocEntry *tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
	layer_info[0].rootDirtocLBA = tocEntryPointer->fileLBA;
	layer_info[0].rootDirtocLength = tocEntryPointer->length;

	// DVD DL support
	if (!(cdvdman_settings.common.flags&IOPCORE_COMPAT_DISABLE_DVDDL)) {
		#ifdef HDD_DRIVER
		int on_dual;
		u32 layer1_start;
		sceCdReadDvdDualInfo(&on_dual, &layer1_start);
		if (on_dual) {
			sceCdRead0(layer1_start + 16, 1, cdvdman_buf, NULL);
			tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
			layer_info[1].rootDirtocLBA = layer1_start + tocEntryPointer->fileLBA;
			layer_info[1].rootDirtocLength = tocEntryPointer->length;
		}
		#else
		u32 volSize = (*((u32 *)&cdvdman_buf[0x50]));
		sceCdRead0(volSize, 1, cdvdman_buf, NULL);
		if ((cdvdman_buf[0x00] == 1) && (!strncmp(&cdvdman_buf[0x01], "CD001", 5))) {
			cdvdman_layer1start = volSize - 16;
			tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
			layer_info[1].rootDirtocLBA = cdvdman_layer1start + tocEntryPointer->fileLBA;
			layer_info[1].rootDirtocLength = tocEntryPointer->length;
		}
		#endif
	}

	fs_inited = 1;
}

//-------------------------------------------------------------------------
#if (defined(HDD_DRIVER) && !defined(HD_PRO)) || defined(SMB_DRIVER)
static void cdvdman_poff_thread(void *arg){
	int stat;

	SleepThread();
	dev9Shutdown();
	sceCdPowerOff(&stat);
}
#endif

static void cdvdman_init(void)
{
#if (defined(HDD_DRIVER) && !defined(HD_PRO)) || defined(SMB_DRIVER)
	iop_thread_t ThreadData;
#endif

	if(!cdvdman_cdinited)
	{
		cdvdman_stat.err = CDVD_ERR_NO;

		USec2SysClock(cdvdman_settings.common.cb_timer, &gCdvdCallback_SysClock);

#ifdef ALT_READ_CORE
		cdvdman_stat.cddiskready = CDVD_READY_NOTREADY;
		cdvdman_sendNCmd(NCMD_INIT, NULL, 0);
		cdvdman_waitsignalNCmdsema();
		cdvdman_stat.cddiskready = CDVD_READY_READY;
#else
		fs_init();
#endif

#if (defined(HDD_DRIVER) && !defined(HD_PRO)) || defined(SMB_DRIVER)
		if(cdvdman_settings.common.flags&IOPCORE_ENABLE_POFF){
			ThreadData.attr=TH_C;
			ThreadData.priority=1;
			ThreadData.stacksize=0x400;
			ThreadData.thread=&cdvdman_poff_thread;
			StartThread(POFFThreadID=CreateThread(&ThreadData), NULL);
		}
#endif

		cdvdman_cdinited = 1;
	}
}

int sceCdInit(int init_mode)
{
	cdvdman_init();
	return 1;
}

//-------------------------------------------------------------------------
int sceCdStandby(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_SPIN;

#ifdef ALT_READ_CORE
	cdvdman_sendNCmd(NCMD_STANDBY, NULL, 0);
#else
	cdvdman_cb_event(SCECdFuncStandby);
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	DPRINTF("sceCdRead lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

#ifdef ALT_READ_CORE
	u8 wdbuf[16];
	cdvdman_stat.err = CDVD_ERR_NO;

	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;

	cdvdman_stat.status = CDVD_STAT_READ;

	cdvdman_sendNCmd(NCMD_READ, (void *)wdbuf, 12);
#else
	cdvdman_stat.status = CDVD_STAT_READ;
	if (QueryIntrContext() || (!(cdvdman_settings.common.flags&IOPCORE_COMPAT_ALT_READ))) {
		if (sync_flag == 1) {
			DPRINTF("sceCdRead: exiting (sync_flag)...\n");
			return 0;
		}

		sync_flag = 1;
		cdvdman_stat.cdread_lba = lsn;
		cdvdman_stat.cdread_sectors = sectors;
		cdvdman_stat.cdread_buf = buf;
		cdvdman_stat.cdread_mode = *mode;

		if (QueryIntrContext())
			iSignalSema(cdvdman_lockreadsema);
		else
			SignalSema(cdvdman_lockreadsema);
	}
	else {
		sync_flag = 1;
		sceCdRead0(lsn, sectors, buf, mode);

		cdvdman_stat.status = CDVD_STAT_PAUSE;

		cdvdman_cb_event(SCECdFuncRead);
		sync_flag = 0;
	}
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdSeek(u32 lsn)
{
	DPRINTF("sceCdSeek %d\n", (int)lsn);

	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	u8 wdbuf[16];

	*((u32 *)&wdbuf[0]) = lsn;

	cdvdman_stat.status = CDVD_STAT_SEEK;

	cdvdman_sendNCmd(NCMD_SEEK, (void *)wdbuf, 4);
#else
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	cdvdman_cb_event(SCECdFuncSeek);
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdGetError(void)
{
	DPRINTF("sceCdGetError %d\n", cdvdman_stat.err);

	return cdvdman_stat.err;
}

//-------------------------------------------------------------------------
int sceCdGetToc(void *toc)
{
	cdvdman_stat.err = CDVD_ERR_READ;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdSearchFile(cd_file_t *pcd_file, const char *name)
{
	DPRINTF("sceCdSearchFile %s\n", name);

	return cdvdman_findfile(pcd_file, name, 0);
}

//-------------------------------------------------------------------------
int sceCdSync(int mode)
{
	DPRINTF("sceCdSync %d sync flag = %d\n", mode, sync_flag);

#ifdef ALT_READ_CORE
	int result;

	switch(mode){
		case 1:
		case 17:
			result = cdSync_noblk();
			break;
		default:
			result = cdSync_blk();
	}

	return result;
#else
	if (!sync_flag)
		return 0;

	if ((mode == 1) || (mode == 17))
		return 1;

	while (sync_flag)
		DelayThread(5000);

	return 0;
#endif
}

//-------------------------------------------------------------------------
static void cdvdman_initDiskType(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef HDD_DRIVER
	fs_init();

	cdvdman_cur_disc_type = (int)apaHeader.discType;
#else
	cdvdman_cur_disc_type = (int)cdvdman_settings.common.media;
#endif
	cdvdman_stat.disc_type_reg = cdvdman_cur_disc_type;
        DPRINTF("DiskType=0x%x\n", cdvdman_cur_disc_type);
}

//-------------------------------------------------------------------------
int sceCdGetDiskType(void)
{
	return 	cdvdman_stat.disc_type_reg;
}

//-------------------------------------------------------------------------
int sceCdDiskReady(int mode)
{
	DPRINTF("sceCdDiskReady %d\n", mode);
	cdvdman_stat.err = CDVD_ERR_NO;

	if (cdvdman_cdinited) {
		if (mode == 0) {
			while (sceCdDiskReady(1) == CDVD_READY_NOTREADY)
				DelayThread(5000);
		}

#ifdef ALT_READ_CORE
		if (!cdvdman_stat.cdNCmd) {
			if (cdvdman_stat.cddiskready)
				return CDVD_READY_READY;
		}
#else
		if (!sync_flag)
			return CDVD_READY_READY;
#endif
	}

	return CDVD_READY_NOTREADY;
}

//-------------------------------------------------------------------------
int sceCdTrayReq(int mode, u32 *traycnt)
{
        DPRINTF("sceCdTrayReq(%d, 0x%lX)\n", mode, *traycnt);

        if (mode == CDVD_TRAY_CHECK) {
                if (traycnt)
                        *traycnt = cdvdman_media_changed;

                cdvdman_media_changed = 0;

                return 1;
        }

        if (mode == CDVD_TRAY_OPEN) {
                cdvdman_stat.status = CDVD_STAT_OPEN;
                cdvdman_stat.disc_type_reg = 0;

                DelayThread(11000);

                cdvdman_stat.err = CDVD_ERR_OPENS; /* not sure about this error code */

                return 1;
        }
        else if (mode == CDVD_TRAY_CLOSE) {
                DelayThread(25000);

                cdvdman_stat.status = CDVD_STAT_PAUSE; /* not sure if the status is right, may be - CDVD_STAT_SPIN */
                cdvdman_stat.err = CDVD_ERR_NO; /* not sure if this error code is suitable here */
                cdvdman_stat.disc_type_reg = cdvdman_cur_disc_type;

                cdvdman_media_changed = 1;

                return 1;
        }

        return 0;
}

//-------------------------------------------------------------------------
int sceCdStop(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	cdvdman_sendNCmd(NCMD_STOP, NULL, 0);
#else
	cdvdman_stat.status = CDVD_STAT_STOP;
	cdvdman_cb_event(SCECdFuncStop);
#endif
	return 1;
}

//-------------------------------------------------------------------------
int sceCdPosToInt(cd_location_t *p)
{	// TODO: Improve with logical ops only

	register int result;

	result = ((u32)p->minute >> 16) *  10 + ((u32)p->minute & 0xF);
	result *= 60;
	result += ((u32)p->second >> 16) * 10 + ((u32)p->second & 0xF);
	result *= 75;
	result += ((u32)p->sector >> 16) *  10 + ((u32)p->sector & 0xF);
	result -= 150;

	return result;
}

//-------------------------------------------------------------------------
cd_location_t *sceCdIntToPos(int i, cd_location_t *p)
{	// TODO: Improve with logical ops only

	register int sc, se, mi;

	i += 150;
	se = i / 75;
	sc = i - se * 75;
	mi = se / 60;
	se = se - mi * 60;
	p->sector = (sc - (sc / 10) * 10) + (sc / 10) * 16;
	p->second = (se / 10) * 16 + se - (se / 10) * 10;
	p->minute = (mi / 10) * 16 + mi - (mi / 10) * 10;

	return p;
}

//-------------------------------------------------------------------------
int sceCdRI(char *buf, int *stat)
{
	u8 rdbuf[16];

	cdvdman_sendSCmd(0x12, NULL, 0, rdbuf, 9);

	if (stat)
		*stat = (int)rdbuf[0];

	mips_memcpy((void *)buf, (void *)&rdbuf[1], 8);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadClock(cd_clock_t *rtc)
{
	int rc;

	cdvdman_stat.err = CDVD_ERR_NO;

	rc = cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);

	rtc->week = 0;
	rtc->month &= 0x7f;

	return rc;
}

//-------------------------------------------------------------------------
int sceCdStatus(void)
{
	DPRINTF("sceCdStatus %d\n", cdvdman_stat.status);

	return cdvdman_stat.status;
}

//-------------------------------------------------------------------------
int sceCdApplySCmd(int cmd, void *in, u32 in_size, void *out)
{
	DPRINTF("sceCdApplySCmd\n");

	return cdvdman_sendSCmd(cmd & 0xff, in, in_size, out, 16);
}

//-------------------------------------------------------------------------
int *sceCdCallback(void *func)
{
	int oldstate;
	void *old_cb;

	DPRINTF("sceCdCallback %p\n", func);

	old_cb = user_cb;

	CpuSuspendIntr(&oldstate);

	user_cb = func;

	CpuResumeIntr(oldstate);

	return (int *)old_cb;
}

//-------------------------------------------------------------------------
int sceCdPause(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

#ifdef ALT_READ_CORE
	cdvdman_sendNCmd(NCMD_PAUSE, NULL, 0);
#else
	cdvdman_stat.status = CDVD_STAT_PAUSE;
	cdvdman_cb_event(SCECdFuncPause);
#endif

	return 1;
}

//-------------------------------------------------------------------------
int sceCdBreak(void)
{
	DPRINTF("sceCdBreak\n");

	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

#ifdef ALT_READ_CORE
	if (QueryIntrContext() == 0) {
		while (cdvdman_stat.cdNCmd)
			DelayThread(100000);
	}
#endif

	cdvdman_stat.err = CDVD_ERR_ABRT;
	cdvdman_cb_event(SCECdFuncBreak);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadCdda(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
#ifdef	ALT_READ_CORE
	u8 wdbuf[16];
	cdvdman_stat.err = CDVD_ERR_NO;

	*((u32 *)&wdbuf[0]) = lsn;
	*((u32 *)&wdbuf[4]) = sectors;
	*((u32 *)&wdbuf[8]) = (u32)buf;

	cdvdman_stat.status = CDVD_STAT_READ;

	cdvdman_sendNCmd(NCMD_READCDDA, (void *)wdbuf, 12);

	return 1;
#else
	return sceCdRead(lsn, sectors, buf, mode);
#endif
}

//-------------------------------------------------------------------------
int sceCdGetReadPos(void)
{
	DPRINTF("sceCdGetReadPos\n");

	return ReadPos;
}

//-------------------------------------------------------------------------
void *sceGetFsvRbuf(void)
{
	return cdvdman_fs_buf;
}

//-------------------------------------------------------------------------
int sceCdSC(int code, int *param)
{
	int result;

        DPRINTF("sceCdSC(0x%X, 0x%X)\n", code, *param);

	switch(code){
		case 0xFFFFFFF5:
			result=cdvdman_stat.intr_ef;
			break;
		case 0xFFFFFFF6:
			if (*param)
			{
				WaitSema(cdrom_io_sema);
			}
			else SignalSema(cdrom_io_sema);

			result = *param;	//EE N-command code.
			break;
		case 0xFFFFFFF7:
			result=CDVDMAN_MODULE_VERSION;
			break;
		case 0xFFFFFFF0:
			*param = (int)&cdvdman_debug_print_flag;
			result=0xFF;
			break;
		default:
			result=1;	// dummy result
        }

        return result;
}

//-------------------------------------------------------------------------
int sceCdRC(cd_clock_t *rtc)
{
	cdvdman_stat.err = CDVD_ERR_NO;

	return cdvdman_sendSCmd(0x08, NULL, 0, (void *)rtc, 8);
}

//-------------------------------------------------------------------------
int sceCdStInit(u32 bufmax, u32 bankmax, void *iop_bufaddr)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.StreamingData.stat = 0;
	cdvdman_stat.StreamingData.bufmax = bufmax;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStRead(u32 sectors, void *buf, u32 mode, u32 *err)
{
	sceCdRead0(cdvdman_stat.StreamingData.lsn, sectors, buf, NULL);
	cdvdman_stat.StreamingData.lsn += sectors;

	if (err)
		*err = sceCdGetError();

	return sectors;
}

//-------------------------------------------------------------------------
int sceCdStSeek(u32 lsn)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.StreamingData.lsn = lsn;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStStart(u32 lsn, cd_read_mode_t *mode)
{
	if (mode->datapattern)
		cdvdman_stat.err = CDVD_ERR_READ;
	else {
		cdvdman_stat.err = CDVD_ERR_NO;
		cdvdman_stat.StreamingData.lsn = lsn;
		cdvdman_stat.StreamingData.stat = 0;
		cdvdman_stat.status = CDVD_STAT_PAUSE;
	}

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStStat(void)
{
	if (cdvdman_stat.StreamingData.stat == 1)
		return 0;

	return cdvdman_stat.StreamingData.bufmax;
}

//-------------------------------------------------------------------------
int sceCdStStop(void)
{
	cdvdman_stat.StreamingData.stat = 1;
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	return 1;
}

//-------------------------------------------------------------------------
#ifndef HDD_DRIVER
static int cdvdman_ReadSect(u32 lsn, u32 nsectors, void *buf)
{
	register u32 r, sectors_to_read, lbound, ubound, nlsn, offslsn;
	register int i, esc_flag = 0;
	u8 *p = (u8 *)buf;

	lbound = 0;
	ubound = (cdvdman_settings.common.NumParts > 1) ? 0x80000 : 0xFFFFFFFF;
	offslsn = lsn;
	r = nlsn = 0;
	sectors_to_read = nsectors;

	for (i=0; i<cdvdman_settings.common.NumParts; i++, lbound=ubound, ubound+=0x80000, offslsn-=0x80000) {

		if (lsn>=lbound && lsn<ubound){
			if ((lsn + nsectors) > (ubound-1)) {
				sectors_to_read = ubound - lsn;
				nsectors -= sectors_to_read;
				nlsn = ubound;
			}
			else
				esc_flag = 1;

#ifdef USB_DRIVER
			mass_stor_ReadCD(offslsn, sectors_to_read, &p[r], i);
#endif
#ifdef SMB_DRIVER
			smb_ReadCD(offslsn, sectors_to_read, &p[r], i);
#endif
			r += sectors_to_read << 11;
			sectors_to_read = nsectors;
			lsn = nlsn;
		}

		if (esc_flag)
			break;
	}

	return 1;
}
#endif

//-------------------------------------------------------------------------
static int cdvdman_read(unsigned int lsn, unsigned int sectors, void *buf, cd_read_mode_t *mode){
	DPRINTF("sceCdRead0 lsn=%d sectors=%d buf=%08x\n", (int)lsn, (int)sectors, (int)buf);

	cdvdman_stat.err = CDVD_ERR_NO;

	#ifdef HDD_DRIVER
	u32 offset = 0;
	while (sectors) {
		if (!((lsn >= cdvdman_partspecs.part_offset) && (lsn < (cdvdman_partspecs.part_offset + (cdvdman_partspecs.part_size >> 11)))))
			cdvdman_get_part_specs(lsn);

		if (cdvdman_stat.err != CDVD_ERR_NO)
			break;

		u32 nsectors = (cdvdman_partspecs.part_offset + (cdvdman_partspecs.part_size >> 11)) - lsn;
		if (sectors < nsectors)
			nsectors = sectors;

		u32 lba = cdvdman_partspecs.data_start + ((lsn - cdvdman_partspecs.part_offset) << 2);	
		if (ata_device_sector_io(0, (void *)(buf + offset), lba, nsectors << 2, ATA_DIR_READ) != 0) {
			cdvdman_stat.err = CDVD_ERR_READ;
			break;
		}
		offset += nsectors << 11;
		sectors -= nsectors;
		lsn += nsectors;
		ReadPos += nsectors << 11;
	}

	#else
	cdvdman_ReadSect(lsn, sectors, buf);
	#endif

	return(cdvdman_stat.err==CDVD_ERR_NO?0:1);
}

int sceCdRead0(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode)
{
	if (QueryIntrContext()) {
		DPRINTF("sceCdRead0 exiting (Intr context)...\n");
		return 0;
	}

	WaitSema(cdvdman_cdreadsema);

	if ((u32)(buf) & 3) {
		WaitSema(cdvdman_searchfilesema);

		u32 nsectors, nbytes;
		u32 rpos = lsn;

		while (sectors > 0) {
			nsectors = sectors;
			if (nsectors > CDVDMAN_BUF_SECTORS)
				nsectors = CDVDMAN_BUF_SECTORS;

			DPRINTF("sceCdRead0 lsn=%d rpos=%d nsectors=%d buf=%08x\n", (int)lsn, (int)rpos, (int)nsectors, (int)buf);

			cdvdman_read(rpos, nsectors, cdvdman_buf, mode);

			rpos += nsectors;
			sectors -= nsectors;
			nbytes = nsectors << 11;

			mips_memcpy(buf, cdvdman_buf, nbytes);

			buf = (void *)(buf + nbytes);
		}

		SignalSema(cdvdman_searchfilesema);
	}
	else {
		cdvdman_read(lsn, sectors, buf, mode);
	}

	ReadPos = 0;	/* Reset the buffer offset indicator. */
	SignalSema(cdvdman_cdreadsema);

	return 1;
}

//-------------------------------------------------------------------------
static int cdvdman_readMechaconVersion(char *mname, u32 *stat)
{
	u8 rdbuf[16];
	u8 wrbuf[16];

	wrbuf[0] = 0;
	cdvdman_sendSCmd(0x03, wrbuf, 1, rdbuf, 4);

	*stat = rdbuf[0] & 0x80;
	rdbuf[0] &= 0x7f;

	mips_memcpy(mname, &rdbuf[0], 4);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdRM(char *m, u32 *stat)
{
	register int r;
	u8 rdbuf[16];
	u8 wrbuf[16];

	*stat = 0;
	r = cdvdman_readMechaconVersion(rdbuf, stat);
	if ((r == 1) && (0x104FE < (rdbuf[3] | (rdbuf[2] << 8) | (rdbuf[1] << 16)))) {

		mips_memcpy(&m[0], "M_NAME_UNKNOWN\0\0", 16);
		*stat |= 0x40;
	}
	else {
		wrbuf[0] = 0;
		cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

		*stat = rdbuf[0];
		mips_memcpy(&m[0], &rdbuf[1], 8);

		wrbuf[0] = 8;
		cdvdman_sendSCmd(0x17, wrbuf, 1, rdbuf, 9);

		*stat |= rdbuf[0];
		mips_memcpy(&m[8], &rdbuf[1], 8);
	}

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStPause(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStResume(void)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.status = CDVD_STAT_PAUSE;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdStSeekF(u32 lsn)
{
	cdvdman_stat.err = CDVD_ERR_NO;
	cdvdman_stat.StreamingData.lsn = lsn;

	return 1;
}

//-------------------------------------------------------------------------
int sceCdPowerOff(int *stat)
{
	return cdvdman_sendSCmd(0x0F, NULL, 0, (unsigned char *)stat, 1);
}

//-------------------------------------------------------------------------
int sceCdReadDiskID(void *DiskID)
{
	int i;
	u8 *p = (u8 *)DiskID;

	for (i=0; i<5; i++) {
		if (p[i]!=0)
			break;
	}
	if (i == 5)
		*((u16 *)DiskID) = (u16)0xadde;
	else
		mips_memcpy(DiskID, cdvdman_settings.common.DiscID, 5);

	return 1;
}

//-------------------------------------------------------------------------
int sceCdReadDvdDualInfo(int *on_dual, u32 *layer1_start)
{
	if (cdvdman_settings.common.flags&IOPCORE_COMPAT_DISABLE_DVDDL) {
		*layer1_start = 0;
		*on_dual = 1;
	}
	else {
		#ifdef HDD_DRIVER
		*layer1_start = apaHeader.layer1_start;
		*on_dual = (apaHeader.layer1_start > 0) ? 1 : 0;
		#else
		*layer1_start = cdvdman_layer1start;
		*on_dual = (cdvdman_layer1start > 0) ? 1 : 0;
		#endif
	}

	return 1;
}

//-------------------------------------------------------------------------
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer)
{
	DPRINTF("sceCdLayerSearchFile %s\n", name);

	return cdvdman_findfile((cd_file_t *)fp, name, layer);
}

//--------------------------------------------------------------
int cdvdman_readID(int mode, u8 *buf)
{
	u8 lbuf[16];
	int stat;
	int r;

	r = sceCdRI(lbuf, &stat);
	if ((r == 0) || (stat))
		return 0;

	if (mode == 0) { // GUID
		u32 *GUID0 = (u32 *)&buf[0];
		u32 *GUID1 = (u32 *)&buf[4];
		*GUID0 = lbuf[0] | 0x08004600;
		*GUID1 = *(u32 *)&lbuf[4];
	}
	else { // ModelID
		u32 *ModelID = (u32 *)&buf[0];
		*ModelID = (*(u32 *)&lbuf[0]) >> 8;
	}

	return 1;
}

//--------------------------------------------------------------
int sceCdReadGUID(void *GUID)
{
	return cdvdman_readID(0, GUID);
}

//--------------------------------------------------------------
int sceCdReadModelID(void *ModelID)
{
	return cdvdman_readID(1, ModelID);
}

//--------------------------------------------------------------
static int cdrom_dummy(void)
{
	return -EPERM;
}

//--------------------------------------------------------------
static int cdrom_init(iop_device_t *dev)
{
	iop_sema_t smp;

	DPRINTF("cdrom_init\n");

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	cdrom_io_sema = CreateSema(&smp);

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	cdvdman_searchfilesema = CreateSema(&smp);

	return 0;
}

//--------------------------------------------------------------
static int cdrom_deinit(iop_device_t *dev)
{
	DPRINTF("cdrom_deinit\n");

	DeleteSema(cdrom_io_sema);
	DeleteSema(cdvdman_searchfilesema);

	return 0;
}

//--------------------------------------------------------------
static FHANDLE *cdvdman_getfilefreeslot(void)
{
	register int i;
	FHANDLE *fh;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&cdvdman_fdhandles[i];
		if (fh->f == NULL)
			return fh;
	}

	return 0;
}

//--------------------------------------------------------------
static int cdrom_open(iop_file_t *f, const char *filename, int mode)
{
	register int r = 0;
	FHANDLE *fh;
	cd_file_t cdfile;

	if (!filename)
		return -ENOENT;

	WaitSema(cdrom_io_sema);

	cdvdman_init();

	DPRINTF("cdrom_open %s mode=%d\n", filename, mode);

	fh = cdvdman_getfilefreeslot();
	if (fh) {
		r = cdvdman_findfile(&cdfile, filename, f->unit);
		if (r) {
			f->privdata = fh;
			fh->f = f;
			if (!(cdvdman_settings.common.flags&IOPCORE_COMPAT_DISABLE_DVDDL)) {
				if (f->mode == 0)
					f->mode = r;
			}
			fh->filesize = cdfile.size;
			fh->lsn = cdfile.lsn;
			fh->position = 0;
			r = 0;

		}
		else
			r = -ENOENT;
	}
	else
		r = -EMFILE;

	DPRINTF("cdrom_open ret=%d lsn=%d size=%d\n", r, (int)fh->lsn, (int)fh->filesize);

	SignalSema(cdrom_io_sema);

	return r;
}

//--------------------------------------------------------------
static int cdrom_close(iop_file_t *f)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_close\n");

	if (fh)
		mips_memset(fh, 0, sizeof(FHANDLE));

	SignalSema(cdrom_io_sema);

	return 0;
}

//--------------------------------------------------------------
static int cdrom_read(iop_file_t *f, void *buf, int size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int rpos, sectorpos;
	register u32 nsectors, nbytes;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_read size=%d file_position=%d\n", size, fh->position);

	rpos = 0;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	while (size) {
		nbytes = CDVDMAN_FS_BUFSIZE;
		if (size < nbytes)
			nbytes = size;

		nsectors = nbytes >> 11;
		sectorpos = fh->position & 2047;

		if (sectorpos)
			nsectors++;

		if (nbytes & 2047)
			nsectors++;

		sceCdRead0(fh->lsn + ((fh->position & -2048) >> 11), nsectors, cdvdman_fs_buf, NULL);
		mips_memcpy(buf, &cdvdman_fs_buf[sectorpos], nbytes);

		rpos += nbytes;
		buf += nbytes;
		size -= nbytes;
		fh->position += nbytes;
	}

	DPRINTF("cdrom_read ret=%d\n", (int)rpos);
	SignalSema(cdrom_io_sema);

	return rpos;
}

//--------------------------------------------------------------
static int cdrom_lseek(iop_file_t *f, u32 offset, int where)
{
	register int r;
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_lseek offset=%ld where=%d\n", offset, where);

	switch (where) {
		case SEEK_CUR:
			r = fh->position + offset;
			if (r > fh->filesize) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_SET:
			r = offset;
			if (fh->filesize < offset) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_END:
			r = fh->filesize - offset;
			break;
		default:
			r = -EINVAL;
			goto ssema;
	}

	fh->position = r;
	if (fh->position > fh->filesize)
		fh->position = fh->filesize;

ssema:
	DPRINTF("cdrom_lseek file offset=%d\n", (int)fh->position);
	SignalSema(cdrom_io_sema);

	return r;
}

//--------------------------------------------------------------
static int cdrom_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
	return sceCdLayerSearchFile((cdl_file_t *)&stat->attr, filename, f->unit) - 1;
}

//--------------------------------------------------------------
static int cdrom_dopen(iop_file_t *f, const char *dirname)
{
	return cdrom_open(f, dirname, 8);
}

//--------------------------------------------------------------
static int cdrom_dread(iop_file_t *f, iox_dirent_t *dirent)
{
	register int r = 0;
	register u32 mode;
	FHANDLE *fh = (FHANDLE *)f->privdata;
	struct dirTocEntry *tocEntryPointer;

	WaitSema(cdrom_io_sema);

	DPRINTF("cdrom_dread fh->lsn=%lu\n", fh->lsn);

	sceCdRead0(fh->lsn, 1, cdvdman_fs_buf, NULL);

	do {
		r = 0;
		tocEntryPointer = (struct dirTocEntry *)&cdvdman_fs_buf[fh->position];
		if (tocEntryPointer->length == 0)
			break;

		fh->position += tocEntryPointer->length;
		r = 1;
	}
	while (tocEntryPointer->filenameLength == 1);

	mode = 0x2124;
	if (tocEntryPointer->fileProperties & 2)
		mode = 0x116d;

	dirent->stat.mode = mode;
	dirent->stat.size = tocEntryPointer->fileSize;
	strncpy(dirent->name, tocEntryPointer->filename, 256);

	DPRINTF("cdrom_dread r=%d mode=%04x name=%s\n", r, (int)mode, dirent->name);

	SignalSema(cdrom_io_sema);

	return r;
}

//--------------------------------------------------------------
static int cdrom_dclose(iop_file_t *f)
{
	return cdrom_close(f);
}

//--------------------------------------------------------------
static int cdrom_ioctl(iop_file_t *f, u32 cmd, void *args)
{
	register int r = 0;

	WaitSema(cdrom_io_sema);

	if (cmd != 0x10000)	// Spin Ctrl op
		r = -EINVAL;

	SignalSema(cdrom_io_sema);

	return r;
}

//--------------------------------------------------------------
static s64 cdrom_lseek64(iop_file_t *f, s64 pos, int where)
{
	DPRINTF("cdrom_lseek64 where=%d\n", (int)where);
	return (s64)cdrom_lseek(f, (u32)pos, where);
}

//--------------------------------------------------------------
static int cdrom_ioctl2(iop_file_t *f, int cmd, void *args, unsigned int arglen, void *buf, unsigned int buflen)
{
	int r = 0;

	WaitSema(cdrom_io_sema);

	switch(cmd){
		case CIOCSTREAMPAUSE:
			sceCdStPause();
			break;
		case CIOCSTREAMRESUME:
			sceCdStResume();
			break;
		case CIOCSTREAMSTAT:
			r = sceCdStStat();
			break;
		default:
			r = -EINVAL;
	}

	SignalSema(cdrom_io_sema);

	return r;
}

//--------------------------------------------------------------
static int cdrom_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
	int result;

	if (!name)
		return -ENOENT;

	WaitSema(cdrom_io_sema);

	result = 0;
	switch(cmd){
		case CDIOC_READCLOCK:
			sceCdReadClock((cd_clock_t *)buf);
			break;
		case CDIOC_READGUID:
			sceCdReadGUID(buf);
			break;
		case CDIOC_READDISKGUID:
			sceCdReadDiskID(buf);
			break;
		case CDIOC_GETDISKTYPE:
			*(int*)buf = sceCdGetDiskType();
			break;
		case CDIOC_GETERROR:
			*(int *)buf = sceCdGetError();
			break;
		case CDIOC_TRAYREQ:
			sceCdTrayReq(*(int *)args, (u32 *)buf);
			break;
		case CDIOC_STATUS:
			*(int *)buf = sceCdStatus();
			break;
		case CDIOC_POWEROFF:
		case CDIOC_MMODE:
			result = 1;
			break;
		case CDIOC_DISKRDY:
			*(int *)buf = sceCdDiskReady(*(int *)args);
			break;
		case CDIOC_READMODELID:
			sceCdReadModelID(buf);
			break;
		case CDIOC_STREAMINIT:
			sceCdStInit(((u32*)args)[0], ((u32*)args)[1], (void *)((u32*)args)[2]);
			break;
		case CDIOC_BREAK:
			sceCdBreak();
			break;
		case CDIOC_SPINNOM:
		case CDIOC_SPINSTM:
		case CDIOC_TRYCNT:
		case CDIOC_READDVDDUALINFO:
		case CDIOC_INIT:
			result = 0;
			break;
		case CDIOC_STANDBY:
			sceCdStandby();
			break;
		case CDIOC_STOP:
			sceCdStop();
			break;
		case CDIOC_PAUSE:
			sceCdPause();
			break;
		case CDIOC_GETTOC:
			sceCdGetToc(buf);
			break;
		case CDIOC_GETINTREVENTFLG:
			*(int *)buf = cdvdman_stat.intr_ef;
			result = cdvdman_stat.intr_ef;
			break;
		default:
			result = -EIO;
			break;
	}

	SignalSema(cdrom_io_sema);

	return result;
}

//-------------------------------------------------------------------------
static void cdvdman_trimspaces(char* str)
{
	int i, len;
	char *p;

	len = strlen(str);
	if (len == 0)
		return;

	i = len - 1;

	for (i = len-1; i != -1; i--) {
		p = &str[i];
		if ((*p != 0x20) && (*p != 0x2e))
			break;
		*p = 0;
	}
}

//-------------------------------------------------------------------------
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer)
{
	char *p = (char *)name;
	char *slash;
	int r, len, filename_len;
	int tocPos;
	struct dirTocEntry *tocEntryPointer;

lbl_startlocate:
	DPRINTF("cdvdman_locatefile start locating, layer=%d\n", layer);

	while (*p == '/')
		p++;

	while (*p == '\\')
		p++;

	slash = strchr(p, '/');

	// if the path doesn't contain a '/' then look for a '\'
	if (!slash)
		slash = strchr (p, '\\');

	len = (u32)slash - (u32)p;

	// if a slash was found
	if (slash != NULL) {

		if (len >= 256)
			return NULL;

		// copy the path into main 'dir' var
		strncpy(cdvdman_dirname, p, len);
		cdvdman_dirname[len] = 0;
	}
	else {
		if (strlen(p) >= 256)
			return NULL;

		strcpy(cdvdman_dirname, p);
	}

	while (tocLength > 0)	{
		sceCdRead0(tocLBA, 1, cdvdman_buf, NULL);
		DPRINTF("cdvdman_locatefile tocLBA read done\n");

		tocLength -= 2048;
		tocLBA++;

		tocPos = 0;
		do {
			tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[tocPos];

			if (tocEntryPointer->length == 0)
				break;

			filename_len = tocEntryPointer->filenameLength;
			if (filename_len) {
				strncpy(cdvdman_curdir, tocEntryPointer->filename, 256); // copy filename
				cdvdman_curdir[filename_len] = 0;

				DPRINTF("cdvdman_locatefile strcmp %s %s\n", cdvdman_dirname, cdvdman_curdir);

				r = strcmp(cdvdman_dirname, cdvdman_curdir);
				if ((!r) && (!slash)) { // we searched a file so it's found
					DPRINTF("cdvdman_locatefile found file! LBA=%d size=%d\n", (int)tocEntryPointer->fileLBA, (int)tocEntryPointer->fileSize);
					return tocEntryPointer;
				}
				else if ((!r) && (tocEntryPointer->fileProperties & 2)) { // we found it but it's a directory
					tocLBA = tocEntryPointer->fileLBA;
					tocLength = tocEntryPointer->fileSize;
					p = &slash[1];

					if (!(cdvdman_settings.common.flags&IOPCORE_COMPAT_DISABLE_DVDDL)) {
						int on_dual;
						u32 layer1_start;
						sceCdReadDvdDualInfo(&on_dual, &layer1_start);

						if (layer)
							tocLBA += layer1_start;
					}

					goto lbl_startlocate;
				}
				else {
					tocPos += (tocEntryPointer->length << 16) >> 16;
				}
			}
		}
		while (tocPos < 2016);
	}

	DPRINTF("cdvdman_locatefile file not found!!!\n");

	return NULL;
}

//-------------------------------------------------------------------------
static int cdvdman_findfile(cd_file_t *pcdfile, const char *name, int layer)
{
	char *p;
	int i;
	u32 lsn;
	struct dirTocEntry *tocEntryPointer;

	if ((!pcdfile) || (!name))
		return 0;

	WaitSema(cdvdman_searchfilesema);

	DPRINTF("cdvdman_findfile %s layer%d\n", name, layer);

	strncpy(cdvdman_filepath, name, sizeof(cdvdman_filepath));
	//Correct filenames (for files), if necessary.
	if((p=strrchr(cdvdman_filepath, '.'))!=NULL){
		for(i=0,p++; i<3 && (*p!='\0'); i++,p++){
			if(p[0]==';') break;
		}

		if(p-cdvdman_filepath+3<sizeof(cdvdman_filepath)){
			p[0]=';';
			p[1]='1';
			p[2]='\0';
		}
	}

	cdvdman_trimspaces(cdvdman_filepath);

	DPRINTF("cdvdman_findfile cdvdman_filepath=%s\n", cdvdman_filepath);

	if (cdvdman_settings.common.flags&IOPCORE_COMPAT_DISABLE_DVDDL)
		layer = 0;

	if (layer < 2) {
		if (layer_info[layer].rootDirtocLBA == 0) {
			SignalSema(cdvdman_searchfilesema);
			return 0;
		}

		tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, layer_info[layer].rootDirtocLBA, layer_info[layer].rootDirtocLength, layer);
		if (tocEntryPointer == NULL) {
			SignalSema(cdvdman_searchfilesema);
			return 0;
		}

		lsn = tocEntryPointer->fileLBA;
		if (layer) {
			sceCdReadDvdDualInfo((int *)&pcdfile->lsn, &pcdfile->size);
			lsn += pcdfile->size;
		}

		pcdfile->lsn = lsn;
		if ((cdvdman_settings.common.flags&IOPCORE_COMPAT_0_PSS) && \
			((!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath)-6], ".PSS", 4)) || \
			(!strncmp(&cdvdman_filepath[strlen(cdvdman_filepath)-6], ".pss", 4))))
			pcdfile->size = 0;
		else
			pcdfile->size = tocEntryPointer->fileSize;

		strcpy(pcdfile->name, strrchr(name, '\\')+1);
	}
	else {
		SignalSema(cdvdman_searchfilesema);
		return 0;
	}

	DPRINTF("cdvdman_findfile found %s\n", name);

	SignalSema(cdvdman_searchfilesema);

	return 1;
}

//-------------------------------------------------------------------------
static int cdvdman_writeSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
	int i;
	u8 dummy;
	u8 *p;
	u8 rdbuf[64];

	WaitSema(cdvdman_cdreadsema);

	if (CDVDreg_SDATAIN & 0x80) {
		SignalSema(cdvdman_cdreadsema);
		return 0;
	}

	if (!(CDVDreg_SDATAIN & 0x40)) {
		do {
			dummy = CDVDreg_SDATAOUT;
		} while (!(CDVDreg_SDATAIN & 0x40));
	}

	if (in_size > 0) {
		for (i = 0; i < in_size; i++) {
			p = (void *)(in + i);
			CDVDreg_SDATAIN = *p;
		}
	}

	CDVDreg_SCOMMAND = cmd;
	dummy = CDVDreg_SCOMMAND;

	while (CDVDreg_SDATAIN & 0x80) {;}

	i = 0;
	if (!(CDVDreg_SDATAIN & 0x40)) {
		do {
			p = (void *)(rdbuf + i);
			*p = CDVDreg_SDATAOUT;
			i++;
		} while (!(CDVDreg_SDATAIN & 0x40));
	}

	if (out_size > i)
		out_size = i;

	mips_memcpy((void *)out, (void *)rdbuf, out_size);

	SignalSema(cdvdman_cdreadsema);

	return 1;
}

//--------------------------------------------------------------
static int cdvdman_sendSCmd(u8 cmd, void *in, u32 in_size, void *out, u32 out_size)
{
	int r, retryCount = 0;

retry:

	r = cdvdman_writeSCmd(cmd & 0xff, in, in_size, out, out_size);
	if (r == 0) {
		DelayThread(2000);
		if (++retryCount <= 2500)
			goto retry;
	}

	DelayThread(2000);

	return 1;
}

//--------------------------------------------------------------
struct cdvdman_cb_data{
	void (*user_cb)(int reason);
	int reason;
};

static int cdvdman_cb_event(int reason)
{
	static struct cdvdman_cb_data cb_data;

	if (user_cb) {

		cb_data.user_cb = user_cb;
		cb_data.reason = reason;

		DPRINTF("cdvdman_cb_event reason: %d - setting cb alarm...\n", reason);

		if (QueryIntrContext())
			iSetAlarm(&gCdvdCallback_SysClock, &event_alarm_cb, &cb_data);
		else
			SetAlarm(&gCdvdCallback_SysClock, &event_alarm_cb, &cb_data);
	}

	return 1;
}

//-------------------------------------------------------------------------
static unsigned int event_alarm_cb(void *args)
{
	struct cdvdman_cb_data *cb_data=args;

	cb_data->user_cb(cb_data->reason);
	return 0;
}

//-------------------------------------------------------------------------
#ifndef ALT_READ_CORE
static void cdvdman_cdread_Thread(void *args)
{
	while (1) {
		WaitSema(cdvdman_lockreadsema);

		sceCdRead0(cdvdman_stat.cdread_lba, cdvdman_stat.cdread_sectors, cdvdman_stat.cdread_buf, &cdvdman_stat.cdread_mode);

		sync_flag = 0;
		cdvdman_stat.status = CDVD_STAT_PAUSE;

		cdvdman_cb_event(SCECdFuncRead);
	}
}

//-------------------------------------------------------------------------
static void cdvdman_startThreads(void)
{
	iop_thread_t thread_param;
	register int thid;

	cdvdman_stat.status = CDVD_STAT_PAUSE;
	cdvdman_stat.err = CDVD_ERR_NO;

	thread_param.thread = &cdvdman_cdread_Thread;
	thread_param.stacksize = 0x400;
	thread_param.priority = 0x0f;
	thread_param.attr = TH_C;
	thread_param.option = 0;

	thid = CreateThread(&thread_param);
	StartThread(thid, NULL);
}
#endif

//-------------------------------------------------------------------------
static void cdvdman_create_semaphores(void)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_cdreadsema = CreateSema(&smp);

#ifndef ALT_READ_CORE
	smp.initial = 0;
	smp.max = 1;
	smp.attr = 0;
	smp.option = 0;

	cdvdman_lockreadsema = CreateSema(&smp);
#endif
}

//-------------------------------------------------------------------------
static void cdvdman_initdev(void)
{
	iop_event_t event;

	event.attr = 2;
	event.option = 0;
	event.bits = 0;

	cdvdman_stat.intr_ef = CreateEventFlag(&event);
	ClearEventFlag(cdvdman_stat.intr_ef, 0);

	DelDrv("cdrom");
	AddDrv((iop_device_t *)&cdrom_dev);
}

//-------------------------------------------------------------------------
#ifdef HDD_DRIVER
static void cdvdman_get_part_specs(u32 lsn)
{
	register int i;
	cdvdman_partspecs_t *ps = (cdvdman_partspecs_t *)&apaHeader.part_specs[0];

	for (i = 0; i < apaHeader.num_partitions; i++) {
		if (lsn >= ps->part_offset) {
			if (lsn < (ps->part_offset + (ps->part_size >> 11))) {
				mips_memcpy(&cdvdman_partspecs, ps, sizeof(cdvdman_partspecs));
				break;
			}
		}
		ps++;
	}

	if (i >= apaHeader.num_partitions)
		cdvdman_stat.err = CDVD_ERR_READ;
}
#endif

//-------------------------------------------------------------------------
static int intrh_cdrom(void *common){
	if(CDVDreg_PWOFF&CDL_DATA_RDY) CDVDreg_PWOFF=CDL_DATA_RDY;

	if(CDVDreg_PWOFF&CDL_DATA_END){
		//If IGR is enabled: Do not acknowledge the interrupt here. The EE-side IGR code will monitor and acknowledge it.
		if(cdvdman_settings.common.flags&IOPCORE_ENABLE_POFF){
			CDVDreg_PWOFF=CDL_DATA_END;	//Acknowldge power-off request.
		}
		iSetEventFlag(cdvdman_stat.intr_ef, 0x14);	//Notify FILEIO and CDVDFSV of the power-off event.

		//Call power-off callback here. OPL doesn't handle one, so do nothing.
#if (defined(HDD_DRIVER) && !defined(HD_PRO)) || defined(SMB_DRIVER)
		if(cdvdman_settings.common.flags&IOPCORE_ENABLE_POFF){
			//If IGR is disabled, switch off the console.
			iWakeupThread(POFFThreadID);
		}
#endif
	}
	else CDVDreg_PWOFF=CDL_DATA_COMPLETE;	//Acknowledge interrupt

	return 1;
}

static inline void InstallIntrHandler(void){
	RegisterIntrHandler(IOP_IRQ_CDVD, 1, &intrh_cdrom, NULL);
	EnableIntr(IOP_IRQ_CDVD);

	//Acknowledge hardware events (e.g. poweroff)
	if(CDVDreg_PWOFF&CDL_DATA_END) CDVDreg_PWOFF=CDL_DATA_END;
	if(CDVDreg_PWOFF&CDL_DATA_RDY) CDVDreg_PWOFF=CDL_DATA_RDY;
}

int _start(int argc, char **argv)
{
	// register exports
	RegisterLibraryEntries(&_exp_cdvdman);
	RegisterLibraryEntries(&_exp_cdvdstm);
#ifdef SMB_DRIVER
	RegisterLibraryEntries(&_exp_dev9);
	dev9d_init();
#endif
#ifdef HDD_DRIVER
#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
	RegisterLibraryEntries(&_exp_dev9);
#endif
#else
	RegisterLibraryEntries(&_exp_dev9);
#endif
	RegisterLibraryEntries(&_exp_atad);

#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
	dev9d_init();
#endif
#else
	dev9d_init();
#endif
	atad_start();

	atad_inited = 1;
#endif
#ifdef USB_DRIVER
#ifdef __USE_DEV9
	RegisterLibraryEntries(&_exp_dev9);
	dev9d_init();
#endif
#endif
	RegisterLibraryEntries(&_exp_smsutils);
#ifdef VMC_DRIVER
	RegisterLibraryEntries(&_exp_oplutils);
#endif
	// create SCMD/searchfile semaphores
	cdvdman_create_semaphores();

	// start cdvdman threads
#ifdef ALT_READ_CORE
	cdvdman_startNCmdthread();
#else
	cdvdman_startThreads();
#endif
	// register cdrom device driver
	cdvdman_initdev();
	InstallIntrHandler();

#ifdef HDD_DRIVER
	if (cdvdman_settings.common.media != 0x69)
		lba_48bit = cdvdman_settings.common.media;
#endif

	// hook MODLOAD's exports
	hookMODLOAD();

	// init disk type stuff
	cdvdman_initDiskType();

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
#ifdef SMB_DRIVER
	smb_Disconnect();
#endif

	return 0;
}

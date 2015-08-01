/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#define PKO_PORT        0x4711
#define PKO_CMD_PORT	0x4712
#define PKO_PRINTF_PORT	0x4712

#define PKO_OPEN_CMD     0xbabe0111
#define PKO_OPEN_RLY     0xbabe0112
#define PKO_CLOSE_CMD    0xbabe0121
#define PKO_CLOSE_RLY    0xbabe0122
#define PKO_READ_CMD     0xbabe0131
#define PKO_READ_RLY     0xbabe0132
#define PKO_WRITE_CMD    0xbabe0141
#define PKO_WRITE_RLY    0xbabe0142
#define PKO_LSEEK_CMD    0xbabe0151
#define PKO_LSEEK_RLY    0xbabe0152
#define PKO_OPENDIR_CMD  0xbabe0161
#define PKO_OPENDIR_RLY  0xbabe0162
#define PKO_CLOSEDIR_CMD 0xbabe0171
#define PKO_CLOSEDIR_RLY 0xbabe0172
#define PKO_READDIR_CMD  0xbabe0181
#define PKO_READDIR_RLY  0xbabe0182
#define PKO_REMOVE_CMD   0xbabe0191
#define PKO_REMOVE_RLY   0xbabe0192
#define PKO_MKDIR_CMD    0xbabe01a1
#define PKO_MKDIR_RLY    0xbabe01a2
#define PKO_RMDIR_CMD    0xbabe01b1
#define PKO_RMDIR_RLY    0xbabe01b2

#define PKO_RESET_CMD       0xbabe0201
#define PKO_EXECIOP_CMD     0xbabe0202
#define PKO_EXECEE_CMD      0xbabe0203
#define PKO_POWEROFF_CMD    0xbabe0204
#define PKO_SCRDUMP_CMD     0xbabe0205
#define PKO_NETDUMP_CMD     0xbabe0206

#define PKO_DUMP_MEM        0xbabe0207
#define PKO_START_VU        0xbabe0208
#define PKO_STOP_VU         0xbabe0209
#define PKO_DUMP_REG 		0xbabe020a
#define PKO_GSEXEC_CMD		0xbabe020b
#define PKO_WRITE_MEM       0xbabe020c
#define PKO_IOPEXCEP_CMD    0xbabe020d
#define PKO_SCRSHOT_CMD     0xbabe020e

#define PKO_RPC_RESET		1
#define PKO_RPC_EXECEE		2
#define PKO_RPC_DUMMY		3
#define PKO_RPC_SCRDUMP		4
#define PKO_RPC_NETDUMP		5
#define PKO_RPC_STARTVU		6
#define PKO_RPC_STOPVU		7
#define PKO_RPC_DUMPMEM		8
#define PKO_RPC_DUMPREG		9
#define PKO_RPC_GSEXEC		10
#define PKO_RPC_WRITEMEM	11
#define PKO_RPC_IOPEXCEP	12
#define PKO_RPC_SCRSHOT		13

#define PKO_MAX_PATH   256

#define REGDMA		0
#define REGINTC		1
#define REGTIMER	2
#define REGGS		3
#define REGSIF		4
#define REGFIFO		5
#define REGGIF		6
#define REGVIF0		7
#define REGVIF1		8
#define REGIPU		9
#define REGALL		10
#define REGVU0		11
#define REGVU1		12

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_hdr;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int retval;
} __attribute__((packed)) pko_pkt_file_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int flags;
    char path[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_open_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
} __attribute__((packed)) pko_pkt_close_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} __attribute__((packed)) pko_pkt_read_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int retval;
    int nbytes;
} __attribute__((packed)) pko_pkt_read_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} __attribute__((packed)) pko_pkt_write_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int offset;
    int whence;
} __attribute__((packed)) pko_pkt_lseek_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    char name[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_remove_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int mode;
    char name[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_mkdir_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    char name[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_rmdir_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
} __attribute__((packed)) pko_pkt_dread_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int retval;
/* from io_common.h (fio_dirent_t) in ps2lib */
    unsigned int mode;
    unsigned int attr;
    unsigned int size;
    unsigned char ctime[8];
    unsigned char atime[8];
    unsigned char mtime[8];
    unsigned int hisize;
    char name[256];
} __attribute__((packed)) pko_pkt_dread_rly;

////

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_reset_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_execee_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_execiop_req;

typedef struct
{
	unsigned int cmd;
	unsigned short len;
	unsigned short size;
	unsigned char file[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_gsexec_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_poweroff_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int vpu;
} __attribute__((packed)) pko_pkt_start_vu;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int vpu;
} __attribute__((packed)) pko_pkt_stop_vu;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int offset;
    unsigned int size;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_mem_io;

typedef struct {
    unsigned int cmd;
    unsigned short len;
    int regs;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_dump_regs;

typedef struct {
	unsigned int cmd;
	unsigned short len;
	unsigned int regs[79];
} __attribute__((packed)) pko_pkt_send_regs;

typedef struct {
	unsigned int cmd;
	unsigned short len;
	unsigned int base;
	unsigned int width;
	unsigned int height;
	unsigned short psm;
} __attribute__((packed)) pko_pkt_screenshot;

#define PKO_MAX_WRITE_SEGMENT (1460 - sizeof(pko_pkt_write_req))
#define PKO_MAX_READ_SEGMENT  (1460 - sizeof(pko_pkt_read_rly))

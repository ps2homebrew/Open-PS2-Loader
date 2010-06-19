/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <types.h>
#include <thbase.h>
#include <thsemap.h>
#include <sysclib.h>
#include <stdio.h>
#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>

#include "ps2ip.h"
#include "net_fio.h"
#include "hostlink.h"

////////////////////////////////////////////////////////////////////////

static int tty_socket = 0;
static int tty_sema = -1;

static char ttyname[] = "tty";

////////////////////////////////////////////////////////////////////////
static int dummy()
{
    return -5;
}

////////////////////////////////////////////////////////////////////////
static int dummy0()
{
    return 0;
}

////////////////////////////////////////////////////////////////////////
static int ttyInit(iop_device_t *driver)
{
    iop_sema_t sema_info;

    sema_info.attr       = 0;
    sema_info.initial = 1;	/* Unlocked.  */
    sema_info.max  = 1;
    if ((tty_sema = CreateSema(&sema_info)) < 0)
	    return -1;

    // Create/open udp socket
    if ((tty_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	    return -1;

	return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyOpen( int fd, char *name, int mode)
{
    return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyClose( int fd)
{
    return 1;
}


////////////////////////////////////////////////////////////////////////
static int ttyWrite(iop_file_t *file, char *buf, int size)
{
    struct sockaddr_in dstaddr;
    int res;

    WaitSema(tty_sema);

    dstaddr.sin_family      = AF_INET;
    dstaddr.sin_addr.s_addr = remote_pc_addr;
    dstaddr.sin_port        = htons(PKO_PRINTF_PORT);

    res = sendto(tty_socket, buf, size, 0, (struct sockaddr *)&dstaddr,
                    sizeof(dstaddr));

    SignalSema(tty_sema);
    return res;
}

iop_device_ops_t tty_functarray = { ttyInit, dummy0, (void *)dummy,
	(void *)ttyOpen, (void *)ttyClose, (void *)dummy,
	(void *)ttyWrite, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy, (void *)dummy,
	(void *)dummy, (void *)dummy };

iop_device_t tty_driver = { ttyname, 3, 1, "TTY via Udp",
							&tty_functarray };

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
int ttyMount(void)
{
    close(0);
    close(1);
    DelDrv(ttyname);
    AddDrv(&tty_driver);
    if(open("tty00:", O_RDONLY) != 0) while(1);
    if(open("tty00:", O_WRONLY) != 1) while(1);

    return 0;
}

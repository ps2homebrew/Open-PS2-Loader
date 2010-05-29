/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: tty.c 629 2004-10-11 00:45:00Z mrbrown $
# TTY filesystem for UDPTTY.

  modified by jimmikaelkael <jimmikaelkael@wanadoo.fr>

*/

#include <tamtypes.h>
#include <loadcore.h>
#include <stdio.h>
#include <thsemap.h>
#include <ioman.h>
#include <sysclib.h>
#include <ps2ip.h>
#include <errno.h>

#define MODNAME "udptty"
IRX_ID(MODNAME, 2, 1);

struct irx_export_table _exp_udptty;

#define DEVNAME "tty"

static int udp_socket;
static int tty_sema = -1;

static int tty_init(iop_device_t *device);
static int tty_deinit(iop_device_t *device);
static int tty_stdout_fd(void);
static int tty_write(iop_file_t *file, void *buf, size_t size);
static int tty_error(void);

/* device ops */
static iop_device_ops_t tty_ops = { 
	tty_init,
	tty_deinit,
	(void *)tty_error, 
	(void *)tty_stdout_fd,
	(void *)tty_stdout_fd,
	(void *)tty_error, 
	(void *)tty_write,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error,
	(void *)tty_error
};

/* device descriptor */
static iop_device_t tty_device = {
	DEVNAME,
	IOP_DT_CHAR|IOP_DT_CONS,
	1,
	"TTY via SMAP UDP",
	&tty_ops
};

int _start(int argc, char** argv)
{
	// register exports
	RegisterLibraryEntries(&_exp_udptty);

	// create the socket
	udp_socket = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udp_socket < 0)
		return MODULE_NO_RESIDENT_END;

	close(0);
	close(1);
	DelDrv(DEVNAME);

	if (AddDrv(&tty_device) < 0)
		return MODULE_NO_RESIDENT_END;

	open(DEVNAME "00:", 0x1000|O_RDWR);
	open(DEVNAME "00:", O_WRONLY);

	return MODULE_RESIDENT_END;
}

int _shutdown()
{
	lwip_close(udp_socket);

	return 0;
}

/* Copy the data into place, calculate the various checksums, and send the
   final packet.  */   
static int udp_send(void *buf, size_t size)
{
	struct sockaddr_in peer;

	peer.sin_family = AF_INET;
	peer.sin_port = htons(18194);
	peer.sin_addr.s_addr = inet_addr("255.255.255.255");

	lwip_sendto(udp_socket, buf, size, 0, (struct sockaddr *)&peer, sizeof(peer));

	return 0;
}

/* TTY driver.  */

static int tty_init(iop_device_t *device)
{
	if ((tty_sema = CreateMutex(IOP_MUTEX_UNLOCKED)) < 0)
		return -1;

	return 0;
}

static int tty_deinit(iop_device_t *device)
{
	DeleteSema(tty_sema);
	return 0;
}

static int tty_stdout_fd(void)
{
	return 1;
}

static int tty_write(iop_file_t *file, void *buf, size_t size)
{
	int res = 0;

	WaitSema(tty_sema);
	res = udp_send(buf, size);

	SignalSema(tty_sema);
	return res;
}

static int tty_error(void)
{
	return -EIO;
}


/*
 * netlog.h - send log messages to remote host
 *
 * Copyright (C) 2009 misfire <misfire@xploderfreax.de>
 *
 * Licensed under the Academic Free License version 2.0.  See file LICENSE.
 */

#ifndef _IOP_NETLOG_H_
#define _IOP_NETLOG_H_

#include <tamtypes.h>
#include <irx.h>

/* Module name and version */
#define NETLOG_MODNAME		"netlog"
#define NETLOG_VER_MAJ		1
#define NETLOG_VER_MIN		1

/* Default IP address and port of remote host */
#define NETLOG_DEF_ADDR		"192.168.0.2"
#define NETLOG_DEF_PORT		7411

/* Max log message length */
#define NETLOG_MAX_MSG		1024

/*
 * netlog_init - Init netlog service.
 * @ipaddr: IP address of remote host (set to NETLOG_DEF_ADDR if NULL)
 * @ipport: UDP/IP port of remote host (set to NETLOG_DEF_PORT if 0)
 * @return: 0: success, -1: error
 */
int netlog_init(const char *ipaddr, int ipport);

/*
 * netlog_exit - Exit netlog service.
 * @return: 0: success, -1: error
 */
int netlog_exit(void);

/*
 * netlog_send - Send log message to remote host.
 * @format: format string of message
 * @...: format string arguments
 * @return: number of characters sent, or -1 on error
 */
int netlog_send(const char *format, ...);


/* IRX import defines */
#define netlog_IMPORTS_start	DECLARE_IMPORT_TABLE(netlog, NETLOG_VER_MAJ, NETLOG_VER_MIN)
#define netlog_IMPORTS_end	END_IMPORT_TABLE

#define I_netlog_init 		DECLARE_IMPORT(4, netlog_init)
#define I_netlog_exit 		DECLARE_IMPORT(5, netlog_exit)
#define I_netlog_send		DECLARE_IMPORT(6, netlog_send)

#endif /* _IOP_NETLOG_H_ */

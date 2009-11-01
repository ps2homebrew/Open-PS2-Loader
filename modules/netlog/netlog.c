/*
 * netlog.c - send log messages to remote host
 *
 * Copyright (C) 2009 misfire <misfire@xploderfreax.de>
 *
 * Licensed under the Academic Free License version 2.0.  See file LICENSE.
 */

#include <tamtypes.h>
#include "irx_imports.h"
#include "xprintf.h"
#include "udpcl.h"
#include "netlog.h"

IRX_ID(NETLOG_MODNAME, NETLOG_VER_MAJ, NETLOG_VER_MIN);

#define M_PRINTF(format, args...) \
	printf(NETLOG_MODNAME ": " format, ## args)

static udpcl_t g_client;
static int g_sema = -1;
static int g_init = 0;

struct irx_export_table _exp_netlog;

/*
 * netlog_send - Send log message to remote host.
 * @format: format string of message
 * @...: format string arguments
 * @return: number of characters sent, or -1 on error
 */
int netlog_send(const char *format, ...)
{
	char buf[NETLOG_MAX_MSG];
	va_list ap;
	int ret;

	WaitSema(g_sema);

	va_start(ap, format);
	vsnprintf(buf, NETLOG_MAX_MSG, format, ap);
	va_end(ap);

	ret = udpcl_send(&g_client, buf, strlen(buf));

	SignalSema(g_sema);

	return ret;
}

/*
 * netlog_init - Init netlog service.
 * @ipaddr: IP address of remote host (set to NETLOG_DEF_ADDR if NULL)
 * @ipport: UDP/IP port of remote host (set to NETLOG_DEF_PORT if 0)
 * @return: 0: success, -1: error
 */
int netlog_init(const char *ipaddr, int ipport)
{
	iop_sema_t sema_info;

	if (g_init)
		return -1;

	sema_info.attr = 1;
	sema_info.initial = 0;
	sema_info.max = 1;
	sema_info.option = 0;

	if ((g_sema = CreateSema(&sema_info)) < 0) {
		M_PRINTF("Error: Can't create semaphore\n");
		return -1;
	}

	if (ipaddr == NULL)
		ipaddr = NETLOG_DEF_ADDR;
	if (!ipport)
		ipport = NETLOG_DEF_PORT;

	if (udpcl_create(&g_client, ipaddr, ipport, 0) < 0) {
		M_PRINTF("Can't create UDP client\n");
		return -1;
	}

	g_init = 1;
	SignalSema(g_sema);

	netlog_send("%s %d.%d ready.\n", NETLOG_MODNAME,
		NETLOG_VER_MAJ, NETLOG_VER_MIN);

	return 0;
}

/*
 * netlog_exit - Exit netlog service.
 * @return: 0: success, -1: error
 */
int netlog_exit(void)
{
	if (!g_init)
		return -1;

	WaitSema(g_sema);
	DeleteSema(g_sema);

	udpcl_destroy(&g_client);
	g_init = 0;

	return 0;
}

#ifdef NETLOG_RPC
#define NETLOG_RPC_ID 0x0d0bfd40

enum {
	NETLOG_RPC_INIT = 1,
	NETLOG_RPC_EXIT,
	NETLOG_RPC_SEND
};

typedef struct {
	char	ipaddr[16];
	u16	ipport;
} init_args_t;

static SifRpcServerData_t g_rpc_sd __attribute__((aligned(64)));
static SifRpcDataQueue_t g_rpc_qd __attribute__((aligned(64)));
static u8 g_rpc_buf[NETLOG_MAX_MSG] __attribute__((aligned(64)));


static void *rpc_handler(int cmd, void *buf, int size)
{
	init_args_t *iargs;
	int ret = -1;

	switch (cmd) {
	case NETLOG_RPC_INIT:
		iargs = (init_args_t*)buf;
		ret = netlog_init(iargs->ipaddr, iargs->ipport);
		break;
	case NETLOG_RPC_EXIT:
		ret = netlog_exit();
		break;
	case NETLOG_RPC_SEND:
		ret = netlog_send((char*)buf);
		break;
	default:
		M_PRINTF("Unknown RPC function call.\n");
	}

	*((int*)buf) = ret;

	return buf;
}

static void rpc_thread(void *arg)
{
	M_PRINTF("RPC thread started.\n");

	SifInitRpc(0);
	SifSetRpcQueue(&g_rpc_qd, GetThreadId());
	SifRegisterRpc(&g_rpc_sd, NETLOG_RPC_ID, (SifRpcFunc_t)rpc_handler,
		&g_rpc_buf, NULL, NULL, &g_rpc_qd);
	SifRpcLoop(&g_rpc_qd);
}

static int start_rpc_server(void)
{
	iop_thread_t thread;
 	int tid, ret;

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = (void*)rpc_thread;
	thread.stacksize = 4 * 1024;
	thread.priority = 79;

	if ((tid = CreateThread(&thread)) <= 0) {
		M_PRINTF("Could not create RPC thread (%i)\n", tid);
		return -1;
	}

	if ((ret = StartThread(tid, NULL)) < 0) {
		M_PRINTF("Could not start RPC thread (%i)\n", ret);
		DeleteThread(tid);
		return -1;
	}

	return tid;
}
#endif /* NETLOG_RPC */

int _start(int argc, char *argv[])
{
	if (RegisterLibraryEntries(&_exp_netlog) != 0) {
		M_PRINTF("Could not register exports.\n");
		return MODULE_NO_RESIDENT_END;
	}

#ifdef NETLOG_RPC
	SifInitRpc(0);
	if (start_rpc_server() < 0) {
		ReleaseLibraryEntries(&_exp_netlog);
		return MODULE_NO_RESIDENT_END;
	}
#endif
	M_PRINTF("Module started.\n");

	netlog_init(NULL, 0);
	
	return MODULE_RESIDENT_END;
}

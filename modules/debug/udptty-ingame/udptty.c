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
#include <intrman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include "smstcpip.h"
#include <errno.h>

#define MODNAME "udptty"
IRX_ID(MODNAME, 2, 1);

extern struct irx_export_table _exp_udptty;

#define DEVNAME "tty"

static int udp_socket;
static int g_uart_sema = -1;

/* Copy the data into place, calculate the various checksums, and send the
   final packet.  */
static int udp_send(void *buf, size_t size)
{
    struct sockaddr_in peer;

    peer.sin_family      = AF_INET;
    peer.sin_port        = htons(18194);
    peer.sin_addr.s_addr = inet_addr("255.255.255.255");

    lwip_sendto(udp_socket, buf, size, 0, (struct sockaddr *)&peer, sizeof(peer));

    return 0;
}

typedef struct _prnt_callback_context
{
     short position;
     char buffer[256];
} PrntCallbackContext;

void prnt_callback(PrntCallbackContext* context, int c)
{
     // Check for special case characters.
     if (c == 0x200)
     {
          // Reset position.
          context->position = 0;
     }
     else if (c == 0x201)
     {
          // Flush buffer.
          if (context->position > 0)
          {
               udp_send(context->buffer, context->position);
               context->position = 0;
          }
     }
     else
     {
          // Check for new line/line feed.
          if (c == 0xA)
               prnt_callback(context, 0xD);
          
          // Store the character and flush buffer if needed.
          context->buffer[context->position++] = (char)c;
          if (context->position == 256)
          {
               udp_send(context->buffer, context->position);
               context->position = 0;
          }
     }
}

void ac_printf(const char* format, ...)
{
     PrntCallbackContext prntContext;
     va_list args;
     
     prntContext.position = 0;
     
     // Format the string and let our prnt callback flush it to rs232.
     va_start(args, format);
     WaitSema(g_uart_sema);
     
     prnt((void(*)(void*, int))prnt_callback, &prntContext, format, args);
     
     SignalSema(g_uart_sema);
     va_end(args);
}

int ac_kprintf(void* context, const char* format, va_list ap)
{
     PrntCallbackContext prntContext;
     
     prntContext.position = 0;
     
     // Format the string and let our prnt callback flush it to rs232.
     WaitSema(g_uart_sema);
     int length = prnt((void(*)(void*, int))prnt_callback, &prntContext, format, ap);
     SignalSema(g_uart_sema);
     
     return length;
}

void hook_printf()
{
     u32 patchIns[2];
     int intrState;

     // Calculate the target address of STDIO printf function.
     u32* ptr = (u32*)&printf;
     u32 printfAddress = ((u32)&ptr[1] & 0xF0000000) | ((ptr[0] & 0x3FFFFFF) << 2);

     //ac_printf("printf import address: 0x%08x\n", (u32)ptr);
     //ac_printf("printf address: 0x%08x\n", printfAddress);
     //ac_printf("printf import ins: 0x%08x 0x%08x\n", ptr[0], ptr[1]);

     // Setup the instructions to patch in.
     patchIns[0] = 0x8000000 | (((u32)&ac_printf & 0xFFFFFFF) >> 2);       // j      <addr>
     patchIns[1] = 0;                                                      // nop

     //ac_printf("ac_printf address: 0x%08x\n", (u32)&ac_printf);
     //ac_printf("hook instructions: 0x%08x 0x%08x\n", patchIns[0], patchIns[1]);

     // Suspend interrupts while we patch in the new code.
     CpuSuspendIntr(&intrState);

     // Patch printf to jump to our hook.
     u32* pPrintf = (u32*)printfAddress;
     pPrintf[0] = patchIns[0];
     pPrintf[1] = patchIns[1];

     // Flush cache.
     FlushIcache();
     FlushDcache();

     // Resume interrupts.
     CpuResumeIntr(intrState);
}

int tty_noop()
{
     return 0;
}

int tty_write(iop_file_t*, void* buffer, int size)
{
     // Acquire the uart lock and write the message.
     WaitSema(g_uart_sema);
     udp_send(buffer, size);
     SignalSema(g_uart_sema);
     
     return size;
}

iop_device_ops_t tty_dev_ops =
{
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_write,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
     tty_noop,
};

/* device descriptor */
static iop_device_t tty_device = {
    DEVNAME,
    IOP_DT_CHAR | IOP_DT_CONS,
    1,
    "TTY via SMAP UDP",
    &tty_dev_ops,
};

int _start(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // register exports
    RegisterLibraryEntries(&_exp_udptty);

    // Create a semaphore for uart writing that is initially signaled, this acts as a lock for
     // multi-threaded write access.
     if ((g_uart_sema = CreateMutex(IOP_MUTEX_UNLOCKED)) < 0)
          return MODULE_NO_RESIDENT_END;

    // create the socket
    udp_socket = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0)
        return MODULE_NO_RESIDENT_END;
     
     ac_printf("Hello from ACDBG!\n");
     
     // Close STDIN and STDOUT.
     close(0);
     close(1);
     
     // Delete the default tty device driver and replace it with our own that forwards printf to
     // the arcade rs232 port.
     DelDrv("tty");
     AddDrv(&tty_device);
     
     // Re-open STDOUT and STDIN.
     if (open("tty00:", FIO_O_RDWR) != 0)
          ac_printf("STDIN file handle != 0!\n");
     
     if (open("tty00:", FIO_O_WRONLY) != 1)
          ac_printf("STDOUT file handle != 1!\n");
     
     // Set our kprintf handler.
     KprintfSet(ac_kprintf, NULL);

     // Hook printf function in STDIO.
     hook_printf();
     
     // Test STDOUT and kprintf.
     printf("** (STDOUT) Hello from ACDBG!\n");
     Kprintf("** (Kprintf) Hello from ACDBG!\n");
     
     return MODULE_RESIDENT_END;
}

int _shutdown()
{
    lwip_close(udp_socket);

    return 0;
}

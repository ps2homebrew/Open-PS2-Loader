/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _PFS_FIOCTL_H
#define _PFS_FIOCTL_H

///////////////////////////////////////////////////////////////////////////////
//	Function declarations

int pfsFioIoctl(iop_file_t *f, int cmd, void *param);
int pfsFioIoctl2(iop_file_t *f, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);
int pfsFioDevctl(iop_file_t *f, const char *name, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);

void pfsFioDevctlCloseAll(void);

#endif /* _PFS_FIOCTL_H */

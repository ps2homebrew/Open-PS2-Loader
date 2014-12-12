/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
*/

#ifndef _MISC_H
#define _MISC_H

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

void *allocMem(int size);
int getPs2Time(ps2time *tm);
void getPasswordHash(char *id, char *password1, char *password2);
int passcmp(char *password1, char *password2);
int getIlinkID(u8 *idbuf);

#endif /* _MISC_H */

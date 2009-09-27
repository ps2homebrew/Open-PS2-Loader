#ifndef IOPMGR_H
#define IOPMGR_H

typedef
struct
{
 unsigned int psize:8;
 unsigned int dsize:24;
 unsigned int daddr;
 unsigned int fcode;
} SifCmdHdr;

typedef
struct
{
 SifCmdHdr chdr;
 int       size;
 int       flag;
 char      arg[0x50];
} SifCmdResetData __attribute__((aligned(16)));

#endif /* IOPMGR */

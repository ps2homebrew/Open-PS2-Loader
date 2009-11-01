#ifndef IOPMGR_H
#define IOPMGR_H

typedef
struct
{
   u32 psize;
   u32 daddr;
   int fcode;
   u32 unknown;
 
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

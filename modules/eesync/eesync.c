#include <loadcore.h>
#include <intrman.h>
#include <ioman.h>
#include <sifman.h>
#include <sysmem.h>

#include "loadcore_add.h"

#define MODNAME "SyncEE"
IRX_ID(MODNAME, 0x01, 0x01);

extern struct irx_export_table _exp_eesync;
u8 memory[16] = "rom0:SECRMAN";
void checkSecrman()
{
  //register int i;
  register int fd;
  register int length;
  register int memorySize;
  /*u8 memory[16];

  for (i = 0; i < 12; i++)
  {
    memory[i] = 0x70 - ((i+1) * 4);
  }

  *(u32*)&memory[0] ^= 0x5009071e;
  *(u32*)&memory[4] ^= 0x13110b66;
  *(u32*)&memory[8] ^= 0x0e05051e;
  *(u32*)&memory[12] = 0;
  */
  //whole above writes:
  //6c 68 64 60 5c 58 54 50 4c 48 44 40 00 00 00 00
  //to memory an then dexors it to
  //"rom0:SECRMAN"
  //Why is it so freaking weird? Why did they want to hide it?
  //I'm also not sure of the purpose of this funciton... all games do work fine without it
  if ((fd = open(memory, O_RDONLY)) >= 0)
  {
    length = lseek(fd, 0, SEEK_END);
    if (close(fd) < 0) return;
    if (length < 0) return;

    if (length == 0x2731) memorySize = 0x1900;
    else memorySize = 0x100;
    
    AllocSysMemory(ALLOC_FIRST, memorySize, NULL);
  }
}
int PostResetCallback()
{
  sceSifSetSMFlag(0x40000);
  return 0;
}
int _start(int argc, char** argv)
{
  register void *ret;
  register u32 bootMode;
  if ((ret = QueryBootMode(3)) != NULL)
  {
    bootMode = *(u32*)((u32)ret+4);
    if (((bootMode & 1) != 0) || ((bootMode & 2) != 0)) return MODULE_NO_RESIDENT_END;
  }
  if (RegisterLibraryEntries(&_exp_eesync) < 0) return MODULE_NO_RESIDENT_END;
  
  checkSecrman();
  
  loadcore20(PostResetCallback, 2, 0);
  
  return MODULE_RESIDENT_END;
}

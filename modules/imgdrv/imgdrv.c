//we don't need most of this... just standard headers
#include <types.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <sysmem.h>
#include <sifman.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <loadcore.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <ioman.h>
#include <kerr.h>


/*#define MODNAME "imgdrv"
IRX_ID(MODNAME, 0x01, 0x00);*/

//int filePos = 0;
unsigned int ioprpimg = 0xDEC1DEC1;
int ioprpsiz = 0xDEC2DEC2;

int dummy_fs()
{
  return 0;
}/*
int open_fs(iop_file_t* fd, const char *name, int mode)
{
  //if (ioprpimg == 0 || ioprpsiz == 0) return -1;
  //if (mode & O_APPEND) filePos = ioprpsiz;
  //else filePos = 0;
  //printf("Img_open: name = %s, mode = %08x\n", name, mode);
  //filePos = 0;
  return 2;
}*/
/*int close_fs(iop_file_t* fd)
{
  //filePos = 0;
  return 0;
}*/
int	lseek_fs(iop_file_t* fd, unsigned long offset, int whence)
{
  //if (ioprpimg == 0 || ioprpsiz == 0) return 0;
  /*if (whence == SEEK_SET)
  {
    filePos = offset;
  } else if (whence == SEEK_CUR)
  {
    filePos += offset;
  } else
  {
    filePos = ioprpsiz + offset;
  }*/
  //printf("Img_seek: offset = %08x, whence = %08x, filepos = %d\n", offset, whence, filePos);*/
  //return filePos;
  if (whence == SEEK_END)
  {
    return ioprpsiz;
  } else
  {
    return 0;
  }
}
int read_fs(iop_file_t* fd, void * buffer, int size )
{
  //int i;
  //if (ioprpimg == 0 || ioprpsiz == 0) return 0;
  
  //printf("Img_read: size = %d, buffer = %08x, filepos = %d\n", size, buffer, filePos);
  /*for (i = 0; (i <  size) && (filePos + i < ioprpsiz); i++)
  {
    ((u8*)buffer)[i] = ((u8*)ioprpimg)[filePos+i];
  }*/
  memcpy(buffer, (void*)ioprpimg, size);
  return size;
}
typedef struct _iop_device_tm {
	const char *name;
	unsigned int type;
	unsigned int version;	/* Not so sure about this one.  */
	const char *desc;
	struct _iop_device_ops_tm *ops;
} iop_device_tm_t;

typedef struct _iop_device_ops_tm {
	int	(*init)(iop_device_t *);
	int	(*deinit)(iop_device_t *);
	int	(*format)(iop_file_t *);
	int	(*open)(iop_file_t *, const char *, int);
	int	(*close)(iop_file_t *);
	int	(*read)(iop_file_t *, void *, int);
	int	(*write)(iop_file_t *, void *, int);
	int	(*lseek)(iop_file_t *, unsigned long, int);
} iop_device_ops_tm_t;

iop_device_ops_t my_device_ops =
          {
              dummy_fs,//init
              dummy_fs,//deinit
              NULL,//dummy_fs,//format
              dummy_fs,//open_fs,//open
              dummy_fs,//close_fs,//close
              read_fs,//read
              NULL,//dummy_fs,//write
              lseek_fs,//lseek
              /*dummy_fs,//ioctl
              dummy_fs,//remove
              dummy_fs,//mkdir
              dummy_fs,//rmdir
              dummy_fs,//dopen
              dummy_fs,//dclose
              dummy_fs,//dread
              dummy_fs,//getstat
              dummy_fs,//chstat*/
          };

const u8 name[] = "img";
iop_device_t my_device = {
              name,
              IOP_DT_FS,
              1,
              name,
              &my_device_ops
           };

int _start( int argc, char **argv)
{
  //int ret;
  //if (argc < 3) return MODULE_NO_RESIDENT_END;
  //ioprpimg = strtol(argv[1], NULL, 0);
  //ioprpsiz = strtol(argv[2], NULL, 0);
  //printf("ioprpPos = 0x%08x, ioprpSize = %d\n", ioprpimg, ioprpsiz);
  //DelDrv("img");
  /*ret = */AddDrv((iop_device_t*)&my_device);
  //printf("Driver add result: %d\n", ret);

  return MODULE_RESIDENT_END;
}

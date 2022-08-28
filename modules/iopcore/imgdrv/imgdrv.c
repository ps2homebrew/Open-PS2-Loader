#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <ioman.h>

#define MODNAME "imgdrv"
IRX_ID(MODNAME, 1, 1);

unsigned int ioprpimg = 0xDEC1DEC1;
int ioprpsiz = 0xDEC2DEC2;

int dummy_fs()
{
    return 0;
}

int lseek_fs(iop_file_t *fd, int offset, int whence)
{
    if (whence == SEEK_END) {
        return ioprpsiz;
    } else {
        return 0;
    }
}

int read_fs(iop_file_t *fd, void *buffer, int size)
{
    memcpy(buffer, (void *)ioprpimg, size);
    return size;
}

typedef struct _iop_device_tm
{
    const char *name;
    unsigned int type;
    unsigned int version; /* Not so sure about this one.  */
    const char *desc;
    struct _iop_device_ops_tm *ops;
} iop_device_tm_t;

typedef struct _iop_device_ops_tm
{
    int (*init)(iop_device_t *);
    int (*deinit)(iop_device_t *);
    int (*format)(iop_file_t *);
    int (*open)(iop_file_t *, const char *, int);
    int (*close)(iop_file_t *);
    int (*read)(iop_file_t *, void *, int);
    int (*write)(iop_file_t *, void *, int);
    int (*lseek)(iop_file_t *, unsigned long, int);
} iop_device_ops_tm_t;

iop_device_ops_t my_device_ops =
    {
        dummy_fs, // init
        dummy_fs, // deinit
        NULL,     // dummy_fs,//format
        dummy_fs, // open_fs,//open
        dummy_fs, // close_fs,//close
        read_fs,  // read
        NULL,     // dummy_fs,//write
        lseek_fs, // lseek
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

const char name[] = "img";
iop_device_t my_device = {
    name,
    IOP_DT_FS,
    1,
    name,
    &my_device_ops};

int _start(int argc, char **argv)
{
    // DelDrv("img");
    AddDrv((iop_device_t *)&my_device);

    return MODULE_RESIDENT_END;
}

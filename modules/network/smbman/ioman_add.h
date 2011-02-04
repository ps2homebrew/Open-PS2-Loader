#ifndef _IOMAN_ADD_H_
#define _IOMAN_ADD_H_

#include <sys/stat.h>

#define IOP_DT_FSEXT	0x10000000

typedef struct _iop_ext_device {
	const char *name;
	unsigned int type;
	unsigned int version;	/* Not so sure about this one.  */
	const char *desc;
	struct _iop_ext_device_ops *ops;
} iop_ext_device_t;

typedef struct _iop_ext_device_ops {
	int	(*init)(iop_device_t *);
	int	(*deinit)(iop_device_t *);
	int	(*format)(iop_file_t *);
	int	(*open)(iop_file_t *, const char *, int);
	int	(*close)(iop_file_t *);
	int	(*read)(iop_file_t *, void *, int);
	int	(*write)(iop_file_t *, void *, int);
	int	(*lseek)(iop_file_t *, unsigned long, int);
	int	(*ioctl)(iop_file_t *, unsigned long, void *);
	int	(*remove)(iop_file_t *, const char *);
	int	(*mkdir)(iop_file_t *, const char *);
	int	(*rmdir)(iop_file_t *, const char *);
	int	(*dopen)(iop_file_t *, const char *);
	int	(*dclose)(iop_file_t *);
	int	(*dread)(iop_file_t *, iox_dirent_t *);
	int	(*getstat)(iop_file_t *, const char *, iox_stat_t *);
	int	(*chstat)(iop_file_t *, const char *, iox_stat_t *, unsigned int);
	/* Extended ops start here.  */
	int	(*rename)(iop_file_t *, const char *, const char *);
	int	(*chdir)(iop_file_t *, const char *);
	int	(*sync)(iop_file_t *, const char *, int);
	int	(*mount)(iop_file_t *, const char *, const char *, int, void *, unsigned int);
	int	(*umount)(iop_file_t *, const char *);
	long long (*lseek64)(iop_file_t *, long long, int);
	int	(*devctl)(iop_file_t *, const char *, int, void *, unsigned int, void *, unsigned int);
	int	(*symlink)(iop_file_t *, const char *, const char *);
	int	(*readlink)(iop_file_t *, const char *, char *, unsigned int);
	int	(*ioctl2)(iop_file_t *, int, void *, unsigned int, void *, unsigned int);
	
} iop_ext_device_ops_t;

#endif

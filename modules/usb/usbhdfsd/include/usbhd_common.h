#ifndef _USBHD_COMMON_H
#define _USBHD_COMMON_H

#ifdef WIN32
#define USBHD_INLINE
typedef unsigned char u8;
#else
#define USBHD_INLINE inline
void *malloc(int size);
void free(void *ptr);
#endif

struct _cache_set;
typedef struct _cache_set cache_set;
struct _mass_dev;
typedef struct _mass_dev mass_dev;

//---------------------------------------------------------------------------
#define getI32(buf) ((int)(((u8 *)(buf))[0] + (((u8 *)(buf))[1] << 8) + (((u8 *)(buf))[2] << 16) + (((u8 *)(buf))[3] << 24)))
#define getI32_2(buf1, buf2) ((int)(((u8 *)(buf1))[0] + (((u8 *)(buf1))[1] << 8) + (((u8 *)(buf2))[0] << 16) + (((u8 *)(buf2))[1] << 24)))
#define getI16(buf) ((int)(((u8 *)(buf))[0] + (((u8 *)(buf))[1] << 8)))

#endif // _USBHD_COMMON_H

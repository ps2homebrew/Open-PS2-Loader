#ifndef __CC_H__
# define __CC_H__

# define BYTE_ORDER LITTLE_ENDIAN

typedef unsigned char  u8_t;
typedef signed   char  s8_t;
typedef unsigned short u16_t;
typedef signed   short s16_t;
typedef unsigned long  u32_t;
typedef signed   long  s32_t;

typedef u32_t mem_ptr_t;

# define PACK_STRUCT_FIELD( x ) x __attribute__(  ( packed )  )
# define PACK_STRUCT_STRUCT       __attribute__(  ( packed )  )
#endif /* __CC_H__ */

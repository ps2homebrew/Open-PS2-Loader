#include <ps2ip.h>
#include <sysclib.h>
#include <stdint.h>

#define assert(expr) \
    ((expr) ||       \
     dbgprintf(F_NUM, __LINE__))

//#include <errno.h>
//#include <malloc.h>

//why not provide lwip/sockets.h ?
//missing in iop/tcpip/tcpip/include/ps2ip.h
#define close(x) lwip_close(x)
typedef int ssize_t;

//TODO: Missing <byteswap.h> in PS2SDK
// pickup from https://gist.github.com/jtbr/7a43e6281e6cca353b33ee501421860c
static inline uint64_t bswap64(uint64_t x)
{
    return (((x & 0xff00000000000000ull) >> 56) | ((x & 0x00ff000000000000ull) >> 40) | ((x & 0x0000ff0000000000ull) >> 24) | ((x & 0x000000ff00000000ull) >> 8) | ((x & 0x00000000ff000000ull) << 8) | ((x & 0x0000000000ff0000ull) << 24) | ((x & 0x000000000000ff00ull) << 40) | ((x & 0x00000000000000ffull) << 56));
}

//TODO: Missing in PS2SK's "common/include/tcpip.h"
#if __BIG_ENDIAN__
#define htonll(x) (x)
#define ntohll(x) (x)
#else
#define htonll(x) bswap64(x)
#define ntohll(x) bswap64(x)
#endif

// Missing in stddef.h
#ifndef offsetof
#define offsetof(st, m) \
    ((size_t)((char *)&((st *)0)->m - (char *)0))
#endif

//TODO: Missing in PS2SK's <stdint.h> , needed for "nbd-protocol.h"
// https://en.cppreference.com/w/c/types/integer
#define UINT64_MAX  0xffffffffffffffff
#define UINT64_C(x) ((x) + (UINT64_MAX - UINT64_MAX))

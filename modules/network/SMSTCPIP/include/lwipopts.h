#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define LWIP_CALLBACK_API 1

/* ---------- Core locking ---------- */
/**
 * LWIP_TCPIP_CORE_LOCKING_INPUT: when LWIP_TCPIP_CORE_LOCKING is enabled,
 * this lets tcpip_input() grab the mutex for input packets as well,
 * instead of allocating a message and passing it to tcpip_thread.
 *
 * ATTENTION: this does not work when tcpip_input() is called from
 * interrupt context!
 */
// #define LWIP_TCPIP_CORE_LOCKING_INPUT 1    //Not supported because of the in-game SMAP driver's design (will deadlock).

/* ---------- Memory options ---------- */
/**
 * MEM_LIBC_MALLOC==1: Use malloc/free/realloc provided by your C-library
 * instead of the lwip internal allocator. Can save code size if you
 * already use it.
 */
#define MEM_LIBC_MALLOC 1

/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#define MEM_ALIGNMENT 4

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
/* Setting this too low may cause tcp_write() to fail when it tries to allocate from PBUF_RAM!
   Up to TCP_SND_BUF * 2 segments may be transmitted at once, thanks to Nagle and Delayed Ack. */
#ifdef INGAME_DRIVER
#define MEM_SIZE 0x400
#else
#define MEM_SIZE (TCP_SND_BUF * 2)
#endif

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_PBUF 10
#endif

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB 1

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_TCP_PCB 1
#else
#define MEMP_NUM_TCP_PCB 2
#endif

/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_TCP_PCB_LISTEN 1
#else
#define MEMP_NUM_TCP_PCB_LISTEN 2
#endif

/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG TCP_SND_QUEUELEN

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
// Only used by tcpip.c, which will be used for IP reassembly and ARP.
#define MEMP_NUM_SYS_TIMEOUT 1

/* MEMP_NUM_NETCONN: the number of struct netconns. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_NETCONN 1
#endif

/* MEMP_NUM_APIMSG: the number of struct api_msg, used for
   communication between the TCP/IP stack and the sequential
   programs. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_API_MSG 5
#endif

/* MEMP_NUM_TCPIPMSG: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#ifdef INGAME_DRIVER
#define MEMP_NUM_TCPIP_MSG 15
#else
#define MEMP_NUM_TCPIP_MSG 40
#endif

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#ifdef INGAME_DRIVER
#define PBUF_POOL_SIZE 8
#else
#define PBUF_POOL_SIZE 25
#endif

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
// Boman666: Should be atleast 1518 to be compatible with ps2smap
// TCP_MSS+TCP header+IP header+PBUF_LINK_HLEN = 1460 + 20 + 20 + 14 = 1514
#define PBUF_POOL_BUFSIZE 1540

/* PBUF_LINK_HLEN: the number of bytes that should be allocated for a
   link level header. */
/* MRB: This needs to be the actual size so that pbuf's are aligned properly.  */
#define PBUF_LINK_HLEN 14

/** SYS_LIGHTWEIGHT_PROT
 * define SYS_LIGHTWEIGHT_PROT in lwipopts.h if you want inter-task protection
 * for certain critical regions during buffer allocation, deallocation and
 * memory allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT 1

/* ---------- TCP options ---------- */
#define LWIP_TCP 1
#define TCP_TTL  255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifdef INGAME_DRIVER
#define TCP_QUEUE_OOSEQ 0
#endif

/* TCP Maximum segment size. */
#define TCP_MSS 1460

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF (TCP_MSS * 2)

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#ifdef INGAME_DRIVER
#define TCP_SND_QUEUELEN (2 * TCP_SND_BUF / TCP_MSS)
#endif

/* TCP receive window. */
#ifdef INGAME_DRIVER
#define TCP_WND 10240
#else
#define TCP_WND 32768
#endif

/* TCP writable space (bytes).  This must be less than or equal
   to TCP_SND_BUF.  It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#define TCP_SNDLOWAT (TCP_SND_BUF / 2)

/* ----- TCPIP thread priority ----- */
#define TCPIP_THREAD_PRIO 2

/* ---------- ARP options ---------- */
#ifdef INGAME_DRIVER
#define ARP_TABLE_SIZE 2
#else
#define ARP_TABLE_SIZE 3
#endif
#define ARP_QUEUEING 0

/**
 * If defined to 1, cache entries are updated or added for every kind of ARP traffic
 * or broadcast IP traffic. Recommended for routers.
 * If defined to 0, only existing cache entries are updated. Entries are added when
 * lwIP is sending to them. Recommended for embedded devices.
 */
// Do not always insert ARP entries, as the ARP table is small.
#define ETHARP_ALWAYS_INSERT 0

/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run lwIP
   on a device with only one network interface, define this to 0. */
#define IP_FORWARD 0

/* IP reassembly and segmentation. These are orthogonal even
   if they both deal with IP fragments */
#define IP_REASSEMBLY 1
#define IP_FRAG       1

/* ---------- ICMP options ---------- */
#define ICMP_TTL 255


/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */

#ifdef PS2IP_DHCP

#define LWIP_DHCP 1

/**
 * DHCP_DOES_ARP_CHECK==1: Do an ARP check on the offered address.
 */
#define DHCP_DOES_ARP_CHECK 0 // Don't do the ARP check because an IP address would be first required.

#else

#define LWIP_DHCP           0
#define DHCP_DOES_ARP_CHECK 0

#endif

/* ---------- UDP options ---------- */
#ifdef INGAME_DRIVER
#define LWIP_UDP 0
#else
#define LWIP_UDP 1
#endif
#define UDP_TTL 255


/* ---------- Statistics options ---------- */
#define LWIP_STATS 0

#if LWIP_STATS
#define LINK_STATS   1
#define IP_STATS     1
#define IPFRAG_STATS 1
#define ICMP_STATS   1
#define UDP_STATS    1
#define TCP_STATS    1
#define MEM_STATS    1
#define MEMP_STATS   1
#define PBUF_STATS   1
#define SYS_STATS    1
#define RAW_STATS    1
#endif /*LWIP_STATS*/

// Boman666: This define will force the TX-data to be splitted in an even number of TCP-segments. This will significantly increase
// the upload speed, atleast on my configuration (PC - WinXP).
//#define    PS2IP_EVEN_TCP_SEG
// spooo:
//     It is a normal TCP behaviour that small data are delayed. This is
//     Nagle's algorithm. If you don't want it there's a legal way of
//     preventing delayed data which is the TCP_NODELAY option.
//
//     I'm not really sure, here are my thoughts about Boman666's patch:
//
//     - This patch hard-breaks (compile time define) this old grandma-TCP
//       behaviour, I'm not sure it is safe.
//
//     - I suspect a possible loss of performance (I have experienced a 1/7
//       waste time on a not only networking algorithm) because in case
//       that ps2 has to send small packets (sort of response to a query)
//       of 2N bytes length, two packets of N bytes size are sent. Which is
//       more overhead: 2*(40+N bytes + latency) instead of (40+2N +
//       latency).
//
//     - I am aware that this patch improves other things, and I have not
//       tested this myself yet.
//
//     - I don't really understand the effects of this patch:
//       My understanding is that XP ("this" XP stack used with "this"
//       protocol, check iop/tcpip/lwip/src/core/tcp_out.c) needs ps2 TCP
//       acks to work efficiently (to send more quickly not waiting TCP
//       200ms).
//
//     - I provide two runtime setsockopt()-configurable replacements
//
//     In replacement, and to make people test with their apps, I added two
//     new socket (TCP level) options which are:
//
//     - TCP_EVENSEG - it re-enables the original Boman666's patch
//     - TCP_ACKNODELAY - This option forces an ack to be sent
//                        at every data packet received.
//     To activate or not these option *at runtime* (not compile time), use the same
//     syntax as standard TCP_NODELAY:
//         int ret, value = 1; /* or 0 */
//         ret = lwip_setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
//     To read option value:
//         int ret, value,
//         int valuesize = sizeof(value);
//         ret = lwip_getsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &value, &valuesize);
//         printf("TCP_ACKNODELAY state: %i\n", ret == 0? value: -1);
//
//     setsockopt/getsockopt are POSIX, manual is available everywhere. They were not
//     exported in ps2ip symbol list, I've added them.
//     One should check performance penalty or improvement by activating any combination
//     of these three options. The only one which is standard is TCP_NODELAY.
//     By default, everything is not active.

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/
#ifdef INGAME_DRIVER
// Disable all higher-level checksum verification mechanisms for performance, relying on the MAC-level checksum.
/**
 * CHECKSUM_CHECK_IP==1: Check checksums in software for incoming IP packets.
 */
#if !defined CHECKSUM_CHECK_IP
#define CHECKSUM_CHECK_IP 0
#endif

/**
 * CHECKSUM_CHECK_UDP==1: Check checksums in software for incoming UDP packets.
 */
#if !defined CHECKSUM_CHECK_UDP
#define CHECKSUM_CHECK_UDP 0
#endif

/**
 * CHECKSUM_CHECK_TCP==1: Check checksums in software for incoming TCP packets.
 */
#if !defined CHECKSUM_CHECK_TCP
#define CHECKSUM_CHECK_TCP 0
#endif

/**
 * CHECKSUM_CHECK_ICMP==1: Check checksums in software for incoming ICMP packets.
 */
#if !defined CHECKSUM_CHECK_ICMP
#define CHECKSUM_CHECK_ICMP 0
#endif
#endif

#endif /* __LWIPOPTS_H__ */

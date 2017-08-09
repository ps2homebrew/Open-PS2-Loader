/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <dev9.h>
#include <atad.h>
#include <ps2ip.h>
#include <poweroff.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <errno.h>

#include "smsutils.h"

//#define FAKE_WRITES

#define MODNAME "hdldsvr"
IRX_ID(MODNAME, 1, 1);

struct irx_export_table _exp_hdldsvr;

#define TCP_SERVER_PORT 45233
#define UDP_SERVER_PORT 45233

#define CMD_SIZE 16
#define CMD_STAT 0x73746174 // 'stat'; get HDD size in KB
#define CMD_READ 0x72656164 // 'read'; read sectors from HDD
#define CMD_WRIT 0x77726974 // 'writ'; write sectors to HDD
#define CMD_WRIS 0x77726973 // 'wris'; get last write status
#define CMD_FLSH 0x666c7368 // 'flsh'; flush write buff
#define CMD_POWX 0x706f7778 // 'powx'; poweroff system

#define HDD_SECTOR_SIZE 512  // HDD sector size in bytes
#define NET_NUM_SECTORS 2048 // max # of sectors to move via network

#define STATUS_AVAIL 0        // expecting command
#define STATUS_BUSY_RESPOND 1 // sending back response (and readen data)
#define STATUS_BUSY_WRITE 2   // expecting write data
#define STATUS_WRITE_STAT 3   // sending back write stat response

#define SETBIT(mask, bit) (mask)[(bit) / 32] |= 1 << ((bit) % 32)
#define GETBIT(mask, bit) ((mask)[(bit) / 32] & (1 << ((bit) % 32)))

void tcp_server_thread(void *args);
void udp_server_thread(void *args);

static int tcp_server_tid, udp_server_tid;
static int udp_mutex = -1;

static u8 tcp_buf[CMD_SIZE + HDD_SECTOR_SIZE * 2] __attribute__((aligned(64)));
static u8 udp_buf[8 + HDD_SECTOR_SIZE * 2] __attribute__((aligned(64)));

struct tcp_packet_header
{
    u32 command;
    u32 start_sector;
    u32 num_sectors;
    u32 result;
} __attribute__((packed));

typedef struct
{
    struct tcp_packet_header hdr;
    u8 data[HDD_SECTOR_SIZE * 2];
} tcp_packet_t __attribute__((packed));

typedef struct
{
    u8 data[HDD_SECTOR_SIZE * 2];
    u32 command;
    u32 start_sector;
} udp_packet_t __attribute__((packed));

struct stats
{
    int status;
    u32 command;
    u32 start_sector;
    u32 num_sectors;
    u8 *data;
    u32 bitmask[(NET_NUM_SECTORS + 31) / 32];
    u32 bitmask_copy[(NET_NUM_SECTORS + 31) / 32];      // endian-neutral
    u8 buffer[HDD_SECTOR_SIZE * (NET_NUM_SECTORS + 1)]; // 1MB on IOP !!! Huge !
};

static struct stats gstats __attribute__((aligned(64)));

//-------------------------------------------------------------------------
int _start(int argc, char **argv)
{
    iop_thread_t thread_param;

    // register exports
    RegisterLibraryEntries(&_exp_hdldsvr);

    // create a locked udp mutex
    udp_mutex = CreateMutex(IOP_MUTEX_LOCKED);

    // create & start the tcp thread
    thread_param.attr = TH_C;
    thread_param.thread = (void *)tcp_server_thread;
    thread_param.priority = 0x10;
    thread_param.stacksize = 0x1000;
    thread_param.option = 0;

    tcp_server_tid = CreateThread(&thread_param);

    StartThread(tcp_server_tid, 0);

    // create & start the udp thread
    thread_param.attr = TH_C;
    thread_param.thread = (void *)udp_server_thread;
    thread_param.priority = 0x10;
    thread_param.stacksize = 0x1000;
    thread_param.option = 0;

    udp_server_tid = CreateThread(&thread_param);

    StartThread(udp_server_tid, 0);

    return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
    // delete threads
    DeleteThread(tcp_server_tid);
    DeleteThread(udp_server_tid);

    // delete udp mutex
    DeleteSema(udp_mutex);

    return 0;
}

//-------------------------------------------------------------------------
static int ack_tcp_command(int sock, void *buf, int bsize)
{
    register int r;

    // acknowledge a received command

    gstats.status = STATUS_BUSY_RESPOND;

    r = lwip_send(sock, buf, bsize, 0);

    gstats.status = STATUS_AVAIL;

    return r;
}

//-------------------------------------------------------------------------
static void reject_tcp_command(int sock, char *error_string)
{
    // reject a received command

    gstats.status = STATUS_BUSY_RESPOND;

    lwip_send(sock, error_string, strlen(error_string), 0);

    gstats.status = STATUS_AVAIL;

    ata_device_flush_cache(0);
}

//-------------------------------------------------------------------------
static int check_datas(u32 command, u32 start_sector, u32 num_sectors)
{
    // check if all write data is here

    register int i, got_all_datas = 1;

    // wait that udp mutex is unlocked
    WaitSema(udp_mutex);

    for (i = 0; i < num_sectors; ++i) {
        if (!GETBIT(gstats.bitmask, i)) {
            got_all_datas = 0;
            break;
        }
    }

    if (!got_all_datas) // some parts are missing; should ask for retransmit
        return -1;

    // all data here -- we can commit
    gstats.command = command;
    return 0;
}

//-------------------------------------------------------------------------
static int handle_tcp_client(int tcp_client_socket)
{
    register int r, i, size;
    ata_devinfo_t *devinfo;
    tcp_packet_t *pkt = (tcp_packet_t *)tcp_buf;

    while (1) {

        // receive incoming packets
        size = lwip_recv(tcp_client_socket, &tcp_buf[0], sizeof(tcp_buf), 0);
        if (size < 0)
            goto error;

        // check if valid command size
        if (size != CMD_SIZE) {
            reject_tcp_command(tcp_client_socket, "handle_recv: invalid packet received");
            goto error;
        }

        // don't accept 'wris' without previous 'writ'
        if ((gstats.status == STATUS_AVAIL) && (pkt->hdr.command == CMD_WRIS)) {
            reject_tcp_command(tcp_client_socket, "write stat denied");
            goto error;
        } else if ((!(gstats.status == STATUS_BUSY_WRITE) && (pkt->hdr.command == CMD_WRIS))) {
            reject_tcp_command(tcp_client_socket, "busy");
            goto error;
        }

        switch (pkt->hdr.command) {

            // ------------------------------------------------
            // 'writ' command
            // ------------------------------------------------
            case CMD_WRIT:
                // confirm write
                pkt->hdr.result = 0;
                r = ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header));
                if (r < 0) {
                    reject_tcp_command(tcp_client_socket, "init_write b0rked");
                    goto error;
                }

                gstats.status = STATUS_BUSY_WRITE;
                gstats.command = pkt->hdr.command;
                gstats.start_sector = pkt->hdr.start_sector;
                gstats.num_sectors = pkt->hdr.num_sectors;

                // set-up buffer and clear bitmask
                gstats.data = (u8 *)(((long)&gstats.buffer[HDD_SECTOR_SIZE - 1]) & ~(HDD_SECTOR_SIZE - 1));
                mips_memset(gstats.bitmask, 0, sizeof(gstats.bitmask));

                break;


            // ------------------------------------------------
            // 'wris' command
            // ------------------------------------------------
            case CMD_WRIS:
                if ((pkt->hdr.start_sector != gstats.start_sector) || (pkt->hdr.num_sectors != gstats.num_sectors)) {
                    reject_tcp_command(tcp_client_socket, "invalid write stat");
                    goto error;
                }
                r = check_datas(pkt->hdr.command, pkt->hdr.start_sector, pkt->hdr.num_sectors);
                if (r < 0) {

                    // means we need retransmission of some datas so we send bitmask stats to the client
                    u32 *out = gstats.bitmask_copy;
                    for (i = 0; i < (NET_NUM_SECTORS + 31) / 32; ++i)
                        out[i] = gstats.bitmask[i];

                    mips_memcpy(pkt->data, (void *)out, sizeof(gstats.bitmask_copy));

                    pkt->hdr.result = 0;
                    r = ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header) + sizeof(gstats.bitmask));
                    if (r < 0) {
                        reject_tcp_command(tcp_client_socket, "init_write_stat failed");
                        goto error;
                    }

                    gstats.status = STATUS_BUSY_WRITE;
                } else {
                    pkt->hdr.result = pkt->hdr.num_sectors;

#ifdef FAKE_WRITES
                    // fake write, we read instead
                    ata_device_sector_io(0, gstats.data, pkt->hdr.start_sector, pkt->hdr.num_sectors, ATA_DIR_READ);
#else
                    // !!! real writes !!!
                    ata_device_sector_io(0, gstats.data, pkt->hdr.start_sector, pkt->hdr.num_sectors, ATA_DIR_WRITE);
#endif
                    ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header));
                }
                break;


            // ------------------------------------------------
            // 'stat' command
            // ------------------------------------------------
            case CMD_STAT:
                if ((devinfo = ata_get_devinfo(0)) == NULL)
                    pkt->hdr.result = -1;
                else
                    pkt->hdr.result = devinfo->total_sectors >> 1;
                ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header));

                break;


            // ------------------------------------------------
            // 'read' command
            // ------------------------------------------------
            case CMD_READ:
                // set up buffer
                gstats.data = (u8 *)(((long)&gstats.buffer + HDD_SECTOR_SIZE - 1) & ~(HDD_SECTOR_SIZE - 1));

                if (pkt->hdr.num_sectors > 2) {
                    r = ata_device_sector_io(0, gstats.data, pkt->hdr.start_sector, pkt->hdr.num_sectors, ATA_DIR_READ);
                    if (r != 0)
                        pkt->hdr.result = -1;
                    else
                        pkt->hdr.result = pkt->hdr.num_sectors;

                    ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header));
                    if (r == 0)
                        ack_tcp_command(tcp_client_socket, gstats.data, pkt->hdr.num_sectors * HDD_SECTOR_SIZE);
                } else {
                    r = ata_device_sector_io(0, pkt->data, pkt->hdr.start_sector, pkt->hdr.num_sectors, ATA_DIR_READ);
                    if (r != 0)
                        pkt->hdr.result = -1;
                    else
                        pkt->hdr.result = pkt->hdr.num_sectors;

                    ack_tcp_command(tcp_client_socket, &tcp_buf[0], sizeof(tcp_packet_t));
                }

                break;


            // ------------------------------------------------
            // 'flsh' command
            // ------------------------------------------------
            case CMD_FLSH:
                ata_device_flush_cache(0);

                break;


            // ------------------------------------------------
            // 'powx' command
            // ------------------------------------------------
            case CMD_POWX:
                ata_device_flush_cache(0);
                dev9Shutdown();
                PoweroffShutdown();

                break;



            default:
                reject_tcp_command(tcp_client_socket, "handle_recv: unknown command");
                goto error;
        }
    }

    return 0;

error:
    return -1;
}

//-------------------------------------------------------------------------
void tcp_server_thread(void *args)
{
    int tcp_socket, client_socket = -1;
    struct sockaddr_in peer;
    int peerlen;
    register int r;

    while (1) {

        peer.sin_family = AF_INET;
        peer.sin_port = htons(TCP_SERVER_PORT);
        peer.sin_addr.s_addr = htonl(INADDR_ANY);

        // create the socket
        tcp_socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0)
            goto error;

        // bind the socket
        r = lwip_bind(tcp_socket, (struct sockaddr *)&peer, sizeof(peer));
        if (r < 0)
            goto error;

        // we want to listen
        r = lwip_listen(tcp_socket, 3);
        if (r < 0)
            goto error;

        while (1) {
            peerlen = sizeof(peer);
            // wait for incoming connection
            client_socket = lwip_accept(tcp_socket, (struct sockaddr *)&peer, &peerlen);
            if (client_socket < 0)
                goto error;

            // got connection, handle the client
            r = handle_tcp_client(client_socket);
            if (r < 0)
                lwip_close(client_socket);
        }

    error:
        // close the socket
        lwip_close(tcp_socket);
    }
}

//-------------------------------------------------------------------------
void udp_server_thread(void *args)
{
    int udp_socket;
    struct sockaddr_in peer;
    register int r;
    udp_packet_t *pkt = (udp_packet_t *)udp_buf;

    while (1) {

        peer.sin_family = AF_INET;
        peer.sin_port = htons(UDP_SERVER_PORT);
        peer.sin_addr.s_addr = htonl(INADDR_ANY);

        // create the socket
        udp_socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
            goto error;

        // bind the socket
        r = lwip_bind(udp_socket, (struct sockaddr *)&peer, sizeof(peer));
        if (r < 0)
            goto error;

        while (1) {

            // wait for packet
            r = lwip_recv(udp_socket, udp_buf, sizeof(udp_buf), 0);

            // check to see if it's the command we're expecting
            if ((r == sizeof(udp_packet_t)) && (pkt->command == gstats.command)) {

                // check the start sector is valid
                if ((gstats.start_sector <= pkt->start_sector) && (pkt->start_sector < gstats.start_sector + gstats.num_sectors)) {

                    u32 start_sector = pkt->start_sector - gstats.start_sector;

                    if (!GETBIT(gstats.bitmask, start_sector)) {
                        mips_memcpy(gstats.data + start_sector * HDD_SECTOR_SIZE, pkt, HDD_SECTOR_SIZE * 2);

                        SETBIT(gstats.bitmask, start_sector);
                        SETBIT(gstats.bitmask, start_sector + 1);

                        // unlock udp mutex
                        SignalSema(udp_mutex);
                    }
                }
            }
        }

    error:
        // close the socket
        lwip_close(udp_socket);
    }
}

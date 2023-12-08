/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

// Fu*k knows why net_fio & net_fsys are separated..

#include <types.h>
#include <ioman.h>
#include <sysclib.h>
#include <stdio.h>
#include <thbase.h>

#include <io_common.h>

#include "ps2ip.h"
#include "net_fio.h"
#include "hostlink.h"

unsigned int remote_pc_addr = 0xffffffff;

#define PACKET_MAXSIZE 4096

static char send_packet[PACKET_MAXSIZE] __attribute__((aligned(16)));
static char recv_packet[PACKET_MAXSIZE] __attribute__((aligned(16)));

static int pko_fileio_sock = -1;
static int pko_fileio_active = 0;

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) \
    do {                   \
    } while (0)
#endif

//----------------------------------------------------------------------
//
void pko_close_socket(void)
{
    int ret;

    ret = disconnect(pko_fileio_sock);
    if (ret < 0) {
        printf("pko_file: disconnect returned error %d\n", ret);
    }
    pko_fileio_sock = -1;
}

//----------------------------------------------------------------------
//
void pko_close_fsys(void)
{
    if (pko_fileio_sock > 0) {
        disconnect(pko_fileio_sock);
    }
    pko_fileio_active = 0;
    return;
}

//----------------------------------------------------------------------
// XXX: Hm, this func should behave sorta like pko_recv_bytes imho..
// i.e. check if it was able to send just a part of the packet etc..
static inline int
pko_lwip_send(int sock, void *buf, int len, int flag)
{
    int ret;
    ret = send(sock, buf, len, flag);
    if (ret < 0) {
        dbgprintf("pko_file: lwip_send() error %d\n", ret);
        pko_close_socket();
        return -1;
    } else {
        return ret;
    }
}


//----------------------------------------------------------------------
// Do repetetive recv() calles until 'bytes' bytes are received
// or error returned
int pko_recv_bytes(int sock, char *buf, int bytes)
{
    int left;
    int len;

    left = bytes;

    while (left > 0) {
        len = recv(sock, &buf[bytes - left], left, 0);
        if (len < 0) {
            dbgprintf("pko_file: pko_recv_bytes error!!\n");
            return -1;
        }
        left -= len;
    }
    return bytes;
}


//----------------------------------------------------------------------
// Receive a 'packet' of the expected type 'pkt_type', and lenght 'len'
int pko_accept_pkt(int sock, char *buf, int len, int pkt_type)
{
    int length;
    pko_pkt_hdr *hdr;
    unsigned int hcmd;
    unsigned short hlen;


    length = pko_recv_bytes(sock, buf, sizeof(pko_pkt_hdr));
    if (length < 0) {
        dbgprintf("pko_file: accept_pkt recv error\n");
        return -1;
    }

    if (length < sizeof(pko_pkt_hdr)) {
        dbgprintf("pko_file: XXX: did not receive a full header!!!! "
                  "Fix this! (%d)\n",
                  length);
    }

    hdr = (pko_pkt_hdr *)buf;
    hcmd = ntohl(hdr->cmd);
    hlen = ntohs(hdr->len);

    if (hcmd != pkt_type) {
        dbgprintf("pko_file: pko_accept_pkt: Expected %x, got %x\n",
                  pkt_type, hcmd);
        return 0;
    }

    if (hlen > len) {
        dbgprintf("pko_file: pko_accept_pkt: hdr->len is too large!! "
                  "(%d, can only receive %d)\n",
                  hlen, len);
        return 0;
    }

    // get the actual packet data
    length = pko_recv_bytes(sock, buf + sizeof(pko_pkt_hdr),
                            hlen - sizeof(pko_pkt_hdr));

    if (length < 0) {
        dbgprintf("pko_file: accept recv2 error!!\n");
        return 0;
    }

    if (length < (hlen - sizeof(pko_pkt_hdr))) {
        dbgprintf("pko_accept_pkt: Did not receive full packet!!! "
                  "Fix this! (%d)\n",
                  length);
    }

    return 1;
}

//----------------------------------------------------------------------
//
int pko_open_file(char *path, int flags)
{
    pko_pkt_open_req *openreq;
    pko_pkt_file_rly *openrly;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: file open req (%s, %x)\n", path, flags);

    openreq = (pko_pkt_open_req *)&send_packet[0];

    // Build packet
    openreq->cmd = htonl(PKO_OPEN_CMD);
    openreq->len = htons((unsigned short)sizeof(pko_pkt_open_req));
    openreq->flags = htonl(flags);
    strncpy(openreq->path, path, PKO_MAX_PATH);
    openreq->path[PKO_MAX_PATH - 1] = 0; // Make sure it's null-terminated

    if (pko_lwip_send(pko_fileio_sock, openreq, sizeof(pko_pkt_open_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_file_rly), PKO_OPEN_RLY)) {
        dbgprintf("pko_file: pko_open_file: did not receive OPEN_RLY\n");
        return -1;
    }

    openrly = (pko_pkt_file_rly *)recv_packet;

    dbgprintf("pko_file: file open reply received (ret %ld)\n", ntohl(openrly->retval));

    return ntohl(openrly->retval);
}


//----------------------------------------------------------------------
//
int pko_close_file(int fd)
{
    pko_pkt_close_req *closereq;
    pko_pkt_file_rly *closerly;


    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: file close req (fd: %d)\n", fd);

    closereq = (pko_pkt_close_req *)&send_packet[0];
    closerly = (pko_pkt_file_rly *)&recv_packet[0];

    closereq->cmd = htonl(PKO_CLOSE_CMD);
    closereq->len = htons((unsigned short)sizeof(pko_pkt_close_req));
    closereq->fd = htonl(fd);

    if (pko_lwip_send(pko_fileio_sock, closereq, sizeof(pko_pkt_close_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, (char *)closerly,
                        sizeof(pko_pkt_file_rly), PKO_CLOSE_RLY)) {
        dbgprintf("pko_file: pko_close_file: did not receive PKO_CLOSE_RLY\n");
        return -1;
    }

    dbgprintf("pko_file: pko_close_file: close reply received (ret %ld)\n",
              ntohl(closerly->retval));

    return ntohl(closerly->retval);
}

//----------------------------------------------------------------------
//
int pko_lseek_file(int fd, unsigned int offset, int whence)
{
    pko_pkt_lseek_req *lseekreq;
    pko_pkt_file_rly *lseekrly;


    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: file lseek req (fd: %d)\n", fd);

    lseekreq = (pko_pkt_lseek_req *)&send_packet[0];
    lseekrly = (pko_pkt_file_rly *)&recv_packet[0];

    lseekreq->cmd = htonl(PKO_LSEEK_CMD);
    lseekreq->len = htons((unsigned short)sizeof(pko_pkt_lseek_req));
    lseekreq->fd = htonl(fd);
    lseekreq->offset = htonl(offset);
    lseekreq->whence = htonl(whence);

    if (pko_lwip_send(pko_fileio_sock, lseekreq, sizeof(pko_pkt_lseek_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, (char *)lseekrly,
                        sizeof(pko_pkt_file_rly), PKO_LSEEK_RLY)) {
        dbgprintf("pko_file: pko_lseek_file: did not receive PKO_LSEEK_RLY\n");
        return -1;
    }

    dbgprintf("pko_file: pko_lseek_file: lseek reply received (ret %ld)\n",
              ntohl(lseekrly->retval));

    return ntohl(lseekrly->retval);
}


//----------------------------------------------------------------------
//
int pko_write_file(int fd, char *buf, int length)
{
    pko_pkt_write_req *writecmd;
    pko_pkt_file_rly *writerly;
    int hlen;
    int writtenbytes;
    int nbytes;
    int retval;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: file write req (fd: %d)\n", fd);

    writecmd = (pko_pkt_write_req *)&send_packet[0];
    writerly = (pko_pkt_file_rly *)&recv_packet[0];

    hlen = (unsigned short)sizeof(pko_pkt_write_req);
    writecmd->cmd = htonl(PKO_WRITE_CMD);
    writecmd->len = htons(hlen);
    writecmd->fd = htonl(fd);

    // Divide the write request
    writtenbytes = 0;
    while (writtenbytes < length) {

        if ((length - writtenbytes) > PKO_MAX_WRITE_SEGMENT) {
            // Need to split in several read reqs
            nbytes = PKO_MAX_READ_SEGMENT;
        } else {
            nbytes = length - writtenbytes;
        }

        writecmd->nbytes = htonl(nbytes);
#ifdef ZEROCOPY
        /* Send the packet header.  */
        if (pko_lwip_send(pko_fileio_sock, writecmd, hlen, 0) < 0)
            return -1;
        /* Send the write() data.  */
        if (pko_lwip_send(pko_fileio_sock, &buf[writtenbytes], nbytes, 0) < 0)
            return -1;
#else
        // Copy data to the acutal packet
        memcpy(&send_packet[sizeof(pko_pkt_write_req)], &buf[writtenbytes],
               nbytes);

        if (pko_lwip_send(pko_fileio_sock, writecmd, hlen + nbytes, 0) < 0)
            return -1;
#endif


        // Get reply
        if (!pko_accept_pkt(pko_fileio_sock, (char *)writerly,
                            sizeof(pko_pkt_file_rly), PKO_WRITE_RLY)) {
            dbgprintf("pko_file: pko_write_file: "
                      "did not receive PKO_WRITE_RLY\n");
            return -1;
        }
        retval = ntohl(writerly->retval);

        dbgprintf("pko_file: wrote %d bytes (asked for %d)\n",
                  retval, nbytes);

        if (retval < 0) {
            // Error
            dbgprintf("pko_file: pko_write_file: received error on write req (%d)\n",
                      retval);
            return retval;
        }

        writtenbytes += retval;
        if (retval < nbytes) {
            // EOF?
            break;
        }
    }
    return writtenbytes;
}

//----------------------------------------------------------------------
//
int pko_read_file(int fd, char *buf, int length)
{
    int nbytes;
    int i;
    pko_pkt_read_req *readcmd;
    pko_pkt_read_rly *readrly;


    if (pko_fileio_sock < 0) {
        return -1;
    }

    readcmd = (pko_pkt_read_req *)&send_packet[0];
    readrly = (pko_pkt_read_rly *)&recv_packet[0];

    readcmd->cmd = htonl(PKO_READ_CMD);
    readcmd->len = htons((unsigned short)sizeof(pko_pkt_read_req));
    readcmd->fd = htonl(fd);

    if (length < 0) {
        dbgprintf("pko_read_file: illegal req!! (whish to read < 0 bytes!)\n");
        return -1;
    }

    readcmd->nbytes = htonl(length);

    i = send(pko_fileio_sock, readcmd, sizeof(pko_pkt_read_req), 0);
    if (i < 0) {
        dbgprintf("pko_file: pko_read_file: send failed (%d)\n", i);
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, (char *)readrly,
                        sizeof(pko_pkt_read_rly), PKO_READ_RLY)) {
        dbgprintf("pko_file: pko_read_file: "
                  "did not receive PKO_READ_RLY\n");
        return -1;
    }

    nbytes = ntohl(readrly->nbytes);
    dbgprintf("pko_file: pko_read_file: Reply said there's %d bytes to read "
              "(wanted %d)\n",
              nbytes, length);

    // Now read the actual file data
    i = pko_recv_bytes(pko_fileio_sock, &buf[0], nbytes);
    if (i < 0) {
        dbgprintf("pko_file: pko_read_file, data read error\n");
        return -1;
    }
    return nbytes;
}

//----------------------------------------------------------------------
//
int pko_remove(char *name)
{
    pko_pkt_remove_req *removereq;
    pko_pkt_file_rly *removerly;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: file remove req (%s)\n", name);

    removereq = (pko_pkt_remove_req *)&send_packet[0];

    // Build packet
    removereq->cmd = htonl(PKO_REMOVE_CMD);
    removereq->len = htons((unsigned short)sizeof(pko_pkt_remove_req));
    strncpy(removereq->name, name, PKO_MAX_PATH);
    removereq->name[PKO_MAX_PATH - 1] = 0; // Make sure it's null-terminated

    if (pko_lwip_send(pko_fileio_sock, removereq, sizeof(pko_pkt_remove_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_file_rly), PKO_REMOVE_RLY)) {
        dbgprintf("pko_file: pko_remove: did not receive REMOVE_RLY\n");
        return -1;
    }

    removerly = (pko_pkt_file_rly *)recv_packet;
    dbgprintf("pko_file: file remove reply received (ret %ld)\n", ntohl(removerly->retval));
    return ntohl(removerly->retval);
}

//----------------------------------------------------------------------
//
int pko_mkdir(char *name, int mode)
{
    pko_pkt_mkdir_req *mkdirreq;
    pko_pkt_file_rly *mkdirrly;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: make dir req (%s)\n", name);

    mkdirreq = (pko_pkt_mkdir_req *)&send_packet[0];

    // Build packet
    mkdirreq->cmd = htonl(PKO_MKDIR_CMD);
    mkdirreq->len = htons((unsigned short)sizeof(pko_pkt_mkdir_req));
    mkdirreq->mode = mode;
    strncpy(mkdirreq->name, name, PKO_MAX_PATH);
    mkdirreq->name[PKO_MAX_PATH - 1] = 0; // Make sure it's null-terminated

    if (pko_lwip_send(pko_fileio_sock, mkdirreq, sizeof(pko_pkt_mkdir_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_file_rly), PKO_MKDIR_RLY)) {
        dbgprintf("pko_file: pko_mkdir: did not receive MKDIR_RLY\n");
        return -1;
    }

    mkdirrly = (pko_pkt_file_rly *)recv_packet;
    dbgprintf("pko_file: make dir reply received (ret %ld)\n", ntohl(mkdirrly->retval));
    return ntohl(mkdirrly->retval);
}

//----------------------------------------------------------------------
//
int pko_rmdir(char *name)
{
    pko_pkt_rmdir_req *rmdirreq;
    pko_pkt_file_rly *rmdirrly;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: remove dir req (%s)\n", name);

    rmdirreq = (pko_pkt_rmdir_req *)&send_packet[0];

    // Build packet
    rmdirreq->cmd = htonl(PKO_RMDIR_CMD);
    rmdirreq->len = htons((unsigned short)sizeof(pko_pkt_rmdir_req));
    strncpy(rmdirreq->name, name, PKO_MAX_PATH);
    rmdirreq->name[PKO_MAX_PATH - 1] = 0; // Make sure it's null-terminated

    if (pko_lwip_send(pko_fileio_sock, rmdirreq, sizeof(pko_pkt_rmdir_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_file_rly), PKO_RMDIR_RLY)) {
        dbgprintf("pko_file: pko_rmdir: did not receive RMDIR_RLY\n");
        return -1;
    }

    rmdirrly = (pko_pkt_file_rly *)recv_packet;
    dbgprintf("pko_file: remove dir reply received (ret %ld)\n", ntohl(rmdirrly->retval));
    return ntohl(rmdirrly->retval);
}

//----------------------------------------------------------------------
//
int pko_open_dir(char *path)
{
    pko_pkt_open_req *openreq;
    pko_pkt_file_rly *openrly;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: dir open req (%s)\n", path);

    openreq = (pko_pkt_open_req *)&send_packet[0];

    // Build packet
    openreq->cmd = htonl(PKO_OPENDIR_CMD);
    openreq->len = htons((unsigned short)sizeof(pko_pkt_open_req));
    openreq->flags = htonl(0);
    strncpy(openreq->path, path, PKO_MAX_PATH);
    openreq->path[PKO_MAX_PATH - 1] = 0; // Make sure it's null-terminated

    if (pko_lwip_send(pko_fileio_sock, openreq, sizeof(pko_pkt_open_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_file_rly), PKO_OPENDIR_RLY)) {
        dbgprintf("pko_file: pko_open_dir: did not receive OPENDIR_RLY\n");
        return -1;
    }

    openrly = (pko_pkt_file_rly *)recv_packet;

    dbgprintf("pko_file: dir open reply received (ret %ld)\n", ntohl(openrly->retval));

    return ntohl(openrly->retval);
}

//----------------------------------------------------------------------
//
int pko_read_dir(int fd, void *buf)
{
    pko_pkt_dread_req *dirreq;
    pko_pkt_dread_rly *dirrly;
    io_dirent_t *dirent;

    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: dir read req (%x)\n", fd);

    dirreq = (pko_pkt_dread_req *)&send_packet[0];

    // Build packet
    dirreq->cmd = htonl(PKO_READDIR_CMD);
    dirreq->len = htons((unsigned short)sizeof(pko_pkt_dread_req));
    dirreq->fd = htonl(fd);

    if (pko_lwip_send(pko_fileio_sock, dirreq, sizeof(pko_pkt_dread_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, recv_packet,
                        sizeof(pko_pkt_dread_rly), PKO_READDIR_RLY)) {
        dbgprintf("pko_file: pko_read_dir: did not receive OPENDIR_RLY\n");
        return -1;
    }

    dirrly = (pko_pkt_dread_rly *)recv_packet;

    dbgprintf("pko_file: dir read reply received (ret %ld)\n", ntohl(dirrly->retval));

    dirent = (io_dirent_t *)buf;
    // now handle the return buffer translation, to build reply bit
    dirent->stat.mode = ntohl(dirrly->mode);
    dirent->stat.attr = ntohl(dirrly->attr);
    dirent->stat.size = ntohl(dirrly->size);
    dirent->stat.hisize = ntohl(dirrly->hisize);
    memcpy(dirent->stat.ctime, dirrly->ctime, 8);
    memcpy(dirent->stat.atime, dirrly->atime, 8);
    memcpy(dirent->stat.mtime, dirrly->mtime, 8);
    strncpy(dirent->name, dirrly->name, 256);

    return ntohl(dirrly->retval);
}


//----------------------------------------------------------------------
//
int pko_close_dir(int fd)
{
    pko_pkt_close_req *closereq;
    pko_pkt_file_rly *closerly;


    if (pko_fileio_sock < 0) {
        return -1;
    }

    dbgprintf("pko_file: dir close req (fd: %d)\n", fd);

    closereq = (pko_pkt_close_req *)&send_packet[0];
    closerly = (pko_pkt_file_rly *)&recv_packet[0];

    closereq->cmd = htonl(PKO_CLOSEDIR_CMD);
    closereq->len = htons((unsigned short)sizeof(pko_pkt_close_req));
    closereq->fd = htonl(fd);

    if (pko_lwip_send(pko_fileio_sock, closereq, sizeof(pko_pkt_close_req), 0) < 0) {
        return -1;
    }

    if (!pko_accept_pkt(pko_fileio_sock, (char *)closerly,
                        sizeof(pko_pkt_file_rly), PKO_CLOSEDIR_RLY)) {
        dbgprintf("pko_file: pko_close_dir: did not receive PKO_CLOSEDIR_RLY\n");
        return -1;
    }

    dbgprintf("pko_file: dir close reply received (ret %ld)\n",
              ntohl(closerly->retval));

    return ntohl(closerly->retval);
}

//----------------------------------------------------------------------
// Thread that waits for a PC to connect/disconnect/reconnect blah..
int pko_file_serv(void *argv)
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int sock;
    int client_sock;
    int client_len;
    int ret;

    dbgprintf(" - PS2 Side application -\n");

    memset((void *)&server_addr, 0, sizeof(server_addr));
    // Should perhaps specify PC side ip..
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PKO_PORT);

    while ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        dbgprintf("pko_file: socket creation error (%d)\n", sock);
        return -1;
    }

    ret = bind(sock, (struct sockaddr *)&server_addr,
               sizeof(server_addr));
    if (ret < 0) {
        dbgprintf("pko_file: bind error (%d)\n", ret);
        disconnect(sock);
        return -1;
    }

    ret = listen(sock, 5);

    if (ret < 0) {
        dbgprintf("pko_file: listen error (%d)\n", ret);
        disconnect(sock);
        return -1;
    }

    // Active flag kinda sux, cause it wont be checked until a new client has
    // connected.. But it's better than nothing and good for now at least
    pko_fileio_active = 1;

    // Connection loop
    while (pko_fileio_active) {
        dbgprintf("Waiting for connection\n");

        client_len = sizeof(client_addr);
        client_sock = accept(sock, (struct sockaddr *)&client_addr,
                             &client_len);
        if (client_sock < 0) {
            dbgprintf("pko_file: accept error (%d)", client_sock);
            continue;
        }

        dbgprintf("Client connected from %lx\n",
                  client_addr.sin_addr.s_addr);

        remote_pc_addr = client_addr.sin_addr.s_addr;

        if (pko_fileio_sock > 0) {
            dbgprintf("Client reconnected\n");
            ret = disconnect(pko_fileio_sock);
            dbgprintf("close ret %d\n", ret);
        }
        pko_fileio_sock = client_sock;
    }

    if (pko_fileio_sock > 0) {
        disconnect(pko_fileio_sock);
    }

    disconnect(sock);

    ExitDeleteThread();
    return 0;
}

/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <iomanX.h>
#include <ps2ip.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <errno.h>

#define MODNAME "hdldsvr"
IRX_ID(MODNAME, 1, 1);

struct irx_export_table _exp_hdldsvr;

#define TCP_SERVER_PORT 	45233
#define UDP_SERVER_PORT 	45233

#define CMD_SIZE		16
#define CMD_STAT		0x73746174	// 'stat'; get HDD size in KB
#define CMD_READ		0x72656164	// 'read'; read sectors from HDD
#define CMD_WRIT		0x77726974	// 'writ'; write sectors to HDD
#define CMD_WRIS		0x77726973	// 'wris'; get last write status
#define CMD_FLSH		0x666c7368	// 'flsh'; flush write buff
#define CMD_POWX		0x706f7778	// 'powx'; poweroff system

#define HDD_SECTOR_SIZE  	512 		// HDD sector size in bytes

#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE			0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP		0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS		0x00004807
#define APA_DEVCTL_FORMAT		0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
#define APA_DEVCTL_GETTIME		0x00006832
#define APA_DEVCTL_SET_OSDMBR		0x00006833	// arg = hddSetOsdMBR_t
#define APA_DEVCTL_GET_SECTOR_ERROR	0x00006834
#define APA_DEVCTL_GET_ERROR_PART_NAME	0x00006835	// bufp = namebuffer[0x20]
#define APA_DEVCTL_ATA_READ		0x00006836	// arg  = hddAtaTransfer_t
#define APA_DEVCTL_ATA_WRITE		0x00006837	// arg  = hddAtaTransfer_t
#define APA_DEVCTL_SCE_IDENTIFY_DRIVE	0x00006838	// bufp = buffer for atadSceIdentifyDrive 
#define APA_DEVCTL_IS_48BIT		0x00006840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x00006841


static int tcp_server_tid, udp_server_tid;
void tcp_server_thread(void *args);
void udp_server_thread(void *args);

static u8 tcp_buf[CMD_SIZE + HDD_SECTOR_SIZE*2] __attribute__((aligned(64)));

struct tcp_packet_header {
	u32 command;
	u32 start_sector;
	u32 num_sectors;
	u32 result;
} __attribute__((packed));

typedef struct {
	struct tcp_packet_header hdr;
	u8  data[HDD_SECTOR_SIZE*2];
} tcp_packet_t __attribute__((packed));


//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	iop_thread_t thread_param;

	RegisterLibraryEntries(&_exp_hdldsvr);

 	thread_param.attr = TH_C;
 	thread_param.thread = (void *)tcp_server_thread;
 	thread_param.priority = 0x60;
 	thread_param.stacksize = 0x1000;
 	thread_param.option = 0;

	tcp_server_tid = CreateThread(&thread_param);

	StartThread(tcp_server_tid, 0);

 	thread_param.attr = TH_C;
 	thread_param.thread = (void *)udp_server_thread;
 	thread_param.priority = 0x64;
 	thread_param.stacksize = 0x1000;
 	thread_param.option = 0;

	udp_server_tid = CreateThread(&thread_param);

	StartThread(udp_server_tid, 0);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
	DeleteThread(tcp_server_tid);
	DeleteThread(udp_server_tid);

	return 0;
}

//-------------------------------------------------------------------------
int recv_nonblocking(int sock, void *buf, int bsize)
{
	register int r;
	fd_set rfd;

	FD_ZERO(&rfd);
	FD_SET(sock, &rfd);

	r = lwip_select(sock+1, &rfd, NULL, NULL, NULL);
	if (r < 0)
		return -1;

	r = lwip_recv(sock, buf, bsize, 0);
	if (r < 0)
		return -2;

	return r;
}

//-------------------------------------------------------------------------
int send_response(int sock, void *buf, int bsize)
{
	return lwip_send(sock, buf, bsize, 0);
}

//-------------------------------------------------------------------------
int handle_tcp_client(int tcp_client_socket)
{
	register int r, size;
	u8 args[16];
	tcp_packet_t *pkt = (tcp_packet_t *)tcp_buf;

	while (1) {
		size = recv_nonblocking(tcp_client_socket, &tcp_buf[0], sizeof(tcp_buf));
		if (size <= 0)
			goto error;

		if (size == CMD_SIZE) {

			switch (pkt->hdr.command) {

				case CMD_STAT:
					r = devctl("hdd0:", APA_DEVCTL_TOTAL_SECTORS, NULL, 0, NULL, 0);
					if (r < 0)
						r = -1;
					pkt->hdr.result = r >> 1;
					send_response(tcp_client_socket, &tcp_buf[0], sizeof(struct tcp_packet_header));
					break;

				case CMD_READ:
					*(u32 *)&args[0] = pkt->hdr.start_sector;
					*(u32 *)&args[4] = pkt->hdr.num_sectors;

					r = devctl("hdd0:", APA_DEVCTL_ATA_READ, args, 8, pkt->data, pkt->hdr.num_sectors*HDD_SECTOR_SIZE);
					if (r < 0)
						r = -1;
					pkt->hdr.result = pkt->hdr.num_sectors;
					send_response(tcp_client_socket, &tcp_buf[0], sizeof(tcp_packet_t));
					break;

				default:
					break;
			}
		}
	}

error:
	return 0;
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

		tcp_socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
		if (tcp_socket < 0)
			goto error;

		r = lwip_bind(tcp_socket, (struct sockaddr *)&peer, sizeof(peer));
		if (r < 0)
			goto error;

		r = lwip_listen(tcp_socket, 3);
		if (r < 0)
			goto error;

		while(1) {
			peerlen = sizeof(peer);
			client_socket = lwip_accept(tcp_socket, (struct sockaddr *)&peer, &peerlen);
			if (client_socket < 0)
				goto error;

			r = handle_tcp_client(client_socket);
			if (r < 0)
				lwip_close(client_socket);
		}

error:
		lwip_close(tcp_socket);
	}
}

//-------------------------------------------------------------------------
void udp_server_thread(void *args)
{
	int udp_socket;
	struct sockaddr_in peer;
	register int r;

	while (1) {

		peer.sin_family = AF_INET;
		peer.sin_port = htons(UDP_SERVER_PORT);
		peer.sin_addr.s_addr = htonl(INADDR_ANY);

		udp_socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
		if (udp_socket < 0)
			goto error;

		r = lwip_bind(udp_socket, (struct sockaddr *)&peer, sizeof(peer));
		if (r < 0)
			goto error;

		while(1) {
			// should wait a sema
		}

error:
		lwip_close(udp_socket);
	}
}


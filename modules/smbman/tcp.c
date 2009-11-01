/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "tcp.h"

u8 SMB_buf[65536+1024];

smb_NegProt_Rsp *NegProtResponse;
smb_SessSetup_Rsp *SessSetupResponse;

static u16 UID, TID, SID;
static int main_socket;

static int getdir_index = 0;

const u8 *dialects[1] = {
	//(const u8 *)"PC NETWORK PROGRAM 1.0",
	//(const u8 *)"MICROSOFT NETWORKS 1.03",
	//(const u8 *)"MICROSOFT NETWORKS 3.0",
	//(const u8 *)"LANMAN1.0",
	//(const u8 *)"LM1.2X002",
	//(const u8 *)"LANMAN2.1",
	//(const u8 *)"Samba",
	(const u8 *)"NT LM 0.12"
};

//-------------------------------------------------------------------------
int RecvTimeout(int sock, u8 *buf, int bsize, int timeout)
{
	int ret;
	struct pollfd pollfd[1];
	
	pollfd->fd = sock;
	pollfd->events = POLLIN;
	pollfd->revents = 0;
	ret = poll(pollfd, 1, timeout);
	
	// a result less than 0 is an error
	if (ret < 0) {
		//printf("smbman - tcp: failed to wait packet...\n");
		return -1;
	}
	
	// 0 is a timeout
	if (ret == 0) {
		//printf("smbman - tcp: wait packet timeout...\n");
		return 0;
	}		
	
	// receive the packet
	ret = lwip_recv(sock, buf, bsize, 0);
	if (ret < 0) {
		//printf("smbman - tcp: failed to receive packet...\n");
		return -2;		
	}

	return ret;
}

//-------------------------------------------------------------------------
int OpenTCPSession(struct in_addr dst_IP, u16 dst_port)
{
	int sock;
	int ret, retries;
	struct sockaddr_in sock_addr;
	
	// Creating socket
	sock = lwip_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		//printf("smbman - tcp: failed to create socket...\n");
		return -1;
	}

    memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_addr = dst_IP;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(dst_port);
		 	
	// 3 attemps to connect
	for (retries = 0; retries < 3; retries++) {
		ret = lwip_connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
		if (ret >= 0)
			break;
		DelayThread(100);	 
	}
	if (retries >= 3) {
		//printf("smbman - tcp: failed to connect socket...\n");
		return -2;
	}		
		
	return sock;
}

//-------------------------------------------------------------------------
int tcp_ConnectSMBClient(char *SMBServerIP, int SMBServerPort)
{
	int size, recv_size, total_packet_size;
	int i, err;
	struct in_addr dst_addr;
	int retries;
	
	// SMB server IP
	dst_addr.s_addr = inet_addr(SMBServerIP);

	retries = 0;
	
conn_open:

	err = 0;
		
	// Opening TCP session
	main_socket = OpenTCPSession(dst_addr, SMBServerPort);
	if (main_socket < 0) {
		//printf("smbman - tcp_ConnectSMBClient: failed to open TCP session on port %d\n", SMBServerPort);
		err = -1;
		goto conn_open;		
	}

	memset(SMB_buf, sizeof(SMB_buf), 0);
	
	// Prepare the Negociate Protocol message
	size = smb_NegProtRequest(SMB_buf, sizeof(SMB_buf), 1, (u8 **)dialects);
	if (size <= 0) {
		//printf("smbman - tcp_ConnectSMBClient: failed to build Negociate Protocol Request message...\n");
		err = -2;
		goto conn_close;		
	}

	// Send the Negociate Protocol Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_ConnectSMBClient: failed to send Negociate Protocol Request...\n");
		err = -3;
		goto conn_close;		
	}
	
	// Receive the Negociate Protocol Response with a timeout of 20 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 20000);
	if (size <= 0) {
		//printf("smbman - tcp_ConnectSMBClient: failed to receive Negociate Protocol Response...\n");
		err = -4;
		goto conn_close;		
	}

	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 20000);
		if (size <= 0) {
			//printf("smbman - tcp_ConnectSMBClient: failed to receive Negociate Protocol Response...\n");
			err = -4;
			goto conn_close;
		}				
		recv_size += size;
	}
			
	// Validate Negociate Protocol Response
	size = smb_NegProtResponse(SMB_buf, recv_size, &NegProtResponse);
	if (size <= 0) {
		//printf("smbman - tcp_ConnectSMBClient: received Invalid Negociate Protocol Response... %d\n", size);
		err = -5;
		goto conn_close;		
	}
		
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse DialectIndex = %d\n", NegProtResponse->DialectIndex[0]);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse SecurityMode = %d\n", NegProtResponse->SecurityMode);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse MaxMpxCount = %d\n", NegProtResponse->MaxMpxCount);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse MaxNumberVCs = %d\n", NegProtResponse->MaxNumberVCs);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse MaxBufferSize = %d\n", (int)NegProtResponse->MaxBufferSize);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse MaxRawSize = %d\n", (int)NegProtResponse->MaxRawSize);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse SessionKey = %d\n", (int)NegProtResponse->SessionKey);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse Capabilities = %d\n", (int)NegProtResponse->Capabilities);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse SystemTime.timeLow = %d\n", (int)NegProtResponse->SystemTime.timeLow);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse SystemTime.timeHigh = %d\n", (int)NegProtResponse->SystemTime.timeHigh);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse ServerTimeZone = %d\n", NegProtResponse->ServerTimeZone);
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse EncryptionKeyLength = %d\n", NegProtResponse->EncryptionKeyLength);

	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse EncryptionKey = ");		
	for (i = 0; i < NegProtResponse->EncryptionKeyLength; i++)
		//printf("%02x ", NegProtResponse->EncryptionKey[i]); 
	//printf("\n");
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse DomainName = %s\n", NegProtResponse->DomainName);		
	//printf("smbman - tcp_ConnectSMBClient: NegProtResponse Server = %s\n", NegProtResponse->Server);		
			
	// connection should be established
	return 0;	
					
conn_close:	

	if (err) {
		// Close the socket
		lwip_close(main_socket);
	
		//printf("smbman - tcp_ConnectSMBClient: TCP session finished\n");
		
		retries++;
	
		if (retries < 3)
			goto conn_open;
	}
		
	return err;		
}

//-------------------------------------------------------------------------
int tcp_SessionSetup(char *User, char *Password, char *Share)
{
	int size, recv_size, total_packet_size;
	
	// Prepare the Session Setup Request message
	size = smb_SessionSetupRequest(SMB_buf, sizeof(SMB_buf), User, Password, Share);
	if (size <= 0) {
		//printf("smbman - tcp_SessionSetup: failed to build Session Setup Request message...\n");
		return -1;	
	}

	// Send the Session Setup Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_SessionSetup: failed to send Session Setup Request...\n");
		return -2;
	}
	
	// Receive the Session Setup Response with a timeout of 10 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
	if (size <= 0) {
		//printf("smbman - tcp_SessionSetup: failed to receive Session Setup Response...\n");
		return -3;
	}

	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
		if (size <= 0) {
			//printf("smbman - tcp_SessionSetup: failed to receive Session Setup Response...\n");
			return -3;
		}				
		recv_size += size;
	}
		
	// Validate Session Setup Response
	size = smb_SessionSetupResponse(SMB_buf, recv_size, &SessSetupResponse, &UID, &TID);
	if (size <= 0) {
		//printf("smbman - tcp_SessionSetup: received Invalid Session Setup Response... %d\n", size);
		return size;
	}
		
	//printf("smbman - tcp_SessionSetup: SessSetupResponse Action = %d\n", SessSetupResponse->SessionSetupAndX_Rsp.Action);
	//printf("smbman - tcp_SessionSetup: SessSetupResponse NativeOS = %s\n", SessSetupResponse->SessionSetupAndX_Rsp.NativeOS);
	//printf("smbman - tcp_SessionSetup: SessSetupResponse NativeLanMan = %s\n", SessSetupResponse->SessionSetupAndX_Rsp.NativeLanMan);
	//printf("smbman - tcp_SessionSetup: SessSetupResponse PrimaryDomain = %s\n", SessSetupResponse->SessionSetupAndX_Rsp.PrimaryDomain);
	//printf("smbman - tcp_SessionSetup: SessSetupResponse Service = %s\n", SessSetupResponse->TreeConnectAndX_Rsp.Service);
	//printf("smbman - tcp_SessionSetup: SessSetupResponse NativeFileSystem = %s\n", SessSetupResponse->TreeConnectAndX_Rsp.NativeFileSystem);
	
	// Login & Tree Connect to the share is done
	
	return 0;
}

//-------------------------------------------------------------------------
int tcp_DisconnectSMBClient(void)
{
	// Close the socket
	lwip_close(main_socket);
	
	//printf("smbman - tcp_DisconnectSMBClient: TCP session finished\n");
	
	return 0;
}

//-------------------------------------------------------------------------
int tcp_GetDir(char *name, int maxent, smb_FindFirst2_Entry *smbT)
{
	int size, nument, EOS, err, recv_size, total_packet_size;
	
	if (getdir_index == 0) {
		// Prepare the Find First2 Request message
		size = smb_FindFirst2Request(SMB_buf, sizeof(SMB_buf), name, UID, TID, maxent);
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to build Find First2 Request message...\n");
			err = -1;
			goto err_out;		
		}

		// Send the Find First2 Request
		size = lwip_send(main_socket, SMB_buf, size, 0); 
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to send Find First2 Request...\n");
			err = -2;
			goto err_out;		
		}
	
		// Receive the Find First2 Response with a timeout of 10 seconds
		size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to receive Find First2 Response...\n");
			err = -3;
			goto err_out;		
		}

		// Handle fragmented packets
		total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
		recv_size = size;
	
		while (recv_size < total_packet_size) {
		
			size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
			if (size <= 0) {
				//printf("smbman - tcp_GetDir: failed to receive Find First2 Response...\n");
				err = -3;
				goto err_out;		
			}				
			recv_size += size;
		}
			
		// Validate Find First2 Response	
		size = smb_FindFirst2Response(SMB_buf, recv_size, &nument, smbT, &EOS, &SID);
		if (size <= 0) {
			if (size == -1000) {
				//printf("smbman - tcp_GetDir: Find First2 Response NO MEDIA IN DEVICE...\n");
				err = -1000;
			}
			else {
				//printf("smbman - tcp_GetDir: received Invalid Find First2 Response %d\n", size);
				err = -4;
			}
			goto err_out;		
		}		
	}
	else {
		// Prepare the Find Next2 Request message
		size = smb_FindNext2Request(SMB_buf, sizeof(SMB_buf), UID, TID, SID, maxent);
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to build Find Next2 Request message...\n");
			err = -1;
			goto err_out;		
		}

		// Send the Find Next2 Request
		size = lwip_send(main_socket, SMB_buf, size, 0); 
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to send Find Next2 Request...\n");
			err = -2;
			goto err_out;		
		}

		// Receive the Find Next2 Response with a timeout of 10 seconds
		size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
		if (size <= 0) {
			//printf("smbman - tcp_GetDir: failed to receive Find Next2 Response...\n");
			err = -3;
			goto err_out;		
		}

		// Handle fragmented packets
		total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
		recv_size = size;
	
		while (recv_size < total_packet_size) {
		
			size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
			if (size <= 0) {
				//printf("smbman - tcp_GetDir: failed to receive Find Next2 Response...\n");
				return -3;
			}				
			recv_size += size;
		}
			
		// Validate Find Next2 Response	
		size = smb_FindNext2Response(SMB_buf, recv_size, &nument, smbT, &EOS);
		if (size <= 0) {
			if (size == -1000) {
				//printf("smbman - tcp_GetDir: Find Next2 Response NO MEDIA IN DEVICE...\n");
				err = -1000;
			}
			else { 
				//printf("smbman - tcp_GetDir: received Invalid Find Next2 Response %d\n", size);
				err = -4;
			}
			goto err_out;		
		}		
	}
			
	getdir_index++;
			
	if (EOS) {
		getdir_index = 0;
		return 0;
	}
	
	return nument;
	
err_out:
	getdir_index = 0;
	return err;
}

//-------------------------------------------------------------------------
int tcp_EchoSMBServer(u8 *msg, int sz_msg)
{
	int size, recv_size, total_packet_size; ;
	
	// Prepare the Echo Request message
	size = smb_EchoRequest(SMB_buf, sizeof(SMB_buf), msg, sz_msg);
	if (size <= 0) {
		//printf("smbman - tcp_EchoSMBServer: failed to build Echo Request message...\n");
		return -1;
	}

	// Send the Echo Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_EchoSMBServer: failed to send Echo Request...\n");
		return -2;
	}
	
	// Receive the Echo Response with a timeout of 10 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
	if (size <= 0) {
		//printf("smbman - tcp_EchoSMBServer: failed to receive Echo Response...\n");
		return -3;
	}

	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
		if (size <= 0) {
			//printf("smbman - tcp_EchoSMBServer: failed to receive Echo Response...\n");
			return -3;
		}				
		recv_size += size;
	}
	
	// Validate Echo Response	
	size = smb_EchoResponse(SMB_buf, recv_size, msg, sz_msg);
	if (size <= 0) {
		//printf("smbman - tcp_EchoSMBServer: received Invalid Echo Response %d\n", size);
		return -4;
	}		
	
	return 0;
}

//-------------------------------------------------------------------------
int tcp_Open(char *filename, u16 *FID, u32 *filesize)
{
	int size, recv_size, total_packet_size; 
	
	// Prepare the NT Create AndX Request message
	size = smb_NTCreateAndXRequest(SMB_buf, sizeof(SMB_buf), UID, TID, filename);
	if (size <= 0) {
		//printf("smbman - tcp_Open: failed to build NT Create AndX Request message...\n");
		return -1;
	}

	// Send the NT Create AndX Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_Open: failed to send NT Create AndX Request...\n");
		return -2;
	}
	
	// Receive the NT Create AndX Response with a timeout of 10 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
	if (size <= 0) {
		//printf("smbman - tcp_Open: failed to receive NT Create AndX Response...\n");
		return -3;
	}

	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
		if (size <= 0) {
			//printf("smbman - tcp_Open: failed to receive NT Create AndX Response...\n");
			return -3;
		}				
		recv_size += size;
	}
	
	// Validate NT Create AndX Response	
	size = smb_NTCreateAndXResponse(SMB_buf, recv_size, FID, filesize);
	if (size <= 0) {
		//printf("smbman - tcp_Open: received Invalid NT Create AndX Response %d\n", size);
		return -4;
	}		
		
	return 0;
}

//-------------------------------------------------------------------------
int tcp_Read(u16 FID, void *buf, u32 offset, u16 nbytes)
{
	int size, recv_size, total_packet_size; 
	u16 readbytes;

	//printf("smbman - tcp_Read: nbytes = %d\n", (int)nbytes);
	
	// Prepare the Read AndX Request message
	size = smb_ReadAndXRequest(SMB_buf, sizeof(SMB_buf), UID, TID, FID, offset, nbytes);
	if (size <= 0) {
		//printf("smbman - tcp_Read: failed to build Read AndX Request message...\n");
		return -1;
	}

	// Send the Read AndX Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_Read: failed to send Read AndX Request...\n");
		return -2;
	}
			
	// Receive the Read AndX Response with a timeout of 10 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
	if (size <= 0) {
		//printf("smbman - tcp_Read: failed to receive Read AndX Response...\n");
		return -3;
	}
	
	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
		if (size <= 0) {
			//printf("smbman - tcp_Read: failed to receive Read AndX Response...\n");
			return -3;
		}				
		recv_size += size;
	}
	
	// Validate Read AndX Response	
	size = smb_ReadAndXResponse(SMB_buf, recv_size, buf, &readbytes);
	if (size <= 0) {
		//printf("smbman - tcp_Read: received Invalid Read AndX Response %d\n", size);
		return -4;
	}		
			
	return readbytes;
}

//-------------------------------------------------------------------------
int tcp_Close(u16 FID)
{
	int size, recv_size, total_packet_size; 
	
	// Prepare the Close Request message
	size = smb_CloseRequest(SMB_buf, sizeof(SMB_buf), UID, TID, FID);
	if (size <= 0) {
		//printf("smbman - tcp_Close: failed to build Close Request message...\n");
		return -1;
	}

	// Send the Close Request
	size = lwip_send(main_socket, SMB_buf, size, 0); 
	if (size <= 0) {
		//printf("smbman - tcp_Close: failed to send Close Request...\n");
		return -2;
	}
	
	// Receive the Close Response with a timeout of 10 seconds
	size = RecvTimeout(main_socket, SMB_buf, sizeof(SMB_buf), 10000);
	if (size <= 0) {
		//printf("smbman - tcp_Close: failed to receive Close Response...\n");
		return -3;
	}

	// Handle fragmented packets
	total_packet_size = rawTCP_GetSessionHeader(SMB_buf) + 4;
	recv_size = size;
	
	while (recv_size < total_packet_size) {
		
		size = RecvTimeout(main_socket, &SMB_buf[recv_size], sizeof(SMB_buf) - recv_size, 10000);
		if (size <= 0) {
			//printf("smbman - tcp_Close: failed to receive Close Response...\n");
			return -3;
		}				
		recv_size += size;
	}	
		
	// Validate Close Response	
	size = smb_CloseResponse(SMB_buf, recv_size);
	if (size <= 0) {
		//printf("smbman - tcp_Close: received Invalid Close Response %d\n", size);
		return -4;
	}		
			
	return 0;
}

/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <sysclib.h>

#include "smb.h"

// SMB header magic
const u8 *smb_hdrSMBmagic = "\xffSMB";

// To hold the Negociate Protocol Response
smb_NegProt_Rsp smb_NegProt_Response __attribute__((aligned(64)));

// To hold the Session Setup Response
smb_SessSetup_Rsp smb_SessSetup_Response __attribute__((aligned(64)));

//-------------------------------------------------------------------------
u16 smb_GetShort(u8 *src, int offset) // Converts little-endian u16 value to big-endian
{
	u16 tmp;
	
	tmp  = (u16)src[offset];
	tmp |= (u16)(src[offset+1] << 8);
	
	return tmp;
}

//-------------------------------------------------------------------------
void smb_SetShort(u8 *dst, int offset, u16 val) // Converts big-endian u16 value to little-endian
{
	dst[offset]   = (u8)(val & 0xff);
	dst[offset+1] = (u8)(val >> 8) & 0xff;
}

//-------------------------------------------------------------------------
u32 smb_GetLong(u8 *src, int offset) // Converts little-endian u32 value to big-endian 
{
	u32 tmp;
	
	tmp  = (u32)src[offset];
	tmp |= (u32)(src[offset+1] << 8);
	tmp |= (u32)(src[offset+2] << 16);
	tmp |= (u32)(src[offset+3] << 24);
	
	return tmp;
}

//-------------------------------------------------------------------------
void smb_SetLong(u8 *dst, int offset, u32 val) // Converts big-endian u32 to little-endian
{
	dst[offset]   = (u8)(val & 0xff);
	dst[offset+1] = (u8)((val >> 8) & 0xff);
	dst[offset+2] = (u8)((val >> 16) & 0xff);
	dst[offset+3] = (u8)((val >> 24) & 0xff);	
}

//-------------------------------------------------------------------------
int smb_hdrInit(u8 *buf, int bsize)	// Initialize an empty SMB header struct
{
	int i;
	
	if (bsize < SMB_HDR_SIZE)
		return -1;
	
	memset(buf, SMB_HDR_SIZE, 0);
	 	
	for (i = 0; i <	4; i++)
		buf[i] = smb_hdrSMBmagic[i];
		
	for (i = 4; i <	SMB_HDR_SIZE; i++)
		buf[i] = '\0';
	
	return SMB_HDR_SIZE;		 
}

//-------------------------------------------------------------------------
int smb_hdrCheck(u8 *buf, int bsize) // Perform SMB header sanity check on a given message
{
	int i;
	
	if (buf == NULL)
		return -1;
		
	if (bsize < SMB_HDR_SIZE)
		return -2;
		
	for (i = 0; i < 4; i++) {
		if (buf[i] != smb_hdrSMBmagic[i])
			return -3;
	}
	
	return SMB_HDR_SIZE;			
}

//-------------------------------------------------------------------------
int rawTCP_SetSessionHeader(u8 *buf, u32 size) // Write Session Service header: careful it's raw TCP transport here and not NBT transport
{
	if (size > 0xffffff) // maximum for raw TCP transport (24 bits)
		return -1;
		
	buf[0] = 0;
	buf[1] = (size >> 16) & 0xff;
	buf[2] = (size >> 8) & 0xff;
	buf[3] = size & 0xff;
		
	return (int)size;	
}

//-------------------------------------------------------------------------
int rawTCP_GetSessionHeader(u8 *buf) // Read Session Service header: careful it's raw TCP transport here and not NBT transport
{
	u32 size;
	
	size  = buf[3];
	size |= buf[2] << 8;
	size |= buf[1] << 16;
	
	if (size > 0xffffff) // maximum for raw TCP transport (24 bits)
		return -1;
			
	return (int)size;	
}

//-------------------------------------------------------------------------
int smb_NegProtRequest(u8 *buf, int bsize, int namec, u8 **namev) // Builds a Negociate Procotol Request message
{
	u8 *smb_buf;
	int i, length, offset;
	u16 bytecount;
	u8 flags;
	u16 flags2;
	
	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
	
	// make sure we have enough room for the header, wodcount field, and bytecount field
	if (bsize < (SMB_HDR_SIZE + 3))
		return -1;
	
	// initialize SMB header
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags = SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_NEGOCIATE);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	
	// fill in the empty parameter block
	smb_buf[SMB_HDR_SIZE] = 0;
	
	// fill in data block
	// copy the dialect names into message
	// set offset to indicate the start of the bytes field, skipping bytecount
	offset = SMB_HDR_SIZE + 3;
	for (bytecount = i = 0; i < namec; i++) {
		length = strlen(namev[i]) + 1;		// includes nul
		if (bsize < (offset + 1 + length))	// includes 0x02
			return -1;
		smb_buf[offset++] = '\x02';
		memcpy(&smb_buf[offset], namev[i], length);
		offset += length;
		bytecount += length + 1;	
	} 

	// bytecount field start one byte beyond the end of the header (one byte for the wordcount field)	
	smb_SetShort(smb_buf, SMB_HDR_SIZE + 1, bytecount);	
		
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -2;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_NegProtResponse(u8 *buf, int bsize, smb_NegProt_Rsp **NegProtRsp) // analyze Negociate Procotol Response message
{
	u8 *smb_buf;
	int i, offset;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0) {
		*NegProtRsp = NULL;
		return -1;
	}

	// check there's no error
	if (smb_hdrGetNTStatus(smb_buf) != STATUS_SUCCESS) {
		*NegProtRsp = NULL;
		return -1000;
	}
	
	// we ensure that wordcount > 0
	if (((smb_NegProt_Rsp *)&smb_buf[SMB_HDR_SIZE])->WordCount <= 0) {
		*NegProtRsp = NULL;
		return -2; 
	}
		
	// we ensure that the SMB server has choosen NT LM 0.12 dialect	
	if (((smb_NegProt_Rsp *)&smb_buf[SMB_HDR_SIZE])->DialectIndex[0] != 0) {
		*NegProtRsp = NULL;
		return -3; 
	}
		
	// copy Negociate Protocol Response Parameters block to main struct, 
	// typically with NT LM 0.12, it should be 35 bytes for this response (17 words + wordcount) 
	memcpy(&smb_NegProt_Response, &smb_buf[SMB_HDR_SIZE], sizeof(smb_NegProt_Response));
	
	// check that wordcount is 17	
	if (smb_NegProt_Response.WordCount != 17) {
		*NegProtRsp = NULL;
		return -4;
	}	
		
	offset = SMB_HDR_SIZE + (smb_NegProt_Response.WordCount * 2) + 1; // offset is on bytecount;
	
	// get bytecount
	bytecount = smb_GetShort(smb_buf, offset);
	
	// Check that bytecount is not 0
	if (bytecount == 0) {
		*NegProtRsp = NULL;
		return -5;
	}
	
	// fill in our struct with ByteCount
	smb_NegProt_Response.ByteCount = bytecount; 

	// Move offset to Bytes field
	offset += 2;
		
	// Capabilities - Extended Security exchanges supported check
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_EXTENDED_SECURITY) {
		// Extended Security has been negociated
		// Not the case with the LinkSys NAS	
	}
	else {
		// Extended Security not supported.
		
		// test NEGOCIATE_SECURITY_CHALLENGE_RESPONSE bit
		if (smb_NegProt_Response.SecurityMode & NEGOCIATE_SECURITY_CHALLENGE_RESPONSE) {
			// Use Challenge/Response
			// Encryption_Key (is the Challenge) is 8 bytes long
			memcpy(&smb_NegProt_Response.EncryptionKey[0], &smb_buf[offset], 8);
			offset += 8;  
		}	// else Use PlainText Passwords
		
		// Extract Domain Name
		i = 0;
		while (smb_buf[offset] != 0) {
			smb_NegProt_Response.DomainName[i++] = smb_buf[offset];
			offset += 2;
		}
		smb_NegProt_Response.DomainName[i] = smb_buf[offset]; // get nul terminater
		offset += 2;
	}
	
	/*
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf)) {
		*NegProtRsp = NULL;
		return -6;
	}
	*/
	
	*NegProtRsp = &smb_NegProt_Response;
	
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
// *** Please Note that the 2 functions below is an ANDX Mutation that build SessionSetupAndX & TreeConnectAndX SMB *** //

int smb_SessionSetupRequest(u8 *buf, int bsize, char *User, char *Password, char *share_name) // Builds a Session Setup Request message, for NT LM 0.12 dialect, Non Extended Security negociated
{
	u8 *smb_buf;
	u8 wordcount = 13; // size of our SMB parameters: 13 words
	int i, offset, bytecount_offset, length1, length2;
	u8 flags;
	u16 flags2;
	u16 bytecount;
	u32 smb_client_capabilities;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags = SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	flags2 |= SMB_FLAGS2_UNICODE_STRING;
	flags2 |= SMB_FLAGS2_32BIT_STATUS;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_SESSION_SETUP_ANDX);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  				// 12 or 13 words for NT LM 0.12
	offset = SMB_OFFSET_ANDX_OFFSET + 2;	
	smb_SetShort(smb_buf, offset, smb_NegProt_Response.MaxBufferSize);// MaxBufferSize
	offset += 2;	
	smb_SetShort(smb_buf, offset, (smb_NegProt_Response.MaxMpxCount >= 2) ? 2 : smb_NegProt_Response.MaxMpxCount); 	// MaxMpxCount
	offset += 2;	
	smb_SetShort(smb_buf, offset, 1);							// VcNumber
	offset += 2;	
	smb_SetLong(smb_buf, offset, smb_NegProt_Response.SessionKey); 	// SessionKey
	offset += 4;	
	smb_SetShort(smb_buf, offset, 0); 							// CaseInsensitivePasswordLength
	offset += 2;
	smb_SetShort(smb_buf, offset, 0); 							// CaseSensitivePasswordLength
	offset += 2;
	smb_SetLong(smb_buf, offset, 0); 							// Reserved, must be 0
	offset += 4;
	smb_client_capabilities = 0;								// Setting client capabilities, after checking Server capabilities
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_STATUS32)
		smb_client_capabilities |= CLIENT_CAP_STATUS32;
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_NT_SMBS)
		smb_client_capabilities |= CLIENT_CAP_NT_SMBS;
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_LARGE_FILES)
		smb_client_capabilities |= CLIENT_CAP_LARGE_FILES;
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_UNICODE)
		smb_client_capabilities |= CLIENT_CAP_UNICODE;		
	if (smb_NegProt_Response.Capabilities & SERVER_CAP_LARGE_READX)
		smb_client_capabilities |= CLIENT_CAP_LARGE_READX;		 		 
	smb_SetLong(smb_buf, offset, smb_client_capabilities);		// Capabilities (client)
	offset += 4;

	// fill in data block
	// set offset to indicate the start of the bytes field, skipping bytecount
	bytecount_offset = offset;
	offset += 2; 										// skip ByteCount field	
	smb_buf[offset++] = 0;								// ANSI password
	//memset(&smb_buf[offset], 0, 24);
	//offset += 24;
	//memset(&smb_buf[offset], 0, 24);
	//offset += 24;
	
	for (length1 = 0; User[length1] != 0; length1++) {;}// Get User Name length		
	if (bsize < (offset + (length1 << 1) + 2)) 			// room space check
		return -2;
	for (i = 0; i < length1; i++) {
		smb_buf[offset++] = User[i]; 					// add User name
		smb_buf[offset++] = 0;
	}
	smb_buf[offset++] = 0;
	smb_buf[offset++] = 0;								// nul terminator	
	for (length2 = 0; smb_NegProt_Response.DomainName[length2] != 0; length2++) {;} // Get DomainName length from Negociate Protocol Response Datas	
	if (bsize < (offset + (length2 << 1) + 2)) 			// room space check
		return -2;			
	for (i = 0; i < length2; i++) {
		smb_buf[offset++] = smb_NegProt_Response.DomainName[i]; // PrimaryDomain, acquired from Negociate Protocol Response Datas
		smb_buf[offset++] = 0;		
	}
	smb_buf[offset++] = 0;
	smb_buf[offset++] = 0;								// nul terminator
	if (bsize < (offset + 10)) 							// room space check
		return -2;			
	memcpy(&smb_buf[offset], "S\0M\0B\0\0\0\0\0", 10);	// NativeOS	
	offset += 10;	
	if (bsize < (offset + 10)) 							// room space check
		return -2;			
	memcpy(&smb_buf[offset], "S\0M\0B\0\0\0\0\0", 10); 	// NativeLanMan
	offset += 10;		
	
	bytecount = 1 + (length1 << 1) + 2 + (length2 << 1) + 2 + 10 + 10;
	
	// fill bytecount field	
	smb_SetShort(smb_buf, bytecount_offset, bytecount);	

	// fill ANDX block in parameter block
	smb_buf[SMB_OFFSET_ANDX_CMD] = SMB_COM_TREE_CONNECT_ANDX;	// Tree Connect ANDX command
	smb_buf[SMB_OFFSET_ANDX_RESERVED] = 0;						// ANDX reserved, must be 0
	smb_SetShort(smb_buf, SMB_OFFSET_ANDX_OFFSET, offset);		// ANDX offset

	// fill Tree Connect ANDX block 
	if (bsize < (offset + (4 * 2))) 	// room space check, 4 words
		return -2;									
	smb_buf[offset++] = 4; 				// Tree Connect ANDX WordCount
	smb_buf[offset++] = SMB_COM_NONE; 	// Last ANDX cmd
	smb_buf[offset++] = 0; 				// ANDX reserved (must be 0)
	smb_SetShort(smb_buf, offset, 0);	// next ANDX offset (0 since no further AndX)
	offset += 2;
	smb_SetShort(smb_buf, offset, 0);	// flags (bit 0 set = disconnect Tid)
	offset += 2;
	smb_SetShort(smb_buf, offset, 1);	// PasswordLength
	offset += 2;
	bytecount_offset = offset;
	offset += 2; 						// skip ByteCount field
	if (bsize < (offset + 1)) 			// room space check
		return -2;
	smb_buf[offset++] = 0;				// Password							
	for (length2 = 0; share_name[length2] != 0; length2++) {;}	// Get Share Name length		
	if (bsize < (offset + (length2 << 1) + 2)) 					// room space check
		return -2;
	for (i = 0; i < length2; i++) {
		smb_buf[offset++] = share_name[i]; 			// add Share name
		smb_buf[offset++] = 0;
	}
	smb_buf[offset++] = 0;
	smb_buf[offset++] = 0;							// nul terminator	
	if (bsize < (offset + 6)) 						// room space check
		return -2;										
	memcpy(&smb_buf[offset], "?????\0", 6); 		// Service, any type of device
	offset += 6;
	bytecount = 1 + (length2 << 1) + 2 + 6;		
	smb_SetShort(smb_buf, bytecount_offset, bytecount); // fill bytecount field	
					
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_SessionSetupResponse(u8 *buf, int bsize, smb_SessSetup_Rsp **SessSetupRsp, u16 *UID, u16 *TID) // analyze Session Setup Response message
{
	u8 *smb_buf;
	int offset, length, i;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0) {
		*SessSetupRsp = NULL;
		return -1;
	}

	// check there's no error
	if (smb_hdrGetNTStatus(smb_buf) != STATUS_SUCCESS) {
		*SessSetupRsp = NULL;		
		return -1000;
	}
	
	// we ensure that first AndX response wordcount > 0
	if (((smb_SessSetup_Rsp *)&smb_buf[SMB_HDR_SIZE])->SessionSetupAndX_Rsp.WordCount <= 0) {
		*SessSetupRsp = NULL;
		return -2; 
	}

	// copy Session Setup AndX Response Wordcount 
	smb_SessSetup_Response.SessionSetupAndX_Rsp.WordCount = smb_buf[SMB_OFFSET_WORDCOUNT];
	
	// check that wordcount is 3
	if (smb_SessSetup_Response.SessionSetupAndX_Rsp.WordCount != 3) {
		*SessSetupRsp = NULL;
		return -3;
	}

	// Get Next ANDX params, should SMB_CMD_NONE and 0
	smb_SessSetup_Response.SessionSetupAndX_Rsp.AndXCommand = smb_buf[SMB_OFFSET_ANDX_CMD];
	smb_SessSetup_Response.SessionSetupAndX_Rsp.AndXReserved = smb_buf[SMB_OFFSET_ANDX_RESERVED];
	offset = SMB_OFFSET_ANDX_OFFSET; 	
	smb_SessSetup_Response.SessionSetupAndX_Rsp.AndXOffset = smb_GetShort(smb_buf, offset);	
	offset += 2; // offset is on Action;		
	
	smb_SessSetup_Response.SessionSetupAndX_Rsp.Action = smb_GetShort(smb_buf, offset);
	offset += 2; // offset is on ByteCount;
				
	// get bytecount
	bytecount = smb_GetShort(smb_buf, offset);
	
	// Check that bytecount is not 0
	if (bytecount == 0) {
		*SessSetupRsp = NULL;
		return -5;
	}
	
	// fill in our struct ByteCount
	smb_SessSetup_Response.SessionSetupAndX_Rsp.ByteCount = bytecount; 

	// Move offset to Bytes field: NativeOS
	offset += 2 + 1;
		
	// Extract NativeOS
	i = 0;
	while (smb_buf[offset] != 0) {
		smb_SessSetup_Response.SessionSetupAndX_Rsp.NativeOS[i++] = smb_buf[offset];
		offset += 2;
	}
	smb_SessSetup_Response.SessionSetupAndX_Rsp.NativeOS[i] = smb_buf[offset]; // get nul terminater
	offset += 2;

	// Extract NativeLanMan
	i = 0;
	while (smb_buf[offset] != 0) {
		smb_SessSetup_Response.SessionSetupAndX_Rsp.NativeLanMan[i++] = smb_buf[offset];
		offset += 2;
	}
	smb_SessSetup_Response.SessionSetupAndX_Rsp.NativeLanMan[i] = smb_buf[offset]; // get nul terminater
	offset += 2;
		
	// Extract Primary Domain
	i = 0;
	while (smb_buf[offset] != 0) {
		smb_SessSetup_Response.SessionSetupAndX_Rsp.PrimaryDomain[i++] = smb_buf[offset];
		offset += 2;
	}
	smb_SessSetup_Response.SessionSetupAndX_Rsp.PrimaryDomain[i] = smb_buf[offset]; // get nul terminater
	offset += 2; // Move offset to the Next AndX WordCount
	
	
	offset = smb_SessSetup_Response.SessionSetupAndX_Rsp.AndXOffset;

	// copy Tree Connect AndX Response Wordcount 
	smb_SessSetup_Response.TreeConnectAndX_Rsp.WordCount = smb_buf[offset++]; // offset is on AndXCommand
	
	// we ensure that TreeConnect AndX response wordcount > 0
	if (smb_SessSetup_Response.TreeConnectAndX_Rsp.WordCount <= 0) {
		*SessSetupRsp = NULL;
		return -6;
	}
		
	// check that wordcount is 3
	if (smb_SessSetup_Response.SessionSetupAndX_Rsp.WordCount != 3) {
		*SessSetupRsp = NULL;
		return -7;
	}
	
	smb_SessSetup_Response.TreeConnectAndX_Rsp.AndXCommand = smb_buf[offset++]; // offset is on AndXReserved
	smb_SessSetup_Response.TreeConnectAndX_Rsp.AndXReserved = smb_buf[offset++]; // offset is on AndXOffset	
	smb_SessSetup_Response.TreeConnectAndX_Rsp.AndXOffset = smb_GetShort(smb_buf, offset);	
	offset += 2; // offset is on OptionalSupport;		
	smb_SessSetup_Response.TreeConnectAndX_Rsp.OptionalSupport = smb_GetShort(smb_buf, offset);	
	offset += 2; // offset is on ByteCount;		
	
	// get bytecount
	bytecount = smb_GetShort(smb_buf, offset);
	
	// Check that bytecount is not 0
	if (bytecount == 0) {
		*SessSetupRsp = NULL;
		return -8;
	}
	
	// fill in our struct ByteCount
	smb_SessSetup_Response.TreeConnectAndX_Rsp.ByteCount = bytecount; 

	// Move offset to Bytes field: Service
	offset += 2;
	
	for (length = 0; smb_buf[offset + length] != 0; length++) {;} // Get Service length	
	memcpy(smb_SessSetup_Response.TreeConnectAndX_Rsp.Service, &smb_buf[offset], length);
	smb_SessSetup_Response.TreeConnectAndX_Rsp.Service[length] = 0;
	offset += length + 1; 	// Move offset to NativeFileSystem
	
	// Extract NativeFileSystem
	i = 0;
	while (smb_buf[offset] != 0) {
		smb_SessSetup_Response.TreeConnectAndX_Rsp.NativeFileSystem[i++] = smb_buf[offset];
		offset += 2;
	}
	smb_SessSetup_Response.TreeConnectAndX_Rsp.NativeFileSystem[i] = smb_buf[offset]; // get nul terminater
	offset += 2;
					
	// a check to see if all went done
	//if (offset != rawTCP_GetSessionHeader(buf)) {
	//	*SessSetupRsp = NULL;
	//	return -9;
	//}
	
	// Save the User ID and the Tree ID
	*UID = smb_hdrGetUID(smb_buf);
	*TID = smb_hdrGetTID(smb_buf);
	
	*SessSetupRsp = &smb_SessSetup_Response;
	
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
/*int smb_FindFirst2Request(u8 *buf, int bsize, char *search_pattern, u16 UID, u16 TID, int maxent) // Builds a Find First2 Request message
{
	u8 *smb_buf;
	u8 wordcount = 15; // size of our SMB parameters: 15 words
	int i, offset, bytecount_offset, bytes_offset, param_offset, param_count, length;
	u8 flags;
	u16 flags2;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags = SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	flags2 |= SMB_FLAGS2_UNICODE_STRING;
	flags2 |= SMB_FLAGS2_32BIT_STATUS;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_TRANSACTION2);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	smb_hdrSetUID(smb_buf, UID);
	smb_hdrSetTID(smb_buf, TID);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  				// 15 words
	smb_buf[SMB_TRANS2_REQ_OFFSET_RESERVED] = 0;				// Reserved
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_FLAGS, 0); 		// Flags (bit 0 set = Tid disconnect)
	smb_SetLong(smb_buf, SMB_TRANS2_REQ_OFFSET_TIMEOUT, 0); 	// Timeout (0 = return immediately)
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_RESERVED2, 0); 	// Reserved2
	smb_buf[SMB_TRANS2_REQ_OFFSET_RESERVED3] = 0;				// Reserved3
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_SETUP0, TRANS2_FIND_FIRST2); // Setup[0] subcommand
	offset = SMB_TRANS2_REQ_OFFSET_SETUP0 + 2;					// offset is on ByteCount
	bytecount_offset = offset;
	offset += 2; 												// skip ByteCount
	bytes_offset = offset;
	if (bsize < (offset + 15)) 									// room space check
		return -2;										
	smb_buf[offset++] = 0; 										// Name, must be null
	smb_SetShort(smb_buf, offset, 0); 							// Pad
	offset += 2;												// offset is on Parameters Bytes
	param_offset = offset;
	smb_SetShort(smb_buf, offset, ATTR_DIRECTORY | ATTR_SYSTEM | ATTR_HIDDEN); // SearchAttributes
	offset += 2; 												// offset is on SearchCount
	smb_SetShort(smb_buf, offset, maxent); 						// SearchCount
	offset += 2;												// offset is on Flags
	smb_SetShort(smb_buf, offset, CLOSE_SEARCH_IF_EOS | RESUME_SEARCH);	// Flags
	offset += 2; 												// offset is on InformationLevel
	smb_SetShort(smb_buf, offset, SMB_FIND_FILE_BOTH_DIRECTORY_INFO);	// InformationLevel
	offset += 2; 												// offset is on SearchStorageType
	smb_SetLong(smb_buf, offset, 0); 							// SearchStorageType
	offset += 4;												// offset is on Filename	
	for (length = 0; search_pattern[length] != 0; length++) {;}	// Get Search pattern length			
	if (bsize < (offset + (length << 1) + 2)) 					// room space check
		return -2;											
	for(i = 0 ; search_pattern[i] != 0; i++) {
		smb_buf[offset++] = search_pattern[i];					// Filename, search pattern
		smb_buf[offset++] = 0;
	}
	smb_buf[offset++] = 0;
	smb_buf[offset++] = 0;										// null terminater
	
	// We're at end of parameters, there no datas 
	param_count = offset - param_offset;
	bytecount = offset - bytes_offset;
	
	// Fills ByteCount									
	smb_SetShort(smb_buf, bytecount_offset, bytecount);			
		
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_TOTALPARAMCOUNT, param_count); 	// TotalParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_TOTALDATACOUNT, 0); 			// TotalDataCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_MAXPARAMCOUNT, 10);				// MaxParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_MAXDATACOUNT, smb_NegProt_Response.MaxBufferSize); // MaxDataCount
	smb_buf[SMB_TRANS2_REQ_OFFSET_MAXSETUPCOUNT] = 0;							// MaxSetupCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_PARAMCOUNT, param_count); 		// ParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_PARAMOFFSET, param_offset); 	// ParameterOffset	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_DATACOUNT, 0); 					// DataCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_DATAOFFSET, offset); 			// DataCount	
	smb_buf[SMB_TRANS2_REQ_OFFSET_SETUPCOUNT] = 1;								// SetupCount
	
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;		
}

//-------------------------------------------------------------------------
int smb_FindFirst2Response(u8 *buf, int bsize, int *nument, smb_FindFirst2_Entry *info, int *EOS, u16 *Sid) // Analyze Find First2 Response message
{
	u8 *smb_buf;
	int i, j, offset, lastname_offset, entry_start;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0) 
		return -1;
	
	// Check there was a drive in device
	if (smb_hdrGetNTStatus(smb_buf) == STATUS_NO_MEDIA_IN_DEVICE)
		return -1000;

	// Check there was no error
	if (smb_hdrGetNTStatus(smb_buf) != STATUS_SUCCESS)
		return -1000;		
		
	// we ensure that wordcount > 0
	if (smb_buf[SMB_OFFSET_WORDCOUNT] <= 0)
		return -2;		
	
	// Get the ByteCount
	bytecount = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_BYTECOUNT);

	// Get response parameters
	offset = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_PARAMOFFSET);  // offset is on FindFirst2 Response Parameters
	*Sid = smb_GetShort(smb_buf, offset);								// SearchID
	offset += 2; 														// offset is on SearchCount
	*nument = smb_GetShort(smb_buf, offset);							// SearchCount: number of entries found
	offset += 2;
	*EOS = smb_GetShort(smb_buf, offset);								// EndOfSearch
	offset += 2;
	offset += 2;														// skip EaErrorOffset			
	lastname_offset = smb_GetShort(smb_buf, offset);					// LastNameOffset
	offset += 2;		

	// Get response datas
	offset = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_DATAOFFSET);   // offset is on FindFirst2 Response Data
		
	for (i = 0; i < *nument; i++) {
		entry_start = offset;
		memset(info, 0, sizeof(smb_FindFirst2_Entry));
		info->NextEntryOffset = smb_GetLong(smb_buf, offset);			// Get NextEntryOffset
		offset += 4;
		info->FileIndex = smb_GetLong(smb_buf, offset); 				// Get FileIndex
		offset += 4;
		info->Created.timeLow = smb_GetLong(smb_buf, offset);			// Get Created Time Low
		offset += 4;
		info->Created.timeHigh = smb_GetLong(smb_buf, offset); 			// Get Created Time High
		offset += 4;
		info->LastAccess.timeLow = smb_GetLong(smb_buf, offset);		// Get LastAccess Time Low
		offset += 4;
		info->LastAccess.timeHigh = smb_GetLong(smb_buf, offset);		// Get LastAccess Time High
		offset += 4;
		info->LastWrite.timeLow = smb_GetLong(smb_buf, offset);			// Get LastWrite Time Low
		offset += 4;
		info->LastWrite.timeHigh = smb_GetLong(smb_buf, offset);		// Get LastWrite Time High
		offset += 4;
		info->Change.timeLow = smb_GetLong(smb_buf, offset); 			// Get Change Time Low
		offset += 4;
		info->Change.timeHigh = smb_GetLong(smb_buf, offset);			// Get Change Time High
		offset += 4;	
		*((u32 *)&info->EOF) = smb_GetLong(smb_buf, offset);			// Get EndOfFile
		*((u32 *)&info->EOF+1) = smb_GetLong(smb_buf, offset+4);
		offset += 8;
		*((u32 *)&info->AllocationSize) = smb_GetLong(smb_buf, offset); 	// Get AllocationSize
		*((u32 *)&info->AllocationSize+1) = smb_GetLong(smb_buf, offset+4);
		offset += 8;
		info->FileAttributes = smb_GetLong(smb_buf, offset);			// Get FileAttributes
		offset += 4;
		info->FileNameLen = smb_GetLong(smb_buf, offset); 				// Get FileNameLen
		offset += 4;
		info->EaListLength = smb_GetLong(smb_buf, offset);				// Get EaListLength
		offset += 4;
		info->ShortFileNameLen = smb_buf[offset++];						// Get ShortFileNameLen
		info->Reserved = smb_buf[offset++];
		memcpy(info->ShortFileName, &smb_buf[offset], 24);
		offset += 24;
		memset(info->FileName, 0, 24);									// Get Filename
		for (j = 0; ((j < (info->FileNameLen >> 1)) && (j <= 24)); j++) {
			info->FileName[j] = smb_buf[offset];
			offset += 2;
		}
		offset = entry_start += info->NextEntryOffset;
		info++;
	}
	
	offset = SMB_TRANS2_RSP_OFFSET_BYTECOUNT + bytecount + 2;
				
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -3;
		
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_FindNext2Request(u8 *buf, int bsize, u16 UID, u16 TID, u16 SID, int maxent) // Builds a Find Next2 Request message
{
	u8 *smb_buf;
	u8 wordcount = 15; // size of our SMB parameters: 15 words
	int offset, bytecount_offset, bytes_offset, param_offset, param_count;
	u8 flags;
	u16 flags2;
	u16 bytecount;
	
	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags  = SMB_FLAGS_CANONICAL_PATHNAMES;
	//flags |= SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_TRANSACTION2);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	smb_hdrSetUID(smb_buf, UID);
	smb_hdrSetTID(smb_buf, TID);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  				// 15 words
	smb_buf[SMB_TRANS2_REQ_OFFSET_RESERVED] = 0;				// Reserved
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_FLAGS, 0); 		// Flags (bit 0 set = Tid disconnect)
	smb_SetLong(smb_buf, SMB_TRANS2_REQ_OFFSET_TIMEOUT, 0); 	// Timeout (0 = return immediately)
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_RESERVED2, 0); 	// Reserved2
	smb_buf[SMB_TRANS2_REQ_OFFSET_RESERVED3] = 0;				// Reserved3
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_SETUP0, TRANS2_FIND_NEXT2); // Setup[0] subcommand
	offset = SMB_TRANS2_REQ_OFFSET_SETUP0 + 2;					// offset is on ByteCount
	bytecount_offset = offset;
	offset += 2; 												// skip ByteCount
	bytes_offset = offset;
	if (bsize < (offset + 15)) 									// room space check
		return -2;										
	smb_buf[offset++] = 0; 										// Name, must be null
	smb_SetShort(smb_buf, offset, 0); 							// Pad
	offset += 2;												// offset is on Parameters Bytes
	param_offset = offset;
	smb_SetShort(smb_buf, offset, SID); 						// Sid
	offset += 2; 												// offset is on SearchCount
	smb_SetShort(smb_buf, offset, maxent); 						// SearchCount
	offset += 2;												// offset is on InformationLevel
	smb_SetShort(smb_buf, offset, SMB_FIND_FILE_BOTH_DIRECTORY_INFO);	// InformationLevel
	offset += 2; 												// offset is on ResumeKey
	smb_SetLong(smb_buf, offset, 0); 							// ResumeKey
	offset += 4;												// offset is on Flags
	smb_SetShort(smb_buf, offset, CLOSE_SEARCH_IF_EOS | RESUME_SEARCH | CONTINUE_SEARCH);	// Flags
	offset += 2; 												// offset is on FileName
	smb_buf[offset++] = 0;										// Filename, null since we resume
	
	// We're at end of parameters, there no datas 
	param_count = offset - param_offset;
	bytecount = offset - bytes_offset;
	
	// Fills ByteCount									
	smb_SetShort(smb_buf, bytecount_offset, bytecount);			
		
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_TOTALPARAMCOUNT, param_count); 	// TotalParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_TOTALDATACOUNT, 0); 			// TotalDataCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_MAXPARAMCOUNT, 10);				// MaxParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_MAXDATACOUNT, smb_NegProt_Response.MaxBufferSize); // MaxDataCount
	smb_buf[SMB_TRANS2_REQ_OFFSET_MAXSETUPCOUNT] = 0;							// MaxSetupCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_PARAMCOUNT, param_count); 		// ParameterCount	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_PARAMOFFSET, param_offset); 	// ParameterOffset	
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_DATACOUNT, 0); 					// DataCount
	smb_SetShort(smb_buf, SMB_TRANS2_REQ_OFFSET_DATAOFFSET, offset); 			// DataCount	
	smb_buf[SMB_TRANS2_REQ_OFFSET_SETUPCOUNT] = 1;								// SetupCount
	
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;		
}

//-------------------------------------------------------------------------
int smb_FindNext2Response(u8 *buf, int bsize, int *nument, smb_FindFirst2_Entry *info, int *EOS) // Analyze Find Next2 Response message
{
	u8 *smb_buf;
	int i, offset, lastname_offset, entry_start;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0) 
		return -1;
	
	// Check there was a drive in device
	if (smb_hdrGetNTStatus(smb_buf) == STATUS_NO_MEDIA_IN_DEVICE)
		return -1000;

	// Check there was no error
	if (smb_hdrGetNTStatus(smb_buf) != STATUS_SUCCESS)
		return -1000;		
		
	// we ensure that wordcount > 0
	if (smb_buf[SMB_OFFSET_WORDCOUNT] <= 0)
		return -2;		
	
	// Get the ByteCount
	bytecount = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_BYTECOUNT);

	// Get response parameters
	offset = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_PARAMOFFSET);  // offset is on FindNext2 Response Parameters
	*nument = smb_GetShort(smb_buf, offset);							// SearchCount: number of entries found
	offset += 2;
	*EOS = smb_GetShort(smb_buf, offset);								// EndOfSearch
	if (!(*nument))
		*EOS = 1;
	offset += 2;
	offset += 2;														// skip EaErrorOffset			
	lastname_offset = smb_GetShort(smb_buf, offset);					// LastNameOffset
	offset += 2;		

	// Get response datas
	offset = smb_GetShort(smb_buf, SMB_TRANS2_RSP_OFFSET_DATAOFFSET);   // offset is on FindNext2 Response Data
		
	for (i = 0; i < *nument; i++) {
		entry_start = offset;
		memset(info, 0, sizeof(smb_FindFirst2_Entry));
		info->NextEntryOffset = smb_GetLong(smb_buf, offset);			// Get NextEntryOffset
		offset += 4;
		info->FileIndex = smb_GetLong(smb_buf, offset);					// Get FileIndex
		offset += 4;
		info->Created.timeLow = smb_GetLong(smb_buf, offset);			// Get Created Time Low
		offset += 4;
		info->Created.timeHigh = smb_GetLong(smb_buf, offset);			// Get Created Time High
		offset += 4;
		info->LastAccess.timeLow = smb_GetLong(smb_buf, offset);		// Get LastAccess Time Low
		offset += 4;
		info->LastAccess.timeHigh = smb_GetLong(smb_buf, offset);		// Get LastAccess Time High
		offset += 4;
		info->LastWrite.timeLow = smb_GetLong(smb_buf, offset);			// Get LastWrite Time Low
		offset += 4;
		info->LastWrite.timeHigh = smb_GetLong(smb_buf, offset);		// Get LastWrite Time High
		offset += 4;
		info->Change.timeLow = smb_GetLong(smb_buf, offset);			// Get Change Time Low
		offset += 4;
		info->Change.timeHigh = smb_GetLong(smb_buf, offset);			// Get Change Time High
		offset += 4;	
		*((u32 *)&info->EOF) = smb_GetLong(smb_buf, offset); 			// Get EndOfFile
		*((u32 *)&info->EOF+1) = smb_GetLong(smb_buf, offset+4);
		offset += 8;
		*((u32 *)&info->AllocationSize) = smb_GetLong(smb_buf, offset); // Get AllocationSize
		*((u32 *)&info->AllocationSize+1) = smb_GetLong(smb_buf, offset+4);
		offset += 8;
		info->FileAttributes = smb_GetLong(smb_buf, offset);			// Get FileAttributes
		offset += 4;
		info->FileNameLen = smb_GetLong(smb_buf, offset);
		offset += 4;
		info->EaListLength = smb_GetLong(smb_buf, offset);
		offset += 4;
		info->ShortFileNameLen = smb_buf[offset++];
		info->Reserved = smb_buf[offset++];
		memcpy(info->ShortFileName, &smb_buf[offset], 24);
		offset += 24;
		memset(info->FileName, 0, 24);
		memcpy(info->FileName, &smb_buf[offset], (info->FileNameLen < 1024) ? info->FileNameLen : 1023);
		offset += info->FileNameLen;
		offset = entry_start += info->NextEntryOffset;
		info++;
	}
	
	offset = SMB_TRANS2_RSP_OFFSET_BYTECOUNT + bytecount + 2;
		
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -3;
		
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_EchoRequest(u8 *buf, int bsize, u8 *msg, int sz_msg) // Builds an Echo Request message
{
	u8 *smb_buf;
	u8 wordcount = 1; // size of our SMB parameters: 1 words
	int offset, bytecount_offset;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
		
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_ECHO);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  // 1 word
	offset = SMB_OFFSET_WORDCOUNT + 1;		
	smb_SetShort(smb_buf, offset, 1);			// Echo Count
	offset += 2;	
	
	// fill in data block
	// set offset to indicate the start of the bytes field, skipping bytecount
	bytecount_offset = offset;
	offset += 2; 								// skip ByteCount field	
	if (bsize < (offset + sz_msg)) 				// room space check
		return -2;		
	memcpy(&smb_buf[offset], &msg[0], sz_msg);  // EchoMessage	
	offset += sz_msg;	
	bytecount = sz_msg;
	
	// fill bytecount field	
	smb_SetShort(smb_buf, bytecount_offset, bytecount);	
						
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_EchoResponse(u8 *buf, int bsize, u8 *msg, int sz_msg) // analyze Echo Response message
{
	u8 *smb_buf;
	int offset;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0)
		return -1;

	// check there's no error
	if (smb_hdrGetEclassDOS(smb_buf) != DOS_ECLASS_SUCCESS)
		return -1000;
		
	// check that wordcount is 1	
	if (smb_buf[SMB_OFFSET_WORDCOUNT] != 1)
		return -2;
		
	offset = SMB_OFFSET_WORDCOUNT + 1; // offset is on SequenceNumber;
	offset += 2;
	
	// get bytecount
	bytecount = smb_GetShort(smb_buf, offset);
		
	// Check that bytecount is good
	if (bytecount != sz_msg)
		return -3;
	
	// Move offset to Bytes field
	offset += 2;
	
	if (memcmp(&smb_buf[offset], &msg[0], sz_msg))
		return -4;
		
	offset += sz_msg;
	
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -5;
		
	// return total size of the packet	
	return offset + 4;	
}
*/
//-------------------------------------------------------------------------
int smb_NTCreateAndXRequest(u8 *buf, int bsize, u16 UID, u16 TID, char *filename) // Builds a NT Create AndX Request message
{
	u8 *smb_buf;
	u8 wordcount = 24; // size of our SMB parameters: 24 words
	int offset, bytecount_offset, length;
	u8 flags;
	u16 flags2;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags  = SMB_FLAGS_CANONICAL_PATHNAMES;
	//flags |= SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_NT_CREATE_ANDX);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	smb_hdrSetUID(smb_buf, UID);
	smb_hdrSetTID(smb_buf, TID);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  	// 24 words
	offset = SMB_OFFSET_ANDX_OFFSET + 2;	
	smb_buf[offset++] = 0;							// Reserved
	for (length = 0; filename[length] != 0; length++) {;}
	smb_SetShort(smb_buf, offset, length);			// NameLength
	offset += 2;	
	smb_SetLong(smb_buf, offset, 0); 				// Flags, must be 0
	offset += 4;
	smb_SetLong(smb_buf, offset, 0); 				// RootDirectoryFid
	offset += 4;
	smb_SetLong(smb_buf, offset, 0x20089); 			// AccessMask
	offset += 4;
	smb_SetLong(smb_buf, offset, 0); 				// AllocationSize
	offset += 4;
	smb_SetLong(smb_buf, offset, 0); 				// AllocationSizeHigh
	offset += 4;
	smb_SetLong(smb_buf, offset, EXT_ATTR_READONLY);// FileAttributes
	offset += 4;
	smb_SetLong(smb_buf, offset, 1); 				// ShareAccess
	offset += 4;
	smb_SetLong(smb_buf, offset, 1); 				// CreateDisposition
	offset += 4;
	smb_SetLong(smb_buf, offset, 0); 				// CreateOptions
	offset += 4;
	smb_SetLong(smb_buf, offset, 2); 				// ImpersonationLevel 2 = Impersonation
	offset += 4;
	smb_buf[offset++] = 0x03;						// SecurityFlags

	// fill in data block
	// set offset to indicate the start of the bytes field, skipping bytecount
	bytecount_offset = offset;
	offset += 2; 									// skip ByteCount field	
	if (bsize < (offset + length + 1)) 				// room space check
		return -2;		
	memcpy(&smb_buf[offset], filename, length);		// Name
	offset += length;
	smb_buf[offset++] = 0;		
	
	bytecount = offset - (bytecount_offset + 2);
	
	// fill bytecount field	
	smb_SetShort(smb_buf, bytecount_offset, bytecount);	
	
	// fill ANDX block in parameter block
	smb_buf[SMB_OFFSET_ANDX_CMD] = SMB_COM_NONE;	// no ANDX command
	smb_buf[SMB_OFFSET_ANDX_RESERVED] = 0;			// ANDX reserved, must be 0
	smb_SetShort(smb_buf, SMB_OFFSET_ANDX_OFFSET, 0); // ANDX offset
					
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_NTCreateAndXResponse(u8 *buf, int bsize, u16 *FID, u32 *filesize) // analyze NT Create AndX Response message
{
	u8 *smb_buf;
	int offset;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0)
		return -1;

	// check there's no error
	if (smb_hdrGetNTStatus(smb_buf) != STATUS_SUCCESS)
		return -1000;		
	
	// check that wordcount is 34
	if (smb_buf[SMB_OFFSET_WORDCOUNT] != 34) {
		return -2;
	}

	offset = SMB_OFFSET_ANDX_OFFSET; 	
	offset += 2;							// offset is on OpLockLevel;	
	offset++; 								// offset is on FID
	*FID = smb_GetShort(smb_buf, offset);	// FID
	offset += 2; 							// offset is on CreateAction;
	//if (smb_GetLong(smb_buf, offset) != 1)	// check the file existed and opened
	//	return -3;
	offset += 4; 							// offset is on Created;
	offset += 8;							// offset is on LastAccess;
	offset += 8;							// offset is on LastWrite;	
	offset += 8;							// offset is on Change;
	offset += 8;							// offset is on FileAttributes
	//if ((smb_GetLong(smb_buf, offset) & EXT_ATTR_READONLY) == 0) // check file attributes
	//	return -4;
	offset += 4;							// offset is on AllocationSize
	offset += 8; 							// offset is on EndOfFile
	*filesize = smb_GetLong(smb_buf, offset); // Get EndOfFile Low, then skip EndOfFile high	
	offset += 8; 							// offset is on FileType
	offset += 2; 							// offset is on IPCState
	offset += 2;							// offset is on IsDirectory
	if (smb_buf[offset++] != 0)				// offset is on ByteCount
		return -3;		
	if (smb_GetShort(smb_buf, offset) != 0)	// check that bytecount is 0
		return -4;
	offset += 2; 
				
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -5;
		
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_ReadAndXRequest(u8 *buf, int bsize, u16 UID, u16 TID, u16 FID, u32 fileoffset, u16 nbytes) // Builds a Read AndX Request message
{
	u8 *smb_buf;
	u8 wordcount = 12; // size of our SMB parameters: 12 words
	int offset;
	u8 flags;
	u16 flags2;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field, bytecount field then skip bytes field check
	if (bsize < (SMB_HDR_SIZE + (wordcount << 1) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);
	
	// hard coded flags values
	flags  = SMB_FLAGS_CANONICAL_PATHNAMES;
	//flags |= SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
	
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_READ_ANDX);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	smb_hdrSetUID(smb_buf, UID);
	smb_hdrSetTID(smb_buf, TID);
			
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  	// 12 words
	offset = SMB_OFFSET_ANDX_OFFSET + 2;	
	smb_SetShort(smb_buf, offset, FID);				// FID
	offset += 2;	
	smb_SetLong(smb_buf, offset, (u32)fileoffset); 	// Offset Low
	offset += 4;
	smb_SetShort(smb_buf, offset, nbytes);			// MaxCount
	offset += 2;	
	smb_SetShort(smb_buf, offset, 0);				// MinCount (Obsolete)
	offset += 2;	
	smb_SetLong(smb_buf, offset, 0); 				// MaxCountHigh
	offset += 4;
	smb_SetShort(smb_buf, offset, 0);				// Remaining (Obsolete)
	offset += 2;		
	smb_SetLong(smb_buf, offset, 0); 				// Offset High
	offset += 4;
	smb_SetShort(smb_buf, offset, 0);				// ByteCount, must be 0
	offset += 2;		
		
	// fill ANDX block in parameter block
	smb_buf[SMB_OFFSET_ANDX_CMD] = SMB_COM_NONE;	// no ANDX command
	smb_buf[SMB_OFFSET_ANDX_RESERVED] = 0;			// ANDX reserved, must be 0
	smb_SetShort(smb_buf, SMB_OFFSET_ANDX_OFFSET, 0); // ANDX offset
					
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -3;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_ReadAndXResponse(u8 *buf, int bsize, u8 *readbuf, u16 *nbytes) // analyze Read AndX Response message
{
	u8 *smb_buf;
	int i, offset, data_offset;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0)
		return -1;

	// check there's no error
	if (smb_hdrGetEclassDOS(smb_buf) != DOS_ECLASS_SUCCESS)
		return -1000;
	
	// check that wordcount is 12
	if (smb_buf[SMB_OFFSET_WORDCOUNT] != 12)
		return -2;

	offset = SMB_OFFSET_ANDX_OFFSET; 	
	offset += 2;							// offset is on Remaining;	
	offset += 2;							// offset is on DataCompactionMode
	offset += 2;							// offset is on Reserved;
	offset += 2;							// offset is on DataLength;
	*nbytes = smb_GetShort(smb_buf, offset);
	offset += 2;							// offset is on DataOffset;
	data_offset = smb_GetShort(smb_buf, offset);
	offset += 2;							// offset is on DataLengthHigh;
	offset += 2;							// offset is on Reserved2[4 ushort]
	offset += 8;							// offset is on bytecount
	offset += 2;							// Bytes field
	
	if (data_offset > 0) {		
		offset = data_offset;				// offset is on data readed 
				
		for (i = 0; i < *nbytes; i++)		// Get readed datas
			readbuf[i] = smb_buf[offset++];
	}
	
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -4;
		
	// return total size of the packet	
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_CloseRequest(u8 *buf, int bsize, u16 UID, u16 TID, u16 FID) // Builds a Close Request message
{
	u8 *smb_buf;
	u8 wordcount = 3; // size of our SMB parameters: 3 words
	int offset;
	u8 flags;
	u16 flags2;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;

	// make sure we have enough room for the header, wordcount field, words field & bytecount field
	if (bsize < (SMB_HDR_SIZE + (wordcount * 2) + 1))
		return -1;
		
	smb_hdrInit(smb_buf, bsize);

	// hard coded flags values
	flags  = SMB_FLAGS_CANONICAL_PATHNAMES;
	//flags |= SMB_FLAGS_CASELESS_PATHNAMES;
	flags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;	
			
	// fill in the header
	smb_hdrSetCmd(smb_buf, SMB_COM_CLOSE);
	smb_hdrSetFlags(smb_buf, flags);
	smb_hdrSetFlags2(smb_buf, flags2);
	smb_hdrSetUID(smb_buf, UID);
	smb_hdrSetTID(smb_buf, TID);
		
	// fill in parameters block
	smb_buf[SMB_OFFSET_WORDCOUNT] = wordcount;  // 3 words
	offset = SMB_OFFSET_WORDCOUNT + 1;		
	smb_SetShort(smb_buf, offset, FID);			// Fid
	offset += 2;	
	smb_SetLong(smb_buf, offset, 0);			// LastWrite.Low
	offset += 4;		
	smb_SetLong(smb_buf, offset, 0);			// LastWrite.High
	offset += 4;								// offset is on ByteCount	
	
	// fill bytecount field, must be 0	
	smb_SetShort(smb_buf, offset, 0);
	offset += 2;	
						
	// offset is now total size of SMB message
	if (rawTCP_SetSessionHeader(buf, (u32)offset) < offset)
		return -2;
	
	// return total size of the packet 
	return offset + 4;	
}

//-------------------------------------------------------------------------
int smb_CloseResponse(u8 *buf, int bsize) // analyze Close Response message
{
	u8 *smb_buf;
	int offset;
	u16 bytecount;

	// set aside four bytes for the session header 
	bsize -= 4;
	smb_buf = buf + 4;	
		
	// check sanity of SMB header
	if (smb_hdrCheck(smb_buf, bsize) < 0)
		return -1;

	// check there's no error
	if (smb_hdrGetEclassDOS(smb_buf) != DOS_ECLASS_SUCCESS)
		return -1000;
		
	// check that wordcount is 0	
	if (smb_buf[SMB_OFFSET_WORDCOUNT] != 0)
		return -2;
		
	offset = SMB_OFFSET_WORDCOUNT + 1; // offset is on ByteCount
	
	// get bytecount
	bytecount = smb_GetShort(smb_buf, offset);
	offset += 2;
	
	// Check that bytecount is 0
	if (bytecount != 0)
		return -3;
		
	// a check to see if all went done
	if (offset != rawTCP_GetSessionHeader(buf))
		return -4;
		
	// return total size of the packet	
	return offset + 4;	
}

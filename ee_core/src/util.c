/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "iopmgr.h"
#include "util.h"

extern void *cdvdman_irx;
extern int size_cdvdman_irx;

extern void *cdvdfsv_irx;
extern int size_cdvdfsv_irx;


/* Do not link to strcpy() from libc */
inline void _strcpy(char *dst, const char *src)
{
	strncpy(dst, src, strlen(src)+1);
}

/* Do not link to strcat() from libc */
inline void _strcat(char *dst, const char *src)
{
	_strcpy(&dst[strlen(dst)], src);
}

/* Do not link to strncmp() from libc */
int _strncmp(const char *s1, const char *s2, int length)
{
	const char *a = s1;
	const char *b = s2;

	while (length > 0) {
		if ((*a == 0) || (*b == 0))
			return -1;
		if (*a++ != *b++)
			return 1;
		length--;
	}

	return 0;
}

/* Do not link to strcmp() from libc */
int _strcmp(const char *s1, const char *s2)
{
	register int i = 0;

	while ((s1[i] != 0) && (s1[i] == s2[i]))
		i++;

	if (s1[i] > s2[i])
		return 1;
	else if (s1[i] < s2[i])
		return -1;

	return 0;
}

/* Do not link to strchr() from libc */
char *_strchr(const char *string, int c)
{
	while (*string) {
		if (*string == c)
			return (char *)string;
		string++;
	}

	if (*string == c)
		return (char *)string;

	return NULL;
}

/* Do not link to strrchr() from libc */
char *_strrchr(const char * string, int c)
{
	/* use the asm strchr to do strrchr */
	char* lastmatch;
	char* result;

	/* if char is never found then this will return 0 */
	/* if char is found then this will return the last matched location
	   before strchr returned 0 */

	lastmatch = 0;
	result = _strchr(string,c);

	while ((int)result != 0) {
		lastmatch=result;
		result = _strchr(lastmatch+1,c);
	}

	return lastmatch;
}

/* Do not link to strtok() from libc */
char *_strtok(char *strToken, const char *strDelimit)
{
	static char* start;
	static char* end;

	if (strToken != NULL)
		start = strToken;
	else {
		if (*end == 0)
			return 0;

		start=end;
	}

	if(*start == 0)
		return 0;

	// Strip out any leading delimiters
	while (_strchr(strDelimit, *start)) {
		// If a character from the delimiting string
		// then skip past it

		start++;

		if (*start == 0)
			return 0;
	}

	if (*start == 0)
		return 0;

	end=start;

	while (*end != 0) {
		if (_strchr(strDelimit, *end)) {
			// if we find a delimiting character
			// before the end of the string, then
			// terminate the token and move the end
			// pointer to the next character
			*end = 0;
			end++;
			return start;
		}
		end++;
	}

	// reached the end of the string before finding a delimiter
	// so dont move on to the next character
	return start;
}

/* Do not link to strstr() from libc */
char *_strstr(const char *string, const char *substring)
{
	char* strpos;

	if (string == 0)
		return 0;

	if (strlen(substring)==0)
		return (char*)string;

	strpos = (char*)string;

	while (*strpos != 0) {
		if (_strncmp(strpos, substring, strlen(substring)) == 0)
			return strpos;

		strpos++;
	}

	return 0;
}

/* Do not link to islower() from libc */
inline int _islower(int c)
{
	if ((c < 'a') || (c > 'z'))
		return 0;

	// passed both criteria, so it
	// is a lower case alpha char
	return 1;
}

/* Do not link to toupper() from libc */
inline int _toupper(int c)
{
	if (_islower(c))
		c -= 32;

	return c;
}

/* Do not link to memcmp() from libc */
int _memcmp(const void *s1, const void *s2, unsigned int length)
{
	const char *a = s1;
	const char *b = s2;

	while (length--) {
		if (*a++ != *b++)
			return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------*/
/* This function converts string to unsigned integer. Stops on illegal characters.        */
/* Put here because including atoi rises the size of loader.elf by another kilobyte       */
/* and that causes some games to stop working.                                            */
/*----------------------------------------------------------------------------------------*/
unsigned int _strtoui(const char* p)
{
	if (!p)
		return 0;

	int r = 0;

	while (*p)
	{
		if ((*p < '0') || (*p > '9'))
			return r;

		r = r * 10 + (*p++ - '0');
	}

	return r;
}

/*----------------------------------------------------------------------------------------*/
/* This function converts string to signed integer. Stops on illegal characters.        */
/* Put here because including atoi rises the size of loader.elf by another kilobyte       */
/* and that causes some games to stop working.                                            */
/*----------------------------------------------------------------------------------------*/

int _strtoi(const char* p)
{
	int k = 1;
	if (!p)
		return 0;

	int r = 0;

	while (*p)
	{
		if (*p == '-')
		{
 			k = -1;
			p++;
		}
		else if ((*p < '0') || (*p > '9'))
			return r;
		r = r * 10 + (*p++ - '0');
	}

	r = r * k;

	return r;
}

/*----------------------------------------------------------------------------------------*/
/* This function format g_ipconfig with ip, netmask, and gateway                          */
/*----------------------------------------------------------------------------------------*/
void set_ipconfig(void)
{
	const char *SmapLinkModeArgs[4]={
		"0x100",
		"0x080",
		"0x040",
		"0x020"
	};

	memset(g_ipconfig, 0, IPCONFIG_MAX_LEN);
	g_ipconfig_len = 0;

	// add ip to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_ip, 16);
	g_ipconfig_len += strlen(g_ps2_ip) + 1;

	// add netmask to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_netmask, 16);
	g_ipconfig_len += strlen(g_ps2_netmask) + 1;

	// add gateway to g_ipconfig buf
	strncpy(&g_ipconfig[g_ipconfig_len], g_ps2_gateway, 16);
	g_ipconfig_len += strlen(g_ps2_gateway) + 1;

	//Add Ethernet operation mode to g_ipconfig buf
	if(g_ps2_ETHOpMode!=ETH_OP_MODE_AUTO){
		strncpy(&g_ipconfig[g_ipconfig_len], "-no_auto", 9);
		g_ipconfig_len += 9;
		strncpy(&g_ipconfig[g_ipconfig_len], SmapLinkModeArgs[g_ps2_ETHOpMode-1], 6);
		g_ipconfig_len += 6;
	}
}

/*----------------------------------------------------------------------------------------*/
/* This function retrieve a pattern in a buffer, using a mask                             */
/*----------------------------------------------------------------------------------------*/
u32 *find_pattern_with_mask(u32 *buf, u32 bufsize, u32 *pattern, u32 *mask, u32 len)
{
	register u32 i, j;

	len /= sizeof(u32);
	bufsize /= sizeof(u32);

	for (i = 0; i < bufsize - len; i++) {
		for (j = 0; j < len; j++) {
			if ((buf[i + j] & mask[j]) != pattern[j])
				break;
		}
		if (j == len)
			return &buf[i];
	}

	return NULL;
}

/*----------------------------------------------------------------------------------------*/
/* Copy 'size' bytes of 'eedata' from EE to 'iopptr' in IOP.                              */
/*----------------------------------------------------------------------------------------*/
void CopyToIop(void *eedata, unsigned int size, void *iopptr)
{
	SifDmaTransfer_t dmadata;
	register int id;

	dmadata.src = eedata;
	dmadata.dest = iopptr;
	dmadata.size = size;
	dmadata.attr = 0;

	do
	{
		id = SifSetDma(&dmadata, 1);
	} while (!id);

	while (SifDmaStat(id) >= 0) {;} 
}

/*----------------------------------------------------------------------------------------*/
/* Replace a module in a IOPRP image.                                                     */
/*----------------------------------------------------------------------------------------*/
int Patch_Mod(ioprp_t *ioprp_img, const char *name, void *modptr, int modsize)
{
	int       i, offset_in, bytes_to_move, next_mod_offset, diff;
	romdir_t *romdir_in;
	u8 *p = (u8 *)ioprp_img->data_in;

	// arrange modsize to next 16 bytes align
	if (modsize % 0x10)
		modsize = (modsize + 0x10) & 0xfffffff0;

	offset_in  = 0;
	romdir_in  = (romdir_t*)ioprp_img->data_in;

	// scan for module offset & size
	while(strlen(romdir_in->fileName) > 0)
	{ 
		//scr_printf("%s:%d ", romdir_in->fileName, romdir_in->fileSize);
		if (!_strcmp(romdir_in->fileName, name))
			break;

		if ((romdir_in->fileSize % 0x10)==0)
			offset_in += romdir_in->fileSize;
		else
			offset_in += (romdir_in->fileSize + 0x10) & 0xfffffff0;

		romdir_in++;
	}

	// if module found
	if (offset_in) {
		if ((romdir_in->fileSize % 0x10)==0)
			next_mod_offset = offset_in + romdir_in->fileSize;
		else
			next_mod_offset = offset_in + ((romdir_in->fileSize + 0x10) & 0xfffffff0);

		bytes_to_move = ioprp_img->size_in - next_mod_offset;

		// and greater than one in IOPRP img
		if (modsize >= romdir_in->fileSize) {
			diff = modsize - romdir_in->fileSize;
			if (diff > 0) {
				if (bytes_to_move > 0) {
					for (i = ioprp_img->size_in; i >= next_mod_offset; i--)
						p[i + diff] = p[i];		

					ioprp_img->size_in += diff;
				}
			}
		}
		// or smaller than one in IOPRP
		else {
			diff = romdir_in->fileSize - modsize;
			if (diff > 0) {
				if (bytes_to_move > 0) {
					for (i = 0; i < bytes_to_move; i++)
						p[offset_in + modsize + i] = p[next_mod_offset + i];

					ioprp_img->size_in -= diff;
				}
			}	
		}

		// finally replace module
		DIntr();
		ee_kmode_enter();
		memcpy((void *)(ioprp_img->data_in+offset_in), modptr, modsize);
		ee_kmode_exit();
		EIntr();		

		// correct filesize in romdir
		romdir_in->fileSize = modsize;
	}

	ioprp_img->size_out = ioprp_img->size_in;

	return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Build an EELOAD Config file				                                  */
/*----------------------------------------------------------------------------------------*/
int Build_EELOADCNF_Img(ioprp_t *ioprp_img, int have_xloadfile)
{
	int romdir_size, extinfo_size, iopbtconf_size, cdvdman_size, cdvdfsv_size;
	romdir_t *romdir_in;

	romdir_in = (romdir_t*)ioprp_img->data_in;
	memset((void *)romdir_in, 0, 7 * sizeof(romdir_t));

	// Build ROMDIR fs
	_strcpy(romdir_in->fileName, "RESET");
	romdir_in->extinfo_size = 8;
	romdir_in++;

	_strcpy(romdir_in->fileName, "ROMDIR");
	romdir_size = 7 * sizeof(romdir_t);
	romdir_in->fileSize = romdir_size;
	romdir_in++;

	_strcpy(romdir_in->fileName, "EXTINFO");
	extinfo_size = 8 + 8 + 24 + 24;
	romdir_in->fileSize = extinfo_size;
	romdir_in++;

	_strcpy(romdir_in->fileName, "IOPBTCONF");
	romdir_in->extinfo_size = 8;

	// Fill EXTINFO
	u8 *ptr = (u8 *)(ioprp_img->data_in + romdir_size);
	memcpy(&ptr[0], "\0\0\x04\x01\x07\x02\x02\x20", 8);
	memcpy(&ptr[8], "\0\0\x04\x01\x07\x02\x02\x20", 8);
	memcpy(&ptr[8+8], "\0\0\x04\x01\x12\x04\x88\x19\x99\x99\0\x02\0\0\x08\x03""CDVDMAN\0", 24);
	memcpy(&ptr[8+8+24], "\0\0\x04\x01\x12\x04\x88\x19\x99\x99\0\x02\0\0\x08\x03""CDVDFSV\0", 24);
	ptr += extinfo_size;

	// Fill IOPBTCONF
	char *iopbtconf_str0 = "@800\nSYSMEM\nLOADCORE\nEXCEPMAN\nINTRMANP\nINTRMANI\nSSBUSC\nDMACMAN\nTIMEMANP\nTIMEMANI\n";
	char *iopbtconf_str1 = "SYSCLIB\nHEAPLIB\nEECONF\nTHREADMAN\nVBLANK\nIOMAN\nMODLOAD\nROMDRV\nSTDIO\nSIFMAN\n";
	char *iopbtconf_str2 = "IGREETING\nSIFCMD\nREBOOT\nLOADFILE\nCDVDMAN\nCDVDFSV\nSIFINIT\nFILEIO\nSECRMAN\nEESYNC\n";
	if (have_xloadfile)
		iopbtconf_str2 = "IGREETING\nSIFCMD\nREBOOT\nXLOADFILE\nCDVDMAN\nCDVDFSV\nSIFINIT\nFILEIO\nSECRMAN\nEESYNC\n";

	memcpy(&ptr[0], iopbtconf_str0, strlen(iopbtconf_str0));
	memcpy(&ptr[strlen(iopbtconf_str0)], iopbtconf_str1, strlen(iopbtconf_str1));
	memcpy(&ptr[strlen(iopbtconf_str0)+strlen(iopbtconf_str1)], iopbtconf_str2, strlen(iopbtconf_str2));

	// Fill IOPBTCONF filesize
	iopbtconf_size = strlen(iopbtconf_str0)+strlen(iopbtconf_str1)+strlen(iopbtconf_str2);
	romdir_in->fileSize = iopbtconf_size;
	// arrange size to next 16 bytes multiple 
	if (iopbtconf_size % 0x10)
		iopbtconf_size = (iopbtconf_size + 0x10) & 0xfffffff0;
	romdir_in++;
	ptr += iopbtconf_size;

	// Fill CDVDMAN
	_strcpy(romdir_in->fileName, "CDVDMAN");
	romdir_in->extinfo_size = 24;
	cdvdman_size = size_cdvdman_irx;
	romdir_in->fileSize = cdvdman_size;
	// arrange size to next 16 bytes multiple
	if (cdvdman_size % 0x10)
		cdvdman_size = (cdvdman_size + 0x10) & 0xfffffff0;
	DIntr();
	ee_kmode_enter();
	memcpy(&ptr[0], cdvdman_irx, size_cdvdman_irx);
	ee_kmode_exit();
	EIntr();
	romdir_in++;
	ptr += cdvdman_size;

	// Fill CDVDFSV
	_strcpy(romdir_in->fileName, "CDVDFSV");
	romdir_in->extinfo_size = 24;
	cdvdfsv_size = size_cdvdfsv_irx;
	romdir_in->fileSize = cdvdfsv_size;
	// arrange size to next 16 bytes multiple
	if (cdvdfsv_size % 0x10)
		cdvdfsv_size = (cdvdfsv_size + 0x10) & 0xfffffff0;
	DIntr();
	ee_kmode_enter();
	memcpy(&ptr[0], cdvdfsv_irx, size_cdvdfsv_irx);
	ee_kmode_exit();
	EIntr();
	romdir_in++;
	ptr += cdvdfsv_size;

	// Fill mini image size
	ioprp_img->data_out = ioprp_img->data_in;
	ioprp_img->size_out = ioprp_img->size_in = romdir_size + extinfo_size + iopbtconf_size + cdvdman_size + cdvdfsv_size;

	return 0;
}

/*----------------------------------------------------------------------------------------*/
/* XLoadfileCheck                                                                          */
/*----------------------------------------------------------------------------------------*/
inline int XLoadfileCheck(void)
{
	register int fd;

	fd = open("rom0:XLOADFILE", O_RDONLY);
	if (fd >= 0) {
		close(fd);
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------*/
/* NOP delay.                                                                             */
/*----------------------------------------------------------------------------------------*/
inline void delay(int count)
{
	int i, ret;

	for (i  = 0; i < count; i++) {
		ret = 0x01000000;
		while(ret--) asm("nop\nnop\nnop\nnop");
	}
}

/*----------------------------------------------------------------------------------------*/
/* Apply SBV patchs.                                                                      */
/*----------------------------------------------------------------------------------------*/
inline void Sbv_Patch(void)
{
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
}

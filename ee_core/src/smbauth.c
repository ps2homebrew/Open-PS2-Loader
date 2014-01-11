/*
  Copyright 2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "ee_core.h"
#include "des.h"
#include "md4.h"
#include "util.h"

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <sifcmd.h>
#include <sifdma.h>
#include <string.h>

#define DMA_ADDR 		0x000cff00

extern void *_gp;

/* EE DMAC registers.  */
#define DMAC_COMM_STAT	0x1000e010
#define DMAC_SIF0_CHCR	0x1000c000
#define CHCR_STR	0x100
#define STAT_SIF0	0x20

static u8 thread_stack[0x800] __attribute__((aligned(16)));
static unsigned int *sifDmaDataPtr = (unsigned int *)UNCACHED_SEG(DMA_ADDR);
static int smbauth_thread_id;
static int sif0_id = -1;

static u8 passwd_buf[512] __attribute__((aligned(64)));

typedef struct {			// size = 156
	u32	MaxBufferSize;
	u32	MaxMpxCount;
	u32	SessionKey;
	u32	StringsCF;
	u8	PrimaryDomainServerName[32];
	u8	EncryptionKey[8];
	int	SecurityMode;		// 0 = share level, 1 = user level
	int	PasswordType;		// 0 = PlainText passwords, 1 = use challenge/response
	char	Username[36];
	u8	Password[48];		// either PlainText, either hashed
	int	PasswordLen;
	int	HashedFlag;
	void	*IOPaddr;
} server_specs_t;

static server_specs_t *server_specs = (void *)UNCACHED_SEG((DMA_ADDR + 0x40));

#define SERVER_USE_PLAINTEXT_PASSWORD	0
#define SERVER_USE_ENCRYPTED_PASSWORD	1


/*
 * LM_Password_Hash: this function create a LM password hash from a given password
 */
static unsigned char *LM_Password_Hash(const unsigned char *password, unsigned char *cipher)
{
	unsigned char tmp_pass[14] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
	unsigned char K1[7];
	unsigned char K2[7];
	int i;

	/* keep only 14 bytes of the password (padded with nul bytes) */
	strncpy(tmp_pass, password, 14);

	/* turn the password to uppercase */
	for (i=0; i<14; i++) {
		tmp_pass[i] = _toupper(tmp_pass[i]);
	}

	/* get 2 7bytes keys from password */
	memcpy(K1, &tmp_pass[0], 7);
	memcpy(K2, &tmp_pass[7], 7);

	/* encrypt the magic string with the keys */
	DES(K1, "KGS!@#$%", &cipher[0]);
	DES(K2, "KGS!@#$%", &cipher[8]);

	return (unsigned char *)cipher;
}

/*
 * NTLM_Password_Hash: this function create a NTLM password hash from a given password
 */
static unsigned char *NTLM_Password_Hash(const unsigned char *password, unsigned char *cipher)
{
	int i, j;

	memset(passwd_buf, 0, sizeof(passwd_buf));

	/* turn the password to unicode */
	for (i=0, j=0; i<strlen(password); i++, j+=2)
		passwd_buf[j] = password[i];

	/* get the message digest */
	MD4(passwd_buf, j, cipher);

	return (unsigned char *)cipher;
}

/*
 * LM_Response: this function create a LM response from a given LM hash & challenge
 */
static unsigned char *LM_Response(const unsigned char *LMpasswordhash, unsigned char *chal, unsigned char *cipher)
{
	unsigned char P21[21];
	unsigned char K[7];
	int i;
 
	/* padd the LM password hash with 5 nul bytes */
	memcpy(&P21[0], LMpasswordhash, 16);
	memset(&P21[16], 0, 5);

	/* encrypt the challenge 3 times, using 7 bytes splitted keys */
	for (i=0; i<3; i++) {

		/* get the 7bytes key */
		memcpy(K, &P21[i*7], 7);		

		/* encrypt each the challenge with the keys */
		DES(K, chal, &cipher[i*8]);
	}

	return (unsigned char *)cipher;
}

/*
 * GenerateLMHashes: function used to generate LM/NTLM hashes
 */
static void GenerateLMHashes(char *Password, int PasswordType, u8 *EncryptionKey, int *PasswordLen, u8 *Buffer)
{
	u8 LMpasswordhash[16];
	u8 NTLMpasswordhash[16];
	u8 LMresponse[24];
	u8 NTLMresponse[24];

	if (PasswordType == SERVER_USE_ENCRYPTED_PASSWORD) {
		/* Generate both LM & NTLM hashes */
		LM_Password_Hash(Password, LMpasswordhash);
		NTLM_Password_Hash(Password, NTLMpasswordhash);
		*PasswordLen = 24;
		LM_Response(LMpasswordhash, EncryptionKey, LMresponse);
		LM_Response(NTLMpasswordhash, EncryptionKey, NTLMresponse);
		memcpy(&Buffer[0], LMresponse, *PasswordLen);
		memcpy(&Buffer[24], NTLMresponse, *PasswordLen);
	}
	else if (PasswordType == SERVER_USE_PLAINTEXT_PASSWORD) {
		/* It seems that PlainText passwords and Unicode isn't meant to be... */
		*PasswordLen = strlen(Password);
		if (*PasswordLen > 14)
			*PasswordLen = 14;
		else if (*PasswordLen == 0)
			*PasswordLen = 1;
		memcpy(Buffer, Password, *PasswordLen);
	}
}

/*
 * _smbauth_thread: main SMB authentification thread
 */
static int _smbauth_thread(void *args)
{
	SifDmaTransfer_t dmat;
	int id;

	/* the thread sleep down until it's awaken up by our DMA interrupt handler */
	SleepThread();

	DPRINTF("smbauth thread woken up !!!\n");

	server_specs_t *ss = (server_specs_t *)server_specs;
	if (ss->HashedFlag == 0) {
		/* generate the LM and NTLM hash then fill Password buffer with both hashes */
		if (strlen(ss->Password) > 0)
			GenerateLMHashes(ss->Password, ss->PasswordType, ss->EncryptionKey, &ss->PasswordLen, ss->Password);
	}

	/* used to notify cdvdman that password are now hashed */
	ss->HashedFlag = 1;

	/* we send back server_specs_t to IOP */
	dmat.dest = (void *)ss->IOPaddr;
	dmat.size = sizeof(server_specs_t);
	dmat.src = (void *)server_specs;
	dmat.attr = SIF_DMA_INT_O;

	id = 0;
	while (!id)
		id = SifSetDma(&dmat, 1);
	while (SifDmaStat(id) >= 0);

	/* remove our DMA interupt handler */
	RemoveDmacHandler(5, sif0_id);

	/* terminate SMB auth thread */
	ExitDeleteThread();
	return 0;
}

/*
 * _SifDmaIntrHandler: our DMA interrupt handler
 */
int _SifDmaIntrHandler()
{
	int flag;

	flag = *sifDmaDataPtr;
	iSifSetDChain();

	if (flag) {
		*sifDmaDataPtr = 0;
		/* Wake Up our SMB auth thread */
		iWakeupThread(smbauth_thread_id);
	}

	/* exit handler */
	__asm__ __volatile__(
		" sync;"
		" ei;"
	);

	return 0;
}

/*
 * start_smbauth_thread: used to start the EE SMB auth thread
 */
void start_smbauth_thread(void)
{
	int ret;
	ee_thread_t thread;

	thread.func = _smbauth_thread;
	thread.stack = thread_stack;
	thread.stack_size = sizeof(thread_stack);
	thread.gp_reg = &_gp;
	thread.initial_priority = 1;

	/* let's create the thread */
	smbauth_thread_id = CreateThread(&thread);
	if (smbauth_thread_id < 0) {
		DPRINTF("EE Core: failed to create smbauth thread...\n");
		GS_BGCOLOUR = 0x008080;
		SleepThread();
	}

	/* start it */
	ret = StartThread(smbauth_thread_id, NULL);
	if (ret < 0) {
		DPRINTF("EE Core: failed to start smbauth thread...\n");
		GS_BGCOLOUR = 0x008080;
		SleepThread();
	}

	/* install our DMA interrupt handler */
	FlushCache(0);
	*sifDmaDataPtr = 0;

	if (_lw(DMAC_COMM_STAT)  & STAT_SIF0)
		_sw(STAT_SIF0, DMAC_COMM_STAT);

	if (!(_lw(DMAC_SIF0_CHCR) & CHCR_STR))
		SifSetDChain();

	sif0_id = AddDmacHandler(5, _SifDmaIntrHandler, 0);
	EnableDmac(5);
}


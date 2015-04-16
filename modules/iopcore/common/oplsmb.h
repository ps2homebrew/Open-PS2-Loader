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

typedef void (*OplSmbPwHashFunc_t)(server_specs_t *ss);

#define oplutils_IMPORTS_start DECLARE_IMPORT_TABLE( oplutils, 1, 1 )
#define oplutils_IMPORTS_end END_IMPORT_TABLE

int smb_OpenAndX(char *filename, u16 *FID, int Write);
#define I_smb_OpenAndX DECLARE_IMPORT(5, smb_OpenAndX)

int smb_ReadFile(u16 FID, u32 offsetlow, u32 offsethigh, void *readbuf, u16 nbytes);
#define I_smb_ReadFile DECLARE_IMPORT(6, smb_ReadFile)

int smb_WriteFile(u16 FID, u32 offsetlow, u32 offsethigh, void *writebuf, u16 nbytes);
#define I_smb_WriteFile DECLARE_IMPORT(7, smb_WriteFile)

void smb_NegotiateProt(OplSmbPwHashFunc_t hash_callback);
#define I_smb_NegotiateProt DECLARE_IMPORT(8, smb_NegotiateProt)

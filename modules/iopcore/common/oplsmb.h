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

#define oplsmb_IMPORTS_start DECLARE_IMPORT_TABLE( oplsmb, 1, 1 )
#define oplsmb_IMPORTS_end END_IMPORT_TABLE

void smb_NegotiateProt(OplSmbPwHashFunc_t hash_callback);
#define I_smb_NegotiateProt DECLARE_IMPORT(4, smb_NegotiateProt)

// RPC IDs
enum NBNS_RPC_ID {
    NBNS_RPC_ID_FIND_NAME = 0,

    NBNS_RPC_ID_COUNT
};

// RPC structures
struct nbnsFindNameResult
{
    int result;
    u8 address[4];
};

// Function prototypes
int nbnsInit(void);
void nbnsDeinit(void);
int nbnsFindName(const char *name, unsigned char *ip_address);

#ifdef _IOP
#define nbnsman_IMPORTS_start DECLARE_IMPORT_TABLE(nbnsman, 1, 1)
#define nbnsman_IMPORTS_end   END_IMPORT_TABLE

#define I_nbnsFindName DECLARE_IMPORT(4, nbnsFindName)
#endif

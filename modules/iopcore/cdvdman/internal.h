struct cdvdman_StreamingData{
	unsigned int lsn;
	unsigned int bufmax;
	int stat;
};

typedef struct {
	int err;
	int status;
	struct cdvdman_StreamingData StreamingData;
	int intr_ef;
	int disc_type_reg;
	u32 cdread_lba;
	u32 cdread_sectors;
	void *cdread_buf;
} cdvdman_status_t;

typedef void (*StmCallback_t)(void);

//Internal (common) function prototypes
void SetStm0Callback(StmCallback_t callback);

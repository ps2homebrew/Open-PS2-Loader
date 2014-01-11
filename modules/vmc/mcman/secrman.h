#ifndef IOP_SECRMAN_H
#define IOP_SECRMAN_H

#include "types.h"
#include "irx.h"

// Encrypted file Data Block info struct
typedef struct t_Block_Data {
	u32 size; 		// Size of data block 
	u32 flags; 		// Flags : 3 -> block to decrypt, 0 -> block to take in input file, 2 -> block to prepare
	u8  checksum[8];
} Block_Data_t;

// Encrypted file BIT table struct 
typedef struct t_Bit_Data {
	u32 headersize; 	// Kelf header size (same as Kelf_Header_t.Kelf_header_size)
	u8 block_count; 	// Number of blocks in the Kelf file 
	u8 pad1;
	u8 pad2;
	u8 pad3;
	Block_Data_t blocks[63];// Kelf blocks infos
} Bit_Data_t;

// Encrypted file header struct 
typedef struct t_Kelf_Header {
	u32 unknown1;             
	u32 unknown2;
	u16 unknown3_half;
	u16 version;
	u32 unknown4;
	u32 ELF_size; 		// Size of data blocks = Decrypted elf size
	u16 Kelf_header_size;	// Kelf header size
	u16 unknown5;
	u16 flags; 		// ? for header purpose
	u16 BIT_count; 		// used to set/get kbit and kc offset
	u32 mg_zones;
} Kelf_Header_t;

#define secrman_IMPORTS_start DECLARE_IMPORT_TABLE(secrman, 1, 3)
#define secrman_IMPORTS_end END_IMPORT_TABLE

/* 04 */ void SetMcCommandCallback(void *addr);
#define I_SetMcCommandCallback DECLARE_IMPORT(4, SetMcCommandCallback)

/* 05 */ void SetMcDevIDCallback(void *addr);
#define I_SetMcDevIDCallback DECLARE_IMPORT(5, SetMcDevIDCallback)

/* 06 */ int SecrAuthCard(int port, int slot, int cnum);
#define I_SecrAuthCard DECLARE_IMPORT(6, SecrAuthCard)

/* 07 */ void SecrResetAuthCard(int port, int slot, int cnum);
#define I_SecrResetAuthCard DECLARE_IMPORT(7, SecrResetAuthCard)

/* 08 */ int SecrCardBootHeader(int port, int slot, void *buf, void *bit, int *psize);
#define I_SecrCardBootHeader DECLARE_IMPORT(8, SecrCardBootHeader)

/* 09 */ int SecrCardBootBlock(void *src, void *dst, int size);
#define I_SecrCardBootBlock DECLARE_IMPORT(9, SecrCardBootBlock)

/* 10 */ void *SecrCardBootFile(int port, int slot, void *buf);
#define I_SecrCardBootFile DECLARE_IMPORT(10, SecrCardBootFile)

/* 11 */ int SecrDiskBootHeader(void *buf, void *bit, int *psize);
#define I_SecrDiskBootHeader DECLARE_IMPORT(11, SecrDiskBootHeader)

/* 12 */ int SecrDiskBootBlock(void *src, void *dst, int size);
#define I_SecrDiskBootBlock DECLARE_IMPORT(12, SecrDiskBootBlock)

/* 13 */ void *SecrDiskBootFile(void *buf);
#define I_SecrDiskBootFile DECLARE_IMPORT(13, SecrDiskBootFile)

/* FOLLOWING EXPORTS ARE ONLY AVAILABLE IN SPECIAL SECRMAN OR FREESECR */ 

/* 14 */ int SecrDownloadHeader(int port, int slot, void *buf, void *bit, int *psize);
#define I_SecrDownloadHeader DECLARE_IMPORT(14, SecrDownloadHeader)

/* 15 */ int SecrDownloadBlock(void *src, int size);
#define I_SecrDownloadBlock DECLARE_IMPORT(15, SecrDownloadBlock)

/* 16 */ void *SecrDownloadFile(int port, int slot, void *buf);
#define I_SecrDownloadFile DECLARE_IMPORT(16, SecrDownloadFile)

/* 17 */ int SecrDownloadGetKbit(int port, int slot, void *kbit);
#define I_SecrDownloadGetKbit DECLARE_IMPORT(17, SecrDownloadGetKbit)

/* 18 */ int SecrDownloadGetKc(int port, int slot, void *kbit);
#define I_SecrDownloadGetKc DECLARE_IMPORT(18, SecrDownloadGetKc)

/* 19 */ int SecrDownloadGetICVPS2(void *icvps2);
#define I_SecrDownloadGetICVPS2 DECLARE_IMPORT(19, SecrDownloadGetICVPS2)

#endif /* IOP_SECRMAN_H */

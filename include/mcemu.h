#ifndef MCEMU_H
#define MCEMU_H

typedef struct {
	u8  magic[ 40 ];
	u16 page_size;
	u16 pages_per_cluster;
	u16 pages_per_block;
	u16 unused0;		             //  always 0xFF00
	u32 clusters_per_card;
	u32 first_allocatable;
	u32 last_allocatable;
	u32 root_cluster;	           //  must be 0
	u32 backup_block1;	         //  1023
	u32 backup_block2;	         //  1024
	u8  unused1[ 8 ];		         //  unused  /  who knows what it is
	u32 indir_fat_clusters[ 32 ];
	u32 bad_block_list[ 32 ];
	u8  mc_type;
	u8  mc_flag;
	u16 unused2;                 //  zero
	u32 cluster_size;            //  1024 always, 0x400
	u32 unused3;                 //  0x100
	u32 size_in_megs;            //  size in megabytes
	u32 unused4;                 //  0xffffffff
	u8  unused5[ 12 ];           //  zero
	u32 max_used;                //  97%of total clusters
	u8  unused6[ 8 ];            //  zero
	u32 unused7;                 //  0xffffffff
	u8  unused8[128];
} vmc_superblock_t;

typedef struct {
	u16 page_size;  /* Page size in bytes (user data only) */
	u16 block_size; /* Block size in pages */
	u32 card_size;  /* Total number of pages */
} vmc_spec_t;

#endif

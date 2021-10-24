/* coff.h
 *   Data structures that describe the MIPS COFF format.
 */

struct coff_filehdr
{
    u16 f_magic;  /* magic number */
    u16 f_nscns;  /* number of sections */
    u32 f_timdat; /* time & date stamp */
    u32 f_symptr; /* file pointer to symbolic header */
    u32 f_nsyms;  /* sizeof(symbolic hdr) */
    u16 f_opthdr; /* sizeof(optional hdr) */
    u16 f_flags;  /* flags */
};

#define MIPSELMAGIC 0x0162

#define OMAGIC  0407
#define SOMAGIC 0x0701

typedef struct aouthdr
{
    u16 magic;      /* see above */
    u16 vstamp;     /* version stamp */
    u32 tsize;      /* text size in bytes, padded to DW bdry */
    u32 dsize;      /* initialized data "  " */
    u32 bsize;      /* uninitialized data "   " */
    u32 entry;      /* entry pt. */
    u32 text_start; /* base of text used for this file */
    u32 data_start; /* base of data used for this file */
    u32 bss_start;  /* base of bss used for this file */
    // Instead of the GPR and CPR masks, these 5 fields exist.
    u32 field_20;
    u32 field_24;
    u32 field_28;
    u32 field_2C;
    struct iopmod_id *mod_id;
    u32 gp_value; /* the gp value used for this object */
} AOUTHDR;

struct scnhdr
{
    u8 s_name[8];  /* section name */
    u32 s_paddr;   /* physical address, aliased s_nlib */
    u32 s_vaddr;   /* virtual address */
    u32 s_size;    /* section size */
    u32 s_scnptr;  /* file ptr to raw data for section */
    u32 s_relptr;  /* file ptr to relocation */
    u32 s_lnnoptr; /* file ptr to gp histogram */
    u16 s_nreloc;  /* number of relocation entries */
    u16 s_nlnno;   /* number of gp histogram entries */
    u32 s_flags;   /* flags */
};

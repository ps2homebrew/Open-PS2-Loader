/* ELF-loading stuff */
#define ELF_MAGIC        0x464c457f
#define ELF_TYPE_SCE_IRX 0xFF80 /* SCE IOP Relocatable eXcutable file */
#define ELF_PT_LOAD      1

/*------------------------------*/
typedef struct
{
    u8 ident[16]; /* Structure of a ELF header */
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;
/*------------------------------*/
typedef struct
{
    u32 type; /* Structure of a header a sections in an ELF */
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;

typedef struct
{
    u32 name;
    u32 type;
    u32 flags;
    u32 addr;
    u32 offset;
    u32 size;
    u32 link;
    u32 info;
    u32 addralign;
    u32 entsize;
} elf_shdr_t;

typedef struct
{
    u32 offset;
    u32 info;
} elf_rel;

typedef struct
{
    u32 offset;
    u32 info;
    u32 addend;
} elf_rela;

enum ELF_SHT_types {
    SHT_NULL = 0,
    SHT_PROGBITS,
    SHT_SYMTAB,
    SHT_STRTAB,
    SHT_RELA,
    SHT_HASH,
    SHT_DYNAMIC,
    SHT_NOTE,
    SHT_NOBITS,
    SHT_REL,
    SHT_SHLIB,
    SHT_DYNSYM
};

enum ELF_reloc_types {
    R_MIPS_NONE = 0,
    R_MIPS_16,
    R_MIPS_32,
    R_MIPS_REL32,
    R_MIPS_26,
    R_MIPS_HI16,
    R_MIPS_LO16
};

#define SHT_LOPROC               0x70000000
#define SHT_LOPROC_EE_IMPORT_TAB 0x90
#define SHT_LOPROC_IOPMOD        0x80
#define SHT_HIPROC               0x7fffffff
#define SHT_LOUSER               0x80000000
#define SHT_HIUSER               0xffffffff

#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4
#define SHF_MASKPROC  0xf0000000

struct iopmod_id
{
    const char *name;
    unsigned short int version;
};

struct iopmod
{
    struct iopmod_id *mod_id;   // 0x00
    void *EntryPoint;           // 0x04
    void *gp;                   // 0x08
    unsigned int text_size;     // 0x0C
    unsigned int data_size;     // 0x10
    unsigned int bss_size;      // 0x14
    unsigned short int version; // 0x18
    char modname[1];            // 0x1A
};

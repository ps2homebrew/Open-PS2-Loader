#ifndef __SYS_ARCH_H__
#define __SYS_ARCH_H__

typedef int sys_prot_t;
typedef int sys_sem_t;
typedef struct sys_mbox *sys_mbox_t;
typedef int sys_thread_t;

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  0

#if MEM_LIBC_MALLOC
void *ps2ip_mem_malloc(int size);
void ps2ip_mem_free(void *rmem);
void *ps2ip_mem_realloc(void *rmem, int newsize);

#define mem_clib_free    ps2ip_mem_free
#define mem_clib_malloc  ps2ip_mem_malloc
#define mem_clib_realloc ps2ip_mem_realloc
#endif

#endif /* __SYS_ARCH_H__ */

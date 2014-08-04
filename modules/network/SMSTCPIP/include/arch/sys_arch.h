#ifndef __SYS_ARCH_H__
# define __SYS_ARCH_H__

typedef int              sys_prot_t;
typedef int              sys_sem_t;
typedef struct sys_mbox* sys_mbox_t;
typedef int              sys_thread_t;

# define SYS_MBOX_NULL      NULL
# define SYS_SEM_NULL       0
#endif /* __SYS_ARCH_H__ */

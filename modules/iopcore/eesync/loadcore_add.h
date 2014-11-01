#ifndef _LOADCORE_ADD_H_
#define _LOADCORE_ADD_H_

typedef int (*Callback)(int* arg1,int arg2);

int loadcore20(Callback func,int priority,int* pResult);
#define I_loadcore20 DECLARE_IMPORT(20, loadcore20);

#endif

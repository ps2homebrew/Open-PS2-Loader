/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open-Ps2-Loader README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#ifndef UTIL_H
#define UTIL_H

#include <tamtypes.h>

typedef struct ioprp {
	void        *data_in;
	void        *data_out;
	unsigned int size_in;
	unsigned int size_out;
} ioprp_t;

inline void _strcpy(char *dst, const char *src);
inline void _strcat(char *dst, const char *src);
int _strncmp(const char *s1, const char *s2, int length);
int _strcmp(const char *s1, const char *s2);
char *_strchr(const char *string, int c);
char *_strrchr(const char * string, int c);
char *_strtok(char *strToken, const char *strDelimit);
char *_strstr(const char *string, const char *substring);
inline int _islower(int c);
inline int _toupper(int c);
int _memcmp(const void *s1, const void *s2, unsigned int length);
unsigned int _strtoui(const char* p);
void set_ipconfig(void);
u32 *find_pattern_with_mask(u32 *buf, u32 bufsize, u32 *pattern, u32 *mask, u32 len);
void CopyToIop(void *eedata, unsigned int size, void *iopptr);
int Patch_Mod(ioprp_t *ioprp_img, const char *name, void *modptr, int modsize);
int Build_EELOADCNF_Img(ioprp_t *ioprp_img, int have_xloadfile);
inline int XLoadfileCheck(void);
inline void delay(int count);
inline void Sbv_Patch(void);

#endif /* UTIL */

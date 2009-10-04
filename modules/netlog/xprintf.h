/*
 * xprinf.h
 *
 * Copyright (C) 2007-2009 misfire <misfire@xploderfreax.de>
 *
 * Licensed under the Academic Free License version 2.0.  See file LICENSE.
 */

#ifndef _XPRINTF_H_
#define _XPRINTF_H_

#include <stdarg.h>

/* Define available functions here */
#define F_vxprintf
#define NOFLOATINGPOINT /* won't compile without it */
#define F___sout

#define F_sprintf
#define F_snprintf
#define F_vsprintf
#define F_vsnprintf

#ifdef F_sprintf
int sprintf(char *str, const char *format, ...);
#endif

#ifdef F_snprintf
int snprintf(char *str, size_t sz, const char *format, ...);
#endif

#ifdef F_vsprintf
int vsprintf(char *buf, const char *fmt, va_list ap);
#endif

#ifdef F_vsnprintf
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
#endif

#endif /* _XPRINTF_H_ */

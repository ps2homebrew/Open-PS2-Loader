#ifndef _MASS_DEBUG_H
#define _MASS_DEBUG_H 1

#ifdef DEBUG
#define XPRINTF printf
#else
#define XPRINTF while(0) printf
#endif

#endif  /* _MASS_DEBUG_H */

/*
 * $Id$ 
 */

#ifndef SPRINTF_IRC
#define SPRINTF_IRC

#include <stdarg.h>

/*=============================================================================
 * Proto types
 */

extern int vsprintf_irc(char *str, const char *format, va_list);
extern int vsnprintf_irc(char *, int, const char*, va_list);
/* old */
/* extern int ircsprintf(char *str, char *format, ...); */
/* */

/*
 * ircsprintf - optimized sprintf
 */
#ifdef __GNUC__
extern int ircsprintf(char*, const char*, ...)
               __attribute__ ((format(printf, 2, 3)));
extern int ircsnprintf(char*, int, const char*, ...)
                __attribute__ ((format(printf, 3, 4)));
#else
extern int ircsprintf(char *str, const char *format, ...);
extern int ircsnprintf(char*, int, const char*);
#endif

#endif /* SPRINTF_IRC */

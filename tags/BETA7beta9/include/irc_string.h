/************************************************************************
 *   IRC - Internet Relay Chat, src/irc_string.h
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id$
 */
#ifndef INCLUDED_irc_string_h
#define INCLUDED_irc_string_h
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif
#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"        /* buffer sizes */
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdarg.h>
/*
 * match - compare name with mask, mask may contain * and ? as wildcards
 * match - returns 1 on successful match, 0 otherwise
 */
extern int match(const char *mask, const char *name);
/*
 * collapse - collapse a string in place, converts multiple adjacent *'s 
 * into a single *.
 * collapse - modifies the contents of pattern 
 */
extern char* collapse(char *pattern);
/*
 * irccmp - case insensitive comparison of s1 and s2
 */
extern int irccmp(const char *s1, const char *s2);
/*
 * ircncmp - counted case insensitive comparison of s1 and s2
 */
extern int ircncmp(const char *s1, const char *s2, int n);
/*
** canonize - reduce a string of duplicate list entries to contain
** only the unique items.
*/  
#ifdef NO_DUPE_MULTI_MESSAGES
extern char* canonize(char *);
#endif
/*
 * inetntoa - optimized inet_ntoa
 */
const char* inetntoa(const char* in_addr);

/* 
 * inetntop() 
 * inetpton()
 * portable interfaces for inet_ntop() and inet_pton()
 */
const char *inetntop(int af, const void *src, char *dst, unsigned int size);
int inetpton(int af, const char *src, void *dst);
                                
/*
 * strncpy_irc - optimized strncpy
 */
char* strncpy_irc(char* s1, const char* s2, size_t n);


#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_SNPRINTF
int snprintf (char *str,size_t count,const char *fmt,...);
#endif

#ifndef HAVE_VSNPRINTF
int vsnprintf (char *str, size_t count, const char *fmt, va_list args);
#endif

/*
 * clean_string - cleanup control and high ascii characters
 * -Dianora
 */
char* clean_string(char* dest, const unsigned char* src, size_t len);
/*
 * strip_tabs - convert tabs to spaces
 * - jdc
 */
char *strip_tabs(char *dest, const unsigned char *src, size_t len);

const char* myctime(time_t);

#define EmptyString(x) (!(x) || (*(x) == '\0'))

char*       strtoken(char** save, char* str, char* fs);

/*
 * deprecate
 */
#define BadPtr(x) (!(x) || (*(x) == '\0'))


/*
 * character macros
 */
extern const unsigned char ToLowerTab[];
#define ToLower(c) (ToLowerTab[(unsigned char)(c)])

extern const unsigned char ToUpperTab[];
#define ToUpper(c) (ToUpperTab[(unsigned char)(c)])

extern const unsigned int CharAttrs[];

#define PRINT_C   0x001
#define CNTRL_C   0x002
#define ALPHA_C   0x004
#define PUNCT_C   0x008
#define DIGIT_C   0x010
#define SPACE_C   0x020
#define NICK_C    0x040
#define CHAN_C    0x080
#define KWILD_C   0x100
#define CHANPFX_C 0x200
#define USER_C    0x400
#define HOST_C    0x800
#define NONEOS_C 0x1000
#define SERV_C   0x2000
#define EOL_C    0x4000

#define IsHostChar(c)   (CharAttrs[(unsigned char)(c)] & HOST_C)
#define IsUserChar(c)   (CharAttrs[(unsigned char)(c)] & USER_C)
#define IsChanPrefix(c) (CharAttrs[(unsigned char)(c)] & CHANPFX_C)
#define IsChanChar(c)   (CharAttrs[(unsigned char)(c)] & CHAN_C)
#define IsKWildChar(c)  (CharAttrs[(unsigned char)(c)] & KWILD_C)
#define IsNickChar(c)   (CharAttrs[(unsigned char)(c)] & NICK_C)
#define IsServChar(c)   (CharAttrs[(unsigned char)(c)] & (NICK_C | SERV_C))
#define IsCntrl(c)      (CharAttrs[(unsigned char)(c)] & CNTRL_C)
#define IsAlpha(c)      (CharAttrs[(unsigned char)(c)] & ALPHA_C)
#define IsSpace(c)      (CharAttrs[(unsigned char)(c)] & SPACE_C)
#define IsLower(c)      (IsAlpha((c)) && ((unsigned char)(c) > 0x5f))
#define IsUpper(c)      (IsAlpha((c)) && ((unsigned char)(c) < 0x60))
#define IsDigit(c)      (CharAttrs[(unsigned char)(c)] & DIGIT_C)
#define IsXDigit(c) (IsDigit(c) || ('a' <= (c) && (c) <= 'f') || \
        ('A' <= (c) && (c) <= 'F'))
#define IsAlNum(c) (CharAttrs[(unsigned char)(c)] & (DIGIT_C | ALPHA_C))
#define IsPrint(c) (CharAttrs[(unsigned char)(c)] & PRINT_C)
#define IsAscii(c) ((unsigned char)(c) < 0x80)
#define IsGraph(c) (IsPrint((c)) && ((unsigned char)(c) != 0x32))
#define IsPunct(c) (!(CharAttrs[(unsigned char)(c)] & \
                                           (CNTRL_C | ALPHA_C | DIGIT_C)))

#define IsNonEOS(c) (CharAttrs[(unsigned char)(c)] & NONEOS_C)
#define IsEol(c) (CharAttrs[(unsigned char)(c)] & EOL_C)

#endif /* INCLUDED_irc_string_h */

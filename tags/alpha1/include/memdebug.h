/*
 * memdebug.h
 * Macros for conditional memory debugging
 * This file must be included *after* stdlib.h and blalloc.h, as it redefines
 * malloc/realloc/free/BlockAlloc
 */

#ifndef INCLUDED_memdebug_h
#define INCLUDED_memdebug_h

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include "irc_string.h"
#include "blalloc.h"

/* Redefine malloc/realloc/free, enforce use of My* variants */

#ifdef malloc
#undef malloc
#define malloc(x) __use_MyMalloc_not_malloc(x)
#endif

#ifdef realloc
#undef realloc
#define realloc(x, y) __use_MyRealloc_not_realloc(x, y)
#endif

#ifdef free
#undef free
#define free(x) __use_MyFree_not_free(x)
#endif


#ifdef DEBUGMEM
#define DBGMEM_MALLOC 1
#define DBGMEM_BLALLOC 2

#define MyMalloc(x) _MyMalloc(x,__FILE__,__LINE__)
#define MyRealloc(x,y) _MyRealloc(x,y,__FILE__,__LINE__)
#define MyFree(x) _MyFree(x,__FILE__,__LINE__)
#define BlockHeapAlloc(x) _BlockHeapAlloc(x,__FILE__,__LINE__)
#define BlockHeapFree(x,y) _BlockHeapFree(x,y,__FILE__,__LINE__)

void DbgMemAlloc(char * file, int line, size_t size, int flags, void * ptr);
void DbgMemFree(char * file, int line, int flags, void * ptr);
void DbgMemRealloc(char * file, int line, int flags, void * ptr, size_t newsize, void * newptr);
void DbgMemInitEnum();
void DbgMemEndEnum();
void _DbgMemEnum(void * ptr);

#define DbgMemEnum(x) _DbgMemEnum(x,__FILE__,__LINE__)

#else 

#define MyMalloc(x) _MyMalloc(x)
#define MyRealloc(x,y) _MyRealloc(x,y)
#define MyFree(x) _MyFree(x)
#define BlockHeapAlloc(x) _BlockHeapAlloc(x)
#define BlockHeapFree(x,y) _BlockHeapFree(x,y)


#endif /* DEBUGMEM */
#endif /* _MEMDEBUG_H */




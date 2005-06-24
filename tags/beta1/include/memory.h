#ifndef _I_MEMORY_H
#define _I_MEMORY_H
#include "ircd_defs.h"
#include "blalloc.h"

#ifdef MEMDEBUG
extern void*       _MyMalloc(size_t size, char * file, int line);
extern void*       _MyRealloc(void* p, size_t size, char * file, int line);
extern void        _MyFree(void* p, char * file, int line);
extern void        _DupString(char**, const char*, char*, int);
#define MyMalloc(x) _MyMalloc(x, __FILE__, __LINE__)
#define MyRealloc(x,y) _MyRealloc(x, y, __FILE__, __LINE__)
#define MyFree(x) _MyFree(x, __FILE__, __LINE__)
#define DupString(x,y) _DupString(&x, y, __FILE__, __LINE__)
#else /* DEBUGMEM */
extern void*       _MyMalloc(size_t size);
extern void*       _MyRealloc(void* p, size_t size);
extern void        _MyFree(void* p);
extern void        _DupString(char**, const char *);
#define MyMalloc(x) _MyMalloc(x)
#define MyRealloc(x,y) _MyRealloc(x, y)
#define MyFree(x) _MyFree(x)
#define DupString(x,y) _DupString(&x, y)
#endif /* !DEBUGMEM */
#endif /* _I_MEMORY_H */

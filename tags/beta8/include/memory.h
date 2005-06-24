/* $Id$ */

#ifndef _I_MEMORY_H
#define _I_MEMORY_H

#include "ircd_defs.h"
#include "setup.h"
#include "balloc.h"
#include <stdlib.h>
#include <string.h>

extern void outofmemory(void);
#ifndef WE_ARE_MEMORY_C
#undef strdup
#undef malloc
#undef realloc
#undef calloc
#undef free
#define malloc do_not_call_old_memory_functions!call_My*functions
#define calloc do_not_call_old_memory_functions!call_My*functions
#define realloc do_not_call_old_memory_functions!call_My*functions
#define strdup do_not_call_old_memory_functions!call_My*functions
#define free do_not_call_old_memory_functions!call_My*functions
#endif

#ifdef MEMDEBUG
void *memlog(void *m, int s, char *f, int l);
void memulog(void *m);

extern void*       _MyMalloc(size_t size, char *file, int line);
extern void*       _MyRealloc(void* p, size_t size, char * file, int line);
extern void        _MyFree(void* p, char * file, int line);
extern void        _DupString(char**, const char*, char*, int);
extern int         _BlockHeapFree(BlockHeap *bh, void *ptr);
extern void *	  _BlockHeapAlloc(BlockHeap *bh);
#define MyMalloc(x) _MyMalloc(x, __FILE__, __LINE__)
#define MyRealloc(x,y) _MyRealloc(x, y, __FILE__, __LINE__)
#define MyFree(x) _MyFree(x, __FILE__, __LINE__)
#define DupString(x,y) _DupString(&x, y, __FILE__, __LINE__)
#ifndef NOBALLOC
#define BlockHeapAlloc(x) memlog(_BlockHeapAlloc(x), \
                                 x->elemSize-sizeof(MemoryEntry), \
                                 __FILE__, __LINE__)
#define BlockHeapFree(x, y) memulog(y); \
                            _BlockHeapFree(x, \
                            ((char*)y)-sizeof(MemoryEntry))
#endif
void log_memory(void);


typedef struct _MemEntry
{
  size_t size;
  time_t ts;
  char file[50];
  int line;
  struct _MemEntry *next, *last;
  /* Data follows... */
} MemoryEntry;
extern MemoryEntry *first_mem_entry;

#else /* MEMDEBUG */


extern void * _MyMalloc(size_t size);
extern void* _MyRealloc(void* x, size_t y);
extern void _MyFree(void *x);
extern void _DupString(char **x, const char *y);
extern int         _BlockHeapFree(BlockHeap *bh, void *ptr);
extern void *	  _BlockHeapAlloc(BlockHeap *bh);

#define MyMalloc(x) _MyMalloc(x)
#define MyRealloc(x,y) _MyRealloc(x, y)
#define MyFree(x) _MyFree(x)
#define DupString(x,y) _DupString(&x, y)
#ifndef NOBALLOC
#define BlockHeapAlloc(x) _BlockHeapAlloc(x)
#define BlockHeapFree(x, y) _BlockHeapFree(x, y)
#endif
#endif /* !MEMDEBUG */

#ifdef NOBALLOC
#define BlockHeapAlloc(x) MyMalloc((int)x)
#define BlockHeapFree(x,y) MyFree(y)
#endif

#endif /* _I_MEMORY_H */


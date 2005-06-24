/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  memory.c: Memory utilities.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include <stdlib.h>
#include <string.h>

#define WE_ARE_MEMORY_C

#include "ircd_defs.h"
#include "ircd.h"
#include "irc_string.h"
#include "memory.h"
#include "list.h"
#include "client.h"
#include "send.h"
#include "tools.h"
#include "s_log.h"
#include "restart.h"
#include <assert.h>
#ifdef MEMDEBUG
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif


#ifdef MEMDEBUG
/* Hopefully this debugger will work better than the existing one...
 * -A1kmm. */

#define DATA(me) (void*)(((char*)me)+sizeof(MemoryEntry))

void *memlog(void *d, int s, char *f, int l)
{
    MemoryEntry *mme;
    mme = (MemoryEntry *) d;
    d += sizeof(MemoryEntry);
    mme->next = first_mem_entry;
    mme->last = NULL;
    if (first_mem_entry != NULL)
	first_mem_entry->last = mme;
    first_mem_entry = mme;
    if (l > 0)
	mme->line = l;
    else
	*mme->file = 0;
    if (f != NULL)
	strncpy(mme->file, f,
		sizeof(mme->file) - 1)[sizeof(mme->file) - 1] = 0;
    else
	*mme->file = 0;
    mme->ts = CurrentTime;
    mme->size = s;
    return d;
}

void memulog(void *m)
{
    MemoryEntry *mme;
    m -= sizeof(MemoryEntry);
    mme = (MemoryEntry *) m;
    if (mme->last != NULL)
	mme->last->next = mme->next;
    if (mme->next != NULL)
	mme->next->last = mme->last;
    if (first_mem_entry == mme)
	first_mem_entry = mme->next;
}

MemoryEntry *first_mem_entry = NULL;

void *_MyMalloc(size_t size, char *file, int line)
{
    void *what = malloc(size + sizeof(MemoryEntry));
    if (what == NULL)
	outofmemory();
#ifndef	NDEBUG
    mem_frob(what, sizeof(MemoryEntry) + size);
#endif
    return memlog(what, size, file, line);
}

void _MyFree(void *what, char *file, int line)
{
    if(what != NULL) {
    	memulog(what);
    	free(what - sizeof(MemoryEntry));
    }
}

void *_MyRealloc(void *what, size_t size, char *file, int line)
{
    MemoryEntry *mme;
    if (!what)
	return _MyMalloc(size, file, line);
    if (!size) {
	_MyFree(what, file, line);
	return NULL;
    }
    mme = (MemoryEntry *) ((char *) what - sizeof(MemoryEntry));
    mme = realloc(mme, size + sizeof(MemoryEntry));
    mme->size = size;
    if (mme->next != NULL)
	mme->next->last = mme;
    if (mme->last != NULL)
	mme->last->next = mme;
    else
	first_mem_entry = mme;
    return DATA(mme);
}

void _DupString(char **x, const char *y, char *file, int line)
{
    *x = _MyMalloc(strlen(y) + 1, file, line);
    strcpy(*x, y);
}

void ReportAllocated(struct Client *);

void ReportAllocated(struct Client *client_p)
{
    int i = 2000;
    MemoryEntry *mme;
    sendto_one(client_p, ":%s NOTICE %s :*** -- Memory Allocation Report",
	       me.name, client_p->name);
    for (i = 0, mme = first_mem_entry; i < 1000 && mme;
	 mme = mme->next, i++)
	sendto_one(client_p,
		   ":%s NOTICE %s :*** -- %u bytes allocated for %lus at %s:%d",
		   me.name, client_p->name, mme->size,
		   CurrentTime - mme->ts, mme->file, mme->line);
    sendto_one(client_p,
	       ":%s NOTICE %s :*** -- End Memory Allocation Report",
	       me.name, client_p->name);
}

void log_memory(void)
{
    MemoryEntry *mme;
    int fd;
    char buffer[200];
    fd = open("memory.log", O_CREAT | O_WRONLY);
    for (mme = first_mem_entry; mme; mme = mme->next) {
	sprintf(buffer, "%u bytes allocated for %lus at %s:%d\n",
		mme->size, CurrentTime - mme->ts, mme->file, mme->line);
	write(fd, buffer, strlen(buffer));
    }
    close(fd);
}

#else				/* MEMDEBUG */
/*
 * MyMalloc - allocate memory, call outofmemory on failure
 */
void *_MyMalloc(size_t size)
{
    void *ret = calloc(1, size);
    if (ret == NULL)
	outofmemory();
    return ret;
}

/*
 * MyRealloc - reallocate memory, call outofmemory on failure
 */
void *_MyRealloc(void *x, size_t y)
{
    char *ret = realloc(x, y);

    if (!ret)
	outofmemory();
    return ret;
}

void _MyFree(void *x)
{
     if(x)
     	free((x));
}

void _DupString(char **x, const char *y)
{
    (*x) = MyMalloc(strlen(y) + 1);
    strcpy((*x), y);
}


#endif				/* !MEMDEBUG */

/*
 * outofmemory()
 *
 * input        - NONE
 * output       - NONE
 * side effects - simply try to report there is a problem. Abort if it was called more than once
 */
void outofmemory()
{
    static int was_here = 0;

    if (was_here)
	abort();

    was_here = 1;

    ilog(L_CRIT, "Out of memory: restarting server...");
#ifdef MEMDEBUG
    log_memory();
#endif
    restart("Out of Memory");
}

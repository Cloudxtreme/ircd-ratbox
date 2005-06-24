/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  win32.c - Win32 specific routines
 *
 *  Copyright 2002 Aaron Sethman <androsyn@ratbox.org>
 *
 *  $Id$
 */

#include "stdinc.h"
#ifdef __MINGW32__


/*
 * This is our gettimeofday() filler.  
 * Don't laugh, it works
 */
int gettimeofday(struct timeval *tv, void *tz)
{
	tv->tv_sec = time(NULL);
	tv->tv_usec = 0;
	if(tv->tv_sec == -1)
		return -1;
	return 0;	
}


int kill(int pid, int sig)
{
	errno = ENOSYS;
	return -1;
}

unsigned int geteuid(void)
{
	/* Return some crap */
	return 65534;
}

#endif /* __MINGW32__ */
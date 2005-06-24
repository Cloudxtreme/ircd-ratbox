/*
 * event.h - defines for event.c, the event system. This has been ported
 * from squid by adrian to simplify scheduling events.
 *
 * $Id$
 */
#ifndef __EVENT_H__
#define __EVENT_H__

/*
 * How many event entries we need to allocate at a time in the block
 * allocator. 16 should be plenty at a time.
 */
#define	EVENT_BLOCK_SIZE	16

typedef void EVH(void *);

extern void eventAdd(const char *name, EVH * func, void *arg, time_t when, int);
extern void eventAddIsh(const char *name, EVH * func, void *arg,
    time_t delta_ish, int);
extern void eventRun(void);
extern time_t eventNextTime(void);
extern void eventDelete(EVH * func, void *arg); 
extern void eventInit(void);
extern void eventFreeMemory(void);
extern int eventFind(EVH *, void *);

extern int show_events( struct Client *sptr );

#endif

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
#define	MAX_EVENTS	50


typedef void EVH(void *);

/* The list of event processes */
struct ev_entry
{
  EVH *func;
  void *arg;
  const char *name;
  time_t frequency;
  time_t when;
  struct ev_entry *next;
  int active;
};

extern void eventAdd(const char *name, EVH *func, void *arg, time_t when);
extern void eventAddIsh(const char *name, EVH *func, void *arg, time_t delta_ish);
extern void eventRun(void);
extern time_t eventNextTime(void);
extern void eventInit(void);
extern void eventDelete(EVH *func, void *);
extern int eventFind(EVH *func, void *);
extern void set_back_events(time_t);

extern void show_events( struct Client *source_p);

#endif

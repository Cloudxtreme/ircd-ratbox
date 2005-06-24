/*
 * Event Processing
 *
 * This code was borrowed from the squid web cache by Adrian Chadd.
 *
 * $Id$
 *
 * Original header follows:
 *
 * Id: event.c,v 1.30 2000/04/16 21:55:10 wessels Exp
 *
 * DEBUG: section 41    Event Processing
 * AUTHOR: Henrik Nordstrom
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  the Regents of the University of California.  Please see the
 *  COPYRIGHT file for full details.  Squid incorporates software
 *  developed and/or copyrighted by other sources.  Please see the
 *  CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

/*
 * How its used:
 *
 * Should be pretty self-explanatory. Events are one-shot, so once your
 * callback is called, you have to re-add the event through eventAdd().
 * Note that the event text is used by last_event_ran to point to the
 * last event run, so you should make sure its a static string. (Don't
 * ask me why, its how it is in the squid code..)
 *   -- Adrian
 */

#include <stdlib.h>

#include "ircd.h"
#include "blalloc.h"
#include "event.h"
#include "client.h"
#include "send.h"
#include "memdebug.h"

/* The list of event processes */
struct ev_entry
{
  EVH *func;
  void *arg;
  const char *name;
  time_t when;
  struct ev_entry *next;
  int weight;
  int id;
};

static struct ev_entry *tasks = NULL;
static int run_id = 0;
static const char *last_event_ran = NULL;
static BlockHeap *event_bl = NULL;

void
eventAdd(const char *name, EVH * func, void *arg, time_t when, int weight)
{
  struct ev_entry *event = (struct ev_entry *)BlockHeapAlloc(event_bl);
  struct ev_entry **E;
  event->func = func;
  event->arg = arg;
  event->name = name;
  event->when = CurrentTime + when;
  event->weight = weight;
  event->id = run_id;
#if SQUID
  debug(41, 7) ("eventAdd: Adding '%s', in %f seconds\n", name, when);
#endif
  /* Insert after the last event with the same or earlier time */
  for (E = &tasks; *E; E = &(*E)->next)
    {
      if ((*E)->when > event->when)
        break;
    }
  event->next = *E;
  *E = event;
}

/* same as eventAdd but adds a random offset within +-1/3 of delta_ish */
void
eventAddIsh(const char *name, EVH * func, void *arg, time_t delta_ish, int weight)
{
  if (delta_ish >= 3.0)
    {
      const time_t two_third = (2 * delta_ish) / 3;
      delta_ish = two_third + ((random() % 1000) * two_third) / 1000;
      /*
       * XXX I hate the above magic, I don't even know if its right.
       * Grr. -- adrian
       */
    }
  eventAdd(name, func, arg, delta_ish, weight);
}

void
eventDelete(EVH * func, void *arg)
{
  struct ev_entry **E;
  struct ev_entry *event;
  for (E = &tasks; (event = *E) != NULL; E = &(*E)->next)
    {
      if (event->func != func)
        continue;
      if (event->arg != arg)
        continue;
      *E = event->next;
      BlockHeapFree(event_bl, event);
      return;
    }
#ifdef SQUID
  debug_trap("eventDelete: event not found");
#endif
}

void
eventRun(void)
{
  struct ev_entry *event = NULL;
  EVH *func;
  void *arg;
  int weight = 0;
  if (NULL == tasks)
    return;
  if (tasks->when > CurrentTime)
    return;
  run_id++;
#ifdef SQUID
  debug(41, 5) ("eventRun: RUN ID %d\n", run_id);
#endif
  while ((event = tasks))
    {
      int valid = 1;
      if (event->when > CurrentTime)
        break;
      if (event->id == run_id)        /* was added during this run */
        break;
      if (weight)
        break;
      func = event->func;
      arg = event->arg;
      event->func = NULL;
      event->arg = NULL;
      tasks = event->next;
      if (valid)
        {
          weight += event->weight;
          /* XXX assumes ->name is static memory! */
          last_event_ran = event->name;
#ifdef SQUID
          debug(41, 5) ("eventRun: Running '%s', id %d\n",
                event->name, event->id);
#endif
          func(arg);
        }
      BlockHeapFree(event_bl, event);
    }
}

time_t
eventNextTime(void)
{
  if (!tasks)
    return (time_t) 100000;
  return (time_t) (tasks->when - CurrentTime);
}

void
eventInit(void)
{
  event_bl = BlockHeapCreate(sizeof(struct ev_entry), EVENT_BLOCK_SIZE);
}

#if SQUID
static void
eventDump(StoreEntry * sentry)
{
  struct ev_entry *e = tasks;
  if (last_event_ran)
    storeAppendPrintf(sentry, "Last event to run: %s\n\n", last_event_ran);
  storeAppendPrintf(sentry, "%s\t%s\t%s\t%s\n",
      "Operation",
      "Next Execution",
      "Weight",
      "Callback Valid?");
  while (e != NULL)
    {
      storeAppendPrintf(sentry, "%s\t%f seconds\t%d\t%s\n",
                        e->name, e->when - CurrentTime, e->weight,
                        e->arg ? cbdataValid(e->arg) ? "yes" : "no" : "N/A");

      e = e->next;
    }
}
#endif

void
eventFreeMemory(void)
{
  struct ev_entry *event;
  while ((event = tasks))
    {
      tasks = event->next;
      BlockHeapFree(event_bl, event);
    }
  tasks = NULL;
}

int
eventFind(EVH * func, void *arg)
{
  struct ev_entry *event;
  for (event = tasks; event != NULL; event = event->next)
    {
      if (event->func == func && event->arg == arg)
        return 1;
    }
  return 0;
}

#ifndef SQUID
int
show_events(struct Client *sptr)
{
  struct ev_entry *e = tasks;
  if (last_event_ran)
    sendto_one(sptr,":%s NOTICE %s :*** Last event to run: %s",
               me.name,sptr->name,
               last_event_ran);

  sendto_one(sptr,
     ":%s NOTICE %s :*** Operation            Next Execution  Weight",
     me.name,sptr->name);

  while (e != NULL)
    {
      sendto_one(sptr,
                 ":%s NOTICE %s :*** %-20s %-3d seconds     %d",
                 me.name,sptr->name,
                 e->name, e->when - CurrentTime, e->weight);
      e = e->next;
    }
  sendto_one(sptr,":%s NOTICE %s :*** Finished",me.name,sptr->name);
  return 0;
}
#endif





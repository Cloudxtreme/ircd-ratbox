/************************************************************************
 *   IRC - Internet Relay Chat, src/s_debug.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

#include <sys/types.h> 
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "tools.h"
#include "s_debug.h"
#include "channel.h"
#include "blalloc.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "fdlist.h"
#include "fileio.h"
#include "hash.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_log.h"
#include "scache.h"
#include "send.h"
#include "whowas.h"
#include "memory.h"

extern  void    count_ip_hash(int *,u_long *);    /* defined in s_conf.c */

/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 */
const char serveropts[] = {
#ifdef  CMDLINE_CONFIG
  'C',
#endif
#ifdef  DEBUGMODE
  'D',
#endif
  'i',  /* SHOW_INVISIBLE_LUSERS (to opers) - always on */
  'I',  /* NO_DEFAULT_INVISIBLE - always on */
  'M',  /* IDLE_FROM_MSG - only /msg re-sets idle time
         * always on */
#ifdef  CRYPT_OPER_PASSWORD
  'p',
#endif
#ifdef  USE_SYSLOG
  'Y',
#endif
  ' ',
  'T',
  'S',
#ifdef TS_CURRENT
  '0' + TS_CURRENT,
#endif
/* ONLY do TS */
/* ALWAYS do TS_WARNINGS */
  'o',
  'w',
  '\0'
};

void debug(int level, char *format, ...)
{
  static char debugbuf[1024];
  va_list args;
  int err = errno;

  if ((debuglevel >= 0) && (level <= debuglevel)) {
    va_start(args, format);

    vsprintf(debugbuf, format, args);
    va_end(args);

    log(L_DEBUG, "%s", debugbuf);
  }
  errno = err;
} /* debug() */

/*
 * This is part of the STATS replies. There is no offical numeric for this
 * since this isnt an official command, in much the same way as HASH isnt.
 * It is also possible that some systems wont support this call or have
 * different field names for "struct rusage".
 * -avalon
 */
void send_usage(struct Client *sptr)
{
  struct rusage  rus;
  time_t         secs;
  time_t         rup;
#ifdef  hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
  int   hzz = 1;
# endif
#endif

#ifdef VMS
  sendto_one(sptr, ":%s NOTICE %s :getrusage not supported on this system");
  return;
#else
  if (getrusage(RUSAGE_SELF, &rus) == -1)
    {
      sendto_one(sptr,":%s NOTICE %s :Getruseage error: %s.",
                 me.name, sptr->name, strerror(errno));
      return;
    }
  secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
  if (0 == secs)
    secs = 1;

  rup = (CurrentTime - me.since) * hzz;
  if (0 == rup)
    rup = 1;


  sendto_one(sptr,
             ":%s %d %s :CPU Secs %lu:%lu User %lu:%lu System %lu:%lu",
             me.name, RPL_STATSDEBUG, sptr->name, secs/60, secs%60,
             rus.ru_utime.tv_sec/60, rus.ru_utime.tv_sec%60,
             rus.ru_stime.tv_sec/60, rus.ru_stime.tv_sec%60);
  sendto_one(sptr, ":%s %d %s :RSS %lu ShMem %lu Data %lu Stack %lu",
             me.name, RPL_STATSDEBUG, sptr->name, rus.ru_maxrss,
             rus.ru_ixrss / rup, rus.ru_idrss / rup,
             rus.ru_isrss / rup);
  sendto_one(sptr, ":%s %d %s :Swaps %lu Reclaims %lu Faults %lu",
             me.name, RPL_STATSDEBUG, sptr->name, rus.ru_nswap,
             rus.ru_minflt, rus.ru_majflt);
  sendto_one(sptr, ":%s %d %s :Block in %lu out %lu",
             me.name, RPL_STATSDEBUG, sptr->name, rus.ru_inblock,
             rus.ru_oublock);
  sendto_one(sptr, ":%s %d %s :Msg Rcv %lu Send %lu",
             me.name, RPL_STATSDEBUG, sptr->name, rus.ru_msgrcv, rus.ru_msgsnd);
  sendto_one(sptr, ":%s %d %s :Signals %lu Context Vol. %lu Invol %lu",
             me.name, RPL_STATSDEBUG, sptr->name, rus.ru_nsignals,
             rus.ru_nvcsw, rus.ru_nivcsw);
#endif /* VMS */
}

void count_memory(struct Client *sptr)
{
  struct Client *acptr;
  struct Channel *chptr;
  struct ConfItem *aconf;
  struct Class *cltmp;
  dlink_node *dlink;

  int lc = 0;           /* local clients */
  int ch = 0;           /* channels */
  int lcc = 0;          /* local client conf links */
  int rc = 0;           /* remote clients */
  int us = 0;           /* user structs */
  int chu = 0;          /* channel users */
  int chi = 0;          /* channel invites */
  int chb = 0;          /* channel bans */
  int wwu = 0;          /* whowas users */
  int cl = 0;           /* classes */
  int co = 0;           /* conf lines */

  int usi = 0;          /* users invited */
  int usc = 0;          /* users in channels */
  int aw = 0;           /* aways set */
  int number_ips_stored;        /* number of ip addresses hashed */
  int number_servers_cached; /* number of servers cached by scache */

  u_long chm = 0;       /* memory used by channels */
  u_long chbm = 0;      /* memory used by channel bans */
  u_long lcm = 0;       /* memory used by local clients */
  u_long rcm = 0;       /* memory used by remote clients */
  u_long awm = 0;       /* memory used by aways */
  u_long wwm = 0;       /* whowas array memory used */
  u_long com = 0;       /* memory used by conf lines */
  u_long rm = 0;        /* res memory used */
  u_long mem_servers_cached; /* memory used by scache */
  u_long mem_ips_stored; /* memory used by ip address hash */

  size_t dbuf_allocated          = 0;
#if 0
  size_t dbuf_used               = 0;
  size_t dbuf_alloc_count        = 0;
  size_t dbuf_used_count         = 0;
#endif

  size_t client_hash_table_size = 0;
  size_t channel_hash_table_size = 0;
  u_long totcl = 0;
  u_long totch = 0;
  u_long totww = 0;

  u_long local_client_memory_used = 0;
  u_long local_client_memory_allocated = 0;

  u_long remote_client_memory_used = 0;
  u_long remote_client_memory_allocated = 0;

  u_long user_memory_used = 0;
  u_long user_memory_allocated = 0;

  u_long links_memory_used = 0;
  u_long links_memory_allocated = 0;

  u_long tot = 0;

  count_whowas_memory(&wwu, &wwm);      /* no more away memory to count */

  for (acptr = GlobalClientList; acptr; acptr = acptr->next)
    {
      if (MyConnect(acptr))
        {
          lc++;
          for (dlink = acptr->localClient->confs.head;
	       dlink; dlink = dlink->next)
            lcc++;
        }
      else
        rc++;
      if (acptr->user)
        {
          us++;
          for (dlink = acptr->user->invited.head; dlink;
               dlink = dlink->next)
            usi++;
          for (dlink = acptr->user->channel.head; dlink;
               dlink = dlink->next)
            usc++;
          if (acptr->user->away)
            {
              aw++;
              awm += (strlen(acptr->user->away)+1);
            }
        }
    }

/* XXX */
#if 0
  lcm = lc * CLIENT_LOCAL_SIZE;
  rcm = rc * CLIENT_REMOTE_SIZE;
#endif

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      ch++;
      chm += (strlen(chptr->chname) + sizeof(struct Channel));

      for (dlink = chptr->peons.head; dlink; dlink = dlink->next)
        chu++;
      for (dlink = chptr->chanops.head; dlink; dlink = dlink->next)
        chu++;
      for (dlink = chptr->voiced.head; dlink; dlink = dlink->next)
        chu++;
      for (dlink = chptr->halfops.head; dlink; dlink = dlink->next)
        chu++;

      for (dlink = chptr->invites.head; dlink; dlink = dlink->next)
        chi++;

      /* XXX invex deny except */
      for (dlink = chptr->banlist.head; dlink; dlink = dlink->next)
        {
          chb++;

	  /* XXX */
#if 0
          chbm += (strlen(dlink->data)+1+sizeof(dlink_node));
          if (dlink->value.banptr->banstr)
            chbm += strlen(dlink->value.banptr->banstr);
          if (dlink->value.banptr->who)
            chbm += strlen(dlink->value.banptr->who);
#endif
        }
    }

  for (aconf = ConfigItemList; aconf; aconf = aconf->next)
    {
      co++;
      com += aconf->host ? strlen(aconf->host)+1 : 0;
      com += aconf->passwd ? strlen(aconf->passwd)+1 : 0;
      com += aconf->name ? strlen(aconf->name)+1 : 0;
      com += sizeof(struct ConfItem);
    }

  for (cltmp = ClassList; cltmp; cltmp = cltmp->next)
    cl++;

  /*
   * need to set dbuf_count here because we use a dbuf when we send
   * the results. since sending the results results in a dbuf being used,
   * the count would be wrong if we just used the globals
   */

#if 0
  count_dbuf_memory(&dbuf_allocated, &dbuf_used);
  dbuf_alloc_count = INITIAL_DBUFS + DBufAllocCount;
  dbuf_used_count  = DBufUsedCount;
#endif

  sendto_one(sptr, ":%s %d %s :Client Local %u(%lu) Remote %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name, lc, lcm, rc, rcm);
  sendto_one(sptr, ":%s %d %s :Users %u(%u) Invites %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name,
	     us, us*sizeof(struct User), usi,
             usi * sizeof(dlink_node));
  sendto_one(sptr, ":%s %d %s :User channels %u(%u) Aways %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name, usc, usc*sizeof(dlink_node),
             aw, awm);
  sendto_one(sptr, ":%s %d %s :Attached confs %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name, lcc, lcc*sizeof(dlink_node));

  totcl = lcm + rcm + us*sizeof(struct User) + usc*sizeof(dlink_node) + awm;
  totcl += lcc*sizeof(dlink_node) + usi*sizeof(dlink_node);

  sendto_one(sptr, ":%s %d %s :Conflines %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name, co, com);

  sendto_one(sptr, ":%s %d %s :Classes %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name, cl, cl*sizeof(struct Class));

  sendto_one(sptr, ":%s %d %s :Channels %u(%lu) Bans %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name, ch, chm, chb, chbm);
  sendto_one(sptr, ":%s %d %s :Channel members %u(%u) invite %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name, chu, chu*sizeof(dlink_node),
             chi, chi*sizeof(dlink_node));

  totch = chm + chbm + chu*sizeof(dlink_node) + chi*sizeof(dlink_node);

  sendto_one(sptr, ":%s %d %s :Whowas users %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name,
	     wwu, wwu*sizeof(struct User));

  sendto_one(sptr, ":%s %d %s :Whowas array %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name, NICKNAMEHISTORYLENGTH, wwm);

  totww = wwu * sizeof(struct User) + wwm;

  client_hash_table_size  = hash_get_client_table_size();
  channel_hash_table_size = hash_get_channel_table_size();

  sendto_one(sptr, ":%s %d %s :Hash: client %u(%u) chan %u(%u)",
             me.name, RPL_STATSDEBUG, sptr->name,
             U_MAX, client_hash_table_size,
             CH_MAX, channel_hash_table_size);

#if 0
  sendto_one(sptr, ":%s %d %s :Dbuf blocks allocated %d(%d), used %d(%d) max allocated by malloc() %d",
             me.name, RPL_STATSDEBUG, sptr->name,
	     dbuf_alloc_count, dbuf_allocated,
             dbuf_used_count, dbuf_used, DBufMaxAllocated );
#endif

#if 0
  rm = cres_mem(sptr);
#endif
  count_scache(&number_servers_cached,&mem_servers_cached);

  sendto_one(sptr, ":%s %d %s :scache %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name,
             number_servers_cached,
             mem_servers_cached);

  count_ip_hash(&number_ips_stored,&mem_ips_stored);
  sendto_one(sptr, ":%s %d %s :iphash %u(%lu)",
             me.name, RPL_STATSDEBUG, sptr->name,
             number_ips_stored,
             mem_ips_stored);

  tot = totww + totch + totcl + com + cl*sizeof(struct Class) +
    dbuf_allocated + rm;
  tot += client_hash_table_size;
  tot += channel_hash_table_size;

  tot += mem_servers_cached;
  sendto_one(sptr, ":%s %d %s :Total: ww %lu ch %lu cl %lu co %lu db %u",
             me.name, RPL_STATSDEBUG, sptr->name, totww, totch, totcl, com, 
             dbuf_allocated);


  count_local_client_memory((int *)&local_client_memory_used,
                            (int *)&local_client_memory_allocated);
  tot += local_client_memory_allocated;
  sendto_one(sptr, ":%s %d %s :Local client Memory in use: %lu Local "
                   "client Memory allocated: %lu",
             me.name, RPL_STATSDEBUG, sptr->name,
             local_client_memory_used, local_client_memory_allocated);


  count_remote_client_memory( (int *)&remote_client_memory_used,
                              (int *)&remote_client_memory_allocated);
  tot += remote_client_memory_allocated;
  sendto_one(sptr, ":%s %d %s :Remote client Memory in use: %lu Remote "
                   "client Memory allocated: %lu",
             me.name, RPL_STATSDEBUG, sptr->name,
             remote_client_memory_used, remote_client_memory_allocated);


  count_user_memory( (int *)&user_memory_used,
                    (int *)&user_memory_allocated);
  tot += user_memory_allocated;
  sendto_one(sptr, ":%s %d %s :struct User Memory in use: %lu struct User "
                   "Memory allocated: %lu",
             me.name, RPL_STATSDEBUG, sptr->name,
             user_memory_used,
             user_memory_allocated);


  count_links_memory( (int *)&links_memory_used,
		      (int *)&links_memory_allocated);
  sendto_one(sptr, ":%s %d %s :Links Memory in use: %lu Links Memory "
                   "allocated: %lu",
             me.name, RPL_STATSDEBUG, sptr->name,
             links_memory_used,
             links_memory_allocated);

  sendto_one(sptr, 
             ":%s %d %s :TOTAL: %lu Available:  Current max RSS: %u",
             me.name, RPL_STATSDEBUG, sptr->name, tot, get_maxrss());

}

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
#include "s_debug.h"
#include "channel.h"
#include "blalloc.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "dbuf.h"
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/resource.h>

extern  void    count_ip_hash(int *,u_long *);    /* defined in s_conf.c */

/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 */
const char serveropts[] = {
#ifdef  SENDQ_ALWAYS
  'A',
#endif
#ifdef  CHROOTDIR
  'c',
#endif
#ifdef  CMDLINE_CONFIG
  'C',
#endif
#ifdef        DO_ID
  'd',
#endif
#ifdef  DEBUGMODE
  'D',
#endif
#ifdef  LOCOP_REHASH
  'e',
#endif
#ifdef  OPER_REHASH
  'E',
#endif
#ifdef  SHOW_INVISIBLE_LUSERS
  'i',
#endif
#ifdef  OPER_KILL
# ifdef  LOCAL_KILL_ONLY
  'k',
# else
  'K',
# endif
#endif
#ifdef  IDLE_FROM_MSG
  'M',
#endif
#ifdef  CRYPT_OPER_PASSWORD
  'p',
#endif
#ifdef  CRYPT_LINK_PASSWORD
  'P',
#endif
#ifdef  LOCOP_RESTART
  'r',
#endif
#ifdef  OPER_RESTART
  'R',
#endif
#ifdef  OPER_REMOTE
  't',
#endif
#ifdef  VALLOC
  'V',
#endif
#ifdef  USE_SYSLOG
  'Y',
#endif
  'Z',
  ' ',
  'T',
  'S',
#ifdef TS_CURRENT
  '0' + TS_CURRENT,
#endif
/* th+hybrid servers ONLY do TS */
/* th+hybrid servers ALWAYS do TS_WARNINGS */
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

    log(L_DEBUG, debugbuf);
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
void send_usage(struct Client *cptr, char *nick)
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

  if (getrusage(RUSAGE_SELF, &rus) == -1)
    {
      sendto_one(cptr,":%s NOTICE %s :Getruseage error: %s.",
                 me.name, nick, strerror(errno));
      return;
    }
  secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
  if (0 == secs)
    secs = 1;

  rup = (CurrentTime - me.since) * hzz;
  if (0 == rup)
    rup = 1;


  sendto_one(cptr,
             ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
             me.name, RPL_STATSDEBUG, nick, secs/60, secs%60,
             rus.ru_utime.tv_sec/60, rus.ru_utime.tv_sec%60,
             rus.ru_stime.tv_sec/60, rus.ru_stime.tv_sec%60);
  sendto_one(cptr, ":%s %d %s :RSS %d ShMem %d Data %d Stack %d",
             me.name, RPL_STATSDEBUG, nick, rus.ru_maxrss,
             rus.ru_ixrss / rup, rus.ru_idrss / rup,
             rus.ru_isrss / rup);
  sendto_one(cptr, ":%s %d %s :Swaps %d Reclaims %d Faults %d",
             me.name, RPL_STATSDEBUG, nick, rus.ru_nswap,
             rus.ru_minflt, rus.ru_majflt);
  sendto_one(cptr, ":%s %d %s :Block in %d out %d",
             me.name, RPL_STATSDEBUG, nick, rus.ru_inblock,
             rus.ru_oublock);
  sendto_one(cptr, ":%s %d %s :Msg Rcv %d Send %d",
             me.name, RPL_STATSDEBUG, nick, rus.ru_msgrcv, rus.ru_msgsnd);
  sendto_one(cptr, ":%s %d %s :Signals %d Context Vol. %d Invol %d",
             me.name, RPL_STATSDEBUG, nick, rus.ru_nsignals,
             rus.ru_nvcsw, rus.ru_nivcsw);

}

void count_memory(struct Client *cptr,char *nick)
{
  struct Client *acptr;
  struct SLink *link;
  struct Channel *chptr;
  struct ConfItem *aconf;
  struct Class *cltmp;

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
  size_t dbuf_used               = 0;
  size_t dbuf_alloc_count        = 0;
  size_t dbuf_used_count         = 0;

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

#ifdef FLUD
  u_long flud_memory_used = 0;
  u_long flud_memory_allocated = 0;
#endif

  u_long tot = 0;

  count_whowas_memory(&wwu, &wwm);      /* no more away memory to count */

  for (acptr = GlobalClientList; acptr; acptr = acptr->next)
    {
      if (MyConnect(acptr))
        {
          lc++;
          for (link = acptr->confs; link; link = link->next)
            lcc++;
        }
      else
        rc++;
      if (acptr->user)
        {
          us++;
          for (link = acptr->user->invited; link;
               link = link->next)
            usi++;
          for (link = acptr->user->channel; link;
               link = link->next)
            usc++;
          if (acptr->user->away)
            {
              aw++;
              awm += (strlen(acptr->user->away)+1);
            }
        }
    }
  lcm = lc * CLIENT_LOCAL_SIZE;
  rcm = rc * CLIENT_REMOTE_SIZE;

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      ch++;
      chm += (strlen(chptr->chname) + sizeof(aChannel));
      for (link = chptr->members; link; link = link->next)
        chu++;
      for (link = chptr->invites; link; link = link->next)
        chi++;
      for (link = chptr->banlist; link; link = link->next)
        {
          chb++;
          chbm += (strlen(link->value.cp)+1+sizeof(struct SLink));
          if (link->value.banptr->banstr)
            chbm += strlen(link->value.banptr->banstr);
          if (link->value.banptr->who)
            chbm += strlen(link->value.banptr->who);
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
  count_dbuf_memory(&dbuf_allocated, &dbuf_used);
  dbuf_alloc_count = INITIAL_DBUFS + DBufAllocCount;
  dbuf_used_count  = DBufUsedCount;

  sendto_one(cptr, ":%s %d %s :Client Local %d(%d) Remote %d(%d)",
             me.name, RPL_STATSDEBUG, nick, lc, lcm, rc, rcm);
  sendto_one(cptr, ":%s %d %s :Users %d(%d) Invites %d(%d)",
             me.name, RPL_STATSDEBUG, nick, us, us*sizeof(struct User), usi,
             usi * sizeof(struct SLink));
  sendto_one(cptr, ":%s %d %s :User channels %d(%d) Aways %d(%d)",
             me.name, RPL_STATSDEBUG, nick, usc, usc*sizeof(struct SLink),
             aw, awm);
  sendto_one(cptr, ":%s %d %s :Attached confs %d(%d)",
             me.name, RPL_STATSDEBUG, nick, lcc, lcc*sizeof(struct SLink));

  totcl = lcm + rcm + us*sizeof(struct User) + usc*sizeof(struct SLink) + awm;
  totcl += lcc*sizeof(struct SLink) + usi*sizeof(struct SLink);

  sendto_one(cptr, ":%s %d %s :Conflines %d(%d)",
             me.name, RPL_STATSDEBUG, nick, co, com);

  sendto_one(cptr, ":%s %d %s :Classes %d(%d)",
             me.name, RPL_STATSDEBUG, nick, cl, cl*sizeof(struct Class));

  sendto_one(cptr, ":%s %d %s :Channels %d(%d) Bans %d(%d)",
             me.name, RPL_STATSDEBUG, nick, ch, chm, chb, chbm);
  sendto_one(cptr, ":%s %d %s :Channel members %d(%d) invite %d(%d)",
             me.name, RPL_STATSDEBUG, nick, chu, chu*sizeof(struct SLink),
             chi, chi*sizeof(struct SLink));

  totch = chm + chbm + chu*sizeof(struct SLink) + chi*sizeof(struct SLink);

  sendto_one(cptr, ":%s %d %s :Whowas users %d(%d)",
             me.name, RPL_STATSDEBUG, nick, wwu, wwu*sizeof(struct User));

  sendto_one(cptr, ":%s %d %s :Whowas array %d(%d)",
             me.name, RPL_STATSDEBUG, nick, NICKNAMEHISTORYLENGTH, wwm);

  totww = wwu * sizeof(struct User) + wwm;

  client_hash_table_size  = hash_get_client_table_size();
  channel_hash_table_size = hash_get_channel_table_size();

  sendto_one(cptr, ":%s %d %s :Hash: client %d(%d) chan %d(%d)",
             me.name, RPL_STATSDEBUG, nick,
             U_MAX, client_hash_table_size,
             CH_MAX, channel_hash_table_size);

  sendto_one(cptr, ":%s %d %s :Dbuf blocks allocated %d(%d), used %d(%d) max allocated by malloc() %d",
             me.name, RPL_STATSDEBUG, nick, dbuf_alloc_count, dbuf_allocated,
             dbuf_used_count, dbuf_used, DBufMaxAllocated );

  rm = cres_mem(cptr);

  count_scache(&number_servers_cached,&mem_servers_cached);

  sendto_one(cptr, ":%s %d %s :scache %d(%d)",
             me.name, RPL_STATSDEBUG, nick,
             number_servers_cached,
             mem_servers_cached);

  count_ip_hash(&number_ips_stored,&mem_ips_stored);
  sendto_one(cptr, ":%s %d %s :iphash %d(%d)",
             me.name, RPL_STATSDEBUG, nick,
             number_ips_stored,
             mem_ips_stored);

  tot = totww + totch + totcl + com + cl*sizeof(struct Class) + dbuf_allocated + rm;
  tot += client_hash_table_size;
  tot += channel_hash_table_size;

  tot += mem_servers_cached;
  sendto_one(cptr, ":%s %d %s :Total: ww %d ch %d cl %d co %d db %d",
             me.name, RPL_STATSDEBUG, nick, totww, totch, totcl, com, 
             dbuf_allocated);


  count_local_client_memory((int *)&local_client_memory_used,
                            (int *)&local_client_memory_allocated);
  tot += local_client_memory_allocated;
  sendto_one(cptr, ":%s %d %s :Local client Memory in use: %d Local client Memory allocated: %d",
             me.name, RPL_STATSDEBUG, nick,
             local_client_memory_used, local_client_memory_allocated);


  count_remote_client_memory( (int *)&remote_client_memory_used,
                              (int *)&remote_client_memory_allocated);
  tot += remote_client_memory_allocated;
  sendto_one(cptr, ":%s %d %s :Remote client Memory in use: %d Remote client Memory allocated: %d",
             me.name, RPL_STATSDEBUG, nick,
             remote_client_memory_used, remote_client_memory_allocated);


  count_user_memory( (int *)&user_memory_used,
                    (int *)&user_memory_allocated);
  tot += user_memory_allocated;
  sendto_one(cptr, ":%s %d %s :struct User Memory in use: %d struct User Memory allocated: %d",
             me.name, RPL_STATSDEBUG, nick,
             user_memory_used,
             user_memory_allocated);


  count_links_memory( (int *)&links_memory_used,
                    (int *)&links_memory_allocated);
  sendto_one(cptr, ":%s %d %s :Links Memory in use: %d Links Memory allocated: %d",
             me.name, RPL_STATSDEBUG, nick,
             links_memory_used,
             links_memory_allocated);

#ifdef FLUD
  count_flud_memory( (int *)&flud_memory_used,
                    (int *)&flud_memory_allocated);
  sendto_one(cptr, ":%s %d %s :FLUD Memory in use: %d FLUD Memory allocated: %d",
             me.name, RPL_STATSDEBUG, nick,
             flud_memory_used,
             flud_memory_allocated);

  tot += flud_memory_allocated;
#endif

  sendto_one(cptr, 
             ":%s %d %s :TOTAL: %d Available:  Current max RSS: %u",
             me.name, RPL_STATSDEBUG, nick, tot, get_maxrss());

}

/*
 * debug function
 * 
 * dump the actual client link lists, then the block allocator
 * link lists
 */

/* defined in client.c *blah* */

extern BlockHeap*        localClientFreeList;
extern BlockHeap*        remoteClientFreeList;

void dump_addresses()
{
  struct Client *acptr;
  int fd_lc_dump;
  int fd_rc_dump;
  int fd_lba_dump;
  int fd_rba_dump;
  char buffer[512];

  fd_lc_dump = open("local_client_list.txt",O_WRONLY|O_TRUNC|O_CREAT,0755);
  fd_rc_dump = open("remote_client_list.txt",O_WRONLY|O_TRUNC|O_CREAT,0755);
  fd_lba_dump = open("local_block_allocator_client_list.txt",O_WRONLY|O_TRUNC|O_CREAT,0755);
  fd_rba_dump = open("remote_block_allocator_client_list.txt",O_WRONLY|O_TRUNC|O_CREAT,0755);

  for (acptr = GlobalClientList; acptr; acptr = acptr->next)
    {
      sprintf(buffer,"%lX\n", (unsigned long)acptr );
      if (MyConnect(acptr))
        {
	  write(fd_lc_dump, buffer, strlen(buffer) );
        }
      else
	{
	  write(fd_rc_dump, buffer, strlen(buffer) );
	}
    }

  BlockHeapDump(localClientFreeList,fd_lba_dump);
  BlockHeapDump(remoteClientFreeList,fd_rba_dump);

  close(fd_rc_dump);
  close(fd_lc_dump);
  close(fd_lba_dump);
  close(fd_rba_dump);
}

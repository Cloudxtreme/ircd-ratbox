/************************************************************************
 *   IRC - Internet Relay Chat, src/s_user.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
 *  $Id$
 */
#include "s_user.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "fdlist.h"
#include "flud.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "listener.h"
#include "motd.h"
#include "ircd_handler.h"
#include "msg.h"
#include "numeric.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "s_stats.h"
#include "scache.h"
#include "send.h"
#include "whowas.h"
#include "flud.h"

#ifdef ANTI_DRONE_FLOOD
#include "dbuf.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>


time_t LastUsedWallops = 0;

static int valid_hostname(const char* hostname);
static int valid_username(const char* username);
static void report_and_set_user_flags( struct Client *, struct ConfItem * );
static int tell_user_off(struct Client *,char **);

/* table of ascii char letters to corresponding bitmask */

struct flag_item
{
  int mode;
  char letter;
};

static struct flag_item user_modes[] =
{
  {FLAGS_ADMIN, 'a'},
  {FLAGS_BOTS,  'b'},
  {FLAGS_CCONN, 'c'},
  {FLAGS_DEBUG, 'd'},
  {FLAGS_FULL,  'f'},
  {FLAGS_INVISIBLE, 'i'},
  {FLAGS_SKILL, 'k'},
  {FLAGS_NCHANGE, 'n'},
  {FLAGS_OPER, 'o'},
  {FLAGS_LOCOP, 'O'},
  {FLAGS_REJ, 'r'},
  {FLAGS_SERVNOTICE, 's'},
  {FLAGS_WALLOP, 'w'},
  {FLAGS_EXTERNAL, 'x'},
  {FLAGS_SPY, 'y'},
  {FLAGS_OPERWALL, 'z'},
  {0, 0}
};

/* memory is cheap. map 0-255 to equivalent mode */

int user_modes_from_c_to_bitmask[] =
{ 
  /* 0x00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x0F */
  /* 0x10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x1F */
  /* 0x20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2F */
  /* 0x30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x3F */
  0,            /* @ */
  0,            /* A */
  0,            /* B */
  0,            /* C */
  0,            /* D */
  0,            /* E */
  0,            /* F */
  0,            /* G */
  0,            /* H */
  0,            /* I */
  0,            /* J */
  0,            /* K */
  0,            /* L */
  0,            /* M */
  0,            /* N */
  FLAGS_LOCOP,  /* O */
  0,            /* P */
  0,            /* Q */
  0,            /* R */
  0,            /* S */
  0,            /* T */
  0,            /* U */
  0,            /* V */
  0,            /* W */
  0,            /* X */
  0,            /* Y */
  0,            /* Z 0x5A */
  0, 0, 0, 0, 0, /* 0x5F */ 
  /* 0x60 */       0,
  FLAGS_ADMIN,  /* a */
  FLAGS_BOTS,   /* b */
  FLAGS_CCONN,  /* c */
  FLAGS_DEBUG,  /* d */
  0,            /* e */
  FLAGS_FULL,   /* f */
  0,            /* g */
  0,            /* h */
  FLAGS_INVISIBLE, /* i */
  0,            /* j */
  FLAGS_SKILL,  /* k */
  0,            /* l */
  0,            /* m */
  FLAGS_NCHANGE, /* n */
  FLAGS_OPER,   /* o */
  0,            /* p */
  0,            /* q */
  FLAGS_REJ,    /* r */
  FLAGS_SERVNOTICE, /* s */
  0,            /* t */
  0,            /* u */
  0,            /* v */
  FLAGS_WALLOP, /* w */
  FLAGS_EXTERNAL, /* x */
  FLAGS_SPY,    /* y */
  FLAGS_OPERWALL, /* z 0x7A */
  0,0,0,0,0,     /* 0x7B - 0x7F */

  /* 0x80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x9F */
  /* 0x90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x9F */
  /* 0xA0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xAF */
  /* 0xB0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xBF */
  /* 0xC0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xCF */
  /* 0xD0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xDF */
  /* 0xE0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xEF */
  /* 0xF0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* 0xFF */
};

/* internally defined functions */
const char *type_of_bot[]={
  "NONE",
  "eggdrop",
  "vald/com/joh bot",
  "spambot",
  "annoy/ojnkbot"
};

unsigned long my_rand(void);    /* provided by orabidoo */



/*
 * show_opers - send the client a list of opers
 * inputs       - pointer to client to show opers to
 * output       - none
 * side effects - show who is opered on this server
 */
void show_opers(struct Client *cptr)
{
  register struct Client        *cptr2;
  register int j=0;

  for(cptr2 = oper_cptr_list; cptr2; cptr2 = cptr2->next_oper_client)
    {
      ++j;
      if (MyClient(cptr) && IsAnOper(cptr))
        {
          sendto_one(cptr, ":%s %d %s :[%c][%s] %s (%s@%s) Idle: %d",
                     me.name, RPL_STATSDEBUG, cptr->name,
                     IsOper(cptr2) ? 'O' : 'o',
                     oper_privs_as_string(cptr2,
                                          cptr2->confs->value.aconf->port),
                     cptr2->name,
                     cptr2->username, cptr2->host,
                     CurrentTime - cptr2->user->last);
        }
      else
        {
          sendto_one(cptr, ":%s %d %s :[%c] %s (%s@%s) Idle: %d",
                     me.name, RPL_STATSDEBUG, cptr->name,
                     IsOper(cptr2) ? 'O' : 'o',
                     cptr2->name,
                     cptr2->username, cptr2->host,
                     CurrentTime - cptr2->user->last);
        }
    }

  sendto_one(cptr, ":%s %d %s :%d OPER%s", me.name, RPL_STATSDEBUG,
             cptr->name, j, (j==1) ? "" : "s");
}

/*
 * show_lusers - total up counts and display to client
 */
int show_lusers(struct Client *cptr, struct Client *sptr, 
                int parc, char *parv[])
{
#define LUSERS_CACHE_TIME 180
  static long last_time=0;
  static int    s_count = 0, c_count = 0, u_count = 0, i_count = 0;
  static int    o_count = 0, m_client = 0, m_server = 0;
  int forced;
  struct Client *acptr;

/*  forced = (parc >= 2); */
  forced = (IsAnOper(sptr) && (parc > 3));

/* (void)collapse(parv[1]); */

  Count.unknown = 0;
  m_server = Count.myserver;
  m_client = Count.local;
  i_count  = Count.invisi;
  u_count  = Count.unknown;
  c_count  = Count.total-Count.invisi;
  s_count  = Count.server;
  o_count  = Count.oper;
  if (forced || (CurrentTime > last_time+LUSERS_CACHE_TIME))
    {
      last_time = CurrentTime;
      /* only recount if more than a second has passed since last request */
      /* use LUSERS_CACHE_TIME instead... */
      s_count = 0; c_count = 0; u_count = 0; i_count = 0;
      o_count = 0; m_client = 0; m_server = 0;

      for (acptr = GlobalClientList; acptr; acptr = acptr->next)
        {
          switch (acptr->status)
            {
            case STAT_SERVER:
              if (MyConnect(acptr))
                m_server++;
            case STAT_ME:
              s_count++;
              break;
            case STAT_CLIENT:
              if (IsAnOper(acptr))
                o_count++;
#ifdef  SHOW_INVISIBLE_LUSERS
              if (MyConnect(acptr))
                m_client++;
              if (!IsInvisible(acptr))
                c_count++;
              else
                i_count++;
#else
              if (MyConnect(acptr))
                {
                  if (IsInvisible(acptr))
                    {
                      if (IsAnOper(sptr))
                        m_client++;
                    }
                  else
                    m_client++;
                }
              if (!IsInvisible(acptr))
                c_count++;
              else
                i_count++;
#endif
              break;
            default:
              u_count++;
              break;
            }
        }
      /*
       * We only want to reassign the global counts if the recount
       * time has expired, and NOT when it was forced, since someone
       * may supply a mask which will only count part of the userbase
       *        -Taner
       */
      if (!forced)
        {
          if (m_server != Count.myserver)
            {
              sendto_realops_flags(FLAGS_DEBUG, 
                                 "Local server count off by %d",
                                 Count.myserver - m_server);
              Count.myserver = m_server;
            }
          if (s_count != Count.server)
            {
              sendto_realops_flags(FLAGS_DEBUG,
                                 "Server count off by %d",
                                 Count.server - s_count);
              Count.server = s_count;
            }
          if (i_count != Count.invisi)
            {
              sendto_realops_flags(FLAGS_DEBUG,
                                 "Invisible client count off by %d",
                                 Count.invisi - i_count);
              Count.invisi = i_count;
            }
          if ((c_count+i_count) != Count.total)
            {
              sendto_realops_flags(FLAGS_DEBUG, "Total client count off by %d",
                                 Count.total - (c_count+i_count));
              Count.total = c_count+i_count;
            }
          if (m_client != Count.local)
            {
              sendto_realops_flags(FLAGS_DEBUG,
                                 "Local client count off by %d",
                                 Count.local - m_client);
              Count.local = m_client;
            }
          if (o_count != Count.oper)
            {
              sendto_realops_flags(FLAGS_DEBUG,
                                 "Oper count off by %d", Count.oper - o_count);
              Count.oper = o_count;
            }
          Count.unknown = u_count;
        } /* Complain & reset loop */
    } /* Recount loop */
  
#ifndef SHOW_INVISIBLE_LUSERS
  if (IsAnOper(sptr) && i_count)
#endif
    sendto_one(sptr, form_str(RPL_LUSERCLIENT), me.name, parv[0],
               c_count, i_count, s_count);
#ifndef SHOW_INVISIBLE_LUSERS
  else
    sendto_one(sptr,
               ":%s %d %s :There are %d users on %d servers", me.name,
               RPL_LUSERCLIENT, parv[0], c_count,
               s_count);
#endif
  if (o_count)
    sendto_one(sptr, form_str(RPL_LUSEROP),
               me.name, parv[0], o_count);
  if (u_count > 0)
    sendto_one(sptr, form_str(RPL_LUSERUNKNOWN),
               me.name, parv[0], u_count);
  /* This should be ok */
  if (Count.chan > 0)
    sendto_one(sptr, form_str(RPL_LUSERCHANNELS),
               me.name, parv[0], Count.chan);
  sendto_one(sptr, form_str(RPL_LUSERME),
             me.name, parv[0], m_client, m_server);
  sendto_one(sptr, form_str(RPL_LOCALUSERS), me.name, parv[0],
             Count.local, Count.max_loc);
  sendto_one(sptr, form_str(RPL_GLOBALUSERS), me.name, parv[0],
             Count.total, Count.max_tot);

  sendto_one(sptr, form_str(RPL_STATSCONN), me.name, parv[0],
             MaxConnectionCount, MaxClientCount);
  if (m_client > MaxClientCount)
    MaxClientCount = m_client;
  if ((m_client + m_server) > MaxConnectionCount)
    {
      MaxConnectionCount = m_client + m_server;
    }

  return 0;
}

  

/*
** m_functions execute protocol messages on this server:
**
**      cptr    is always NON-NULL, pointing to a *LOCAL* client
**              structure (with an open socket connected!). This
**              identifies the physical socket where the message
**              originated (or which caused the m_function to be
**              executed--some m_functions may call others...).
**
**      sptr    is the source of the message, defined by the
**              prefix part of the message if present. If not
**              or prefix not found, then sptr==cptr.
**
**              (!IsServer(cptr)) => (cptr == sptr), because
**              prefixes are taken *only* from servers...
**
**              (IsServer(cptr))
**                      (sptr == cptr) => the message didn't
**                      have the prefix.
**
**                      (sptr != cptr && IsServer(sptr) means
**                      the prefix specified servername. (?)
**
**                      (sptr != cptr && !IsServer(sptr) means
**                      that message originated from a remote
**                      user (not local).
**
**              combining
**
**              (!IsServer(sptr)) means that, sptr can safely
**              taken as defining the target structure of the
**              message in this server.
**
**      *Always* true (if 'parse' and others are working correct):
**
**      1)      sptr->from == cptr  (note: cptr->from == cptr)
**
**      2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**              *cannot* be a local connection, unless it's
**              actually cptr!). [MyConnect(x) should probably
**              be defined as (x == x->from) --msa ]
**
**      parc    number of variable parameter strings (if zero,
**              parv is allowed to be NULL)
**
**      parv    a NULL terminated list of parameter pointers,
**
**                      parv[0], sender (prefix string), if not present
**                              this points to an empty string.
**                      parv[1]...parv[parc-1]
**                              pointers to additional parameters
**                      parv[parc] == NULL, *always*
**
**              note:   it is guaranteed that parv[0]..parv[parc-1] are all
**                      non-NULL pointers.
*/

/*
** register_user
**      This function is called when both NICK and USER messages
**      have been accepted for the client, in whatever order. Only
**      after this, is the USER message propagated.
**
**      NICK's must be propagated at once when received, although
**      it would be better to delay them too until full info is
**      available. Doing it is not so simple though, would have
**      to implement the following:
**
**      (actually it has been implemented already for a while) -orabidoo
**
**      1) user telnets in and gives only "NICK foobar" and waits
**      2) another user far away logs in normally with the nick
**         "foobar" (quite legal, as this server didn't propagate
**         it).
**      3) now this server gets nick "foobar" from outside, but
**         has already the same defined locally. Current server
**         would just issue "KILL foobar" to clean out dups. But,
**         this is not fair. It should actually request another
**         nick from local user or kill him/her...
*/

int register_user(struct Client *cptr, struct Client *sptr, 
                         char *nick, char *username)
{
  struct ConfItem*  aconf;
  char*       parv[3];
  static char ubuf[12];
  struct User*     user = sptr->user;
  char*       reason;
  char        tmpstr2[IRCD_BUFSIZE];

  assert(0 != sptr);
  assert(sptr->username != username);

  user->last = CurrentTime;
  parv[0] = sptr->name;
  parv[1] = parv[2] = NULL;

  /* pointed out by Mortiis, never be too careful */
  if(strlen(username) > USERLEN)
    username[USERLEN] = '\0';

  reason = NULL;

#define NOT_AUTHORIZED  (-1)
#define SOCKET_ERROR    (-2)
#define I_LINE_FULL     (-3)
#define I_LINE_FULL2    (-4)
#define BANNED_CLIENT   (-5)

  if (MyConnect(sptr))
    {
#ifndef USE_IAUTH
      switch( check_client(sptr,username,&reason))
        {
        case SOCKET_ERROR:
          return exit_client(cptr, sptr, &me, "Socket Error");
          break;

        case I_LINE_FULL:
        case I_LINE_FULL2:
          sendto_realops_flags(FLAGS_FULL, "%s for %s.",
                               "I-line is full", get_client_host(sptr));
          log(L_INFO,"Too many connections from %s.", get_client_host(sptr));
          ServerStats->is_ref++;
          return exit_client(cptr, sptr, &me, 
                 "No more connections allowed in your connection class" );
          break;

        case NOT_AUTHORIZED:

#ifdef REJECT_HOLD

          /* Slow down the reconnectors who are rejected */
          if( (reject_held_fds != REJECT_HELD_MAX ) )
            {
              SetRejectHold(cptr);
              reject_held_fds++;
              release_client_dns_reply(cptr);
              return 0;
            }
          else
#endif
            {
              ServerStats->is_ref++;
	/* jdc - lists server name & port connections are on */
	/*       a purely cosmetical change */
              sendto_realops_flags(FLAGS_CCONN,
				 "%s from %s [%s] on [%s/%u].",
                                 "Unauthorized client connection",
                                 get_client_host(sptr),
                                 inetntoa((char *)&sptr->ip),
				 sptr->listener->name,
				 sptr->listener->port
				 );
              log(L_INFO,
		  "Unauthorized client connection from %s on [%s/%u].",
                  get_client_host(sptr),
		  sptr->listener->name,
		  sptr->listener->port
		  );

              return exit_client(cptr, sptr, &me,
                                 "You are not authorized to use this server");
            }
          break;

        case BANNED_CLIENT:
          {
            if (!IsGotId(sptr))
              {
                if (IsNeedId(sptr))
                  {
                    *sptr->username = '~';
                    strncpy_irc(&sptr->username[1], username, USERLEN - 1);
                  }
                else
                  strncpy_irc(sptr->username, username, USERLEN);
                sptr->username[USERLEN] = '\0';
              }

            if ( tell_user_off( sptr, &reason ))
              {
                ServerStats->is_ref++;
                return exit_client(cptr, sptr, &me, "Banned" );
              }
            else
              return 0;

            break;
          }
        default:
          release_client_dns_reply(cptr);
          break;
        }

      if(!valid_hostname(sptr->host))
        {
          sendto_one(sptr,":%s NOTICE %s :*** Notice -- You have an illegal character in your hostname", 
                     me.name, sptr->name );

          strncpy(sptr->host,sptr->sockhost,HOSTIPLEN+1);
        }

      aconf = sptr->confs->value.aconf;
      if (!aconf)
        return exit_client(cptr, sptr, &me, "*** Not Authorized");
      if (!IsGotId(sptr))
        {
          if (IsNeedIdentd(aconf))
            {
              ServerStats->is_ref++;
              sendto_one(sptr,
 ":%s NOTICE %s :*** Notice -- You need to install identd to use this server",
                         me.name, cptr->name);
               return exit_client(cptr, sptr, &me, "Install identd");
             }
           if (IsNoTilde(aconf))
             {
                strncpy_irc(sptr->username, username, USERLEN);
             }
           else
             {
                *sptr->username = '~';
                strncpy_irc(&sptr->username[1], username, USERLEN - 1);
             }
           sptr->username[USERLEN] = '\0';
        }

      /* password check */
      if (!BadPtr(aconf->passwd) && 0 != strcmp(sptr->passwd, aconf->passwd))
        {
          ServerStats->is_ref++;
          sendto_one(sptr, form_str(ERR_PASSWDMISMATCH),
                     me.name, parv[0]);
          return exit_client(cptr, sptr, &me, "Bad Password");
        }
      memset(sptr->passwd,0, sizeof(sptr->passwd));

      /* report if user has &^>= etc. and set flags as needed in sptr */
      report_and_set_user_flags(sptr, aconf);

      /* Limit clients */
      /*
       * We want to be able to have servers and F-line clients
       * connect, so save room for "buffer" connections.
       * Smaller servers may want to decrease this, and it should
       * probably be just a percentage of the MAXCLIENTS...
       *   -Taner
       */
      /* Except "F:" clients */
      if ( (ConfigFileEntry.botcheck && (
          !sptr->isbot &&
          ((Count.local + 1) >= (
				 GlobalSetOptions.maxclients+MAX_BUFFER)))) ||
            (((Count.local +1) >= (GlobalSetOptions.maxclients - 5)) &&
	     !(IsFlined(sptr))))
        {
          sendto_realops_flags(FLAGS_FULL,
                               "Too many clients, rejecting %s[%s].",
                               nick, sptr->host);
          ServerStats->is_ref++;
          return exit_client(cptr, sptr, &me,
                             "Sorry, server is full - try later");
        }
      /* botcheck */
      if(ConfigFileEntry.botcheck) {
        if(sptr->isbot)
          {
            if(IsBlined(sptr))
              {
                sendto_realops_flags(FLAGS_BOTS,
                                   "Possible %s: %s (%s@%s) [B-lined]",
                                   type_of_bot[sptr->isbot],
                                   sptr->name, sptr->username, sptr->host);
              }
            else
              {
                sendto_realops_flags(FLAGS_BOTS, "Rejecting %s: %s",
                                   type_of_bot[sptr->isbot],
                                   get_client_name(sptr,FALSE));
                ServerStats->is_ref++;
                return exit_client(cptr, sptr, sptr, type_of_bot[sptr->isbot] );
              }
          }
      }
      /* End of botcheck */

      /* valid user name check */

      if (!valid_username(sptr->username))
        {
          sendto_realops_flags(FLAGS_REJ,"Invalid username: %s (%s@%s)",
                             nick, sptr->username, sptr->host);
          ServerStats->is_ref++;
          ircsprintf(tmpstr2, "Invalid username [%s]", sptr->username);
          return exit_client(cptr, sptr, &me, tmpstr2);
        }
      /* end of valid user name check */

      if(!IsAnOper(sptr))
        {
          char *reason;

          if ( (aconf = find_special_conf(sptr->info,CONF_XLINE)))
            {
              if(aconf->passwd)
                reason = aconf->passwd;
              else
                reason = "NONE";
              
              if(aconf->port)
                {
                  if (aconf->port == 1)
                    {
                      sendto_realops_flags(FLAGS_REJ,
                                           "X-line Rejecting [%s] [%s], user %s",
                                           sptr->info,
                                           reason,
                                           get_client_name(cptr, FALSE));
                    }
                  ServerStats->is_ref++;      
                  return exit_client(cptr, sptr, &me, "Bad user info");
                }
              else
                sendto_realops_flags(FLAGS_REJ,
                                   "X-line Warning [%s] [%s], user %s",
                                   sptr->info,
                                   reason,
                                   get_client_name(cptr, FALSE));
            }
        }
#endif /* USE_IAUTH */

      sendto_realops_flags(FLAGS_CCONN,
                         "Client connecting: %s (%s@%s) [%s] {%s}",
                         nick, sptr->username, sptr->host,
                         inetntoa((char *)&sptr->ip),
                         get_client_class(sptr));

      if ((++Count.local) > Count.max_loc)
        {
          Count.max_loc = Count.local;
          if (!(Count.max_loc % 10))
            sendto_ops("New Max Local Clients: %d",
                       Count.max_loc);
        }
    }
  else
    strncpy_irc(sptr->username, username, USERLEN);

  SetClient(sptr);

  sptr->servptr = find_server(user->server);
  if (!sptr->servptr)
    {
      sendto_ops("Ghost killed: %s on invalid server %s",
                 sptr->name, sptr->user->server);
      sendto_one(cptr,":%s KILL %s :%s (Ghosted, %s doesn't exist)",
                 me.name, sptr->name, me.name, user->server);
      sptr->flags |= FLAGS_KILLED;
      return exit_client(NULL, sptr, &me, "Ghost");
    }
  add_client_to_llist(&(sptr->servptr->serv->users), sptr);

/* Increment our total user count here */
  if (++Count.total > Count.max_tot)
    Count.max_tot = Count.total;

  if (MyConnect(sptr))
    {
      sendto_one(sptr, form_str(RPL_WELCOME), me.name, nick, nick);
      /* This is a duplicate of the NOTICE but see below...*/
      sendto_one(sptr, form_str(RPL_YOURHOST), me.name, nick,
                 get_listener_name(sptr->listener), version);
      
      /*
      ** Don't mess with this one - IRCII needs it! -Avalon
      */
      sendto_one(sptr,
                 "NOTICE %s :*** Your host is %s, running version %s",
                 nick, get_listener_name(sptr->listener), version);
      
      sendto_one(sptr, form_str(RPL_CREATED),me.name,nick,creation);
      sendto_one(sptr, form_str(RPL_MYINFO), me.name, parv[0],
                 me.name, version);
      show_lusers(sptr, sptr, 1, parv);

      if (ConfigFileEntry.short_motd) {
        sendto_one(sptr,"NOTICE %s :*** Notice -- motd was last changed at %s",
                   sptr->name,
                   ConfigFileEntry.motd.lastChangedDate);

        sendto_one(sptr,
                   "NOTICE %s :*** Notice -- Please read the motd if you haven't read it",
                   sptr->name);
      
        sendto_one(sptr, form_str(RPL_MOTDSTART),
                   me.name, sptr->name, me.name);
      
        sendto_one(sptr,
                   form_str(RPL_MOTD),
                   me.name, sptr->name,
                   "*** This is the short motd ***"
                   );

        sendto_one(sptr, form_str(RPL_ENDOFMOTD),
                   me.name, sptr->name);
      } else  
        SendMessageFile(sptr, &ConfigFileEntry.motd);
      
#ifdef LITTLE_I_LINES
      if(sptr->confs && sptr->confs->value.aconf &&
         (sptr->confs->value.aconf->flags
          & CONF_FLAGS_LITTLE_I_LINE))
        {
          SetRestricted(sptr);
          sendto_one(sptr,"NOTICE %s :*** Notice -- You are in a restricted access mode",nick);
          sendto_one(sptr,"NOTICE %s :*** Notice -- You can not chanop others",nick);
        }
#endif

#ifdef NEED_SPLITCODE
      if (server_was_split)
        {
          sendto_one(sptr,"NOTICE %s :*** Notice -- server is currently in split-mode",nick);
        }

      nextping = CurrentTime;
#endif


    }
  else if (IsServer(cptr))
    {
      struct Client *acptr;
      if ((acptr = find_server(user->server)) && acptr->from != sptr->from)
        {
          sendto_realops_flags(FLAGS_DEBUG, 
                             "Bad User [%s] :%s USER %s@%s %s, != %s[%s]",
                             cptr->name, nick, sptr->username,
                             sptr->host, user->server,
                             acptr->name, acptr->from->name);
          sendto_one(cptr,
                     ":%s KILL %s :%s (%s != %s[%s] USER from wrong direction)",
                     me.name, sptr->name, me.name, user->server,
                     acptr->from->name, acptr->from->host);
          sptr->flags |= FLAGS_KILLED;
          return exit_client(sptr, sptr, &me,
                             "USER server wrong direction");
          
        }
      /*
       * Super GhostDetect:
       *        If we can't find the server the user is supposed to be on,
       * then simply blow the user away.        -Taner
       */
      if (!acptr)
        {
          sendto_one(cptr,
                     ":%s KILL %s :%s GHOST (no server %s on the net)",
                     me.name,
                     sptr->name, me.name, user->server);
          sendto_realops("No server %s for user %s[%s@%s] from %s",
                          user->server, sptr->name, sptr->username,
                          sptr->host, sptr->from->name);
          sptr->flags |= FLAGS_KILLED;
          return exit_client(sptr, sptr, &me, "Ghosted Client");
        }
    }

  send_umode(NULL, sptr, 0, SEND_UMODES, ubuf);
  if (!*ubuf)
    {
      ubuf[0] = '+';
      ubuf[1] = '\0';
    }
  
  /* LINKLIST 
   * add to local client link list -Dianora
   * I really want to move this add to link list
   * inside the if (MyConnect(sptr)) up above
   * but I also want to make sure its really good and registered
   * local client
   *
   * double link list only for clients, traversing
   * a small link list for opers/servers isn't a big deal
   * but it is for clients -Dianora
   */

  if (MyConnect(sptr))
    {
      if(LocalClientList)
        LocalClientList->previous_local_client = sptr;
      sptr->previous_local_client = (struct Client *)NULL;
      sptr->next_local_client = LocalClientList;
      LocalClientList = sptr;
    }
  
  sendto_serv_butone(cptr, "NICK %s %d %lu %s %s %s %s :%s",
                     nick, sptr->hopcount+1, sptr->tsinfo, ubuf,
                     sptr->username, sptr->host, user->server,
                     sptr->info);
  if (ubuf[1])
    send_umode_out(cptr, sptr, 0);
  return 0;
}

/* 
 * valid_hostname - check hostname for validity
 *
 * Inputs       - pointer to user
 * Output       - YES if valid, NO if not
 * Side effects - NONE
 *
 * NOTE: this doesn't allow a hostname to begin with a dot and
 * will not allow more dots than chars.
 */
static int valid_hostname(const char* hostname)
{
  int         dots  = 0;
  int         chars = 0;
  const char* p     = hostname;

  assert(0 != p);

  if ('.' == *p)
    return NO;

  while (*p)
    {
      if (!IsHostChar(*p))
        return NO;
      if ('.' == *p++)
        ++dots;
      else
        ++chars;
    }
  return (0 == dots || chars < dots) ? NO : YES;
}

/* 
 * valid_username - check username for validity
 *
 * Inputs       - pointer to user
 * Output       - YES if valid, NO if not
 * Side effects - NONE
 * 
 * Absolutely always reject any '*' '!' '?' '@' in an user name
 * reject any odd control characters names.
 * Allow ONE '.' in username to allow for "first.last"
 * style of username
 */
static int valid_username(const char* username)
{
  int dots = 0;
  const char *p = username;
  assert(0 != p);

  if ('~' == *p)
    ++p;
  /* 
   * reject usernames that don't start with an alphanum
   * i.e. reject jokers who have '-@somehost' or '.@somehost'
   * or "-hi-@somehost", "h-----@somehost" would still be accepted.
   *
   * -Dianora
   */
  if (!IsAlNum(*p))
    return NO;

  while (*++p)
    {
      if(*p == '.')
        {
          dots++;
          if(dots > 1)
            return NO;
          if(!IsUserChar(p[1]))
            return NO;
        }
      else if (!IsUserChar(*p))
        return NO;
    }
  return YES;
}

/* 
 * tell_user_off
 *
 * inputs       - client pointer of user to tell off
 *              - pointer to reason user is getting told off
 * output       - drop connection now YES or NO (for reject hold)
 * side effects -
 */

static int
tell_user_off(struct Client *cptr, char **preason )
{
  char* p = 0;

  /* Ok... if using REJECT_HOLD, I'm not going to dump
   * the client immediately, but just mark the client for exit
   * at some future time, .. this marking also disables reads/
   * writes from the client. i.e. the client is "hanging" onto
   * an fd without actually being able to do anything with it
   * I still send the usual messages about the k line, but its
   * not exited immediately.
   * - Dianora
   */
            
#ifdef REJECT_HOLD
  if( (reject_held_fds != REJECT_HELD_MAX ) )
    {
      SetRejectHold(cptr);
      reject_held_fds++;
#endif

      if(ConfigFileEntry.kline_with_reason && *preason)
        {
          if(( p = strchr(*preason, '|')) )
            *p = '\0';

          sendto_one(cptr, ":%s NOTICE %s :*** Banned: %s",
                     me.name,cptr->name,*preason);
           
          if(p)
            *p = '|';
        }
        else
        sendto_one(cptr, ":%s NOTICE %s :*** Banned: No Reason",
                   me.name,cptr->name);
#ifdef REJECT_HOLD
      return NO;
    }
#endif

  return YES;
}

/* report_and_set_user_flags
 *
 * Inputs       - pointer to sptr
 *              - pointer to aconf for this user
 * Output       - NONE
 * Side effects -
 * Report to user any special flags they are getting, and set them.
 */

static void 
report_and_set_user_flags(struct Client *sptr,struct ConfItem *aconf)
{
  /* If this user is being spoofed, tell them so */
  if(IsConfDoSpoofIp(aconf))
    {
      sendto_one(sptr,
                 ":%s NOTICE %s :*** Spoofing your IP. congrats.",
                 me.name,sptr->name);
    }

  /* If this user is in the exception class, Set it "E lined" */
  if(IsConfElined(aconf))
    {
      SetElined(sptr);
      sendto_one(sptr,
         ":%s NOTICE %s :*** You are exempt from K/D/G lines. congrats.",
                 me.name,sptr->name);
    }

  /* If this user can run bots set it "B lined" */
  if(IsConfBlined(aconf))
    {
      SetBlined(sptr);
      sendto_one(sptr,
                 ":%s NOTICE %s :*** You can run bots here. congrats.",
                 me.name,sptr->name);
    }

  /* If this user is exempt from user limits set it F lined" */
  if(IsConfFlined(aconf))
    {
      SetFlined(sptr);
      sendto_one(sptr,
                 ":%s NOTICE %s :*** You are exempt from user limits. congrats.",
                 me.name,sptr->name);
    }
#ifdef IDLE_CHECK
  /* If this user is exempt from idle time outs */
  if(IsConfIdlelined(aconf))
    {
      SetIdlelined(sptr);
      sendto_one(sptr,
         ":%s NOTICE %s :*** You are exempt from idle limits. congrats.",
                 me.name,sptr->name);
    }
#endif
}


/*
** do_user
*/
int do_user(char* nick, struct Client* cptr, struct Client* sptr,
                   char* username, char *host, char *server, char *realname)
{
  unsigned int oflags;
  struct User* user;

  assert(0 != sptr);
  assert(sptr->username != username);

  user = make_user(sptr);

  oflags = sptr->flags;

  if (!MyConnect(sptr))
    {
      /*
       * coming from another server, take the servers word for it
       */
      user->server = find_or_add(server);
      strncpy_irc(sptr->host, host, HOSTLEN); 
    }
  else
    {
      if (!IsUnknown(sptr))
        {
          sendto_one(sptr, form_str(ERR_ALREADYREGISTRED), me.name, nick);
          return 0;
        }
#ifndef NO_DEFAULT_INVISIBLE
      sptr->flags |= FLAGS_INVISIBLE;
#endif

#if 0
      /* Undocumented lame extension to the protocol
       * if user provides a number, the bits set are
       * used to set the umode flags. arghhhhh -db
       */
      sptr->flags |= (UFLAGS & atoi(host));
#endif

      if (!(oflags & FLAGS_INVISIBLE) && IsInvisible(sptr))
        Count.invisi++;
      /*
       * don't take the clients word for it, ever
       *  strncpy_irc(user->host, host, HOSTLEN); 
       */
      user->server = me.name;
    }
  strncpy_irc(sptr->info, realname, REALLEN);

  if (sptr->name[0]) /* NICK already received, now I have USER... */
    return register_user(cptr, sptr, sptr->name, username);
  else
    {
      if (!IsGotId(sptr)) 
        {
          /*
           * save the username in the client
           */
          strncpy_irc(sptr->username, username, USERLEN);
        }
    }
  return 0;
}

/*
 * user_mode - set get current users mode
 *
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int user_mode(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  int   flag;
  int   i;
  char  **p, *m;
  struct Client *acptr;
  int   what, setflags;
  int   badflag = NO;   /* Only send one bad flag notice -Dianora */
  char  buf[BUFSIZE];

  what = MODE_ADD;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "MODE");
      return 0;
    }

  if (!(acptr = find_person(parv[1], NULL)))
    {
      if (MyConnect(sptr))
        sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                   me.name, parv[0], parv[1]);
      return 0;
    }

  if (IsServer(sptr) || sptr != acptr || acptr->from != sptr->from)
    {
      if (IsServer(cptr))
        sendto_ops_butone(NULL, &me,
                          ":%s WALLOPS :MODE for User %s From %s!%s",
                          me.name, parv[1],
                          get_client_name(cptr, FALSE), sptr->name);
      else
        sendto_one(sptr, form_str(ERR_USERSDONTMATCH),
                   me.name, parv[0]);
      return 0;
    }
 
  if (parc < 3)
    {
      m = buf;
      *m++ = '+';

      for (i = 0; user_modes[i].letter && (m - buf < BUFSIZE - 4);i++)
        if (sptr->umodes & user_modes[i].mode)
          *m++ = user_modes[i].letter;
      *m = '\0';
      sendto_one(sptr, form_str(RPL_UMODEIS), me.name, parv[0], buf);
      return 0;
    }

  /* find flags already set for user */
  setflags = sptr->umodes;
  
  /*
   * parse mode change string(s)
   */
  for (p = &parv[2]; p && *p; p++ )
    for (m = *p; *m; m++)
      switch(*m)
        {
        case '+' :
          what = MODE_ADD;
          break;
        case '-' :
          what = MODE_DEL;
          break;        

        case 'O': case 'o' :
          if(what == MODE_ADD)
            {
              if(IsServer(cptr) && !IsOper(sptr))
                {
                  ++Count.oper;
                  SetOper(sptr);
                }
            }
          else
            {
	      /* Only decrement the oper counts if an oper to begin with
               * found by Pat Szuta, Perly , perly@xnet.com 
               */

              if(!IsAnOper(sptr))
                break;

              sptr->umodes &= ~(FLAGS_OPER|FLAGS_LOCOP);

              Count.oper--;

              if (MyConnect(sptr))
                {
                  struct Client *prev_cptr = (struct Client *)NULL;
                  struct Client *cur_cptr = oper_cptr_list;

                  fdlist_delete(sptr->fd, FDL_OPER | FDL_BUSY);
                  detach_conf(sptr,sptr->confs->value.aconf);
                  sptr->flags2 &= ~(FLAGS2_OPER_GLOBAL_KILL|
                                    FLAGS2_OPER_REMOTE|
                                    FLAGS2_OPER_UNKLINE|
                                    FLAGS2_OPER_GLINE|
                                    FLAGS2_OPER_N|
                                    FLAGS2_OPER_K|
					                FLAGS2_OPER_ADMIN);
                  while(cur_cptr)
                    {
                      if(sptr == cur_cptr) 
                        {
                          if(prev_cptr)
                            prev_cptr->next_oper_client = cur_cptr->next_oper_client;
                          else
                            oper_cptr_list = cur_cptr->next_oper_client;
                          cur_cptr->next_oper_client = (struct Client *)NULL;
                          break;
                        }
                      else
                        prev_cptr = cur_cptr;
                      cur_cptr = cur_cptr->next_oper_client;
                    }
                }
            }
          break;

          /* we may not get these,
           * but they shouldnt be in default
           */
        case ' ' :
        case '\n' :
        case '\r' :
        case '\t' :
          break;
        default :
          if( (flag = user_modes_from_c_to_bitmask[(unsigned char)*m]))
            {
              if (what == MODE_ADD)
                sptr->umodes |= flag;
              else
                sptr->umodes &= ~flag;  
            }
          else
            {
              if ( MyConnect(sptr))
                badflag = YES;
            }
          break;
        }

  if(badflag)
    sendto_one(sptr, form_str(ERR_UMODEUNKNOWNFLAG), me.name, parv[0]);

  if ((sptr->umodes & FLAGS_NCHANGE) && !IsSetOperN(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :*** You need oper and N flag for +n",
                 me.name,parv[0]);
      sptr->umodes &= ~FLAGS_NCHANGE; /* only tcm's really need this */
    }
  if ((sptr->umodes & FLAGS_ADMIN) && !IsSetOperAdmin(sptr)) {
	  sendto_one(sptr, ":%s NOTICE %s :*** You need oper and A flag for +a",
				 me.name, parv[0]);
	  sptr->umodes &= ~FLAGS_ADMIN;  /* shouldn't let normal opers set this */
  }
  
  if (!(setflags & FLAGS_INVISIBLE) && IsInvisible(sptr))
    ++Count.invisi;
  if ((setflags & FLAGS_INVISIBLE) && !IsInvisible(sptr))
    --Count.invisi;
  /*
   * compare new flags with old flags and send string which
   * will cause servers to update correctly.
   */
  send_umode_out(cptr, sptr, setflags);

  return 0;
}
        
/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void send_umode(struct Client *cptr, struct Client *sptr, int old, int sendmask,
                char *umode_buf)
{
  int   i;
  int flag;
  char  *m;
  int   what = MODE_NULL;

  /*
   * build a string in umode_buf to represent the change in the user's
   * mode between the new (sptr->flag) and 'old'.
   */
  m = umode_buf;
  *m = '\0';

  for (i = 0; user_modes[i].letter; i++ )
    {
      flag = user_modes[i].mode;

      if (MyClient(sptr) && !(flag & sendmask))
        continue;
      if ((flag & old) && !(sptr->umodes & flag))
        {
          if (what == MODE_DEL)
            *m++ = user_modes[i].letter;
          else
            {
              what = MODE_DEL;
              *m++ = '-';
              *m++ = user_modes[i].letter;
            }
        }
      else if (!(flag & old) && (sptr->umodes & flag))
        {
          if (what == MODE_ADD)
            *m++ = user_modes[i].letter;
          else
            {
              what = MODE_ADD;
              *m++ = '+';
              *m++ = user_modes[i].letter;
            }
        }
    }
  *m = '\0';
  if (*umode_buf && cptr)
    sendto_one(cptr, ":%s MODE %s :%s",
               sptr->name, sptr->name, umode_buf);
}

/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
/*
 * extra argument evenTS added to send to TS servers or not -orabidoo
 *
 * extra argument evenTS no longer needed with TS only th+hybrid
 * server -Dianora
 */
void send_umode_out(struct Client *cptr,
                       struct Client *sptr,
                       int old)
{
  struct Client *acptr;
  char buf[BUFSIZE];

  send_umode(NULL, sptr, old, SEND_UMODES, buf);

  for(acptr = serv_cptr_list; acptr; acptr = acptr->next_server_client)
    {
      if((acptr != cptr) && (acptr != sptr) && (*buf))
        {
          sendto_one(acptr, ":%s MODE %s :%s",
                   sptr->name, sptr->name, buf);
        }
    }

  if (cptr && MyClient(cptr))
    send_umode(cptr, sptr, old, ALL_UMODES, buf);
}




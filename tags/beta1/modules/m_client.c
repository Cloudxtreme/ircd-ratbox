/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_client.c
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
 *   $Id$
 */
#include "handlers.h"
#include "client.h"
#include "fdlist.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_stats.h"
#include "s_user.h"
#include "hash.h"
#include "whowas.h"
#include "s_serv.h"
#include "send.h"
#include "list.h"
#include "channel.h"
#include "s_log.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int nick_from_server(struct Client *, struct Client *, int, char **,
                            time_t, char *);

int clean_nick_name(char* nick);

static int ms_client(struct Client*, struct Client*, int, char**);

struct Message client_msgtab = {
  "CLIENT", 0, 10, 0, MFLG_SLOW, 0,
  {m_unregistered, m_ignore, ms_client, m_ignore}
};

void
_modinit(void)
{
  mod_add_cmd(&client_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&client_msgtab);
}

char *_version = "20001122";

/*
** ms_client
**
** when its a server introducing a new nick
**      parv[0] = sender prefix
**      parv[1] = nickname
**      parv[2] = hop count
**      parv[3] = TS
**      parv[4] = umode
**      parv[5] = username
**      parv[6] = hostname
**      parv[7] = server
**      parv[8] = ID
**      parv[9] = ircname
*/

static int ms_client(struct Client *cptr, struct Client *sptr,
                     int parc, char *parv[])
{
  struct Client* acptr;
  char     nick[NICKLEN + 2];
  time_t   newts = 0;
  int      sameuser = 0;
  char    *id;
  char    *name;
  
  id = parv[8];
  name = parv[9];
  
   /*
   * nick is an auto, need to terminate the string
   */
  strncpy_irc(nick, parv[1], NICKLEN);
  nick[NICKLEN] = '\0';

  /*
   * if clean_nick_name() returns a null name OR if the server sent a nick
   * name and clean_nick_name() changed it in some way (due to rules of nick
   * creation) then reject it. If from a server and we reject it,
   * and KILL it. -avalon 4/4/92
   */
  if (clean_nick_name(nick) == 0 || strcmp(nick, parv[1]) )
    {
      sendto_one(sptr, form_str(ERR_ERRONEUSNICKNAME),
                 me.name, BadPtr(parv[0]) ? "*" : parv[0], parv[1]);
      
      if (IsServer(cptr))
        {
          ServerStats->is_kill++;
          sendto_realops_flags(FLAGS_DEBUG, "Bad Nick: %s From: %s %s",
			       parv[1], parv[0],
			       get_client_name(cptr, HIDE_IP));
          sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
                     me.name, parv[1], me.name, parv[1],
                     nick, cptr->name);
          if (sptr != cptr) /* bad nick change */
            {
              sendto_ll_serv_butone(cptr, sptr, 0,
                                 ":%s KILL %s :%s (%s <- %s!%s@%s)",
                                 me.name, parv[0], me.name,
                                 get_client_name(cptr, HIDE_IP),
                                 parv[0],
                                 sptr->username,
                                 sptr->user ? sptr->user->server :
                                 cptr->name);
              sptr->flags |= FLAGS_KILLED;
              return exit_client(cptr,sptr,&me,"BadNick");
            }
        }
      return 0;
    }
  /* Okay, we should be safe to cut off the username... -A1kmm */
  if (strlen(parv[5]) > USERLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long username from server %s for %s",
                parv[0], parv[1]);
     parv[5][USERLEN] = 0;
    }
  /* Okay, we should be safe to cut off the hostname... -A1kmm */
  if (strlen(parv[6]) > HOSTLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long hostname from server %s for %s",
                parv[0], parv[1]);
     parv[6][HOSTLEN] = 0;
    }
  /* Okay, we should be safe to cut off the realname... -A1kmm */
  if (strlen(name) > REALLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long realname from server %s for %s",
                parv[0], parv[1]);
     name[REALLEN] = 0;
    }
  
  newts = atol(parv[3]);

  if (!(acptr = find_client(nick, NULL)))
    return(nick_from_server(cptr,sptr,parc,parv,newts,nick));
  
  /*
  ** Note: From this point forward it can be assumed that
  ** acptr != sptr (point to different client structures).
  */

  /*
  ** If the older one is "non-person", the new entry is just
  ** allowed to overwrite it. Just silently drop non-person,
  ** and proceed with the nick. This should take care of the
  ** "dormant nick" way of generating collisions...
  */
  if (IsUnknown(acptr)) 
   {
    if (MyConnect(acptr))
      {
        exit_client(NULL, acptr, &me, "Overridden");
        return(nick_from_server(cptr,sptr,parc,parv,newts,nick));
      }
    else
      {
        if (!acptr->user)
          {
            sendto_realops_flags(FLAGS_ALL,
                   "Nick Collision on %s(%s(NOUSER) <- %s!%s@%s)(TS:%s)",
                   acptr->name, acptr->from->name, parv[1], parv[5], parv[6],
                   cptr->name);

#ifndef LOCAL_NICK_COLLIDE
            /* If we got the message from a LL, ensure it gets the kill */
            if (ServerInfo.hub && IsCapable(cptr,CAP_LL))
              add_lazylinkclient(cptr, acptr);
            
	    sendto_ll_serv_butone(NULL, acptr, 0, /* all servers */
		       ":%s KILL %s :%s (%s(NOUSER) <- %s!%s@%s)(TS:%s)",
			       me.name,
			       acptr->name,
			       me.name,
			       acptr->from->name,
			       parv[1],
			       parv[5],
			       parv[6],
			       cptr->name);
#endif

            acptr->flags |= FLAGS_KILLED;
            /* Having no USER struct should be ok... */
            return exit_client(cptr, acptr, &me,
                           "Got TS NICK before Non-TS USER");
        }
      }
   }

  /*
  ** NICK was coming from a server connection. Means that the same
  ** nick is registered for different users by different server.
  ** This is either a race condition (two users coming online about
  ** same time, or net reconnecting) or just two net fragments becoming
  ** joined and having same nicks in use. We cannot have TWO users with
  ** same nick--purge this NICK from the system with a KILL... >;)
  */
  /*
  ** This seemingly obscure test (sptr == cptr) differentiates
  ** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
  */
  /* 
  ** Changed to something reasonable like IsServer(sptr)
  ** (true if "NICK new", false if ":old NICK new") -orabidoo
  */

#ifdef LOCAL_NICK_COLLIDE
  /* XXX what does this code do?
       * This is a new nick that's being introduced, not a nickchange
       */
      /* just propogate it through */
      sendto_ll_serv_butone(cptr, sptr, 0, ":%s NICK %s :%lu",
                         parv[0], nick, sptr->tsinfo);
#endif
      /*
      ** A new NICK being introduced by a neighbouring
      ** server (e.g. message type "NICK new" received)
      */
      if (!newts || !acptr->tsinfo
          || (newts == acptr->tsinfo))
	  {
          sendto_realops_flags(FLAGS_ALL,
							   "Nick collision on %s(%s <- %s)(both killed)",
							   acptr->name, acptr->from->name,
							   get_client_name(cptr, HIDE_IP));
		  
#ifndef LOCAL_NICK_COLLIDE
          /* If we got the message from a LL, ensure it gets the kill */
          if (ServerInfo.hub && IsCapable(cptr,CAP_LL))
			  add_lazylinkclient(cptr, acptr);

          sendto_ll_serv_butone(NULL, acptr, 0,/* all servers */
								":%s KILL %s :%s (%s <- %s)",
								me.name, acptr->name, me.name,
								acptr->from->name,
								/* NOTE: Cannot use get_client_name twice
								** here, it returns static string pointer:
								** the other info would be lost
								*/
								get_client_name(cptr, HIDE_IP));
#endif
          ServerStats->is_kill++;
          sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                     me.name, acptr->name, acptr->name);
		  
          acptr->flags |= FLAGS_KILLED;
          return exit_client(cptr, acptr, &me, "Nick collision");
        }
      else
        {
          sameuser =  acptr->user &&
            irccmp(acptr->username, parv[5]) == 0 &&
            irccmp(acptr->host, parv[6]) == 0;
          if ((sameuser && newts < acptr->tsinfo) ||
              (!sameuser && newts > acptr->tsinfo))
            {
              /* We don't need to kill the user, the other end does */
              client_burst_if_needed(cptr, acptr);
              return 0;
            }
          else
            {
              if (sameuser)
                sendto_realops_flags(FLAGS_ALL,
                       "Nick collision on %s(%s <- %s)(older killed)",
		       acptr->name, acptr->from->name,
		       get_client_name(cptr, HIDE_IP));
              else
                sendto_realops_flags(FLAGS_ALL,
			     "Nick collision on %s(%s <- %s)(newer killed)",
			     acptr->name, acptr->from->name,
			     get_client_name(cptr, HIDE_IP));
              
              ServerStats->is_kill++;
              sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                         me.name, acptr->name, acptr->name);

#ifndef LOCAL_NICK_COLLIDE
              /* If it came from a LL server, it'd have been sptr,
               * so we don't need to mark acptr as known */
	      sendto_ll_serv_butone(sptr, acptr, 0, /* all servers but sptr */
				 ":%s KILL %s :%s (%s <- %s)",
				 me.name, acptr->name, me.name,
				 acptr->from->name,
				 get_client_name(cptr, HIDE_IP));
#endif

              acptr->flags |= FLAGS_KILLED;
              (void)exit_client(cptr, acptr, &me, "Nick collision");
              return nick_from_server(cptr,sptr,parc,parv,newts,nick);
            }
        }
  /*
  ** A NICK change has collided (e.g. message type
  ** ":old NICK new". This requires more complex cleanout.
  ** Both clients must be purged from this server, the "new"
  ** must be killed from the incoming connection, and "old" must
  ** be purged from all outgoing connections.
  */
  if ( !newts || !acptr->tsinfo || (newts == acptr->tsinfo) ||
      !sptr->user)
    {
      sendto_realops_flags(FLAGS_ALL,
	   "Nick change collision from %s to %s(%s <- %s)(both killed)",
           sptr->name, acptr->name, acptr->from->name,
	   get_client_name(cptr, HIDE_IP));
      ServerStats->is_kill++;
      sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                 me.name, acptr->name, acptr->name);

#ifndef LOCAL_NICK_COLLIDE
      /* If we got the message from a LL, it would know
         about sptr already */
      sendto_ll_serv_butone(NULL, sptr, 0, /* KILL old from outgoing servers */
			 ":%s KILL %s :%s (%s(%s) <- %s)",
			 me.name, sptr->name, me.name, acptr->from->name,
			 acptr->name, get_client_name(cptr, HIDE_IP));
#endif

      ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
      /* If we got the message from a LL, ensure it gets the kill */
      if (ServerInfo.hub && IsCapable(cptr,CAP_LL))
        add_lazylinkclient(cptr, acptr);

      sendto_ll_serv_butone(NULL, acptr, 0, /* Kill new from incoming link */
			 ":%s KILL %s :%s (%s <- %s(%s))",
			 me.name, acptr->name, me.name, acptr->from->name,
			 get_client_name(cptr, HIDE_IP), sptr->name);
#endif

      acptr->flags |= FLAGS_KILLED;
      (void)exit_client(NULL, acptr, &me, "Nick collision(new)");
      sptr->flags |= FLAGS_KILLED;
      return exit_client(cptr, sptr, &me, "Nick collision(old)");
    }
  else
    {
      sameuser = irccmp(acptr->username, sptr->username) == 0 &&
                 irccmp(acptr->host, sptr->host) == 0;
      if ((sameuser && newts < acptr->tsinfo) ||
          (!sameuser && newts > acptr->tsinfo))
        {
          if (sameuser)
            sendto_realops_flags(FLAGS_ALL,
                 "Nick change collision from %s to %s(%s <- %s)(older killed)",
                 sptr->name, acptr->name, acptr->from->name,
                 get_client_name(cptr, HIDE_IP));
          else
            sendto_realops_flags(FLAGS_ALL,
                 "Nick change collision from %s to %s(%s <- %s)(newer killed)",
                 sptr->name, acptr->name, acptr->from->name,
                 get_client_name(cptr, HIDE_IP));

          ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
          /* this won't go back to the incoming link, so it doesn't
           * matter if it is an LL */
	  sendto_ll_serv_butone(cptr, sptr, 0, /* KILL old from outgoing servers */
			     ":%s KILL %s :%s (%s(%s) <- %s)",
			     me.name, sptr->name, me.name, acptr->from->name,
			     acptr->name, get_client_name(cptr, HIDE_IP));
#endif

          sptr->flags |= FLAGS_KILLED;
          if (sameuser)
            return exit_client(cptr, sptr, &me, "Nick collision(old)");
          else
            return exit_client(cptr, sptr, &me, "Nick collision(new)");
        }
      else
        {
          if (sameuser)
            sendto_realops_flags(FLAGS_ALL,
                               "Nick collision on %s(%s <- %s)(older killed)",
                               acptr->name, acptr->from->name,
                               get_client_name(cptr, HIDE_IP));
          else
            sendto_realops_flags(FLAGS_ALL,
                         "Nick collision on %s(%s <- %s)(newer killed)",
                         acptr->name, acptr->from->name,
                         get_client_name(cptr, HIDE_IP));
          
#ifndef LOCAL_NICK_COLLIDE
          /* this won't go back to the incoming link, so it doesn't
           * matter if it's an LL */
	  sendto_ll_serv_butone(sptr, acptr, 0, /* all servers but sptr */
			     ":%s KILL %s :%s (%s <- %s)",
			     me.name, acptr->name, me.name,
			     acptr->from->name,
			     get_client_name(cptr, HIDE_IP));
#endif

          ServerStats->is_kill++;
          sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                     me.name, acptr->name, acptr->name);
          acptr->flags |= FLAGS_KILLED;
          (void)exit_client(cptr, acptr, &me, "Nick collision");
        }
    }
  return(nick_from_server(cptr,sptr,parc,parv,newts,nick));
}

/*
 * nick_from_server
 *
 * input        - pointer to physical struct Client
 *              - pointer to source struct Client
 *              - argument count
 *              - arguments
 *              - newts time
 *              - nick
 * output       -
 * side effects -
 */
static int
nick_from_server(struct Client *cptr, struct Client *sptr, int parc,
                 char *parv[], time_t newts,char *nick)
{
  char *name;
  char *id;
  int   flag;
  char* m;
	
  id = parv[8];
  name = parv[9];
  
  sptr = make_client(cptr);
  add_client_to_list(sptr);         /* double linked list */

  /* We don't need to introduce leafs clients back to them! */
  if (ConfigFileEntry.hub && IsCapable(cptr, CAP_LL))
    add_lazylinkclient(cptr, sptr);

  sptr->hopcount = atoi(parv[2]);
  sptr->tsinfo = newts;

  /* copy the nick in place */
  (void)strcpy(sptr->name, nick);
  (void)add_to_client_hash_table(nick, sptr);
  add_to_id_hash_table(id, sptr);

  /*
  ** parse the usermodes -orabidoo
  */
  m = &parv[4][1];
  while (*m)
    {
      flag = user_modes_from_c_to_bitmask[(unsigned char)*m];
      if( flag & FLAGS_INVISIBLE )
	{
	  Count.invisi++;
	}
      if( flag & FLAGS_OPER )
	{
	  Count.oper++;
	}
      sptr->umodes |= flag & SEND_UMODES;
      m++;
    }
	
  return do_remote_user(nick, cptr, sptr, parv[5], parv[6],
			parv[7], name, id);
}


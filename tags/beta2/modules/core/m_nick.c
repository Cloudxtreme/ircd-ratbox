/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_nick.c
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void mr_nick(struct Client*, struct Client*, int, char**);
static void m_nick(struct Client*, struct Client*, int, char**);
static void ms_nick(struct Client*, struct Client*, int, char**);

static int nick_from_server(struct Client *, struct Client *, int, char **,
                            time_t, char *);

struct Message nick_msgtab = {
  "NICK", 0, 1, 0, MFLG_SLOW, 0,
  {mr_nick, m_nick, ms_nick, m_nick}
};

void
_modinit(void)
{
  mod_add_cmd(&nick_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&nick_msgtab);
}

char *_version = "20001122";

/*
** mr_nick
**      parv[0] = sender prefix
**      parv[1] = nickname
*/
static void mr_nick(struct Client *cptr, struct Client *sptr, int parc,
                   char *parv[])
{
  struct   Client *acptr, *ucptr;
  char     nick[NICKLEN + 2];
  char*    s;
  dlink_node *ptr;
  

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, BadPtr(parv[0]) ? "*" : parv[0]);
      return;
    }

  /*
   * parc == 2 on a normal client sign on (local) and a normal
   *      client nick change
   */

  /*
   * XXX - ok, we terminate the nick with the first '~' ?  hmmm..
   *  later we allow them? isvalid used to think '~'s were ok
   *  IsNickChar allows them as well
   */
  if ((s = strchr(parv[1], '~')))
    *s = '\0';

  /*
   * nick is an auto, need to terminate the string
   */
  strncpy_irc(nick, parv[1], NICKLEN);
  nick[NICKLEN] = '\0';

  /*
   * if clean_nick_name() returns a null name its bad
   */
  if (clean_nick_name(nick) == 0)
    {
      sendto_one(sptr, form_str(ERR_ERRONEUSNICKNAME),
                 me.name, BadPtr(parv[0]) ? "*" : parv[0], parv[1]);
      return;
    }

  if(find_q_conf(nick, sptr->username, sptr->host)) 
    {
      sendto_realops_flags(FLAGS_REJ,
			   "Quarantined nick [%s] from user %s",
			   nick, get_client_name(cptr, HIDE_IP));
      sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
		 me.name, BadPtr(parv[0]) ? "*" : parv[0], nick);
      return;
    }

  if ( (acptr = find_client(nick, NULL)) == NULL )
   {
    if (!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
    {
      /* We don't know anyone called nick, but our hub might */
      for( ptr = unknown_list.head; ptr; ptr = ptr->next )
      {
        ucptr = ptr->data;

        if( !strcmp(nick, ucptr->llname) )
        {
          /* We're already waiting for a reply about this nick
           * for someone else. */
          sendto_one(sptr, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);
          return;
        }
      }
      /* Set their llname so we can find them later */
      strcpy(sptr->llname, nick);

      /* Ask the hub about their requested name */
      sendto_one(uplink, ":%s NBURST %s %s !%s", me.name, nick,
                 nick, nick);
      /* wait for LLNICK... */
      return;
    }
    else
      {
        set_initial_nick(cptr, sptr, nick);
        return;
      }
   }
  else
   {
     sendto_one(sptr, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);
   }

  /* NICK message ignored */
}

/*
 * m_nick
 *      parv[0] = sender prefix
 *      parv[1] = nickname
 *
 * Any client seen here is guaranteed to be MyConnect()
 */

static void m_nick(struct Client *cptr, struct Client *sptr,
                  int parc, char *parv[])
{
  char     nick[NICKLEN + 2];
  struct   Client *acptr;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return;
    }

  /*
   * parc == 2 on a normal client sign on (local) and a normal
   *      client nick change
   */

  if (parc != 2)
    return;

  /*
   * nick is an auto, need to terminate the string
   */
  strncpy_irc(nick, parv[1], NICKLEN);
  nick[NICKLEN] = '\0';

  /*
   * if clean_nick_name() its bad.
   */
  if (clean_nick_name(nick) == 0)
    {
      sendto_one(sptr, form_str(ERR_ERRONEUSNICKNAME),
                 me.name, parv[0], nick);
      return;
    }

  if (!IsOper(sptr) && find_q_conf(nick, sptr->username, sptr->host))
    {
      sendto_realops_flags(FLAGS_REJ,
			   "Quarantined nick [%s] from user %s",
			   nick, get_client_name(cptr, HIDE_IP));
      sendto_one(sptr, form_str(ERR_UNAVAILRESOURCE),
		 me.name, parv[0], nick);
      return;
    }

  if ((acptr = find_client(nick, NULL)))
    {
      /*
      ** If acptr == sptr, then we have a client doing a nick
      ** change between *equivalent* nicknames as far as server
      ** is concerned (user is changing the case of his/her
      ** nickname or somesuch)
      */
      if (acptr == sptr)
	{
	  if (strcmp(acptr->name, nick) != 0)
            {
              /*
               * Allows change of case in his/her nick
               * -- go and process change
               */
              change_local_nick(cptr,sptr,nick);
              return;
            }
	  else
	    {
	      /*
	      ** This is just ':old NICK old' type thing.
	      ** Just forget the whole thing here. There is
	      ** no point forwarding it to anywhere,
	      ** especially since servers prior to this
	      ** version would treat it as nick collision.
	      */
	      return; /* NICK Message ignored */
	    }
	}

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
	      change_local_nick(cptr,sptr,nick);
              return;
	    }
	  else
	    {
	      sendto_one(sptr, form_str(ERR_NICKNAMEINUSE),
			 me.name, parv[0], nick);
	    }
	}
      else
	{
	  if (MyConnect(sptr))
	    sendto_one(sptr, form_str(ERR_NICKNAMEINUSE), me.name,
		       parv[0], nick);
	}
    }
  else
    {
      if (!ServerInfo.hub && uplink && IsCapable(uplink, CAP_LL))
      {
        /* The uplink might know someone by this name already. */
        sendto_one(uplink, ":%s NBURST %s %s %s", me.name, nick,
                   nick, sptr->name);
      }
      else
      {
        change_local_nick(cptr,sptr,nick);
        return;
      }
    }
}


/*
** ms_nick
**
** when its a server to server nick change:
**      parv[0] = sender prefix
**      parv[1] = nickname
**      parv[2] = TS when nick change
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
**      parv[8] = ircname
*/
static void ms_nick(struct Client *cptr, struct Client *sptr,
                   int parc, char *parv[])
{
  struct Client* acptr;
  char     nick[NICKLEN + 2];
  time_t   newts = 0;
  int      sameuser = 0;
  int      fromTS = 0;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN), me.name, parv[0]);
      return;
    }

  /*
   * parc == 3 on a normal server-to-server client nick change
   *      notice
   * parc == 9 on a normal TS style server-to-server NICK
   *      introduction
   */
  if( (parc != 3) && (parc != 9) )
    return;

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


	  /* XXX UID */
          sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
                     me.name, parv[1], me.name, parv[1],
                     nick, cptr->name);
          if (sptr != cptr) /* bad nick change */
            {
              kill_client_ll_serv_butone(cptr, sptr,
					 "%s (%s <- %s!%s@%s)",
					 me.name,
					 get_client_name(cptr, HIDE_IP),
					 parv[0],
					 sptr->username,
					 sptr->user ?
					 sptr->user->server : cptr->name);
#if 0
              sendto_ll_serv_butone(cptr, sptr, 0,
                                 ":%s KILL %s :%s (%s <- %s!%s@%s)",
                                 me.name, parv[0], me.name,
                                 get_client_name(cptr, HIDE_IP),
                                 parv[0],
                                 sptr->username,
                                 sptr->user ? sptr->user->server :
                                 cptr->name);
#endif
              sptr->flags |= FLAGS_KILLED;
              exit_client(cptr,sptr,&me,"BadNick");
              return;
            }
        }
      return;
    }
  /* Okay, we should be safe to cut off the username... -A1kmm */
  if (parc > 8 && strlen(parv[5]) > USERLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long username from server %s for %s",
                parv[0], parv[1]);
     parv[5][USERLEN] = 0;
    }
  /* Okay, we should be safe to cut off the hostname... -A1kmm */
  if (parc > 8 && strlen(parv[6]) > HOSTLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long hostname from server %s for %s",
                parv[0], parv[1]);
     parv[6][HOSTLEN] = 0;
    }
  /* Okay, we should be safe to cut off the realname... -A1kmm */
  if (parc > 8 && strlen(parv[8]) > REALLEN)
    {
     sendto_realops_flags(FLAGS_ALL, "Long realname from server %s for %s",
                parv[0], parv[1]);
     parv[8][REALLEN] = 0;
    }
  if (!IsServer(sptr) && parc > 2)
    newts = atol(parv[2]);
  else if (IsServer(sptr) && parc > 3)
    newts = atol(parv[3]);

  fromTS = (parc > 6);

  if (!(acptr = find_client(nick, NULL)))
  {
    nick_from_server(cptr,sptr,parc,parv,newts,nick);
    return;
  }

  /*
  ** If acptr == sptr, then we have a client doing a nick
  ** change between *equivalent* nicknames as far as server
  ** is concerned (user is changing the case of his/her
  ** nickname or somesuch)
  */
  if (acptr == sptr)
   {
    if (strcmp(acptr->name, nick) != 0)
      {
        nick_from_server(cptr,sptr,parc,parv,newts,nick);
        return;
      }
    else
      {
        return; /* NICK Message ignored */
      }
   }

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
        nick_from_server(cptr,sptr,parc,parv,newts,nick);
        return;
      }
    else
      {
        if (fromTS && !(acptr->user))
          {
            sendto_realops_flags(FLAGS_ALL,
                   "Nick Collision on %s(%s(NOUSER) <- %s!%s@%s)(TS:%s)",
                   acptr->name, acptr->from->name, parv[1], parv[5], parv[6],
                   cptr->name);

#ifndef LOCAL_NICK_COLLIDE
            /* If we got the message from a LL, ensure it gets the kill */
            if (ServerInfo.hub && IsCapable(cptr,CAP_LL))
              add_lazylinkclient(cptr, acptr);
            
	    kill_client_ll_serv_butone(NULL, acptr,
				       "%s (%s(NOUSER) <- %s!%s@%s)(TS:%s)",
				       me.name,
				       acptr->from->name,
				       parv[1],
				       parv[5],
				       parv[6],
				       cptr->name);
#if 0
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
#endif

            acptr->flags |= FLAGS_KILLED;
            /* Having no USER struct should be ok... */
            exit_client(cptr, acptr, &me,
                           "Got TS NICK before Non-TS USER");
            return;
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

  if (IsServer(sptr))
    {
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

          kill_client_ll_serv_butone(NULL, acptr,
				     "%s (%s <- %s)",
				     me.name,
				     acptr->from->name,
				     get_client_name(cptr, HIDE_IP));
#if 0
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
#endif
          ServerStats->is_kill++;
          sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                     me.name, acptr->name, acptr->name);

          acptr->flags |= FLAGS_KILLED;
          exit_client(cptr, acptr, &me, "Nick collision");
          return;
        }
      else
        {
          sameuser =  fromTS && (acptr->user) &&
            irccmp(acptr->username, parv[5]) == 0 &&
            irccmp(acptr->host, parv[6]) == 0;
          if ((sameuser && newts < acptr->tsinfo) ||
              (!sameuser && newts > acptr->tsinfo))
            {
              /* We don't need to kill the user, the other end does */
              client_burst_if_needed(cptr, acptr);
              return;
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
               * so we don't need to mark acptr as known
	       */

	      kill_client_ll_serv_butone(sptr, acptr,
					 "%s (%s <- %s)",
					 me.name,
					 acptr->from->name,
					 get_client_name(cptr, HIDE_IP));
#if 0
	      sendto_ll_serv_butone(sptr, acptr, 0, /* all servers but sptr */
				 ":%s KILL %s :%s (%s <- %s)",
				 me.name, acptr->name, me.name,
				 acptr->from->name,
				 get_client_name(cptr, HIDE_IP));
#endif
#endif

              acptr->flags |= FLAGS_KILLED;
              (void)exit_client(cptr, acptr, &me, "Nick collision");
              nick_from_server(cptr,sptr,parc,parv,newts,nick);
              return;
            }
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

      kill_client_ll_serv_butone(NULL, sptr,
				 "%s (%s(%s) <- %s)",
				 me.name,
				 acptr->from->name,
				 acptr->name,
				 get_client_name(cptr, HIDE_IP));
#if 0
      sendto_ll_serv_butone(NULL, sptr, 0, /* KILL old from outgoing servers */
			 ":%s KILL %s :%s (%s(%s) <- %s)",
			 me.name, sptr->name, me.name, acptr->from->name,
			 acptr->name, get_client_name(cptr, HIDE_IP));
#endif
#endif

      ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
      /* If we got the message from a LL, ensure it gets the kill */
      if (ServerInfo.hub && IsCapable(cptr,CAP_LL))
        add_lazylinkclient(cptr, acptr);

      kill_client_ll_serv_butone(NULL, acptr,
				 "%s (%s <- %s(%s))",
				 me.name, 
				 acptr->from->name,
				 get_client_name(cptr, HIDE_IP),
				 sptr->name);
#if 0
      sendto_ll_serv_butone(NULL, acptr, 0, /* Kill new from incoming link */
			 ":%s KILL %s :%s (%s <- %s(%s))",
			 me.name, acptr->name, me.name, acptr->from->name,
			 get_client_name(cptr, HIDE_IP), sptr->name);
#endif
#endif

      acptr->flags |= FLAGS_KILLED;
      exit_client(NULL, acptr, &me, "Nick collision(new)");
      sptr->flags |= FLAGS_KILLED;
      exit_client(cptr, sptr, &me, "Nick collision(old)");
      return;
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

	  kill_client_ll_serv_butone(cptr, sptr,
				     /* KILL old from outgoing servers */
				     "%s (%s(%s) <- %s)",
				     me.name, acptr->from->name,
				     acptr->name,
				     get_client_name(cptr, HIDE_IP));
#if 0

	  sendto_ll_serv_butone(cptr, sptr, 0, /* KILL old from outgoing servers */
			     ":%s KILL %s :%s (%s(%s) <- %s)",
			     me.name, sptr->name, me.name, acptr->from->name,
			     acptr->name, get_client_name(cptr, HIDE_IP));
#endif
#endif

          sptr->flags |= FLAGS_KILLED;
          if (sameuser)
            exit_client(cptr, sptr, &me, "Nick collision(old)");
          else
            exit_client(cptr, sptr, &me, "Nick collision(new)");
          return;
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
           * matter if it's an LL
	   */
	  kill_client_ll_serv_butone(sptr, acptr,
				     "%s (%s <- %s)",
				     me.name,
				     acptr->from->name,
				     get_client_name(cptr, HIDE_IP));
#if 0
	  sendto_ll_serv_butone(sptr, acptr, 0, /* all servers but sptr */
			     ":%s KILL %s :%s (%s <- %s)",
			     me.name, acptr->name, me.name,
			     acptr->from->name,
			     get_client_name(cptr, HIDE_IP));
#endif
#endif

          ServerStats->is_kill++;
          sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                     me.name, acptr->name, acptr->name);
          acptr->flags |= FLAGS_KILLED;
          (void)exit_client(cptr, acptr, &me, "Nick collision");
        }
    }
  nick_from_server(cptr,sptr,parc,parv,newts,nick);
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
  if (IsServer(sptr))
    {
      /* A server introducing a new client, change source */
      
      sptr = make_client(cptr);
      add_client_to_list(sptr);         /* double linked list */

      /* We don't need to introduce leafs clients back to them! */
      if (ConfigFileEntry.hub && IsCapable(cptr, CAP_LL))
        add_lazylinkclient(cptr, sptr);
          
      if (parc > 2)
        sptr->hopcount = atoi(parv[2]);
      if (newts)
        sptr->tsinfo = newts;
      else
        {
          newts = sptr->tsinfo = CurrentTime;
          ts_warn("Remote nick %s (%s) introduced without a TS", nick, parv[0]);
        }
      /* copy the nick in place */
      (void)strcpy(sptr->name, nick);
      (void)add_to_client_hash_table(nick, sptr);
      if (parc > 8)
        {
          int   flag;
          char* m;
          
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
				parv[7], parv[8], NULL);
        }
    }
  else if (sptr->name[0])
    {
      /*
      ** Client just changing his/her nick. If he/she is
      ** on a channel, send note of change to all clients
      ** on that channel. Propagate notice to other servers.
      */
      if (irccmp(parv[0], nick))
        sptr->tsinfo = newts ? newts : CurrentTime;

          sendto_common_channels_local(sptr, ":%s!%s@%s NICK :%s",
				       sptr->name,sptr->username,sptr->host,
				       nick);
          if (sptr->user)
            {
              add_history(sptr,1);
              sendto_ll_serv_butone(cptr, sptr, 0, ":%s NICK %s :%lu",
                                    parv[0], nick, sptr->tsinfo);
            }
    }

  /*
  **  Finally set new nick name.
  */
  if (sptr->name[0])
    del_from_client_hash_table(sptr->name, sptr);
  strcpy(sptr->name, nick);
  add_to_client_hash_table(nick, sptr);

  /* Make sure everyone that has this client on its accept list
   * loses that reference. 
   */

  del_all_accepts(sptr);
  return 0;
}


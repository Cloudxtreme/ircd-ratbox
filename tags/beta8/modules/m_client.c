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
static void ms_client(struct Client*, struct Client*, int, char**);

struct Message client_msgtab = {
  "CLIENT", 0, 10, 0, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_client, m_ignore}
};

#ifndef STATIC_MODULES
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
#endif
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

static void ms_client(struct Client *client_p, struct Client *source_p,
                     int parc, char *parv[])
{
  struct Client* target_p;
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
      sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
                 me.name, BadPtr(parv[0]) ? "*" : parv[0], parv[1]);
      
      if (IsServer(client_p))
        {
          ServerStats->is_kill++;
          sendto_realops_flags(FLAGS_DEBUG, "Bad Nick: %s From: %s %s",
			       parv[1], parv[0],
			       get_client_name(client_p, HIDE_IP));
          sendto_one(client_p, ":%s KILL %s :%s (%s <- %s[%s])",
                     me.name, parv[1], me.name, parv[1],
                     nick, client_p->name);
          if (source_p != client_p) /* bad nick change */
            {
              sendto_server(client_p, source_p, NULL, NOCAPS, NOCAPS,
                            NOFLAGS, ":%s KILL %s :%s (%s <- %s!%s@%s)",
                            me.name, parv[0], me.name,
                            get_client_name(client_p, HIDE_IP),
                            parv[0],
                            source_p->username,
                            (source_p->user ? source_p->user->server :
                             client_p->name));
              source_p->flags |= FLAGS_KILLED;
              exit_client(client_p,source_p,&me,"BadNick");
              return;
            }
        }
      return;
    }
  /* Okay, we should be safe to cut off the username... -A1kmm */
  if (strlen(parv[5]) > USERLEN)
    {
     sendto_realops_flags(FLAGS_ALL, L_ALL, "Long username from server %s for %s",
                parv[0], parv[1]);
     parv[5][USERLEN] = 0;
    }
  /* Okay, we should be safe to cut off the hostname... -A1kmm */
  if (strlen(parv[6]) > HOSTLEN)
    {
     sendto_realops_flags(FLAGS_ALL, L_ALL, "Long hostname from server %s for %s",
                parv[0], parv[1]);
     parv[6][HOSTLEN] = 0;
    }
  /* Okay, we should be safe to cut off the realname... -A1kmm */
  if (strlen(name) > REALLEN)
    {
     sendto_realops_flags(FLAGS_ALL, L_ALL, "Long realname from server %s for %s",
                parv[0], parv[1]);
     name[REALLEN] = 0;
    }
  
  newts = atol(parv[3]);

  if (!(target_p = find_client(nick, NULL)))
  {
    nick_from_server(client_p,source_p,parc,parv,newts,nick);
    return;
  }
  
  /*
  ** Note: From this point forward it can be assumed that
  ** target_p != source_p (point to different client structures).
  */

  /*
  ** If the older one is "non-person", the new entry is just
  ** allowed to overwrite it. Just silently drop non-person,
  ** and proceed with the nick. This should take care of the
  ** "dormant nick" way of generating collisions...
  */
  if (IsUnknown(target_p)) 
   {
    if (MyConnect(target_p))
      {
        exit_client(NULL, target_p, &me, "Overridden");
        nick_from_server(client_p,source_p,parc,parv,newts,nick);
        return;
      }
    else
      {
        if (!target_p->user)
          {
            sendto_realops_flags(FLAGS_ALL, L_ALL,
                   "Nick Collision on %s(%s(NOUSER) <- %s!%s@%s)(TS:%s)",
                   target_p->name, target_p->from->name, parv[1], parv[5], parv[6],
                   client_p->name);

#ifndef LOCAL_NICK_COLLIDE
            /* If we got the message from a LL, ensure it gets the kill */
            if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
              add_lazylinkclient(client_p, target_p);
            
	    sendto_server(NULL, target_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                          ":%s KILL %s :%s (%s(NOUSER) <- %s!%s@%s)(TS:%s)",
                          me.name,
                          target_p->name,
                          me.name,
                          target_p->from->name,
                          parv[1],
                          parv[5],
                          parv[6],
                          client_p->name);
#endif

            target_p->flags |= FLAGS_KILLED;
            /* Having no USER struct should be ok... */
            exit_client(client_p, target_p, &me,
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
  ** This seemingly obscure test (source_p == client_p) differentiates
  ** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
  */
  /* 
  ** Changed to something reasonable like IsServer(source_p)
  ** (true if "NICK new", false if ":old NICK new") -orabidoo
  */

#ifdef LOCAL_NICK_COLLIDE
  /* XXX what does this code do?
       * This is a new nick that's being introduced, not a nickchange
       */
      /* just propogate it through */
      sendto_ll_serv_butone(client_p, source_p, 0, ":%s NICK %s :%lu",
                         parv[0], nick, source_p->tsinfo);
#endif
      /*
      ** A new NICK being introduced by a neighbouring
      ** server (e.g. message type "NICK new" received)
      */
      if (!newts || !target_p->tsinfo
          || (newts == target_p->tsinfo))
	  {
          sendto_realops_flags(FLAGS_ALL, L_ALL,
			   "Nick collision on %s(%s <- %s)(both killed)",
			   target_p->name, target_p->from->name,
			   get_client_name(client_p, HIDE_IP));
		  
#ifndef LOCAL_NICK_COLLIDE
          /* If we got the message from a LL, ensure it gets the kill */
          if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
			  add_lazylinkclient(client_p, target_p);

          sendto_server(NULL, target_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                        ":%s KILL %s :%s (%s <- %s)",
                        me.name, target_p->name, me.name,
                        target_p->from->name,
                        /* NOTE: Cannot use get_client_name twice
                         ** here, it returns static string pointer:
                         ** the other info would be lost
                         */
                        get_client_name(client_p, HIDE_IP));
#endif
          ServerStats->is_kill++;
          sendto_one(target_p, form_str(ERR_NICKCOLLISION),
                     me.name, target_p->name, target_p->name);
		  
          target_p->flags |= FLAGS_KILLED;
          exit_client(client_p, target_p, &me, "Nick collision");
          return;
        }
      else
        {
          sameuser =  target_p->user &&
            irccmp(target_p->username, parv[5]) == 0 &&
            irccmp(target_p->host, parv[6]) == 0;
          if ((sameuser && newts < target_p->tsinfo) ||
              (!sameuser && newts > target_p->tsinfo))
            {
              /* We don't need to kill the user, the other end does */
              client_burst_if_needed(client_p, target_p);
              return;
            }
          else
            {
              if (sameuser)
                sendto_realops_flags(FLAGS_ALL, L_ALL,
                       "Nick collision on %s(%s <- %s)(older killed)",
		       target_p->name, target_p->from->name,
		       get_client_name(client_p, HIDE_IP));
              else
                sendto_realops_flags(FLAGS_ALL, L_ALL,
			     "Nick collision on %s(%s <- %s)(newer killed)",
			     target_p->name, target_p->from->name,
			     get_client_name(client_p, HIDE_IP));
              
              ServerStats->is_kill++;
              sendto_one(target_p, form_str(ERR_NICKCOLLISION),
                         me.name, target_p->name, target_p->name);

#ifndef LOCAL_NICK_COLLIDE
              /* If it came from a LL server, it'd have been source_p,
               * so we don't need to mark target_p as known */
	      sendto_server(source_p, target_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                            ":%s KILL %s :%s (%s <- %s)",
                            me.name, target_p->name, me.name,
                            target_p->from->name,
                            get_client_name(client_p, HIDE_IP));
#endif

              target_p->flags |= FLAGS_KILLED;
              (void)exit_client(client_p, target_p, &me, "Nick collision");
              nick_from_server(client_p,source_p,parc,parv,newts,nick);
              return;
            }
        }
  /*
  ** A NICK change has collided (e.g. message type
  ** ":old NICK new". This requires more complex cleanout.
  ** Both clients must be purged from this server, the "new"
  ** must be killed from the incoming connection, and "old" must
  ** be purged from all outgoing connections.
  */
  if ( !newts || !target_p->tsinfo || (newts == target_p->tsinfo) ||
      !source_p->user)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
	   "Nick change collision from %s to %s(%s <- %s)(both killed)",
           source_p->name, target_p->name, target_p->from->name,
	   get_client_name(client_p, HIDE_IP));
      ServerStats->is_kill++;
      sendto_one(target_p, form_str(ERR_NICKCOLLISION),
                 me.name, target_p->name, target_p->name);

#ifndef LOCAL_NICK_COLLIDE
      /* If we got the message from a LL, it would know
         about source_p already */
      sendto_server(NULL, source_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                    ":%s KILL %s :%s (%s(%s) <- %s)",
                    me.name, source_p->name, me.name, target_p->from->name,
                    target_p->name, get_client_name(client_p, HIDE_IP));
#endif

      ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
      /* If we got the message from a LL, ensure it gets the kill */
      if (ServerInfo.hub && IsCapable(client_p,CAP_LL))
        add_lazylinkclient(client_p, target_p);

      sendto_server(NULL, target_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                    ":%s KILL %s :%s (%s <- %s(%s))",
                    me.name, target_p->name, me.name, target_p->from->name,
                    get_client_name(client_p, HIDE_IP), source_p->name);
#endif

      target_p->flags |= FLAGS_KILLED;
      exit_client(NULL, target_p, &me, "Nick collision(new)");
      source_p->flags |= FLAGS_KILLED;
      exit_client(client_p, source_p, &me, "Nick collision(old)");
      return;
    }
  else
    {
      sameuser = irccmp(target_p->username, source_p->username) == 0 &&
                 irccmp(target_p->host, source_p->host) == 0;
      if ((sameuser && newts < target_p->tsinfo) ||
          (!sameuser && newts > target_p->tsinfo))
        {
          if (sameuser)
            sendto_realops_flags(FLAGS_ALL, L_ALL,
                 "Nick change collision from %s to %s(%s <- %s)(older killed)",
                 source_p->name, target_p->name, target_p->from->name,
                 get_client_name(client_p, HIDE_IP));
          else
            sendto_realops_flags(FLAGS_ALL, L_ALL,
                 "Nick change collision from %s to %s(%s <- %s)(newer killed)",
                 source_p->name, target_p->name, target_p->from->name,
                 get_client_name(client_p, HIDE_IP));

          ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
          /* this won't go back to the incoming link, so it doesn't
           * matter if it is an LL */
          /* KILL old from outgoing servers */
	  sendto_server(client_p, source_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                        ":%s KILL %s :%s (%s(%s) <- %s)",
                        me.name, source_p->name, me.name, target_p->from->name,
                        target_p->name, get_client_name(client_p, HIDE_IP));
#endif

          source_p->flags |= FLAGS_KILLED;
          if (sameuser)
            exit_client(client_p, source_p, &me, "Nick collision(old)");
          else
            exit_client(client_p, source_p, &me, "Nick collision(new)");
          return;
        }
      else
        {
          if (sameuser)
            sendto_realops_flags(FLAGS_ALL, L_ALL,
                               "Nick collision on %s(%s <- %s)(older killed)",
                               target_p->name, target_p->from->name,
                               get_client_name(client_p, HIDE_IP));
          else
            sendto_realops_flags(FLAGS_ALL, L_ALL,
                         "Nick collision on %s(%s <- %s)(newer killed)",
                         target_p->name, target_p->from->name,
                         get_client_name(client_p, HIDE_IP));
          
#ifndef LOCAL_NICK_COLLIDE
          /* this won't go back to the incoming link, so it doesn't
           * matter if it's an LL */
	  sendto_server(source_p, target_p, NULL, NOCAPS, NOCAPS, NOFLAGS,
                        ":%s KILL %s :%s (%s <- %s)",
                        me.name, target_p->name, me.name,
                        target_p->from->name,
                        get_client_name(client_p, HIDE_IP));
#endif

          ServerStats->is_kill++;
          sendto_one(target_p, form_str(ERR_NICKCOLLISION),
                     me.name, target_p->name, target_p->name);
          target_p->flags |= FLAGS_KILLED;
          (void)exit_client(client_p, target_p, &me, "Nick collision");
        }
    }
  nick_from_server(client_p,source_p,parc,parv,newts,nick);
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
nick_from_server(struct Client *client_p, struct Client *source_p, int parc,
                 char *parv[], time_t newts,char *nick)
{
  char *name;
  char *id;
  int   flag;
  char* m;
	
  id = parv[8];
  name = parv[9];
  
  source_p = make_client(client_p);
  add_client_to_list(source_p);         /* double linked list */

  /* We don't need to introduce leafs clients back to them! */
  if (ConfigFileEntry.hub && IsCapable(client_p, CAP_LL))
    add_lazylinkclient(client_p, source_p);

  source_p->hopcount = atoi(parv[2]);
  source_p->tsinfo = newts;

  /* copy the nick in place */
  (void)strcpy(source_p->name, nick);
  (void)add_to_client_hash_table(nick, source_p);
  add_to_id_hash_table(id, source_p);

  /*
  ** parse the usermodes -orabidoo
  */
  m = &parv[4][1];
  while (*m)
    {
      flag = user_modes_from_c_to_bitmask[(unsigned char)*m];
      if(!(source_p->umodes & FLAGS_INVISIBLE) && (flag & FLAGS_INVISIBLE))
	{
	  Count.invisi++;
	}
      if(!(source_p->umodes & FLAGS_OPER) && (flag & FLAGS_OPER))
	{
	  Count.oper++;
	}
      source_p->umodes |= flag & SEND_UMODES;
      m++;
    }
	
  return do_remote_user(nick, client_p, source_p, parv[5], parv[6],
			parv[7], name, id);
}


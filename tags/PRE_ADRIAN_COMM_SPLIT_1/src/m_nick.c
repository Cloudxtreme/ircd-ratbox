/************************************************************************
 *   IRC - Internet Relay Chat, src/m_nick.c
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
/*
 * m_functions execute protocol messages on this server:
 *
 *      cptr    is always NON-NULL, pointing to a *LOCAL* client
 *              structure (with an open socket connected!). This
 *              identifies the physical socket where the message
 *              originated (or which caused the m_function to be
 *              executed--some m_functions may call others...).
 *
 *      sptr    is the source of the message, defined by the
 *              prefix part of the message if present. If not
 *              or prefix not found, then sptr==cptr.
 *
 *              (!IsServer(cptr)) => (cptr == sptr), because
 *              prefixes are taken *only* from servers...
 *
 *              (IsServer(cptr))
 *                      (sptr == cptr) => the message didn't
 *                      have the prefix.
 *
 *                      (sptr != cptr && IsServer(sptr) means
 *                      the prefix specified servername. (?)
 *
 *                      (sptr != cptr && !IsServer(sptr) means
 *                      that message originated from a remote
 *                      user (not local).
 *
 *              combining
 *
 *              (!IsServer(sptr)) means that, sptr can safely
 *              taken as defining the target structure of the
 *              message in this server.
 *
 *      *Always* true (if 'parse' and others are working correct):
 *
 *      1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *      2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *              *cannot* be a local connection, unless it's
 *              actually cptr!). [MyConnect(x) should probably
 *              be defined as (x == x->from) --msa ]
 *
 *      parc    number of variable parameter strings (if zero,
 *              parv is allowed to be NULL)
 *
 *      parv    a NULL terminated list of parameter pointers,
 *
 *                      parv[0], sender (prefix string), if not present
 *                              this points to an empty string.
 *                      parv[1]...parv[parc-1]
 *                              pointers to additional parameters
 *                      parv[parc] == NULL, *always*
 *
 *              note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                      non-NULL pointers.
 */

static int nickkilldone(struct Client *, struct Client *, int, char **, time_t, char *);
static int clean_nick_name(char* nick);

/*
** m_nick
**      parv[0] = sender prefix
**      parv[1] = nickname
**      parv[2] = optional hopcount when new user; TS when nick change
**      parv[3] = optional TS
**      parv[4] = optional umode
**      parv[5] = optional username
**      parv[6] = optional hostname
**      parv[7] = optional server
**      parv[8] = optional ircname
*/
int m_nick(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client* acptr;
  char     nick[NICKLEN + 2];
  char*    s;
  time_t   newts = 0;
  int      sameuser = 0;
  int      fromTS = 0;

  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  if (!IsServer(sptr) && IsServer(cptr) && parc > 2)
    newts = atol(parv[2]);
  else if (IsServer(sptr) && parc > 3)
    newts = atol(parv[3]);
  else parc = 2;

  /*
   * parc == 2 on a normal client sign on (local) and a normal
   *      client nick change
   * parc == 4 on a normal server-to-server client nick change
   *      notice
   * parc == 9 on a normal TS style server-to-server NICK
   *      introduction
   */
  if (IsServer(sptr) && (parc < 9))
    {
      /*
       * We got the wrong number of params. Someone is trying
       * to trick us. Kill it. -ThemBones
       * As discussed with ThemBones, not much point to this code now
       * sending a whack of global kills would also be more annoying
       * then its worth, just note the problem, and continue
       * -Dianora
       */
      ts_warn("BAD NICK: %s[%s@%s] on %s (from %s)", parv[1],
                     (parc >= 6) ? parv[5] : "-",
                     (parc >= 7) ? parv[6] : "-",
                     (parc >= 8) ? parv[7] : "-", parv[0]);
      return 0;
     }

  if ((parc >= 7) && (!strchr(parv[6], '.')))
    {
      /*
       * Ok, we got the right number of params, but there
       * isn't a single dot in the hostname, which is suspicious.
       * Don't fret about it just kill it. - ThemBones
       */
      ts_warn("BAD HOSTNAME: %s[%s@%s] on %s (from %s)",
                     parv[0], parv[5], parv[6], parv[7], parv[0]);
      return 0;
    }

  fromTS = (parc > 6);

  /*
   * XXX - ok, we terminate the nick with the first '~' ?  hmmm..
   *  later we allow them? isvalid used to think '~'s were ok
   *  IsNickChar allows them as well
   */
  if (MyConnect(sptr) && (s = strchr(parv[1], '~')))
    *s = '\0';
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
  if (clean_nick_name(nick) == 0 ||
      (IsServer(cptr) && strcmp(nick, parv[1])))
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
              sendto_serv_butone(cptr,
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

  if (MyConnect(sptr) && !IsServer(sptr) && !IsAnOper(sptr) &&
     find_q_line(nick, sptr->username, sptr->host)) 
    {
      sendto_realops_flags(FLAGS_REJ,
                         "Quarantined nick [%s] from user %s",
                         nick,get_client_name(cptr, HIDE_IP));
      sendto_one(sptr, form_str(ERR_ERRONEUSNICKNAME),
		 me.name, parv[0], parv[1]);
      return 0;
    }

  /* if quiet_on_ban is set, don't allow nick changes if the user
   * is banned from a channel.
   */

  if (ConfigFileEntry.quiet_on_ban && !IsServer(sptr) && sptr->user) {
    struct SLink *tmp = sptr->user->channel;
    while (tmp) {
      if (is_banned(sptr, tmp->value.chptr)) {
        sendto_one(sptr, form_str(ERR_BANNEDNICK), me.name, BadPtr(parv[0]) ? "*" : parv[0], tmp->value.chptr->chname);
        return 0; /* NICK message ignored */
      }
      tmp = tmp->next;
    }
  }

  /*
  ** Check against nick name collisions.
  **
  ** Put this 'if' here so that the nesting goes nicely on the screen :)
  ** We check against server name list before determining if the nickname
  ** is present in the nicklist (due to the way the below for loop is
  ** constructed). -avalon
  */
  if ((acptr = find_server(nick)))
    if (MyConnect(sptr))
      {
        sendto_one(sptr, form_str(ERR_NICKNAMEINUSE), me.name,
                   BadPtr(parv[0]) ? "*" : parv[0], nick);
        return 0; /* NICK message ignored */
      }
  /*
  ** acptr already has result from previous find_server()
  */
  /*
   * Well. unless we have a capricious server on the net,
   * a nick can never be the same as a server name - Dianora
   */

  if (acptr)
    {
      /*
      ** We have a nickname trying to use the same name as
      ** a server. Send out a nick collision KILL to remove
      ** the nickname. As long as only a KILL is sent out,
      ** there is no danger of the server being disconnected.
      ** Ultimate way to jupiter a nick ? >;-). -avalon
      */
      sendto_ops("Nick collision on %s(%s <- %s)",
                 sptr->name, acptr->from->name,
                 get_client_name(cptr, HIDE_IP));
      ServerStats->is_kill++;
      sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
                 me.name, sptr->name, me.name, acptr->from->name,
                 /* NOTE: Cannot use get_client_name
                 ** twice here, it returns static
                 ** string pointer--the other info
                 ** would be lost
                 */
                 get_client_name(cptr, HIDE_IP));
      sptr->flags |= FLAGS_KILLED;
      return exit_client(cptr, sptr, &me, "Nick/Server collision");
    }
  

  if (!(acptr = find_client(nick, NULL)))
    return(nickkilldone(cptr,sptr,parc,parv,newts,nick));
                                                      /* No collisions,
                                                       * all clear...
                                                       */

  /*
  ** If acptr == sptr, then we have a client doing a nick
  ** change between *equivalent* nicknames as far as server
  ** is concerned (user is changing the case of his/her
  ** nickname or somesuch)
  */
  if (acptr == sptr)
   {
    if (strcmp(acptr->name, nick) != 0)
      /*
      ** Allows change of case in his/her nick
      */
      return(nickkilldone(cptr,sptr,parc,parv,newts,nick)); /* -- go and process change */
    else
      {
        /*
        ** This is just ':old NICK old' type thing.
        ** Just forget the whole thing here. There is
        ** no point forwarding it to anywhere,
        ** especially since servers prior to this
        ** version would treat it as nick collision.
        */
        return 0; /* NICK Message ignored */
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
        return(nickkilldone(cptr,sptr,parc,parv,newts,nick));
      }
    else
      {
        if (fromTS && !(acptr->user))
          {
            sendto_ops("Nick Collision on %s(%s(NOUSER) <- %s!%s@%s)(TS:%s)",
                   acptr->name, acptr->from->name, parv[1], parv[5], parv[6],
                   cptr->name);

#ifndef LOCAL_NICK_COLLIDE
	    sendto_serv_butone(NULL, /* all servers */
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
  ** Decide, we really have a nick collision and deal with it
  */
  if (!IsServer(cptr))
    {
      /*
      ** NICK is coming from local client connection. Just
      ** send error reply and ignore the command.
      */
      sendto_one(sptr, form_str(ERR_NICKNAMEINUSE),
                 /* parv[0] is empty when connecting */
                 me.name, BadPtr(parv[0]) ? "*" : parv[0], nick);
      return 0; /* NICK message ignored */
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
      /* As discussed with chris (comstud) nick kills can
       * be handled locally, provided all NICK's are propogated
       * globally. Just like channel joins are handled.
       *
       * I think I got this right.
       * -Dianora
       * There are problems with this, due to lag it looks like.
       * backed out for now...
       */

#ifdef LOCAL_NICK_COLLIDE
      /* just propogate it through */
      sendto_serv_butone(cptr, ":%s NICK %s :%lu",
                         parv[0], nick, sptr->tsinfo);
#endif
      /*
      ** A new NICK being introduced by a neighbouring
      ** server (e.g. message type "NICK new" received)
      */
      if (!newts || !acptr->tsinfo
          || (newts == acptr->tsinfo))
        {
          sendto_ops("Nick collision on %s(%s <- %s)(both killed)",
                     acptr->name, acptr->from->name,
                     get_client_name(cptr, HIDE_IP));

#ifndef LOCAL_NICK_COLLIDE
	  sendto_serv_butone(NULL, /* all servers */
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
          sameuser =  fromTS && (acptr->user) &&
            irccmp(acptr->username, parv[5]) == 0 &&
            irccmp(acptr->host, parv[6]) == 0;
          if ((sameuser && newts < acptr->tsinfo) ||
              (!sameuser && newts > acptr->tsinfo))
            return 0;
          else
            {
              if (sameuser)
                sendto_ops("Nick collision on %s(%s <- %s)(older killed)",
                           acptr->name, acptr->from->name,
                           get_client_name(cptr, HIDE_IP));
              else
                sendto_ops("Nick collision on %s(%s <- %s)(newer killed)",
                           acptr->name, acptr->from->name,
                           get_client_name(cptr, HIDE_IP));
              
              ServerStats->is_kill++;
              sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                         me.name, acptr->name, acptr->name);

#ifndef LOCAL_NICK_COLLIDE
	      sendto_serv_butone(sptr, /* all servers but sptr */
				 ":%s KILL %s :%s (%s <- %s)",
				 me.name, acptr->name, me.name,
				 acptr->from->name,
				 get_client_name(cptr, HIDE_IP));
#endif

              acptr->flags |= FLAGS_KILLED;
              (void)exit_client(cptr, acptr, &me, "Nick collision");
              return nickkilldone(cptr,sptr,parc,parv,newts,nick);
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
      sendto_ops("Nick change collision from %s to %s(%s <- %s)(both killed)",
                 sptr->name, acptr->name, acptr->from->name,
                 get_client_name(cptr, HIDE_IP));
      ServerStats->is_kill++;
      sendto_one(acptr, form_str(ERR_NICKCOLLISION),
                 me.name, acptr->name, acptr->name);

#ifndef LOCAL_NICK_COLLIDE
      sendto_serv_butone(NULL, /* KILL old from outgoing servers */
			 ":%s KILL %s :%s (%s(%s) <- %s)",
			 me.name, sptr->name, me.name, acptr->from->name,
			 acptr->name, get_client_name(cptr, HIDE_IP));
#endif

      ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
      sendto_serv_butone(NULL, /* Kill new from incoming link */
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
            sendto_ops("Nick change collision from %s to %s(%s <- %s)(older killed)",
                       sptr->name, acptr->name, acptr->from->name,
                       get_client_name(cptr, HIDE_IP));
          else
            sendto_ops("Nick change collision from %s to %s(%s <- %s)(newer killed)",
                       sptr->name, acptr->name, acptr->from->name,
                       get_client_name(cptr, HIDE_IP));

          ServerStats->is_kill++;

#ifndef LOCAL_NICK_COLLIDE
	  sendto_serv_butone(cptr, /* KILL old from outgoing servers */
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
            sendto_ops("Nick collision on %s(%s <- %s)(older killed)",
                       acptr->name, acptr->from->name,
                       get_client_name(cptr, HIDE_IP));
          else
            sendto_ops("Nick collision on %s(%s <- %s)(newer killed)",
                       acptr->name, acptr->from->name,
                       get_client_name(cptr, HIDE_IP));
          
#ifndef LOCAL_NICK_COLLIDE
	  sendto_serv_butone(sptr, /* all servers but sptr */
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
  return(nickkilldone(cptr,sptr,parc,parv,newts,nick));
}

/*
 * nickkilldone
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

static int nickkilldone(struct Client *cptr, struct Client *sptr, int parc,
                 char *parv[], time_t newts,char *nick)
{

  if (IsServer(sptr))
    {
      /* A server introducing a new client, change source */
      
      sptr = make_client(cptr);
      add_client_to_list(sptr);         /* double linked list */
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
          
          return do_user(nick, cptr, sptr, parv[5], parv[6],
                         parv[7], parv[8]);
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

      if(MyConnect(sptr) && IsRegisteredUser(sptr))
        {     
          if( (sptr->last_nick_change + ConfigFileEntry.max_nick_time) < CurrentTime)
            sptr->number_of_nick_changes = 0;
          sptr->last_nick_change = CurrentTime;
          sptr->number_of_nick_changes++;

          if((ConfigFileEntry.anti_nick_flood && 
             (sptr->number_of_nick_changes <= ConfigFileEntry.max_nick_changes)) ||
             !ConfigFileEntry.anti_nick_flood)
            {
              sendto_realops_flags(FLAGS_NCHANGE,
                                 "Nick change: From %s to %s [%s@%s]",
                                 parv[0], nick, sptr->username,
                                 sptr->host);

              sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
              if (sptr->user)
                {
                  add_history(sptr,1);
              
                  sendto_serv_butone(cptr, ":%s NICK %s :%lu",
                                     parv[0], nick, sptr->tsinfo);
                }
            }
          else
            {
              sendto_one(sptr,
                         ":%s NOTICE %s :*** Notice -- Too many nick changes wait %d seconds before trying to change it again.",
                         me.name,
                         sptr->name,
                         ConfigFileEntry.max_nick_time);
              return 0;
            }
        }
      else
        {
          sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
          if (sptr->user)
            {
              add_history(sptr,1);
              sendto_serv_butone(cptr, ":%s NICK %s :%lu",
                                 parv[0], nick, sptr->tsinfo);
            }
        }
    }
  else
    {
      /* Client setting NICK the first time */
      

      /* This had to be copied here to avoid problems.. */
      strcpy(sptr->name, nick);
      sptr->tsinfo = CurrentTime;
      if (sptr->user)
        {
          char buf[USERLEN + 1];
          strncpy_irc(buf, sptr->username, USERLEN);
          buf[USERLEN] = '\0';
          /*
          ** USER already received, now we have NICK.
          ** *NOTE* For servers "NICK" *must* precede the
          ** user message (giving USER before NICK is possible
          ** only for local client connection!). register_user
          ** may reject the client and call exit_client for it
          ** --must test this and exit m_nick too!!!
          */
#ifdef USE_IAUTH
          /*
           * Send the client to the iauth module for verification
           */
            BeginAuthorization(sptr);
#else
            if (register_user(cptr, sptr, nick, buf) == CLIENT_EXITED)
              return CLIENT_EXITED;
#endif
        }
    }

  /*
  **  Finally set new nick name.
  */
  if (sptr->name[0])
    del_from_client_hash_table(sptr->name, sptr);
  strcpy(sptr->name, nick);
  add_to_client_hash_table(nick, sptr);

  return 0;
}

/*
 * clean_nick_name - ensures that the given parameter (nick) is
 * really a proper string for a nickname (note, the 'nick'
 * may be modified in the process...)
 *
 *      RETURNS the length of the final NICKNAME (0, if
 *      nickname is illegal)
 *
 *  Nickname characters are in range
 *      'A'..'}', '_', '-', '0'..'9'
 *  anything outside the above set will terminate nickname.
 *  In addition, the first character cannot be '-'
 *  or a Digit.
 *
 *  Note:
 *      '~'-character should be allowed, but
 *      a change should be global, some confusion would
 *      result if only few servers allowed it...
 */
static int clean_nick_name(char* nick)
{
  char* ch   = nick;
  char* endp = ch + NICKLEN;
  assert(0 != nick);

  if (*nick == '-' || IsDigit(*nick)) /* first character in [0..9-] */
    return 0;
  
  for ( ; ch < endp && *ch; ++ch) {
    if (!IsNickChar(*ch))
      break;
  }
  *ch = '\0';

  return (ch - nick);
}

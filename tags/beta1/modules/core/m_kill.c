/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_kill.c
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
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_serv.h"
#include "s_conf.h"
#include "send.h"
#include "whowas.h"
#include "irc_string.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>

static char buf[BUFSIZE];

static int ms_kill(struct Client*, struct Client*, int, char**);
static int mo_kill(struct Client*, struct Client*, int, char**);
static void relay_kill(struct Client *, struct Client *, struct Client *,
                       const char *, const char *);

struct Message kill_msgtab = {
  "KILL", 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, ms_kill, mo_kill}
};

void
_modinit(void)
{
  mod_add_cmd(&kill_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&kill_msgtab);
}

char *_version = "20001122";

/*
** mo_kill
**      parv[0] = sender prefix
**      parv[1] = kill victim
**      parv[2] = kill path
*/
int mo_kill(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client*    acptr;
  const char* inpath = cptr->name;
  char*       user;
  char*       reason;

  user = parv[1];
  reason = parv[2]; /* Either defined or NULL (parc >= 2!!) */

  if (!IsSetOperK(sptr))
    {
      sendto_one(sptr,":%s NOTICE %s :You have no K flag",me.name,parv[0]);
      return 0;
    }

  if (!BadPtr(reason))
    {
      if(strlen(reason) > (size_t) KILLLEN)
	reason[KILLLEN] = '\0';
    }
  else
    reason = "<No reason given>";

  if (!(acptr = find_client(user, NULL)))
    {
      /*
      ** If the user has recently changed nick, automatically
      ** rewrite the KILL for this new nickname--this keeps
      ** servers in synch when nick change and kill collide
      */
      if (!(acptr = get_history(user, (long)KILLCHASETIMELIMIT)))
        {
          sendto_one(sptr, form_str(ERR_NOSUCHNICK),
                     me.name, parv[0], user);
          return 0;
        }
      sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
                 me.name, parv[0], user, acptr->name);
    }
  if (IsServer(acptr) || IsMe(acptr))
    {
      sendto_one(sptr, form_str(ERR_CANTKILLSERVER),
                 me.name, parv[0]);
      return 0;
    }

  if (!MyConnect(acptr) && (!IsOperGlobalKill(sptr)))
    {
      sendto_one(sptr, ":%s NOTICE %s :Nick %s isnt on your server",
                 me.name, parv[0], acptr->name);
      return 0;
    }

  /*
  ** The kill originates from this server
  **
  **        ...!operhost!oper
  **        ...!operhost!oper (comment)
  */
  ircsprintf(buf, "%s!%s (%s)",
	     inpath, cptr->username, reason);

  sendto_realops_flags(FLAGS_ALL,
		       "Received KILL message for %s. From %s Path: %s (%s)", 
		       acptr->name, parv[0], me.name, reason);
  log(L_INFO,"KILL From %s For %s Path %s ",
      parv[0], acptr->name, buf );

  /*
  ** And pass on the message to other servers. Note, that if KILL
  ** was changed, the message has to be sent to all links, also
  ** back.
  ** Suicide kills are NOT passed on --SRB
  */
  if (!MyConnect(acptr))
    {
      relay_kill(cptr, sptr, acptr, inpath, reason);
      /*
      ** Set FLAGS_KILLED. This prevents exit_one_client from sending
      ** the unnecessary QUIT for this. (This flag should never be
      ** set in any other place)
      */
      acptr->flags |= FLAGS_KILLED;
    }

  return exit_client(cptr, acptr, sptr, reason);
}

/*
** ms_kill
**      parv[0] = sender prefix
**      parv[1] = kill victim
**      parv[2] = kill path
*/
int ms_kill(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client*    acptr;
  const char* inpath = cptr->name;
  char*       user;
  char*       path;
  char*       reason;
  int         chasing = 0;

  if (*parv[1] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "KILL");
      return 0;
    }

  user = parv[1];
  path = parv[2]; /* Either defined or NULL (parc >= 2!!) */

  if (!(acptr = find_client(user, NULL)))
    {
      /*
       * If the user has recently changed nick, but only if its 
       * not an uid, automatically rewrite the KILL for this new nickname.
       * --this keeps servers in synch when nick change and kill collide
       */
      if( (*user == '.')  ||
	  (!(acptr = get_history(user, (long)KILLCHASETIMELIMIT))))
        {
          sendto_one(sptr, form_str(ERR_NOSUCHNICK),
                     me.name, parv[0], user);
          return 0;
        }
      sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
                 me.name, parv[0], user, acptr->name);
      chasing = 1;
    }
  if (IsServer(acptr) || IsMe(acptr))
    {
      sendto_one(sptr, form_str(ERR_CANTKILLSERVER),
                 me.name, parv[0]);
      return 0;
    }

  if (BadPtr(path))
    path = "*no-path*"; /* Bogus server sending??? */

  /*
  ** Notify all *local* opers about the KILL (this includes the one
  ** originating the kill, if from this server--the special numeric
  ** reply message is not generated anymore).
  **
  ** Note: "acptr->name" is used instead of "user" because we may
  **     have changed the target because of the nickname change.
  */
  if(BadPtr(parv[2]))
      reason = "<No reason given>";
  else
    {
      reason = strchr(parv[2], ' ');
      if(reason)
        reason++;
      else
        reason = parv[2];
    }

  if (IsOper(sptr)) /* send it normally */
    {
      sendto_realops_flags(FLAGS_ALL,
			   "Received KILL message for %s. From %s Path: %s %s",
			   acptr->name, parv[0], sptr->user->server, reason);
    }
  else
    {
      sendto_realops_flags(FLAGS_SKILL,
			   "Received KILL message for %s %s. From %s",
			   acptr->name, parv[0], reason);
    }

  log(L_INFO,"KILL From %s For %s Path %s!%s (%s)",
      parv[0], acptr->name, inpath, path, reason);
  /*
  ** And pass on the message to other servers. Note, that if KILL
  ** was changed, the message has to be sent to all links, also
  ** back.
  ** Suicide kills are NOT passed on --SRB
  */

  if (!MyConnect(acptr) || !MyConnect(sptr) || !IsOper(sptr))
    {
      relay_kill(cptr, sptr, acptr, inpath, reason);

      /*
      ** Set FLAGS_KILLED. This prevents exit_one_client from sending
      ** the unnecessary QUIT for this. (This flag should never be
      ** set in any other place)
      */
      acptr->flags |= FLAGS_KILLED;
    }

  return exit_client(cptr, acptr, sptr, reason );
}

static void relay_kill(struct Client *one, struct Client *sptr,
                       struct Client *acptr,
                       const char *inpath,
		       const char *reason)
{
  dlink_node *ptr;
  struct Client *cptr;
  int introduce_killed_client;
  char* user; 
  
  /* LazyLinks:
   * Check if each lazylink knows about acptr.
   *   If it does, send the kill, introducing sptr if required.
   *   If it doesn't either:
   *     a) don't send the kill (risk ghosts)
   *     b) introduce the client (and sptr, if required)
   *        [rather redundant]
   *
   * Use a) if IsServer(sptr), but if an oper kills someone,
   * ensure we blow away any ghosts.
   *
   * -davidt
   */

  if(IsServer(sptr))
    introduce_killed_client = 0;
  else
    introduce_killed_client = 1;

  for( ptr = serv_list.head; ptr; ptr = ptr->next )
  {
    cptr = (struct Client *) ptr->data;
    
    if( !cptr || cptr == one )
      continue;

    if( !introduce_killed_client )
    {
      if( ServerInfo.hub && IsCapable(cptr, CAP_LL) )
      {
        if(((cptr->localClient->serverMask &
             acptr->lazyLinkClientExists) == 0))
        {
          /* target isn't known to lazy leaf, skip it */
          continue;
        }
      }
    }
    /* force introduction of killed client but check that
     * its not on the server we're bursting too.. */
    else if(strcmp(acptr->user->server,cptr->name))
      client_burst_if_needed(cptr, acptr);

    /* introduce source of kill */
    client_burst_if_needed(cptr, sptr);

    /* check the server supports UID */
    if (IsCapable(cptr, CAP_UID))
      user = ID(acptr);
    else
      user = acptr->name;

    if(MyConnect(sptr))
    {
      sendto_one(cptr, ":%s KILL %s :%s!%s!%s!%s %s",
                 sptr->name, user,
                 me.name, sptr->host, sptr->username,
                 sptr->name, reason);
    }
    else
    {
      sendto_one(cptr, ":%s KILL %s :%s!%s %s",
                 sptr->name, user, me.name,
                 inpath, reason);
    }
  }
}


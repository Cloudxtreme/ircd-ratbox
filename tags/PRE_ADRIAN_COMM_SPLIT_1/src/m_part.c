/************************************************************************
 *   IRC - Internet Relay Chat, src/m_part.c
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
#include "channel.h"
#include "vchannel.h"
#include "client.h"
#include "common.h"   /* bleah */
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"

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

/*
** m_part
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int     m_part(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel *chptr;
  struct Channel *vchan;
  char  *p, *name;

  name = strtoken( &p, parv[1], ",");

#ifdef ANTI_SPAMBOT     /* Dianora */
      /* if its my client, and isn't an oper */

      if (name && MyConnect(sptr))
        {
          if(GlobalSetOptions.spam_num &&
	     (sptr->join_leave_count >= GlobalSetOptions.spam_num))
            {
              sendto_ops_flags(FLAGS_BOTS,
                               "User %s (%s@%s) is a possible spambot",
                               sptr->name,
                               sptr->username, sptr->host);
              sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
            }
          else
            {
              int t_delta;

              if( (t_delta = (CurrentTime - sptr->last_leave_time)) >
                  JOIN_LEAVE_COUNT_EXPIRE_TIME)
                {
                  int decrement_count;
                  decrement_count = (t_delta/JOIN_LEAVE_COUNT_EXPIRE_TIME);

                  if(decrement_count > sptr->join_leave_count)
                    sptr->join_leave_count = 0;
                  else
                    sptr->join_leave_count -= decrement_count;
                }
              else
                {
                  if( (CurrentTime - (sptr->last_join_time)) < 
		      GlobalSetOptions.spam_time)
                    {
                      /* oh, its a possible spambot */
                      sptr->join_leave_count++;
                    }
                }
              sptr->last_leave_time = CurrentTime;
            }
        }
#endif

  while ( name )
    {
      chptr = get_channel(sptr, name, 0);
      if (!chptr)
        {
          sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                     me.name, parv[0], name);
          name = strtoken(&p, (char *)NULL, ",");
          continue;
        }

      if (HasVchans(chptr))
	{
	  vchan = map_vchan(chptr,sptr);
	  if(vchan == 0)
	    {
	      if (!IsMember(sptr, chptr))
		{
		  sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
			     me.name, parv[0], name);
		  name = strtoken(&p, (char *)NULL, ",");
		  continue;
		}
	      /*
	      **  Remove user from the old channel (if any)
	      */
            
	      sendto_match_servs(chptr, cptr, ":%s PART %s", parv[0], name);
            
	      sendto_channel_butserv(chptr, sptr, ":%s PART %s", parv[0],
				     name);
	      remove_user_from_channel(sptr, chptr, 0);
	    }
	  else
	    {
	      if (!IsMember(sptr, vchan))
		{
		  sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
			     me.name, parv[0], name);
		  name = strtoken(&p, (char *)NULL, ",");
		  continue;
		}
	      /*
	      **  Remove user from the old channel (if any)
	      */
            
	      sendto_match_servs(chptr, cptr, ":%s PART %s", parv[0], name);
            
	      sendto_channel_butserv(vchan, sptr, ":%s PART %s", parv[0],
				     name);
	      remove_user_from_channel(sptr, vchan, 0);
	    }
	}
      else
	{
	  if (!IsMember(sptr, chptr))
	    {
	      sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
			 me.name, parv[0], name);
	      name = strtoken(&p, (char *)NULL, ",");
	      continue;
	    }
	  /*
	  **  Remove user from the old channel (if any)
	  */

	  sendto_match_servs(chptr, cptr, ":%s PART %s", parv[0], name);
            
	  sendto_channel_butserv(chptr, sptr, ":%s PART %s", parv[0], name);
	  remove_user_from_channel(sptr, chptr, 0);
	}
            
      name = strtoken(&p, (char *)NULL, ",");
    }
  return 0;
}

int     ms_part(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel *chptr;
  char  *p, *name;

  if (parc < 2 || parv[1][0] == '\0')
    {
      sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "PART");
      return 0;
    }

  name = strtoken( &p, parv[1], ",");

#ifdef ANTI_SPAMBOT     /* Dianora */
      /* if its my client, and isn't an oper */

      if (name && MyConnect(sptr) && !IsAnOper(sptr))
        {
          if(GlobalSetOptions.spam_num &&
	     (sptr->join_leave_count >= GlobalSetOptions.spam_num))
            {
              sendto_ops_flags(FLAGS_BOTS,
                               "User %s (%s@%s) is a possible spambot",
                               sptr->name,
                               sptr->username, sptr->host);
              sptr->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
            }
          else
            {
              int t_delta;

              if( (t_delta = (CurrentTime - sptr->last_leave_time)) >
                  JOIN_LEAVE_COUNT_EXPIRE_TIME)
                {
                  int decrement_count;
                  decrement_count = (t_delta/JOIN_LEAVE_COUNT_EXPIRE_TIME);

                  if(decrement_count > sptr->join_leave_count)
                    sptr->join_leave_count = 0;
                  else
                    sptr->join_leave_count -= decrement_count;
                }
              else
                {
                  if( (CurrentTime - (sptr->last_join_time)) < 
		      GlobalSetOptions.spam_time)
                    {
                      /* oh, its a possible spambot */
                      sptr->join_leave_count++;
                    }
                }
              sptr->last_leave_time = CurrentTime;
            }
        }
#endif

  while ( name )
    {
      chptr = get_channel(sptr, name, 0);
      if (!chptr)
        {
          sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                     me.name, parv[0], name);
          name = strtoken(&p, (char *)NULL, ",");
          continue;
        }

      if (!IsMember(sptr, chptr))
        {
          sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
                     me.name, parv[0], name);
          name = strtoken(&p, (char *)NULL, ",");
          continue;
        }
      /*
      **  Remove user from the old channel (if any)
      */
            
      sendto_match_servs(chptr, cptr, ":%s PART %s", parv[0], name);
            
      sendto_channel_butserv(chptr, sptr, ":%s PART %s", parv[0], name);
      remove_user_from_channel(sptr, chptr, 0);
      name = strtoken(&p, (char *)NULL, ",");
    }
  return 0;
}

int     mo_part(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel *chptr;
  char  *p, *name;

  name = strtoken( &p, parv[1], ",");

  while ( name )
    {
      chptr = get_channel(sptr, name, 0);
      if (!chptr)
        {
          sendto_one(sptr, form_str(ERR_NOSUCHCHANNEL),
                     me.name, parv[0], name);
          name = strtoken(&p, (char *)NULL, ",");
          continue;
        }

      if (!IsMember(sptr, chptr))
        {
          sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
                     me.name, parv[0], name);
          name = strtoken(&p, (char *)NULL, ",");
          continue;
        }
      /*
      **  Remove user from the old channel (if any)
      */
            
      sendto_match_servs(chptr, cptr, ":%s PART %s", parv[0], name);
            
      sendto_channel_butserv(chptr, sptr, ":%s PART %s", parv[0], name);
      remove_user_from_channel(sptr, chptr, 0);
      name = strtoken(&p, (char *)NULL, ",");
    }
  return 0;
}

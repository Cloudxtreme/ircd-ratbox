/************************************************************************
 *   IRC - Internet Relay Chat, src/m_list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
 *
 * $Id$ 
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
 */
#include "handlers.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "vchannel.h"
#include "list.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int     m_list(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
  struct Channel *chptr;
  struct Channel *root_chptr;
  struct Channel *tmpchptr;
  char  *name, *p = NULL;
  char  vname[CHANNELLEN+NICKLEN+4];
  static time_t last_used=0L;
  int i,j;

  /* If its a LazyLinks connection, allow a list on its behalf */
  if( ConfigFileEntry.hub )
    {
      if(!IsCapable(sptr->from,CAP_LL) && !MyConnect(sptr))
	return 0;
    }
  else
    {
       if( serv_cptr_list && IsCapable( serv_cptr_list, CAP_LL) )
	 {
	   if(parc < 2)
	     sendto_one( serv_cptr_list, ":%s LIST", sptr->name );
	   else
	     sendto_one( serv_cptr_list, ":%s LIST %s", sptr->name, parv[1] );
	   return 0;
	 }
    }

  if(!IsAnOper(sptr))
    {
      if(((last_used + ConfigFileEntry.pace_wait) > CurrentTime) && (!IsDoingList(sptr)))
        {
          sendto_one(sptr,form_str(RPL_LOAD2HI),me.name,parv[0]);
          return 0;
        }
      else
        last_used = CurrentTime;
    }

  /* right.. if we are already involved in a "blocked" /list, we will simply
     continue where we left off */
  if (IsDoingList(sptr)) {
    if (sptr->listprogress != -1) {
      for (i=sptr->listprogress; i<CH_MAX; i++) {
        int progress2 = sptr->listprogress2;
        for (j=0, chptr=(struct Channel*)(hash_get_channel_block(i).list);
             (chptr) && (j<hash_get_channel_block(i).links); chptr=chptr->hnextch, j++) {
          if (j<progress2) continue;  /* wind up to listprogress2 */
          if (!chptr->members || !sptr->user ||
              (SecretChannel(chptr) && !IsMember(sptr, chptr)))
            continue;
          if (ShowChannel(sptr, chptr))
            {
              root_chptr = find_bchan(chptr);

              if( (IsVchan(chptr) || HasVchans(chptr)) && 
                  (root_chptr->members || root_chptr->next_vchan->next_vchan) )
                {
                  ircsprintf(vname, "%s<!%s>", root_chptr->chname,
                             chptr->members->value.cptr->name);
                }
              else
                ircsprintf(vname, "%s", root_chptr->chname);

              sendto_one(sptr, form_str(RPL_LIST), me.name, parv[0],
                         vname, chptr->users, chptr->topic);
            }
          else
            sendto_one(sptr, form_str(RPL_LIST), me.name, parv[0],
                       "*", chptr->users, "");
          if (IsSendqPopped(sptr)) {
            /* we popped again! : P */
            sptr->listprogress=i;
            sptr->listprogress2=j;
            return 0;
          }
        }
        sptr->listprogress2 = 0;
      }
    }
    sendto_one(sptr, form_str(RPL_LISTEND), me.name, parv[0]);
    if (IsSendqPopped(sptr)) { /* popped with the RPL_LISTEND code. d0h */
      sptr->listprogress = -1;
      return 0;
    }
    ClearDoingList(sptr);   /* yupo, its over */
    return 0;
    
  }
  
  sendto_one(sptr, form_str(RPL_LISTSTART), me.name, parv[0]);

  if (parc < 2 || BadPtr(parv[1]))
    {
      SetDoingList(sptr);     /* only set if its a full list */
      ClearSendqPop(sptr);    /* just to make sure */
      /* we'll do this by looking through each hash table bucket */
      for (i=0; i<CH_MAX; i++) {
        for (j=0, chptr = (struct Channel*)(hash_get_channel_block(i).list);
             (chptr) && (j<hash_get_channel_block(i).links); chptr = chptr->hnextch, j++) {
          if (!chptr->members || !sptr->user ||
              (SecretChannel(chptr) && !IsMember(sptr, chptr)))
            continue;
          /* EVIL!  sendto_one doesnt return status of any kind!  Forcing us
             to make up yet another stupid client flag (we could just
             negate the DOING_LIST flag, but that might confuse people) -good*/
          if (ShowChannel(sptr, chptr))
            {
              root_chptr = find_bchan(chptr);
 
              if( (IsVchan(chptr) || HasVchans(chptr)) &&
                  (root_chptr->members || root_chptr->next_vchan->next_vchan) )
                {
                  ircsprintf(vname, "%s<!%s>", root_chptr->chname,  
                             chptr->members->value.cptr->name);
                }
              else
                ircsprintf(vname, "%s", root_chptr->chname);

              sendto_one(sptr, form_str(RPL_LIST), me.name, parv[0],
                         vname, chptr->users, chptr->topic);
            }
          else
            sendto_one(sptr, form_str(RPL_LIST), me.name, parv[0],
                       "*", chptr->users, "");
          if (IsSendqPopped(sptr)) {
            /* GAAH!  We popped our sendq.  Mark our location in the /list */
            sptr->listprogress=i;
            sptr->listprogress2=j;
            return 0;
          }
        }
      
      }

      sendto_one(sptr, form_str(RPL_LISTEND), me.name, parv[0]);
      if (IsSendqPopped(sptr)) {
        sptr->listprogress=-1;
        return 0;
      }
      ClearDoingList(sptr);   /* yupo, its over */
      return 0;
    }   
          
  p = strchr(parv[1],',');
  if(p)
    *p = '\0';
  name = parv[1]; /* strtoken(&p, parv[1], ","); */

  /* while(name) */
  if(name)
    {
      chptr = hash_find_channel(name, NullChn);
      root_chptr = find_bchan(chptr);
      for (tmpchptr = root_chptr; tmpchptr; tmpchptr = tmpchptr->next_vchan)
        if (ShowChannel(sptr, tmpchptr) && tmpchptr->members && sptr->user)
          {
            if( (IsVchan(tmpchptr) || HasVchans(tmpchptr)) &&
                (root_chptr->members || root_chptr->next_vchan->next_vchan) )
              {
                ircsprintf(vname, "%s<!%s>", root_chptr->chname,
                           tmpchptr->members->value.cptr->name);
              }
            else
              ircsprintf(vname, "%s", root_chptr->chname);

            sendto_one(sptr, form_str(RPL_LIST), me.name, parv[0],
                       vname, tmpchptr->users, tmpchptr->topic);
          }
      /*      name = strtoken(&p, (char *)NULL, ","); */
    }
  sendto_one(sptr, form_str(RPL_LISTEND), me.name, parv[0]);
  return 0;
}

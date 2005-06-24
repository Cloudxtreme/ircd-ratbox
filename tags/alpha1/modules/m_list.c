/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_list.c
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
#include "tools.h"
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
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct Message list_msgtab = {
  MSG_LIST, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_list, ms_list, mo_list}
};

void
_modinit(void)
{
  mod_add_cmd(&list_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&list_msgtab);
}

int list_all_channels(struct Client *sptr);
int list_named_channel(struct Client *sptr,char *name);

char *_version = "20001122";

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
  static time_t last_used=0L;

  /* If its a LazyLinks connection, let uplink handle the list */

  if( uplink && IsCapable(uplink,CAP_LL) )
    {
      if(parc < 2)
	sendto_one( uplink, ":%s LIST", sptr->name );
      else
	sendto_one( uplink, ":%s LIST %s", sptr->name, parv[1] );
      return 0;
    }

  /* If not a LazyLink connection, see if its still paced */

  if( ( (last_used + ConfigFileEntry.pace_wait) > CurrentTime) )
    {
      sendto_one(sptr,form_str(RPL_LOAD2HI),me.name,parv[0]);
      return 0;
    }
  else
    last_used = CurrentTime;

  /* If no arg, do all channels *whee*, else just one channel */
  if (parc < 2 || BadPtr(parv[1]))
    {
      list_all_channels(sptr);
    }
  else
    {
      list_named_channel(sptr,parv[1]);
    }

  return 0;
}


/*
** mo_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int     mo_list(struct Client *cptr,
               struct Client *sptr,
               int parc,
               char *parv[])
{
/* Opers don't get paced */

  /* If its a LazyLinks connection, let uplink handle the list
   * even for opers!
   */

  if( uplink && IsCapable( uplink, CAP_LL) )
    {
      if(parc < 2)
	sendto_one( uplink, ":%s LIST", sptr->name );
      else
	sendto_one( uplink, ":%s LIST %s", sptr->name, parv[1] );
      return 0;
    }

  /* If no arg, do all channels *whee*, else just one channel */
  if (parc < 2 || BadPtr(parv[1]))
    {
      list_all_channels(sptr);
    }
  else
    {
      list_named_channel(sptr,parv[1]);
    }

  return 0;
}

/*
** ms_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int     ms_list(struct Client *cptr,
		struct Client *sptr,
		int parc,
		char *parv[])
{
  /* Only allow remote list if LazyLink request */

  if( ConfigFileEntry.hub )
    {
      if(!IsCapable(cptr->from,CAP_LL) && !MyConnect(sptr))
	return 0;

      if (parc < 2 || BadPtr(parv[1]))
	{
	  list_all_channels(sptr);
	}
      else
	{
	  list_named_channel(sptr,parv[1]);
	}
    }
  return 0;
}

/*
 * list_all_channels
 * inputs	- pointer to client requesting list
 * output	- 0/1
 * side effects	- list all channels to sptr
 */
int list_all_channels(struct Client *sptr)
{
  struct Channel *chptr;

  for ( chptr = GlobalChannelList; chptr; chptr = chptr->nextch )
    {
      if ( !sptr->user ||
	   (SecretChannel(chptr) && !IsMember(sptr, chptr)))
	continue;
      list_one_channel(sptr,chptr);
    }

  sendto_one(sptr, form_str(RPL_LISTEND), me.name, sptr->name);
  return 0;
}   
          
/*
 * list_named_channel
 * inputs       - pointer to client requesting list
 * output       - 0/1
 * side effects	- list all channels to sptr
 */
int list_named_channel(struct Client *sptr,char *name)
{
  char  vname[CHANNELLEN+NICKLEN+4];
  dlink_node *ptr;
  struct Channel *chptr;
  struct Channel *root_chptr;
  struct Channel *tmpchptr;
  char *p;

  if((p = strchr(name,',')))
    *p = '\0';
  
  if(*name == '\0')
    return 0;

  chptr = hash_find_channel(name, NullChn);

  if (chptr == NULL)
    {
      sendto_one(sptr,form_str(ERR_NOSUCHNICK),me.name, sptr->name, name);
      sendto_one(sptr, form_str(RPL_LISTEND), me.name, sptr->name);
      return 0;
    }

  if (HasVchans(chptr))
    ircsprintf(vname, "%s<!%s>", chptr->chname,
               pick_vchan_id(chptr));
  else
    ircsprintf(vname, "%s", chptr->chname);
               
  if (ShowChannel(sptr, chptr))
    sendto_one(sptr, form_str(RPL_LIST), me.name, sptr->name,
               vname, chptr->users, chptr->topic);
      
  /* Deal with subvchans */
  
  for (ptr = chptr->vchan_list.head; ptr; ptr = ptr->next)
    {
      tmpchptr = ptr->data;

      if (ShowChannel(sptr, tmpchptr))
	{
          root_chptr = find_bchan(tmpchptr);
          if(root_chptr != NULL)
            ircsprintf(vname, "%s<!%s>", root_chptr->chname,
                       pick_vchan_id(tmpchptr));
          sendto_one(sptr, form_str(RPL_LIST), me.name, sptr->name,
                     vname, tmpchptr->users, tmpchptr->topic);
        }
    }
  
  sendto_one(sptr, form_str(RPL_LISTEND), me.name, sptr->name);
  return 0;
}

/*
 * list_one_channel
 *
 * inputs       - client pointer to return result to
 *              - pointer to channel to list
 * ouput	- none
 * side effects -
 */
void list_one_channel(struct Client *sptr,struct Channel *chptr)
{
  struct Channel *root_chptr;
  char  vname[CHANNELLEN+NICKLEN+5]; /* <!!>, and null */

  if( (IsVchan(chptr) || HasVchans(chptr)) )
    {
      root_chptr = find_bchan(chptr);
      if(root_chptr != NULL)
	{
	  ircsprintf(vname, "%s<!%s>", root_chptr->chname,
		     pick_vchan_id(chptr));
	}
      else
	ircsprintf(vname, "%s<!%s>", chptr->chname, pick_vchan_id(chptr));
    }
  else
    ircsprintf(vname, "%s", chptr->chname);


  sendto_one(sptr, form_str(RPL_LIST), me.name, sptr->name,
	     vname, chptr->users, chptr->topic);
}



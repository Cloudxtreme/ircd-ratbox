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

static void m_list(struct Client*, struct Client*, int, char**);
static void ms_list(struct Client*, struct Client*, int, char**);
static void mo_list(struct Client*, struct Client*, int, char**);
static int list_all_channels(struct Client *);
static void list_one_channel(struct Client *,struct Channel *);

struct Message list_msgtab = {
  "LIST", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_list, ms_list, mo_list}
};
#ifndef STATIC_MODULES

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
char *_version = "20001122";
#endif
static int list_all_channels(struct Client *source_p);
static int list_named_channel(struct Client *source_p,char *name);


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void m_list(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
  static time_t last_used=0L;

  /* If its a LazyLinks connection, let uplink handle the list */

  if( uplink && IsCapable(uplink,CAP_LL) )
    {
      if(parc < 2)
	sendto_one( uplink, ":%s LIST", source_p->name );
      else
	sendto_one( uplink, ":%s LIST %s", source_p->name, parv[1] );
      return;
    }

  /* If not a LazyLink connection, see if its still paced */

  if( ( (last_used + ConfigFileEntry.pace_wait) > CurrentTime) )
    {
      sendto_one(source_p,form_str(RPL_LOAD2HI),me.name,parv[0]);
      return;
    }
  else
    last_used = CurrentTime;

  /* If no arg, do all channels *whee*, else just one channel */
  if (parc < 2 || BadPtr(parv[1]))
    {
      list_all_channels(source_p);
    }
  else
    {
      list_named_channel(source_p,parv[1]);
    }
}


/*
** mo_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void mo_list(struct Client *client_p,
                   struct Client *source_p,
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
	sendto_one( uplink, ":%s LIST", source_p->name );
      else
	sendto_one( uplink, ":%s LIST %s", source_p->name, parv[1] );
      return;
    }

  /* If no arg, do all channels *whee*, else just one channel */
  if (parc < 2 || BadPtr(parv[1]))
    {
      list_all_channels(source_p);
    }
  else
    {
      list_named_channel(source_p,parv[1]);
    }
}

/*
** ms_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static void ms_list(struct Client *client_p,
                   struct Client *source_p,
                   int parc,
                   char *parv[])
{
  /* Only allow remote list if LazyLink request */

  if( ServerInfo.hub )
    {
      if(!IsCapable(client_p->from,CAP_LL) && !MyConnect(source_p))
	return;

      if (parc < 2 || BadPtr(parv[1]))
	{
	  list_all_channels(source_p);
	}
      else
	{
	  list_named_channel(source_p,parv[1]);
	}
    }
}

/*
 * list_all_channels
 * inputs	- pointer to client requesting list
 * output	- 0/1
 * side effects	- list all channels to source_p
 */
static int list_all_channels(struct Client *source_p)
{
  struct Channel *chptr;

  sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

  for ( chptr = GlobalChannelList; chptr; chptr = chptr->nextch )
    {
      if ( !source_p->user ||
	   (SecretChannel(chptr) && !IsMember(source_p, chptr)))
	continue;
      list_one_channel(source_p,chptr);
    }

  sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
  return 0;
}   
          
/*
 * list_named_channel
 * inputs       - pointer to client requesting list
 * output       - 0/1
 * side effects	- list all channels to source_p
 */
static int list_named_channel(struct Client *source_p,char *name)
{
  char  vname[CHANNELLEN+NICKLEN+4];
  dlink_node *ptr;
  struct Channel *chptr;
  struct Channel *root_chptr;
  struct Channel *tmpchptr;
  char *p;

  sendto_one(source_p, form_str(RPL_LISTSTART), me.name, source_p->name);

  if((p = strchr(name,',')))
    *p = '\0';
      
  if(*name == '\0')
    {
      sendto_one(source_p,form_str(ERR_NOSUCHNICK),me.name, source_p->name, name);
      sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
      return 0;
    }

  chptr = hash_find_channel(name, NullChn);

  if (chptr == NULL)
    {
      sendto_one(source_p,form_str(ERR_NOSUCHNICK),me.name, source_p->name, name);
      sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
      return 0;
    }

  if (HasVchans(chptr))
    ircsprintf(vname, "%s<!%s>", chptr->chname,
               pick_vchan_id(chptr));
  else
    ircsprintf(vname, "%s", chptr->chname);
               
  if (ShowChannel(source_p, chptr))
    sendto_one(source_p, form_str(RPL_LIST), me.name, source_p->name,
               vname, chptr->users, chptr->topic);
      
  /* Deal with subvchans */
  
  for (ptr = chptr->vchan_list.head; ptr; ptr = ptr->next)
    {
      tmpchptr = ptr->data;

      if (ShowChannel(source_p, tmpchptr))
	{
          root_chptr = find_bchan(tmpchptr);
          if(root_chptr != NULL)
            ircsprintf(vname, "%s<!%s>", root_chptr->chname,
                       pick_vchan_id(tmpchptr));
          sendto_one(source_p, form_str(RPL_LIST), me.name, source_p->name,
                     vname, tmpchptr->users, tmpchptr->topic);
        }
    }
  
  sendto_one(source_p, form_str(RPL_LISTEND), me.name, source_p->name);
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
static void list_one_channel(struct Client *source_p,struct Channel *chptr)
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


  sendto_one(source_p, form_str(RPL_LIST), me.name, source_p->name,
	     vname, chptr->users, chptr->topic);
}



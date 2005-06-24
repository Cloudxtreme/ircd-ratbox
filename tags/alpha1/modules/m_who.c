/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_who.c
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
#include "tools.h"
#include "common.h"   /* bleah */
#include "handlers.h"
#include "client.h"
#include "common.h"   /* bleah */
#include "channel.h"
#include "vchannel.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "list.h"
#include "irc_string.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

struct Message who_msgtab = {
  MSG_WHO, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_who, ms_who, m_who}
};

void
_modinit(void)
{
  mod_add_cmd(&who_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&who_msgtab);
}

void do_who_on_channel(struct Client *sptr,
			      struct Channel *chptr, char *real_name,
			      int oper, int member);

void do_who_list(struct Client *sptr, struct Channel *chptr,
		 dlink_list *list, char *chname, char *op_flags);

void who_global(struct Client *sptr, char *mask, int oper);

void    do_who(struct Client *sptr,
	       struct Client *acptr,
	       struct Channel *chptr,
	       char *repname,
	       char *op_flags);

char *_version = "20001122";

/*
** m_who
**      parv[0] = sender prefix
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag, only 'o' for now.
*/
int     m_who(struct Client *cptr,
              struct Client *sptr,
              int parc,
              char *parv[])
{
  struct Client *acptr;
  char  *mask = parc > 1 ? parv[1] : NULL;
  dlink_node *lp;
  struct Channel *chptr=NULL;
  struct Channel *vchan;
  struct Channel *mychannel = NULL;
  char  *chanop_flag;
  char  *halfop_flag;
  char  *voiced_flag;
  int   oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
  int   member;

  /*
  **  Following code is some ugly hacking to preserve the
  **  functions of the old implementation. (Also, people
  **  will complain when they try to use masks like "12tes*"
  **  and get people on channel 12 ;) --msa
  */

  /* You people code like its supposed to be hard to read *why?* -db */

  /* See if mask is there, collapse it or return if not there */

  if (mask != (char *)NULL)
    {
      (void)collapse(mask);

      if (*mask == '\0')
	{
	  sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], "*" );
	  return 0;
	}
    }
  else
    {
      who_global(sptr, mask, oper);
      sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], "*" );
      return 0;
    }

  /* mask isn't NULL at this point. repeat after me... -db */

  /* '/who *' */

  if ((*(mask+1) == (char) 0) && (*mask == '*'))
    {
      if (sptr->user)
	if ((lp = sptr->user->channel.head))
	  mychannel = lp->data;

      if (!mychannel)
        {
          sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], "*");
          return 0;
        }

      if (HasVchans(mychannel))
	{
	  vchan = map_vchan(mychannel,sptr);
	  if(vchan != 0) 
	    do_who_on_channel(sptr,vchan,"*",NO,YES);
	  else
	    do_who_on_channel(sptr,mychannel,"*",NO,YES);
	}
      else
	do_who_on_channel(sptr,mychannel,"*",NO,YES);
      sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], "*");
      return 0;
    }

  /* '/who #some_channel' */

  if (IsChannelName(mask))
    {
      /*
       * List all users on a given channel
       */
      chptr = hash_find_channel(mask, NULL);
      if (chptr)
	{
	  if (HasVchans(chptr))
	    {
	      vchan = map_vchan(chptr,sptr);

	      /* If vchan not 0, that makes them a member automatically */
	      if ( vchan != 0 )
		do_who_on_channel(sptr,vchan,chptr->chname,NO,YES);
	      else
		{
		  if ( IsMember(sptr, chptr) )
		    do_who_on_channel(sptr,chptr,chptr->chname,NO,YES);
		  else if(!SecretChannel(chptr))
		    do_who_on_channel(sptr,chptr,chptr->chname,NO,NO);
		}
	    }
	  else
	    {
	      if ( IsMember(sptr, chptr) )
		do_who_on_channel(sptr,chptr,chptr->chname,NO,YES);
	      else if(!SecretChannel(chptr))
		do_who_on_channel(sptr,chptr,chptr->chname,NO,NO);
	    }
	}
      sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], mask );
      return 0;
    }

  /* '/who nick' */

  if (((acptr = find_client(mask, NULL)) != NULL) &&
      IsPerson(acptr) && (!oper || IsOper(acptr)))
    {
      struct Channel *bchan;
      char *chname=NULL;
      int isinvis = 0;

      if(IsServer(cptr))
	client_burst_if_needed(cptr,acptr);

      isinvis = IsInvisible(acptr);
      for (lp = acptr->user->channel.head; lp; lp = lp->next)
	{
	  chptr = lp->data;

          member = IsMember(sptr, chptr);
          if (isinvis && !member)
            continue;
          if (member || (!isinvis && PubChannel(chptr)))
            {
              break;
            }
	}

      if (chptr != NULL)
	{
	  chname = chptr->chname;
	  if (IsVchan(chptr))
	    {
	      bchan = find_bchan (chptr);
	      if (bchan != NULL)
		chname = bchan->chname;
	    }

	  /* XXX globalize this inside m_who.c ? */
	  if(chptr->mode.mode & MODE_HIDEOPS)
	    {
	      chanop_flag = "";
	      halfop_flag = "";
	      voiced_flag = "";
	    }
	  else
	    {
	      chanop_flag = "@";
	      halfop_flag = "%";
	      voiced_flag = "+";
	    }

	  if (is_chan_op(chptr,acptr))
	    do_who(sptr,acptr,chptr,chname,chanop_flag);
	  else if(is_half_op(chptr,acptr))
	    do_who(sptr,acptr,chptr,chname,halfop_flag);
	  else if(is_voiced(chptr,acptr))
	    do_who(sptr,acptr,chptr,chname,voiced_flag);
	  else
	    do_who(sptr,acptr,chptr,chname,"");
	}

      sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], mask );
      return 0;
    }

  /* Wasn't a nick, wasn't a channel, wasn't a '*' so ... */
  who_global(sptr, mask, oper);
  sendto_one(sptr, form_str(RPL_ENDOFWHO), me.name, parv[0], mask );
  return 0;
}

/*
 * who_global
 *
 * inputs	- pointer to client requesting who
 *		- char * mask to match
 *		- int if oper or not
 * output	- NONE
 * side effects - do a global scan of all clients looking for match
 *		  this is slightly expensive on EFnet ...
 */

void who_global(struct Client *sptr,char *mask, int oper)
{
  struct Channel *chptr=NULL;
  struct Channel *bchan;
  struct Client *acptr;
  dlink_node  *lp;
  char  *chname=NULL;
  int   showperson;
  int   member;
  int   isinvis;
  int   maxmatches = 500;

  for (acptr = GlobalClientList; acptr; acptr = acptr->next)
    {
      if (!IsPerson(acptr))
        continue;
      if (oper && !IsOper(acptr))
        continue;
      
      showperson = NO;
      /*
       * Show user if they are on the same channel, or not
       * invisible and on a non secret channel (if any).
       * Do this before brute force match on all relevant fields
       * since these are less cpu intensive (I hope :-) and should
       * provide better/more shortcuts - avalon
       */
      isinvis = IsInvisible(acptr);
      for (lp = acptr->user->channel.head; lp; lp = lp->next)
        {
          chptr = lp->data;
          member = IsMember(sptr, chptr);
          if (isinvis && !member)
            continue;
          if (member || (!isinvis && PubChannel(chptr)))
            {
              showperson = YES;
              break;
            }
          if (HiddenChannel(chptr) && !SecretChannel(chptr) &&
              !isinvis)
            showperson = YES;
        }

      if (!acptr->user->channel.head && !isinvis)
        showperson = YES;

      if (showperson &&
          (!mask ||
           match(mask, acptr->name) ||
           match(mask, acptr->username) ||
           match(mask, acptr->host) ||
           match(mask, acptr->user->server) ||
           match(mask, acptr->info)))
        {
	  if (IsVchan(chptr))
	    {
	      bchan = find_bchan (chptr);
	      if (bchan != NULL)
		chname = bchan->chname;
	    }

	  do_who_list(sptr, chptr, &chptr->chanops, chname, "@");
	  do_who_list(sptr, chptr, &chptr->halfops, chname, "%");
	  do_who_list(sptr, chptr, &chptr->voiced,  chname, "+");
	  do_who_list(sptr, chptr, &chptr->peons,   chname, "");
	}
      chname = chptr->chname;

      if (maxmatches > 0)
	{
	  --maxmatches;
	  if( maxmatches == 0 )
	    return;
	}
    }
}


/*
 * do_who_on_channel
 *
 * inputs	- pointer to client requesting who
 *		- pointer to channel to do who on
 *		- The "real name" of this channel
 *		- int if sptr is oper or not
 *		- int if client is member or not
 * output	- NONE
 * side effects - do a who on given channel
 */

void do_who_on_channel(struct Client *sptr,
			      struct Channel *chptr,
			      char *chname,
			      int oper, int member)
{
  do_who_list(sptr, chptr, &chptr->chanops, chname, "@");
  do_who_list(sptr, chptr, &chptr->halfops, chname, "%");
  do_who_list(sptr, chptr, &chptr->voiced,  chname, "+");
  do_who_list(sptr, chptr, &chptr->peons,   chname, "");
}

void do_who_list(struct Client *sptr, struct Channel *chptr,
		  dlink_list *list, char *chname, char *op_flags)
{
  dlink_node *ptr;
  struct Client *acptr;

  for(ptr = list->head; ptr; ptr = ptr->next)
    {
      acptr = ptr->data;
      do_who(sptr,acptr,chptr,chname,op_flags);
    }
}

/*
 * do_who
 *
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- pointer to channel
 *		- The reported name
 *		- channel flags
 * output	- NONE
 * side effects - do a who on given person
 */

void    do_who(struct Client *sptr,
	       struct Client *acptr,
	       struct Channel *chptr,
	       char *chname,
	       char *op_flags)
{
  char  status[5];

  if(chptr->mode.mode & MODE_HIDEOPS && !is_any_op(chptr,sptr))
    {
      ircsprintf(status,"%c%s", 
		 acptr->user->away ? 'G' : 'H',
		 IsOper(acptr) ? "*" : "");
    }
  else
    {
      ircsprintf(status,"%c%s%s", 
		 acptr->user->away ? 'G' : 'H',
		 IsOper(acptr) ? "*" : "", op_flags );
    }

  if(GlobalSetOptions.hide_server)
    {
      sendto_one(sptr, form_str(RPL_WHOREPLY), me.name, sptr->name,
		 chname, acptr->username,
		 acptr->host, IsOper(sptr) ? acptr->user->server : "*",
		 acptr->name,
		 status, acptr->hopcount, acptr->info);
    }
  else
    {
      sendto_one(sptr, form_str(RPL_WHOREPLY), me.name, sptr->name,
             chname, acptr->username,
             acptr->host,  acptr->user->server, acptr->name,
             status, acptr->hopcount, acptr->info);
    }
}

/*
** ms_who
**      parv[0] = sender prefix
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag, only 'o' for now.
*/
int     ms_who(struct Client *cptr,
              struct Client *sptr,
              int parc,
              char *parv[])
{
  /* If its running as a hub, and linked with lazy links
   * then allow leaf to use normal client m_who()
   * other wise, ignore it.
   */

  if( ConfigFileEntry.hub )
    {
      if(!IsCapable(cptr->from,CAP_LL))
	return 0;
    }

  return( m_who(cptr,sptr,parc,parv) );
}

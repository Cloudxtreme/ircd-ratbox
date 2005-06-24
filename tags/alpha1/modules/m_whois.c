/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_whois.c
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
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "list.h"
#include "irc_string.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>
#include "memdebug.h"

int do_whois(struct Client *cptr, struct Client *sptr, int parc, char *parv[]);
int single_whois(struct Client *sptr, struct Client *acptr, int wilds, int glob);
void whois_person(struct Client *sptr,struct Client *acptr,int glob);
int global_whois(struct Client *sptr, char *nick, int wilds, int glob);

struct Message whois_msgtab = {
  MSG_WHOIS, 0, 0, 0, MFLG_SLOW, 0L,
  {m_unregistered, m_whois, ms_whois, mo_whois}
};

void
_modinit(void)
{
  mod_add_cmd(&whois_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&whois_msgtab);
}

char *_version = "20010101";

/*
** m_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
int     m_whois(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])
{
   struct Client *acptr;
  
  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  /* We need this to keep compatibility with hyb6 */
  if ((acptr = hash_find_client(parv[1], (struct Client*)NULL)) &&
      !MyConnect(acptr) && IsClient(acptr) && parc > 2)
    {
      client_burst_if_needed(acptr->from, sptr);
      sendto_one(acptr->from, ":%s WHOIS %s :%s", parv[0], parv[1],
                 parv[1]);
      return 0;
    }
  

 return(do_whois(cptr,sptr,parc,parv));
}

/*
** mo_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
int     mo_whois(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])

{
  if(parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  if(parc > 2)
    {
      if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1, parc, parv) !=
          HUNTED_ISME)
        {
          return 0;
        }
      parv[1] = parv[2];
    }

  return(do_whois(cptr,sptr,parc,parv));
}


/* do_whois
 *
 * inputs	- pointer to 
 * output	- 
 * side effects -
 */
int do_whois(struct Client *cptr, struct Client *sptr, int parc, char *parv[])
{
  struct Client *acptr;
  char  *nick;
  char  *p = NULL;
  int   found=NO;
  int   wilds;
  int   glob=0;

  /* This lets us make all "whois nick" queries look the same, and all
   * "whois nick nick" queries look the same.  We have to pass it all
   * the way down to whois_person() though -- fl */

  if(parc > 2)
    glob = 1;

  nick = parv[1];
  if ( (p = strchr(parv[1],',')) )
    *p = '\0';

  (void)collapse(nick);
  wilds = (strchr(nick, '?') || strchr(nick, '*'));

  if(!wilds)
    {
      if( (acptr = hash_find_client(nick,(struct Client *)NULL)) )
	{
	  if (IsServer(cptr))
	    client_burst_if_needed(cptr,acptr);

	  if(IsPerson(acptr))
	    {
	      (void)single_whois(sptr,acptr,wilds,glob);
	      found = YES;
            }
        }
      else
	{
	  if (uplink && IsCapable(uplink,CAP_LL))
	    {
	      if(glob == 1)
   	        sendto_one(uplink,":%s WHOIS %s :%s",
		  	   sptr->name, nick, nick);
	      else
		sendto_one(uplink,":%s WHOIS %s",
			   sptr->name, nick);
	      return 0;
	    }
	}
    }
  else
    {
      /* disallow wild card whois on lazylink leafs for now */

      if (uplink && IsCapable(uplink,CAP_LL))
	{
	  return 0;
	}
      /* Oh-oh wilds is true so have to do it the hard expensive way */
      found = global_whois(sptr,nick,wilds,glob);
    }

  if(found)
    sendto_one(sptr, form_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);
  else
    sendto_one(sptr, form_str(ERR_NOSUCHNICK), me.name, parv[0], nick);

  return 0;
}

/*
 * global_whois()
 *
 * Inputs	- sptr client to report to
 *		- acptr client to report on
 *		- wilds whether wildchar char or not
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to sptr
 */
int global_whois(struct Client *sptr, char *nick, int wilds, int glob)
{
  struct Client *acptr;
  int found = NO;

  for (acptr = GlobalClientList; (acptr = next_client(acptr, nick));
       acptr = acptr->next)
    {
      if (IsServer(acptr))
	continue;
      /*
       * I'm always last :-) and acptr->next == NULL!!
       */
      if (IsMe(acptr))
	break;
      /*
       * 'Rules' established for sending a WHOIS reply:
       *
       *
       * - if wildcards are being used dont send a reply if
       *   the querier isnt any common channels and the
       *   client in question is invisible and wildcards are
       *   in use (allow exact matches only);
       *
       * - only send replies about common or public channels
       *   the target user(s) are on;
       */

      if(!IsRegistered(acptr))
	continue;

      if(single_whois(sptr, acptr, wilds, glob))
	found = 1;
    }

  return (found);
}

/*
 * single_whois()
 *
 * Inputs	- sptr client to report to
 *		- acptr client to report on
 *		- wilds whether wildchar char or not
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to sptr
 */
int single_whois(struct Client *sptr,struct Client *acptr,int wilds, int glob)
{
  dlink_node *ptr;
  struct Channel *chptr;
  char *name;
  int invis;
  int member;
  int showperson;

  if (acptr->name[0] == '\0')
    name = "?";
  else
    name = acptr->name;

  if( acptr->user == NULL )
    {
      sendto_one(sptr, form_str(RPL_WHOISUSER), me.name,
		 sptr->name, name,
		 acptr->username, acptr->host, acptr->info);
	  sendto_one(sptr, form_str(RPL_WHOISSERVER),
		 me.name, sptr->name, name, "<Unknown>",
		 "*Not On This Net*");
      return 0;
    }

  invis = IsInvisible(acptr);
  member = (acptr->user->channel.head) ? 1 : 0;
  showperson = (wilds && !invis && !member) || !wilds;

  for (ptr = acptr->user->channel.head; ptr; ptr = ptr->next)
    {
      chptr = ptr->data;
      member = IsMember(sptr, chptr);
      if (invis && !member)
	continue;
      if (member || (!invis && PubChannel(chptr)))
	{
	  showperson = 1;
	  break;
	}
      if (!invis && HiddenChannel(chptr) && !SecretChannel(chptr))
	{
	  showperson = 1;
	  break;
	}
    }

  if(showperson)
    whois_person(sptr,acptr,glob);
  return 0;
}

/*
 * whois_person()
 *
 * Inputs	- sptr client to report to
 *		- acptr client to report on
 * Output	- NONE
 * Side Effects	- 
 */
void whois_person(struct Client *sptr,struct Client *acptr, int glob)
{
  char buf[BUFSIZE];
  char *chname;
  char *server_name;
  dlink_node  *lp;
  struct Client *a2cptr;
  struct Channel *chptr;
  struct Channel *bchan;
  int cur_len = 0;
  int mlen;
  char *t;
  int tlen;
  int reply_to_send = NO;

  a2cptr = find_server(acptr->user->server);
          
  sendto_one(sptr, form_str(RPL_WHOISUSER), me.name,
	 sptr->name, acptr->name,
	 acptr->username, acptr->host, acptr->info);
  server_name = (char *)acptr->user->server;

  ircsprintf(buf, form_str(RPL_WHOISCHANNELS),
	       me.name, sptr->name, acptr->name, "");

  mlen = strlen(buf);
  cur_len = mlen;
  t = buf + mlen;

  for (lp = acptr->user->channel.head; lp; lp = lp->next)
    {
      chptr = lp->data;
      chname = chptr->chname;

      if (IsVchan(chptr))
	{
	  bchan = find_bchan (chptr);
	  if (bchan != NULL)
	    chname = bchan->chname;
	}

      if (ShowChannel(sptr, chptr))
	{
	  if (chptr->mode.mode & MODE_HIDEOPS && !is_any_op(chptr,sptr))
	    {
	      ircsprintf(t,"%s ",chname);
	    }
	  else
	    {
	      ircsprintf(t,"%s%s ", channel_chanop_or_voice(chptr,acptr),
			 chname);
	    }

	  tlen = strlen(t);
	  t += tlen;
	  cur_len += tlen;
	  reply_to_send = YES;

	  if ((cur_len + NICKLEN) > (BUFSIZE - 4))
	    {
	      sendto_one(sptr, "%s", buf);
	      cur_len = mlen;
	      t = buf + mlen;
	      reply_to_send = NO;
	    }
	}
    }

  if (reply_to_send)
    sendto_one(sptr, "%s", buf);
          
  if ((IsOper(sptr) || !GlobalSetOptions.hide_server) || acptr == sptr)
    sendto_one(sptr, form_str(RPL_WHOISSERVER),
	       me.name, sptr->name, acptr->name, server_name,
	       a2cptr?a2cptr->info:"*Not On This Net*");
  else
    sendto_one(sptr, form_str(RPL_WHOISSERVER),
	       me.name, sptr->name, acptr->name,
               ConfigFileEntry.network_name,
	       ConfigFileEntry.network_desc);

  if (acptr->user->away)
    sendto_one(sptr, form_str(RPL_AWAY), me.name,
	       sptr->name, acptr->name, acptr->user->away);

  if (IsOper(acptr))
      sendto_one(sptr, form_str(RPL_WHOISOPERATOR),
		 me.name, sptr->name, acptr->name);

  if (IsAdmin(acptr) && glob == 1)
	sendto_one(sptr, form_str(RPL_WHOISADMIN),
		   me.name, sptr->name, acptr->name);


  if(glob == 1)
    sendto_one(sptr, form_str(RPL_WHOISIDLE),
	       me.name, sptr->name, acptr->name,
	       CurrentTime - acptr->user->last,
	       acptr->firsttime);

  return;
}

/*
** ms_whois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
int     ms_whois(struct Client *cptr,
                struct Client *sptr,
                int parc,
                char *parv[])
{
  if (parc < 2)
    {
      sendto_one(sptr, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return 0;
    }

  /* If its running as a hub, and linked with lazy links
   * then allow leaf to use normal client m_whois()
   * other wise, ignore it.
   */
  if( !IsOper(sptr) && GlobalSetOptions.hide_server
      && !IsCapable(cptr->from,CAP_LL) )
    {
 	return 0;
    }
 
  return( m_whois(cptr,sptr,parc,parv) );
}

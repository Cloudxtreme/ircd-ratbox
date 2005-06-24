/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  m_owhois.c: Shows who a user is to an operator.  This is rather
 *              evil as it shows *all* the channels the user is in,
 *              regardless of modes. 
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "tools.h"
#include "common.h"  
#include "handlers.h"
#include "client.h"
#include "hash.h"
#include "channel.h"
#include "channel_mode.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hook.h"
#include "newconf.h"

#ifndef OPER_SPY
#define OPER_SPY 0x0400
#define IsOperSpy(x) ((x)->flags2 & OPER_SPY)
#endif

static void conf_set_oper_spy(void *);
static int do_whois(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);
static int single_whois(struct Client *source_p, struct Client *target_p,
                        int wilds, int glob);
static void whois_person(struct Client *source_p,struct Client *target_p,int glob);
static int global_whois(struct Client *source_p, char *nick, int wilds, int glob);

static void mo_owhois(struct Client*, struct Client*, int, char**);

struct Message whois_msgtab = {
  "OWHOIS", 0, 0, 0, 0, MFLG_SLOW, 0L,
  {m_unregistered, m_ignore, m_ignore, mo_owhois}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  hook_add_event("doing_whois");
  mod_add_cmd(&whois_msgtab);
  add_conf_item("operator", "oper_spy", CF_YESNO, conf_set_oper_spy);
}

void
_moddeinit(void)
{
  hook_del_event("doing_whois");
  mod_del_cmd(&whois_msgtab);
  remove_conf_item("operator", "oper_spy");
}

const char *_version = "$Revision$";
#endif

/*
** mo_owhois
**      parv[0] = sender prefix
**      parv[1] = nickname masklist
*/
static void mo_owhois(struct Client *client_p,
                    struct Client *source_p,
                    int parc,
                    char *parv[])
{
  if(!IsOperSpy(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :You need oper_spy = yes;",
               me.name, source_p->name);
    return;
  }

  if(parc < 2)
    {
      sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
                 me.name, parv[0]);
      return;
    }

  if(parc > 2)
    {
      if (hunt_server(client_p,source_p,":%s WHOIS %s :%s", 1, parc, parv) !=
          HUNTED_ISME)
        {
          return;
        }
      parv[1] = parv[2];
    }

  do_whois(client_p,source_p,parc,parv);
}


/* do_whois
 *
 * inputs	- pointer to 
 * output	- 
 * side effects -
 */
static int do_whois(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  struct Client *target_p;
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
      if((target_p = find_client(nick)) != NULL)
	{
	  /* im being asked to reply to a client that isnt mine..
	   * I cant answer authoritively, so better make it non-detailed
	   */
	  if(!MyClient(target_p))
	    glob=0;
	    

	  if(IsPerson(target_p))
	    {
	      (void)single_whois(source_p,target_p,wilds,glob);
	      found = YES;
            }
        }
    }
  else
    {

      /* Oh-oh wilds is true so have to do it the hard expensive way */
      found = global_whois(source_p,nick,wilds,glob);
    }

  if(!found)
    sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, parv[0], nick);

  sendto_one(source_p, form_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

  return 0;
}

/*
 * global_whois()
 *
 * Inputs	- source_p client to report to
 *		- target_p client to report on
 *		- wilds whether wildchar char or not
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to source_p
 */
static int global_whois(struct Client *source_p, char *nick,
                        int wilds, int glob)
{
  struct Client *target_p;
  dlink_node *ptr;
  int found = NO;
  
  DLINK_FOREACH(ptr, global_client_list.head)
    {
      target_p = (struct Client *)ptr->data;
      if(!match(nick, target_p->name))
      {
        continue;
      }
      if (IsServer(target_p))
	continue;
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

      if(!IsRegistered(target_p))
	continue;

      if(single_whois(source_p, target_p, wilds, glob))
	found = 1;
    }

  return (found);
}

/*
 * single_whois()
 *
 * Inputs	- source_p client to report to
 *		- target_p client to report on
 *		- wilds whether wildchar char or not
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to source_p
 */
static int single_whois(struct Client *source_p,struct Client *target_p,
                        int wilds, int glob)
{
  char *name;
  
  if (target_p->name[0] == '\0')
    name = "?";
  else
    name = target_p->name;

  if( target_p->user == NULL )
    {
      sendto_one(source_p, form_str(RPL_WHOISUSER), me.name,
		 source_p->name, name,
		 target_p->username, target_p->host, target_p->info);
	  sendto_one(source_p, form_str(RPL_WHOISSERVER),
		 me.name, source_p->name, name, "<Unknown>",
		 "*Not On This Net*");
      return 0;
    }

  whois_person(source_p,target_p,glob);
  return 1;
}

/*
 * whois_person()
 *
 * Inputs	- source_p client to report to
 *		- target_p client to report on
 * Output	- NONE
 * Side Effects	- 
 */
static void whois_person(struct Client *source_p,struct Client *target_p, int glob)
{
  char buf[BUFSIZE];
  char *chname;
  char *server_name;
  dlink_node  *lp;
  struct Client *a2client_p;
  struct Channel *chptr;
  int cur_len = 0;
  int mlen;
  char *t;
  int tlen;
  int reply_to_send = NO;
  struct hook_mfunc_data hd;
  
  a2client_p = find_server(target_p->user->server);
          
  sendto_one(source_p, form_str(RPL_WHOISUSER), me.name,
	 source_p->name, target_p->name,
	 target_p->username, target_p->host, target_p->info);
  server_name = (char *)target_p->user->server;

  ircsprintf(buf, form_str(RPL_WHOISCHANNELS),
	       me.name, source_p->name, target_p->name, "");

  mlen = strlen(buf);
  cur_len = mlen;
  t = buf + mlen;

  DLINK_FOREACH(lp, target_p->user->channel.head)
    {
      chptr = lp->data;
      chname = chptr->chname;


          if ((cur_len + strlen(chname) + 2) > (BUFSIZE - 4))
            {
              sendto_one(source_p, "%s", buf);
              cur_len = mlen;
              t = buf + mlen;
            }

	  if (chptr->mode.mode & MODE_HIDEOPS && !is_chan_op(chptr,source_p))
            {
              ircsprintf(t,"%s ",chname);
            }
          else
            {
              ircsprintf(t,"%s%s ", channel_chanop_or_voice(chptr,target_p),
                       chname);
	    }

	  tlen = strlen(t);
	  t += tlen;
	  cur_len += tlen;
	  reply_to_send = YES;

	}

  if (reply_to_send)
    sendto_one(source_p, "%s", buf);
          
  if ((IsOper(source_p) || !ConfigServerHide.hide_servers) || target_p == source_p)
    sendto_one(source_p, form_str(RPL_WHOISSERVER),
	       me.name, source_p->name, target_p->name, server_name,
	       a2client_p?a2client_p->info:"*Not On This Net*");
  else
    sendto_one(source_p, form_str(RPL_WHOISSERVER),
	       me.name, source_p->name, target_p->name,
               ServerInfo.network_name,
	       ServerInfo.network_desc);

  if (target_p->user->away)
    sendto_one(source_p, form_str(RPL_AWAY), me.name,
	       source_p->name, target_p->name, target_p->user->away);

  if (IsOper(target_p))
    {
      sendto_one(source_p, form_str(RPL_WHOISOPERATOR),
		 me.name, source_p->name, target_p->name);

      if (IsAdmin(target_p))
	sendto_one(source_p, form_str(RPL_WHOISADMIN),
		   me.name, source_p->name, target_p->name);
    }

  if ( (glob == 1) || (MyConnect(target_p) && (IsOper(source_p) ||
       !ConfigServerHide.hide_servers)) || (target_p == source_p) )
    {
      sendto_one(source_p, form_str(RPL_WHOISACTUALLY),
  		 me.name, source_p->name, target_p->name,
        	 target_p->username, target_p->host,
                 show_ip(source_p, target_p) ? 
		 target_p->localClient->sockhost : "255.255.255.255");

      sendto_one(source_p, form_str(RPL_WHOISIDLE),
                 me.name, source_p->name, target_p->name,
                 CurrentTime - target_p->user->last,
                 target_p->firsttime);
    }

  hd.client_p = target_p;
  hd.source_p = source_p;

/* although we should fill in parc and parv, we don't ..
	 be careful of this when writing whois hooks */
  if(MyClient(source_p)) 
    hook_call_event("doing_whois", &hd);
  
  return;
}

void
conf_set_oper_spy(void *data)
{
  int yesno = *(int*) data;

  if(yesno)
    yy_achead->port |= OPER_SPY;
  else
    yy_achead->port &= ~OPER_SPY;
}

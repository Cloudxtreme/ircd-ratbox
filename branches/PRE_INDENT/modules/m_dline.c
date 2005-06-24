/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_dline.c: Bans a user.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002 ircd-ratbox development team
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
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "send.h"
#include "hash.h"
#include "handlers.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void mo_dline(struct Client *,struct Client *,int,char **);

struct Message dline_msgtab = {
  "DLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_not_oper, m_error, mo_dline}
};

#ifndef STATIC_MODULES

void
_modinit(void)
{
  mod_add_cmd(&dline_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&dline_msgtab);
}
const char *_version = "$Revision$";
#endif

/* Local function prototypes */

static time_t valid_tkline(struct Client *source_p, char *string);
static int valid_comment(const char *comment);

char user[USERLEN+2];
char host[HOSTLEN+2];

/*
 * mo_dline
 *
 * inputs	- pointer to server
 *		- pointer to client
 *		- parameter count
 *		- parameter list
 * output	-
 * side effects - D line is added
 *
 */
static void
mo_dline(struct Client *client_p, struct Client *source_p,
	 int parc, char *parv[])
{
  char *dlhost, *oper_reason;
  const char *reason = "<No Reason>";
#ifndef IPV6
  char *p;
  struct Client *target_p;
#endif
  struct irc_inaddr daddr;
  char cidr_form_host[HOSTLEN + 1];
  struct ConfItem *aconf;
  int bits, t;
  char dlbuffer[1024];
  const char* current_date;
  int tdline_time = 0;
  int loc = 0;

  if (!IsOperK(source_p))
    {
      sendto_one(source_p,":%s NOTICE %s :You need kline = yes;",
		 me.name, parv[0]);
      return;
    }

  loc++;

  tdline_time = valid_tkline(source_p, parv[loc]);

  if(tdline_time == -1)
    return;
  else if(tdline_time) 
    loc++;

  if(parc < loc+1)
  {
    sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
	       me.name, source_p->name, "DLINE");
    return;
  }
  
  dlhost = parv[loc];
  strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));

  if ((t=parse_netmask(dlhost, NULL, &bits)) == HM_HOST)
  {
#ifdef IPV6
   sendto_one(source_p, ":%s NOTICE %s :Sorry, please supply an address.",
              me.name, parv[0]);
   return;
#else
      if (!(target_p = find_chasing(source_p, parv[loc], NULL)))
        return;

      if(!target_p->user)
        return;
      t = HM_IPV4;
      if (IsServer(target_p))
        {
          sendto_one(source_p,
                     ":%s NOTICE %s :Can't DLINE a server silly",
                     me.name, parv[0]);
          return;
        }
              
      if (!MyConnect(target_p))
        {
          sendto_one(source_p,
                     ":%s NOTICE %s :Can't DLINE nick on another server",
                     me.name, parv[0]);
          return;
        }

      if (IsExemptKline(target_p))
        {
          sendto_one(source_p,
                     ":%s NOTICE %s :%s is E-lined",me.name,parv[0],
                     target_p->name);
          return;
        }

      /*
       * XXX - this is always a fixed length output, we can get away
       * with strcpy here
       *
       * strncpy_irc(cidr_form_host, inetntoa((char *)&target_p->ip), 32);
       * cidr_form_host[32] = '\0';
       */
       strcpy(cidr_form_host, inetntoa((char*) &target_p->localClient->ip));
      
       if ((p = strchr(cidr_form_host,'.')) == NULL)
        return;
      /* 192. <- p */

      p++;
      if ((p = strchr(p,'.')) == NULL)
        return;
      /* 192.168. <- p */

      p++;
      if ((p = strchr(p,'.')) == NULL)
        return;
      /* 192.168.0. <- p */

      p++;
      *p++ = '0';
      *p++ = '/';
      *p++ = '2';
      *p++ = '4';
      *p++ = '\0';
      dlhost = cidr_form_host;

      bits = 0xFFFFFF00UL;
/* XXX: Fix me for IPV6 */
#endif
    }

  loc++;

  if (parc >= loc+1) /* host :reason */
    {
      if (!valid_comment(parv[loc]))
	return;

      if(*parv[loc])
        reason = parv[loc];
      else
        reason = "No reason";
    }
  else
    reason = "No reason";


  if (IsOperAdmin(source_p))
    {
      if (bits < 8)
	{
	  sendto_one(source_p,
	":%s NOTICE %s :For safety, bitmasks less than 8 require conf access.",
		     me.name, parv[0]);
	  return;
	}
    }
  else
    {
      if (bits < 24)
	{
	  sendto_one(source_p,
	     ":%s NOTICE %s :Dline bitmasks less than 24 are for admins only.",
		     me.name, parv[0]);
	  return;
	}
    }

#ifdef IPV6
  if (t == HM_IPV6)
    t = AF_INET6;
  else
#endif
  t = AF_INET;
  if (ConfigFileEntry.non_redundant_klines)
    {
      const char *creason;
      (void)parse_netmask(dlhost, &daddr, NULL);

      if((aconf = find_dline(&daddr, t)) != NULL)
	{
	  creason = aconf->passwd ? aconf->passwd : "<No Reason>";
	  if (IsConfExemptKline(aconf))
	    sendto_one(source_p,
		       ":%s NOTICE %s :[%s] is (E)d-lined by [%s] - %s",
		       me.name, parv[0], dlhost, aconf->host, creason);
	  else
	    sendto_one(source_p,
		       ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
		       me.name, parv[0], dlhost, aconf->host, creason);
	  return;
	}
    }

  set_time();
  current_date = smalldate();

  aconf = make_conf();

  /* Look for an oper reason */
  if ((oper_reason = strchr(reason, '|')) != NULL)
    {
      *oper_reason = '\0';
      oper_reason++;
    }

  aconf->status = CONF_DLINE;
  DupString(aconf->host, dlhost);
  
  if(tdline_time)
  {
    ircsprintf(dlbuffer, "Temporary D-line %d min. - %s (%s)",
	       (int)(tdline_time/60), reason, current_date);
    DupString(aconf->passwd, dlbuffer);
    aconf->hold = CurrentTime + tdline_time;
    add_temp_dline(aconf);

    if(BadPtr(oper_reason))
    {
      sendto_realops_flags(UMODE_ALL, L_ALL,
		           "%s added temporary %d min. D-Line for [%s] [%s]",
		           get_oper_name(source_p), tdline_time/60, 
		           aconf->host, reason);
      ilog(L_TRACE, "%s added temporary %d min. D-Line for [%s] [%s]",
           source_p->name, tdline_time/60, aconf->host, reason);
    }
    else
    {
      sendto_realops_flags(UMODE_ALL, L_ALL,
                           "%s added temporary %d min. D-Line for [%s] [%s|%s]",
                           get_oper_name(source_p), tdline_time/60,
                           aconf->host, reason, oper_reason);
      ilog(L_TRACE, "%s added temporary %d min. D-Line for [%s] [%s|%s]",
           source_p->name, tdline_time/60, aconf->host, reason, oper_reason);
    }

    sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. D-Line for [%s]",
               me.name, source_p->name, tdline_time/60, aconf->host);
  }
  else
  {
    ircsprintf(dlbuffer, "%s (%s)",reason, current_date);
    DupString(aconf->passwd, dlbuffer);
    add_conf_by_address(aconf->host, CONF_DLINE, NULL, aconf);
    write_confitem(DLINE_TYPE, source_p, NULL, aconf->host, reason,
                   oper_reason, current_date, 0);
  }

  check_dlines();
}

/*
 * valid_tkline()
 * 
 * inputs       - pointer to client requesting kline
 *              - argument count
 *              - pointer to ascii string in
 * output       - -1 not enough parameters
 *              - 0 if not an integer number, else the number
 * side effects - none
 */
static time_t
valid_tkline(struct Client *source_p, char *p)
{
  time_t result = 0;

  while(*p)
    {
      if(IsDigit(*p))
        {
          result *= 10;
          result += ((*p) & 0xF);
          p++;
        }
      else
        return(0);
    }
  /* in the degenerate case where oper does a /quote kline 0 user@host :reason 
   * i.e. they specifically use 0, I am going to return 1 instead
   * as a return value of non-zero is used to flag it as a temporary kline
   */

  if(result == 0)
    result = 1;

  if(result > (24*60*7*4))
    result = (24*60*7*4); /* Max it at 4 weeks */

  result = (time_t)result * (time_t)60;  /* turn it into seconds */

  return(result);
}

/*
 * valid_comment
 * inputs	- pointer to client
 *              - pointer to comment
 * output       - 0 if no valid comment, 1 if valid
 * side effects - NONE
 */
static int
valid_comment(const char *comment)
{
  if(strchr(comment, '"'))
    return 0;

  return 1;
}


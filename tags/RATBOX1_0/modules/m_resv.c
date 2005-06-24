/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_resv.c: Reserves(jupes) a nickname or channel.
 *
 *  Copyright (C) 2001-2002 Hybrid Development Team
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
#include "handlers.h"
#include "client.h"
#include "channel.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "resv.h"
#include "hash.h"
#include "s_log.h"
#include "sprintf_irc.h"

static void mo_resv(struct Client *, struct Client *, int, char **);
static void mo_unresv(struct Client *, struct Client *, int, char **);

struct Message resv_msgtab = {
  "RESV", 0, 0, 3, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {m_ignore, m_not_oper, m_ignore, mo_resv}
};

struct Message unresv_msgtab = {
  "UNRESV", 0, 0, 2, 0, MFLG_SLOW | MFLG_UNREG, 0,
  {m_ignore, m_not_oper, m_ignore, mo_unresv}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&resv_msgtab);
  mod_add_cmd(&unresv_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&resv_msgtab);
  mod_del_cmd(&unresv_msgtab);
}

const char *_version = "$Revision$";
#endif

/*
 * mo_resv()
 *      parv[0] = sender prefix
 *      parv[1] = channel/nick to forbid
 */
static void mo_resv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  if(BadPtr(parv[1]) || BadPtr(parv[2]))
  {
    sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
               me.name, source_p->name, "RESV");
    return;
  }

  if(IsChannelName(parv[1]))
  {
    struct ResvEntry *resv_p;
    
    resv_p = create_resv(parv[1], parv[2], RESV_CHANNEL);
  
    if(resv_p == NULL)
    {
      sendto_one(source_p,
                 ":%s NOTICE %s :A RESV has already been placed on channel: %s",
                 me.name, source_p->name, parv[1]);
      return;
    }
    
    write_confitem(RESV_TYPE, source_p, NULL, resv_p->name, resv_p->reason,
                   NULL, NULL, 0);
  }
  else if(clean_resv_nick(parv[1]))
  {
    struct ResvEntry *resv_p;

    if(!IsOperAdmin(source_p) && (strchr(parv[1], '*') || strchr(parv[1], '?')))
    {
      sendto_one(source_p,
                 ":%s NOTICE %s :You must be an admin to perform a wildcard RESV",
		 me.name, source_p->name);
      return;
    }

    resv_p = create_resv(parv[1], parv[2], RESV_NICK);

    if(resv_p == NULL)
    {
      sendto_one(source_p,
                 ":%s NOTICE %s :A RESV has already been placed on nick: %s",
		 me.name, source_p->name, parv[1]);
      return;
    }

    write_confitem(RESV_TYPE, source_p, NULL, resv_p->name, resv_p->reason,
                    NULL, NULL, 0);
  }			 
  else
    sendto_one(source_p, 
              ":%s NOTICE %s :You have specified an invalid resv: [%s]",
	      me.name, source_p->name, parv[1]);
}

/*
 * mo_unresv()
 *     parv[0] = sender prefix
 *     parv[1] = channel/nick to unforbid
 */
static void mo_unresv(struct Client *client_p, struct Client *source_p,
                      int parc, char *parv[])
{
  FBFILE *in, *out;
  char buf[BUFSIZE];
  char buff[BUFSIZE];
  char temppath[BUFSIZE];
  const char *filename;
  mode_t oldumask;
  char *p;
  int error_on_write = 0;
  int found_resv = 0;

  if(BadPtr(parv[1]))
  {
    sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
               me.name, source_p->name, "RESV");
    return;
  }

  ircsprintf(temppath, "%s.tmp", ConfigFileEntry.resvfile);
  filename = get_conf_name(RESV_TYPE);

  if((in = fbopen(filename, "r")) == NULL)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, source_p->name, filename);
    return;
  }

  oldumask = umask(0);

  if((out = fbopen(temppath, "w")) == NULL)
  {
    sendto_one(source_p, ":%s NOTICE %s :Cannot open %s",
               me.name, source_p->name, temppath);
    fbclose(in);
    umask(oldumask);
    return;
  }

  umask(oldumask);

  while(fbgets(buf, sizeof(buf), in))
  {
    char *resv_name;

    if(error_on_write)
    {
      if(temppath != NULL)
        (void) unlink(temppath);

      break;
    }

    strlcpy(buff, buf, sizeof(buff));

    if((p = strchr(buff, '\n')) != NULL)
      *p = '\0';

    if((*buff == '\0') || (*buff == '#'))
    {
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
      continue;
    }

    if((resv_name = getfield(buff)) == NULL)
    {
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
      continue;
    }

    if(irccmp(resv_name, parv[1]) == 0)
    {
      found_resv++;
    }
    else
    {
      error_on_write = (fbputs(buf, out) < 0) ? YES : NO;
    }
  }

  fbclose(in);
  fbclose(out);

  if(!error_on_write)
  {
    (void) rename(temppath, filename);
    rehash(0);
  }
  else
  {
    sendto_one(source_p,
               ":%s NOTICE %s :Couldn't write temp resv file, aborted",
               me.name, source_p->name);
    return;
  }

  if(!found_resv)
  {
    sendto_one(source_p, ":%s NOTICE %s :No RESV for %s",
               me.name, source_p->name, parv[1]);
    return;
  }

  sendto_one(source_p, ":%s NOTICE %s :RESV for [%s] is removed",
             me.name, source_p->name, parv[1]);
  sendto_realops_flags(UMODE_ALL, L_ALL,
                       "%s has removed the RESV for: [%s]",
                       get_oper_name(source_p), parv[1]);
  ilog(L_NOTICE, "%s has removed the RESV for [%s]",
       get_oper_name(source_p), parv[1]);
}

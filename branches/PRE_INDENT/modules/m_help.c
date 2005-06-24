/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_help.c: Provides help information to a user/operator.
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
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "motd.h"
#include "ircd_handler.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_log.h"
#include "parse.h"
#include "modules.h"

static void m_help(struct Client*, struct Client*, int, char**);
static void mo_help(struct Client*, struct Client*, int, char**);
static void mo_uhelp(struct Client*, struct Client*, int, char**);
static void dohelp(struct Client *, const char *, const char *, const char *);
static void sendhelpfile(struct Client *, const char *, const char *, const char *);

struct Message help_msgtab = {
  "HELP", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_help, m_ignore, mo_help}
};

struct Message uhelp_msgtab = {
  "UHELP", 0, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_help, m_ignore, mo_uhelp}
};
#ifndef STATIC_MODULES

  void
_modinit(void)
{
  mod_add_cmd(&help_msgtab);
  mod_add_cmd(&uhelp_msgtab);
}

  void
_moddeinit(void)
{
  mod_del_cmd(&help_msgtab);
  mod_del_cmd(&uhelp_msgtab);
}

const char *_version = "$Revision$";
#endif
/*
 * m_help - HELP message handler
 *      parv[0] = sender prefix
 */
static void
m_help(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
  static time_t last_used = 0;

  if(ConfigFileEntry.use_help)
  {
    /* HELP is always local */
    if ((last_used + ConfigFileEntry.pace_wait_simple) > CurrentTime)
    {
      /* safe enough to give this on a local connect only */
      sendto_one(source_p,form_str(RPL_LOAD2HI),me.name,parv[0]);
        return;
    }
    else
    {
      last_used = CurrentTime;
    }

    if(parc > 1)
      dohelp(source_p, UHPATH, parv[1], parv[0]);
    else
      dohelp(source_p, UHPATH, NULL, parv[0]);
  }
  else
  {
    list_commands(source_p);
  }
}

/*
 * mo_help - HELP message handler
 *      parv[0] = sender prefix
 */
static void
mo_help(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[])
{
  if(parc > 1)
    dohelp(source_p, HPATH, parv[1], parv[0]);
  else
    dohelp(source_p, HPATH, NULL, parv[0]);
}

/*
 * mo_uhelp - HELP message handler
 * This is used so that opers can view the user help file without deopering
 *      parv[0] = sender prefix
 */

static void
mo_uhelp(struct Client *client_p, struct Client *source_p,
            int parc, char *parv[])
{
  if(parc > 1)
    dohelp(source_p, UHPATH, parv[1], parv[0]);
  else
    dohelp(source_p, UHPATH, NULL, parv[0]);
}

static void
dohelp(struct Client *source_p, const char *hpath,
                   const char *topic, const char *nick)
{
  char path[MAXPATHLEN + 1];
  char ntopic[25];
  struct stat sb;
  int i;

  if (topic != NULL)
  {
    /* convert to lower case */
    for (i = 0; topic[i] != '\0' && i < sizeof(ntopic) - 1; i++)
    {
      ntopic[i] = ToLower(topic[i]);
    }
  }
  else
    strcpy(ntopic,"index"); /* list available help topics */

  if (strchr(topic, '/'))
  {
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, ntopic);
    return;
  }

  if (strlen(hpath) + strlen(topic) + 1 > MAXPATHLEN)
  {
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, ntopic);
    return;
  }

  sprintf(path, "%s/%s", hpath, ntopic);

  if (stat(path, &sb) < 0)
  {
    ilog(L_NOTICE, "help file %s not found", path);
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, ntopic);
    return;
  }

  if (!S_ISREG(sb.st_mode))
  {
    ilog(L_NOTICE, "help file %s not found", path);
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, ntopic);
    return;
  }

  sendhelpfile(source_p, path, ntopic, nick);
  return;
}

static void 
sendhelpfile(struct Client *source_p, const char *path,
                         const char *topic, const char *nick)
{
  FBFILE *file;
  char line[HELPLEN];

  if ((file = fbopen(path, "r")) == NULL)
  {
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
    return;
  }

  if (fbgets(line, sizeof(line), file) == NULL)
  {
    sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
    return;
  }

  sendto_one(source_p, form_str(RPL_HELPSTART), me.name, nick, topic, line);

  while (fbgets(line, sizeof(line), file))
  {
    sendto_one(source_p, form_str(RPL_HELPTXT), me.name, nick, topic, line);
  }

  sendto_one(source_p, form_str(RPL_ENDOFHELP), me.name, nick, topic);
  return;
}


/*
 * modules/m_help.c
 * Copyright (C) 2001 Hybrid Development Team
 *
 *   $Id$
 */
 
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "motd.h"
#include "ircd_handler.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "parse.h"
#include "modules.h"

static void m_help(struct Client*, struct Client*, int, char**);
static void mo_help(struct Client*, struct Client*, int, char**);
static void mo_uhelp(struct Client*, struct Client*, int, char**);
static void dohelp(struct Client *, char *, char *, char *);
static void sendhelpfile(struct Client *, char *, char *, char *);

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

char *_version = "20001122";
#endif
/*
 * m_help - HELP message handler
 *      parv[0] = sender prefix
 */
static void m_help(struct Client *client_p, struct Client *source_p,
                  int parc, char *parv[])
{
  static time_t last_used = 0;

  /* HELP is always local */
  if ((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
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
    dohelp(source_p, UHPATH, "index", parv[0]);
}

/*
 * mo_help - HELP message handler
 *      parv[0] = sender prefix
 */
static void mo_help(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
  if(parc > 1)
    dohelp(source_p, HPATH, parv[1], parv[0]);
  else
    dohelp(source_p, HPATH, "index", parv[0]);
}

/*
 * mo_uhelp - HELP message handler
 * This is used so that opers can view the user help file without deopering
 *      parv[0] = sender prefix
 */

static void mo_uhelp(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
  if(parc > 1)
    dohelp(source_p, UHPATH, parv[1], parv[0]);
  else
    dohelp(source_p, UHPATH, "", parv[0]);
}

static void dohelp(source_p, hpath, topic, nick)
	 struct Client *source_p;
	 char *hpath, *topic, *nick;
{
  char path[MAXPATHLEN + 1];
  struct stat sb;

  if (strchr(topic, '/'))
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}

  if (strlen(hpath) + strlen(topic) + 1 > MAXPATHLEN)
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}

  sprintf(path, "%s/%s", hpath, topic);

  if (stat(path, &sb) < 0)
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}

  if (!S_ISREG(sb.st_mode))
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}
    
  sendhelpfile(source_p, path, topic, nick);
  return;
}

static void sendhelpfile(source_p, path, topic, nick)
	 struct Client *source_p;
	 char *path, *topic, *nick;
{
  FILE *file;
  char line[HELPLEN];

  if ((file = fopen(path, "r")) == NULL)
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}

  if (fgets(line, sizeof(line), file) == NULL)
	{
	  sendto_one(source_p, form_str(ERR_HELPNOTFOUND), me.name, nick, topic);
	  return;
	}

  sendto_one(source_p, form_str(RPL_HELPSTART), me.name, nick, topic, line);

  while (fgets(line, sizeof(line), file))
	{
	  sendto_one(source_p, form_str(RPL_HELPTXT), me.name, nick, topic, line);
	}

  sendto_one(source_p, form_str(RPL_ENDOFHELP), me.name, nick, topic);
  return;
}


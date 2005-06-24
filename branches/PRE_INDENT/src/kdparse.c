/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  kdparse.c: Parses K and D lines.
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
#include "s_log.h"
#include "s_conf.h"
#include "hostmask.h"
#include "client.h"
#include "irc_string.h"
#include "memory.h"
#include "resv.h"

/*
 * parse_k_file
 * Inputs       - pointer to line to parse
 * Output       - NONE
 * Side Effects - Parse one new style K line
 */

void
parse_k_file(FBFILE *file)
{
  struct ConfItem *aconf;
  char* user_field=NULL;
  char* reason_field=NULL;
  char* host_field=NULL;
  char  line[BUFSIZE];
  char* p;

  while (fbgets(line, sizeof(line), file))
    {
      if ((p = strchr(line, '\n')) != NULL)
        *p = '\0';

      if ((*line == '\0') || (*line == '#'))
        continue;

      user_field = getfield(line);
      if(BadPtr(user_field))
	continue;

      host_field = getfield(NULL);
      if(BadPtr(host_field))
	continue;

      reason_field = getfield(NULL);
      if(BadPtr(reason_field))
	continue;
	  
      aconf = make_conf();
      aconf->status = CONF_KILL;
      conf_add_fields(aconf,host_field,reason_field,user_field,0,NULL);

      if (aconf->host != NULL)
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, aconf);
    }
}

/*
 * parse_d_file
 * Inputs       - pointer to line to parse
 * Output       - NONE
 * Side Effects - Parse one new style D line
 */

void parse_d_file(FBFILE *file)
{
  struct ConfItem *aconf;
  char* reason_field=NULL;
  char* host_field=NULL;
  char  line[BUFSIZE];
  char* p;

  while (fbgets(line, sizeof(line), file))
    {
      if ((p = strchr(line, '\n')))
        *p = '\0';

      if ((*line == '\0') || (line[0] == '#'))
        continue;

      host_field = getfield(line);
      if(BadPtr(host_field))
	continue;

      reason_field = getfield(NULL);
      if(BadPtr(reason_field))
	continue;
	  
      aconf = make_conf();
      aconf->status = CONF_DLINE;
      conf_add_fields(aconf,host_field,reason_field,"",0,NULL);
      conf_add_d_conf(aconf);
    }
}

void
parse_x_file(FBFILE *file)
{
  struct xline *xconf;
  char *reason_field = NULL;
  char *host_field = NULL;
  char *port_field = NULL;
  char line[BUFSIZE];
  char *p;

  while(fbgets(line, sizeof(line), file))
  {
    if((p = strchr(line, '\n')))
      *p = '\0';

    if((*line == '\0') || (line[0] == '#'))
      continue;

    host_field = getfield(line);
    if(BadPtr(host_field))
      continue;

    port_field = getfield(NULL);
    if(BadPtr(port_field))
      continue;

    reason_field = getfield(NULL);
    if(BadPtr(reason_field))
      continue;
	  
    xconf = make_xline(host_field, reason_field, atoi(port_field));
    dlinkAddAlloc(xconf, &xline_list);
  }
}

void
parse_resv_file(FBFILE *file)
{
  char *reason_field;
  char *host_field;
  char line[BUFSIZE];
  char *p;

  while(fbgets(line, sizeof(line), file))
  {
    if((p = strchr(line, '\n')))
      *p = '\0';

    if((*line == '\0') || (line[0] == '#'))
      continue;

    host_field = getfield(line);
    if(BadPtr(host_field))
      continue;

    reason_field = getfield(NULL);
    if(BadPtr(reason_field))
      continue;

    if(IsChannelName(host_field))
      create_resv(host_field, reason_field, RESV_CHANNEL);
    else if(clean_resv_nick(host_field))
      create_resv(host_field, reason_field, RESV_NICK);
  }
}

/*
 * getfield
 *
 * inputs	- input buffer
 * output	- next field
 * side effects	- field breakup for ircd.conf file.
 */
char *getfield(char *newline)
{
  static char *line = NULL;
  char  *end, *field;
        
  if (newline)
    line = newline;

  if (line == NULL)
    return(NULL);

  field = line;

  /* XXX make this skip to first " if present */
  if(*field == '"')
    field++;
  else
    return(NULL);	/* mal-formed field */

  if ((end = strchr(line,',')) == NULL)
    {
      end = line + strlen(line);
      line = NULL;
      /* XXX verify properly terminating " */
      if(*end == '"')
	*end = '\0';
      else
	return(NULL);
    }
  else
    {
      line = end + 1;
      --end;
      if(*end == '"')
	*end = '\0';
      else
	return(NULL);
    }
  return(field);
}


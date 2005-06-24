/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_version.c: Shows ircd version information.
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

#include <stdinc.h>
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static char *confopts(struct Client *source_p);

static int m_version(struct Client *, struct Client *, int, const char **);
static int ms_version(struct Client *, struct Client *, int, const char **);
static int mo_version(struct Client *, struct Client *, int, const char **);

struct Message version_msgtab = {
	"VERSION", 0, 0, 0, 0, MFLG_SLOW, 0,
	{m_unregistered, m_version, ms_version, mo_version}
};

mapi_clist_av1 version_clist[] = { &version_msgtab, NULL };
DECLARE_MODULE_AV1(version, NULL, NULL, version_clist, NULL, NULL, NULL, "$Revision$");

/*
 * m_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static int
m_version(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1 && !ConfigServerHide.disable_remote)
	{
		if((last_used + ConfigFileEntry.pace_wait) > CurrentTime)
		{
			/* safe enough to give this on a local connect only */
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, parv[0], "VERSION");
			return 0;
		}
		else
		{
			last_used = CurrentTime;
		}

		if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) != HUNTED_ISME)
			return 0;
	}

	sendto_one(source_p, form_str(RPL_VERSION), me.name,
		   parv[0], ircd_version, serno, debugmode,
		   me.name, confopts(source_p), serveropts);

	show_isupport(source_p);

	return 0;
}

/*
 * mo_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static int
mo_version(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	sendto_one(source_p, form_str(RPL_VERSION), me.name, parv[0], ircd_version,
		   serno, debugmode, me.name, confopts(source_p), serveropts);

	show_isupport(source_p);

	return 0;
}

/*
 * ms_version - VERSION command handler
 *      parv[0] = sender prefix
 *      parv[1] = remote server
 */
static int
ms_version(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendto_one(source_p, form_str(RPL_VERSION), me.name,
			   parv[0], ircd_version, serno, debugmode,
			   me.name, confopts(source_p), serveropts);
		show_isupport(source_p);
	}

	return 0;
}

/* confopts()
 * input  - client pointer
 * output - ircd.conf option string
 * side effects - none
 */
static char *
confopts(struct Client *source_p)
{
	static char result[15];
	char *p;

	result[0] = '\0';
	p = result;

#ifdef DEBUGMODE
	*p++ = 'D';
#endif

	if(ConfigChannel.use_except)
		*p++ = 'e';

	if(ConfigFileEntry.glines)
		*p++ = 'g';
	*p++ = 'G';

	/* might wanna hide this :P */
	if(ServerInfo.hub && (!ConfigServerHide.disable_remote || IsOper(source_p)))
	{
		*p++ = 'H';
	}

	if(ConfigChannel.use_invex)
		*p++ = 'I';

	if(ConfigChannel.use_knock)
		*p++ = 'K';

	*p++ = 'M';
	*p++ = 'p';

#ifdef IGNORE_BOGUS_TS
	*p++ = 'T';
#endif

#ifdef USE_SYSLOG
	*p++ = 'Y';
#endif

#ifdef HAVE_LIBZ
	*p++ = 'Z';
#endif

#ifdef IPV6
	*p++ = '6';
#endif

	*p = '\0';

	return result;
}

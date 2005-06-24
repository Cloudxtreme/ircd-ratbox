/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * banconf.c - code for dealing with the ban config files
 *
 * Copyright (C) 2004 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */
#include "stdinc.h"
#include "tools.h"
#include "banconf.h"
#include "irc_string.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hostmask.h"
#include "hash.h"

#define MAX_CONF_FIELDS	10

/* getfields()
 *   Given a line, will break it up into parameters, delimited by ',' taking
 *   account of escapes.  Remaining fields are reset to NULL.
 */
static const char **
getfields(char *line)
{
	static char *args[MAX_CONF_FIELDS+1];
	char *s;
	char *p = line;
	int i;

	for(i = 0; i < MAX_CONF_FIELDS && !EmptyString(line); i++)
	{
		s = line;

		/* we need to loop until we find a non-escaped ',' or if the
		 * field is quoted, one that has a quote to the left of it
		 */
		while(1)
		{
			if((p = strchr(s, ',')))
			{
				if(*(p - 1) == '\\')
				{
					s = p + 1;
					continue;
				}

				if(*line == '"')
				{
					if(*(p - 1) != '"')
					{
						s = p + 1;
						continue;
					}

					line++;

					if(*(p - 1) == '"')
						*(p - 1) = '\0';
				}

				p++;
			}
			else if(*line == '"')
			{
				line++;

				if((p = strrchr(line, '"')))
				{
					*p = '\0';
					p = NULL;
				}
			}

			break;
		}

		args[i] = line;
		line = p;
	}

	while(i <= MAX_CONF_FIELDS)
		args[i++] = NULL;

	return((const char **) args);
}

void
read_kline_conf(const char *filename, int perm)
{
	FILE *in;
	struct ConfItem *aconf;
	const char **args;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
	{
		if(!perm)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"Can't open %s file klines could be missing!",
					filename);
			ilog(L_MAIN, "Failed reading kline file %s", filename);
		}

		return;
	}

	while(fgets(line, sizeof(line), in))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if(EmptyString(line) || (*line == '#'))
			continue;

		args = getfields(line);

		/* "user","host","reason","operreason","date","oper",TS */
		if(EmptyString(args[0]) || EmptyString(args[1]) ||
		   EmptyString(args[2]))
			continue;

		aconf = make_conf();
		aconf->status = CONF_KILL;

		if(perm)
			aconf->flags |= CONF_FLAGS_PERMANENT;

		DupString(aconf->user, args[0]);
		DupString(aconf->host, args[1]);
		DupString(aconf->passwd, args[2]);

		if(!EmptyString(args[3]))
			DupString(aconf->spasswd, args[3]);

		add_conf_by_address(aconf->host, CONF_KILL, aconf->user, aconf);
	}

	fclose(in);
}

void
read_dline_conf(const char *filename, int perm)
{
	FILE *in;
	struct ConfItem *aconf;
	const char **args;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
	{
		if(!perm)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"Can't open %s file dlines could be missing!",
					filename);
			ilog(L_MAIN, "Failed reading dline file %s", filename);
		}
		return;
	}

	while(fgets(line, sizeof(line), in))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if(EmptyString(line) || (*line == '#'))
			continue;

		args = getfields(line);

		/* "ip","reason","operreason","date","oper",TS */
		if(EmptyString(args[0]) || EmptyString(args[1]))
			continue;

		if(already_dlined(args[0]))
			continue;

		aconf = make_conf();
		aconf->status = CONF_DLINE;

		if(perm)
			aconf->flags |= CONF_FLAGS_PERMANENT;

		DupString(aconf->host, args[0]);
		DupString(aconf->passwd, args[1]);

		if(!EmptyString(args[2]))
			DupString(aconf->spasswd, args[2]);

		if(parse_netmask(aconf->host, NULL, NULL) == HM_HOST)
		{
			ilog(L_MAIN, "Invalid Dline %s ignored", aconf->host);
			free_conf(aconf);
		}
		else
			add_dline(aconf);
	}

	fclose(in);
}

void
read_xline_conf(const char *filename, int perm)
{
	FILE *in;
	struct ConfItem *aconf;
	const char **args;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
	{
		if(!perm)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"Can't open %s file xlines could be missing!",
					filename);
			ilog(L_MAIN, "Failed reading xline file %s", filename);
		}

		return;
	}

	while(fgets(line, sizeof(line), in))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if(EmptyString(line) || (*line == '#'))
			continue;

		args = getfields(line);

		/* "gecos","type","reason","date","oper",TS */
		if(EmptyString(args[0]) || EmptyString(args[2]))
			continue;

		if(find_xline(args[0], 0) || strchr(args[2], ':'))
			continue;

		aconf = make_conf();
		aconf->status = CONF_XLINE;

		if(perm)
			aconf->flags |= CONF_FLAGS_PERMANENT;

		DupString(aconf->name, args[0]);
		DupString(aconf->passwd, args[2]);

		dlinkAdd(aconf, &aconf->dnode, &xline_conf_list);
	}

	fclose(in);
}

void
read_resv_conf(const char *filename, int perm)
{
	FILE *in;
	struct ConfItem *aconf;
	const char **args;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
	{
		if(!perm)
		{
			sendto_realops_flags(UMODE_ALL, L_ALL,
					"Can't open %s file resvs could be missing!",
					filename);
			ilog(L_MAIN, "Failed reading resv file %s", filename);
		}

		return;
	}

	while(fgets(line, sizeof(line), in))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if(EmptyString(line) || (*line == '#'))
			continue;

		args = getfields(line);

		/* "resv","reason","date","oper",TS */
		if(EmptyString(args[0]) || EmptyString(args[1]))
			continue;

		if(IsChannelName(args[0]))
		{
			if(hash_find_resv(args[0]))
				continue;

			aconf = make_conf();
			aconf->status = CONF_RESV_CHANNEL;

			if(perm)
				aconf->flags |= CONF_FLAGS_PERMANENT;

			DupString(aconf->name, args[0]);
			DupString(aconf->passwd, args[1]);
			add_to_resv_hash(aconf->name, aconf);
		}
		else if(clean_resv_nick(args[0]))
		{
			if(find_nick_resv(args[0]))
				continue;

			aconf = make_conf();
			aconf->status = CONF_RESV_NICK;

			if(perm)
				aconf->flags |= CONF_FLAGS_PERMANENT;

			DupString(aconf->name, args[0]);
			DupString(aconf->passwd, args[1]);
			dlinkAdd(aconf, &aconf->dnode, &resv_conf_list);
		}
	}

	fclose(in);
}

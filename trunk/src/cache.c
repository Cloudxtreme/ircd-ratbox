/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * cache.c - code for caching files
 *
 * Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2003-2005 ircd-ratbox development team
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
#include "ircd_lib.h"
#include "struct.h"
#include "s_conf.h"
#include "client.h"
#include "hash.h"
#include "cache.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"

struct cachefile *user_motd = NULL;
struct cachefile *oper_motd = NULL;
struct cacheline *emptyline = NULL;
dlink_list links_cache_list;
char user_motd_changed[MAX_DATE_STRING];

/* init_cache()
 *
 * inputs	-
 * outputs	-
 * side effects - inits the file/line cache blockheaps, loads motds
 */
void
init_cache(void)
{
	/* allocate the emptyline */
	emptyline = ircd_malloc(sizeof(struct cacheline));
	emptyline->data[0] = ' ';
	emptyline->data[1] = '\0';
	user_motd_changed[0] = '\0';

	user_motd = cache_file(MPATH, "ircd.motd", 0);
	oper_motd = cache_file(OPATH, "opers.motd", 0);
	memset(&links_cache_list, 0, sizeof(links_cache_list));
}

/* cache_file()
 *
 * inputs	- file to cache, files "shortname", flags to set
 * outputs	- pointer to file cached, else NULL
 * side effects -
 */
struct cachefile *
cache_file(const char *filename, const char *shortname, int flags)
{
	FILE *in;
	struct cachefile *cacheptr;
	struct cacheline *lineptr;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
		return NULL;


	cacheptr = ircd_malloc(sizeof(struct cachefile));

	ircd_strlcpy(cacheptr->name, shortname, sizeof(cacheptr->name));
	cacheptr->flags = flags;

	/* cache the file... */
	while(fgets(line, sizeof(line), in) != NULL)
	{
		if((p = strpbrk(line, "\r\n")) != NULL)
			*p = '\0';

		if(!EmptyString(line))
		{
			lineptr = ircd_malloc(sizeof(struct cacheline));
			ircd_strlcpy(lineptr->data, line, sizeof(lineptr->data));
			ircd_dlinkAddTail(lineptr, &lineptr->linenode, &cacheptr->contents);
		}
		else
			ircd_dlinkAddTailAlloc(emptyline, &cacheptr->contents);
	}

	fclose(in);
	return cacheptr;
}

void
cache_links(void *unused)
{
	struct Client *target_p;
	dlink_node *ptr;
	dlink_node *next_ptr;
	char *links_line;

	DLINK_FOREACH_SAFE(ptr, next_ptr, links_cache_list.head)
	{
		ircd_free(ptr->data);
		ircd_free_dlink_node(ptr);
	}

	links_cache_list.head = links_cache_list.tail = NULL;
	links_cache_list.length = 0;

	DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		/* skip ourselves (done in /links) and hidden servers */
		if(IsMe(target_p) ||
		   (IsHidden(target_p) && !ConfigServerHide.disable_hidden))
			continue;

		/* if the below is ever modified, change LINKSLINELEN */
		links_line = ircd_malloc(LINKSLINELEN);
		ircd_snprintf(links_line, LINKSLINELEN, "%s %s :1 %s",
			   target_p->name, me.name, 
			   target_p->info[0] ? target_p->info : 
			    "(Unknown Location)");

		ircd_dlinkAddTailAlloc(links_line, &links_cache_list);
	}
}

/* free_cachefile()
 *
 * inputs	- cachefile to free
 * outputs	-
 * side effects - cachefile and its data is free'd
 */
void
free_cachefile(struct cachefile *cacheptr)
{
	dlink_node *ptr;
	dlink_node *next_ptr;

	if(cacheptr == NULL)
		return;

	DLINK_FOREACH_SAFE(ptr, next_ptr, cacheptr->contents.head)
	{
		if(ptr->data != emptyline)
			ircd_free(ptr->data);
	}

	ircd_free(cacheptr);
}

/* load_help()
 *
 * inputs	-
 * outputs	-
 * side effects - contents of help directories are loaded.
 */
void
load_help(void)
{
	DIR *helpfile_dir = NULL;
	struct dirent *ldirent= NULL;
	char filename[MAXPATHLEN];
	struct cachefile *cacheptr;

#if defined(S_ISLNK) && defined(HAVE_LSTAT)
	struct stat sb;
#endif

	/* opers must be done first */
	helpfile_dir = opendir(HPATH);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		ircd_snprintf(filename, sizeof(filename), "%s/%s", HPATH, ldirent->d_name);
		cacheptr = cache_file(filename, ldirent->d_name, HELP_OPER);
		add_to_help_hash(cacheptr->name, cacheptr);
	}

	closedir(helpfile_dir);
	helpfile_dir = opendir(UHPATH);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		ircd_snprintf(filename, sizeof(filename), "%s/%s", UHPATH, ldirent->d_name);

#if defined(S_ISLNK) && defined(HAVE_LSTAT)
		if(lstat(filename, &sb) < 0)
			continue;

		/* ok, if its a symlink, we work on the presumption if an
		 * oper help exists of that name, its a symlink to that --fl
		 */
		if(S_ISLNK(sb.st_mode))
		{
			cacheptr = hash_find_help(ldirent->d_name, HELP_OPER);

			if(cacheptr != NULL)
			{
				cacheptr->flags |= HELP_USER;
				continue;
			}
		}
#endif

		cacheptr = cache_file(filename, ldirent->d_name, HELP_USER);
		add_to_help_hash(cacheptr->name, cacheptr);
	}

	closedir(helpfile_dir);
}

/* send_user_motd()
 *
 * inputs	- client to send motd to
 * outputs	- client is sent motd if exists, else ERR_NOMOTD
 * side effects -
 */
void
send_user_motd(struct Client *source_p)
{
	struct cacheline *lineptr;
	dlink_node *ptr;
	const char *myname = get_id(&me, source_p);
	const char *nick = get_id(source_p, source_p);

	if(user_motd == NULL || ircd_dlink_list_length(&user_motd->contents) == 0)
	{
		sendto_one(source_p, POP_QUEUE, form_str(ERR_NOMOTD), myname, nick);
		return;
	}

	sendto_one(source_p, HOLD_QUEUE, form_str(RPL_MOTDSTART), myname, nick, me.name);

	DLINK_FOREACH(ptr, user_motd->contents.head)
	{
		lineptr = ptr->data;
		sendto_one(source_p, HOLD_QUEUE, form_str(RPL_MOTD), myname, nick, lineptr->data);
	}

	sendto_one(source_p, POP_QUEUE, form_str(RPL_ENDOFMOTD), myname, nick);
}

void
cache_user_motd(void)
{
	struct stat sb;
	struct tm *local_tm;
	
	if(stat(MPATH, &sb) == 0) 
	{
		local_tm = localtime(&sb.st_mtime);

		if(local_tm != NULL) 
		{
			ircd_snprintf(user_motd_changed, sizeof(user_motd_changed),
				 "%d/%d/%d %d:%d",
				 local_tm->tm_mday, local_tm->tm_mon + 1,
				 1900 + local_tm->tm_year, local_tm->tm_hour,
				 local_tm->tm_min);
		}
	} 
	free_cachefile(user_motd);
	user_motd = cache_file(MPATH, "ircd.motd", 0);
}


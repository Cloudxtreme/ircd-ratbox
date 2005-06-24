/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * help.c - code for dealing with conf stuff like k/d/x lines
 *
 * Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2003 ircd-ratbox development team
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
#include "ircd_defs.h"
#include "common.h"
#include "s_conf.h"
#include "tools.h"
#include "client.h"
#include "memory.h"
#include "balloc.h"
#include "event.h"
#include "hash.h"
#include "help.h"

static BlockHeap *helpfile_heap = NULL;
static BlockHeap *helpline_heap = NULL;

const char emptyline[] = " ";

void
init_help(void)
{
	helpfile_heap = BlockHeapCreate(sizeof(struct helpfile), HELPFILE_HEAP_SIZE);
	helpline_heap = BlockHeapCreate(sizeof(struct helpline), HELPLINE_HEAP_SIZE);
}

static void
load_help_file(const char *hfname, const char *helpname, int flags)
{
	FBFILE *in;
	struct helpfile *hptr;
	struct helpline *lineptr;
	char line[BUFSIZE];
	char *p;

	if((in = fbopen(hfname, "r")) == NULL)
		return;

	if(fbgets(line, sizeof(line), in) == NULL)
		return;

	if((p = strchr(line, '\n')) != NULL)
		*p = '\0';

	hptr = BlockHeapAlloc(helpfile_heap);
	memset(hptr, 0, sizeof(struct helpfile));

	strlcpy(hptr->helpname, helpname, sizeof(hptr->helpname));
	strlcpy(hptr->firstline, line, sizeof(hptr->firstline));
	hptr->flags = flags;

	while(fbgets(line, sizeof(line), in) != NULL)
	{
		if((p = strchr(line, '\n')) != NULL)
			*p = '\0';

		if(!EmptyString(line))
		{
			lineptr = BlockHeapAlloc(helpline_heap);
			strlcpy(lineptr->data, line, sizeof(lineptr->data));
			dlinkAddTail(lineptr, &lineptr->linenode, &hptr->contents);
		}
		else
			dlinkAddTailAlloc((char *) emptyline, &hptr->contents);
	}

	fbclose(in);

	if(dlink_list_length(&hptr->contents) > 0)
		add_to_help_hash(helpname, hptr);
	else
		free_help(hptr);

}

void
load_help(void)
{
	DIR *helpfile_dir = NULL;
	struct dirent *ldirent= NULL;
	char filename[MAXPATHLEN];
#if defined(S_ISLNK) && defined(HAVE_LSTAT)
	struct stat sb;
#endif

	/* opers must be done first */
	helpfile_dir = opendir(HPATH);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		snprintf(filename, sizeof(filename), "%s/%s", HPATH, ldirent->d_name);
		load_help_file(filename, ldirent->d_name, HELP_OPER);
	}

	helpfile_dir = opendir(UHPATH);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		snprintf(filename, sizeof(filename), "%s/%s", UHPATH, ldirent->d_name);

#if defined(S_ISLNK) && defined(HAVE_LSTAT)
		if(lstat(filename, &sb) < 0)
			continue;

		/* ok, if its a symlink, we work on the presumption if an
		 * oper help exists of that name, its a symlink to that --fl
		 */
		if(S_ISLNK(sb.st_mode))
		{
			struct helpfile *hptr = hash_find_help(ldirent->d_name, HELP_OPER);

			if(hptr != NULL)
			{
				hptr->flags |= HELP_USER;
				continue;
			}
		}
#endif

		load_help_file(filename, ldirent->d_name, HELP_USER);
	}

	closedir(helpfile_dir);
}

void
free_help(struct helpfile *hptr)
{
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, hptr->contents.head)
	{
		if(ptr->data != emptyline)
			BlockHeapFree(helpline_heap, ptr->data);
	}

	BlockHeapFree(helpfile_heap, hptr);
}

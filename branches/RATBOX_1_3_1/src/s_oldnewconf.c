/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * s_newconf.c - code for dealing with conf stuff like k/d/x lines
 *
 * Copyright (C) 2002-2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2002-2003 ircd-ratbox development team
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
#include "s_newconf.h"
#include "tools.h"
#include "client.h"
#include "memory.h"
#include "s_serv.h"
#include "send.h"
#include "balloc.h"
#include "hash.h"

static BlockHeap *rxconf_heap = NULL;
static BlockHeap *shared_heap = NULL;

dlink_list xline_list;
dlink_list xline_hash_list;
dlink_list resv_list;
dlink_list resv_hash_list;
dlink_list shared_list;
dlink_list encap_list;

static int is_wildcard_esc(const char *);
static char *strip_escapes(const char *);

/* init_conf()
 *
 * inputs       -
 * outputs      -
 * side effects - loads some things for this file
 */
void
init_conf(void)
{
	rxconf_heap = BlockHeapCreate(sizeof(struct rxconf), RXCONF_HEAP_SIZE);
	shared_heap = BlockHeapCreate(sizeof(struct shared), SHARED_HEAP_SIZE);
}

/* make_rxconf()
 *
 * inputs       - gecos, reason, type, flags
 * outputs      -
 * side effects - creates an xline/resv based on information
 */
struct rxconf *
make_rxconf(const char *name, const char *reason, int type, int flags)
{
	struct rxconf *rxptr;

	rxptr = BlockHeapAlloc(rxconf_heap);
	memset(rxptr, 0, sizeof(struct rxconf));

	if(!EmptyString(name))
		DupString(rxptr->name, name);

	if(!EmptyString(reason))
		DupString(rxptr->reason, reason);

	rxptr->type = type;
	rxptr->flags = flags;
	return rxptr;
}

/* add_rxconf()
 *
 * inputs	- pointer to resv/xline, presumes sanity checking done
 * outputs	-
 * side effects - adds xline/resv to their respective hashes/dlinks
 */
void
add_rxconf(struct rxconf *rxptr)
{
	/* reasons too long, nuke it */
	if(strlen(rxptr->reason) > REASONLEN)
	{
		MyFree(rxptr->reason);
		DupString(rxptr->reason, "No Reason");
	}

	if(IsResv(rxptr))
	{
		if(IsResvChannel(rxptr))
		{
			add_to_resv_hash(rxptr->name, rxptr);
			dlinkAddAlloc(rxptr, &resv_hash_list);
		}
		else if(IsResvNick(rxptr))
		{
			if((strchr(rxptr->name, '?') == NULL) &&
			   (strchr(rxptr->name, '*') == NULL))
			{
				add_to_resv_hash(rxptr->name, rxptr);
				dlinkAddAlloc(rxptr, &resv_hash_list);
			}
			else
			{
				rxptr->flags |= RESV_NICKWILD;
				dlinkAddAlloc(rxptr, &resv_list);
			}
		}
	}
	else if(IsXline(rxptr))
	{
		if(!is_wildcard_esc(rxptr->name))
		{
			const char *name = strip_escapes(rxptr->name);
			MyFree(rxptr->name);
			DupString(rxptr->name, name);
			add_to_xline_hash(rxptr->name, rxptr);
			dlinkAddAlloc(rxptr, &xline_hash_list);
		}
		else
		{
			rxptr->flags |= XLINE_WILD;
			dlinkAddAlloc(rxptr, &xline_list);
		}
	}
}

/* free_rxconf()
 *
 * inputs	- pointer to rxconf to free
 * outputs	-
 * side effects - rxconf is free'd
 */
void
free_rxconf(struct rxconf *rxptr)
{
	s_assert(rxptr != NULL);
	if(rxptr == NULL)
		return;

	MyFree(rxptr->name);
	MyFree(rxptr->reason);
	BlockHeapFree(rxconf_heap, rxptr);
}

/* clear_xlines()
 *
 * inputs       -
 * outputs      -
 * side effects - nukes list of xlines
 */
void
clear_xlines(void)
{
	struct rxconf *rxptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	/* this is more optimised for clearing the whole list */
	DLINK_FOREACH_SAFE(ptr, next_ptr, xline_list.head)
	{
		free_rxconf(ptr->data);
		free_dlink_node(ptr);
	}

	xline_list.head = xline_list.tail = NULL;
	xline_list.length = 0;

	DLINK_FOREACH_SAFE(ptr, next_ptr, xline_hash_list.head)
	{
		rxptr = ptr->data;
		del_from_xline_hash(rxptr->name, rxptr);
		free_rxconf(rxptr);
		free_dlink_node(ptr);
	}

	xline_hash_list.head = xline_hash_list.tail = NULL;
	xline_hash_list.length = 0;
}

/* clear_resvs()
 *
 * inputs	-
 * outputs	-
 * side effects - nukes list of resvs
 */
void
clear_resvs(void)
{
	struct rxconf *rxptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, resv_list.head)
	{
		free_rxconf(ptr->data);
		free_dlink_node(ptr);
	}

	resv_list.head = resv_list.tail = NULL;
	resv_list.length = 0;

	DLINK_FOREACH_SAFE(ptr, next_ptr, resv_hash_list.head)
	{
		rxptr = ptr->data;
		del_from_resv_hash(rxptr->name, rxptr);
		free_rxconf(rxptr);
		free_dlink_node(ptr);
	}

	resv_hash_list.head = resv_hash_list.tail = NULL;
	resv_hash_list.length = 0;
}

/* find_xline()
 *
 * inputs	- gecos to find
 * outputs	- NULL/pointer to found xline
 * side effects -
 */
struct rxconf *
find_xline(const char *gecos)
{
	struct rxconf *rxptr;
	dlink_node *ptr;

	rxptr = hash_find_xline(gecos);
	if(rxptr != NULL)
		return rxptr;

	/* then hunt the less efficient dlink list */
	DLINK_FOREACH(ptr, xline_list.head)
	{
		rxptr = ptr->data;

		if(match_esc(rxptr->name, gecos))
			return rxptr;
	}

	return NULL;
}

/* find_channel_resv()
 *
 * inputs	- channel to find
 * outputs	- 1 if found, else 0
 * side effects -
 */
int
find_channel_resv(const char *name)
{
	struct rxconf *rxptr;

	rxptr = hash_find_resv(name);
	if(rxptr != NULL)
		return 1;

	return 0;
}

/* find_nick_resv()
 *
 * inputs	- resv to find
 * output	- 1 if found, else 0
 * side effects -
 */
int
find_nick_resv(const char *name)
{
	struct rxconf *rxptr;
	dlink_node *ptr;

	rxptr = hash_find_resv(name);
	if(rxptr != NULL)
		return 1;

	DLINK_FOREACH(ptr, resv_list.head)
	{
		rxptr = ptr->data;

		if(match(rxptr->name, name))
			return 1;
	}

	return 0;
}

/* clean_resv_nick()
 *
 * inputs	- nick
 * outputs	- 1 if nick is vaild resv, 0 otherwise
 * side effects -
 */
int
clean_resv_nick(const char *nick)
{
	char tmpch;
	int as = 0;
	int q = 0;
	int ch = 0;

	if(*nick == '-' || IsDigit(*nick))
		return 0;

	while ((tmpch = *nick++))
	{
		if(tmpch == '?')
			q++;
		else if(tmpch == '*')
			as++;
		else if(IsNickChar(tmpch))
			ch++;
		else
			return 0;
	}

	if(!ch && as)
		return 0;

	return 1;
}

/* valid_wild_card_simple()
 *
 * inputs	- "thing" to test
 * outputs	- 1 if enough wildcards, else 0
 * side effects -
 */
int
valid_wild_card_simple(const char *data)
{
	const char *p;
	char tmpch;
	int nonwild = 0;

	/* check the string for minimum number of nonwildcard chars */
	p = data;

	while((tmpch = *p++))
	{
		/* found an escape, p points to the char after it, so skip
		 * that and move on.
		 */
		if(tmpch == '\\')
		{
			p++;
		}
		else if(!IsMWildChar(tmpch))
		{
			/* if we have enough nonwildchars, return */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard_simple)
				return 1;
		}
	}

	return 0;
}

/* wildcard_esc()
 *
 * inputs	- string to test for wildcards (inc escaping)
 * outputs	- 1 if wildcards, else 0
 * side effects -
 */
static int
is_wildcard_esc(const char *data)
{
	const char *p;
	char tmpch;

	p = data;

	while((tmpch = *p++))
	{
		/* found an escape, so skip the char after */
		if(tmpch == '\\')
			p++;
		else if(IsMWildChar(tmpch))
			return 1;
	}

	return 0;
}

/* strip_escapes()
 *
 * inputs	- string to strip escapes from
 * outputs	- string stripped of escapes
 * side effects -
 */
static char *
strip_escapes(const char *data)
{
	static char buf[BUFSIZE];
	const char *p;
	char *s;
	char tmpch;
	
	p = data;
	s = buf;

	while((tmpch = *p++))
	{
		/* found an escape, use the char after */
		if(tmpch == '\\')
			tmpch = *p++;
		*s++ = tmpch;
	}

	*s = '\0';
	return buf;
}

/* make_shared()
 *
 * inputs       -
 * outputs      -
 * side effects - creates a shared block
 */
struct shared *
make_shared(void)
{
	struct shared *uconf;

	uconf = BlockHeapAlloc(shared_heap);
	memset(uconf, 0, sizeof(struct shared));

	return uconf;
}

/* free_shared()
 *
 * inputs       - shared block to free
 * outputs      -
 * side effects - shared block is freed.
 */
void
free_shared(struct shared *uconf)
{
	assert(uconf != NULL);
	if(uconf == NULL)
		return;

	MyFree(uconf->username);
	MyFree(uconf->host);
	MyFree(uconf->servername);
	BlockHeapFree(shared_heap, uconf);
}

/* clear_shared()
 *
 * inputs       -
 * outputs      -
 * side effects - shared blocks are nuked
 */
void
clear_shared(void)
{
	struct shared *uconf;
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, shared_list.head)
	{
		uconf = ptr->data;

		free_shared(uconf);
		dlinkDestroy(ptr, &shared_list);
	}
}

/* find_shared()
 *
 * inputs       - username, hostname, servername, type to find shared for
 * outputs      - YES if one found, else NO
 * side effects -
 */
int
find_shared(const char *username, const char *host, const char *servername, int flags)
{
	struct shared *uconf;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, shared_list.head)
	{
		uconf = ptr->data;

		if((uconf->flags & flags) == 0)
			continue;

		if((EmptyString(uconf->servername)
		    || match(uconf->servername, servername))
		   && (EmptyString(uconf->username)
		       || match(uconf->username, username))
		   && (EmptyString(uconf->host) || match(uconf->host, host)))
			return YES;
	}

	return NO;
}


/* ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * operhash.c - Hashes nick!user@host{oper}
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005 ircd-ratbox development team
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
#include "struct.h"
#include "tools.h"
#include "match.h"
#include "hash.h"
#include "operhash.h"

#define OPERHASH_MAX_BITS (32-7)
#define OPERHASH_MAX 128	/* 2^7 */

#define hash_opername(x) fnv_hash_upper_len((const unsigned char *)(x), OPERHASH_MAX_BITS, 30)

static dlink_list *operhash_table;

void
init_operhash(void)
{
	operhash_table = ircd_malloc(sizeof(dlink_list) * OPERHASH_MAX);
}

const char *
operhash_add(const char *name)
{
	struct operhash_entry *ohash;
	unsigned int hashv;
	dlink_node *ptr;

	if(EmptyString(name))
		return NULL;

	hashv = hash_opername(name);

	DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash = ptr->data;

		if(!irccmp(ohash->name, name))
		{
			ohash->refcount++;
			return ohash->name;
		}
	}

	ohash = ircd_malloc(sizeof(struct operhash_entry));
	ohash->refcount = 1;
	DupString(ohash->name, name);

	ircd_dlinkAddAlloc(ohash, &operhash_table[hashv]);

	return ohash->name;
}

const char *
operhash_find(const char *name)
{
	struct operhash_entry *ohash;
	unsigned int hashv;
	dlink_node *ptr;

	if(EmptyString(name))
		return NULL;

	hashv = hash_opername(name);

	DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash = ptr->data;

		if(!irccmp(ohash->name, name))
			return ohash->name;
	}

	return NULL;
}

void
operhash_delete(const char *name)
{
	struct operhash_entry *ohash;
	unsigned int hashv;
	dlink_node *ptr;

	if(EmptyString(name))
		return;

	hashv = hash_opername(name);

	DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash = ptr->data;

		if(irccmp(ohash->name, name))
			continue;

		ohash->refcount--;

		if(ohash->refcount == 0)
		{
			ircd_free(ohash->name);
			ircd_free(ohash);
			ircd_dlinkDestroy(ptr, &operhash_table[hashv]);
			return;
		}
	}
}


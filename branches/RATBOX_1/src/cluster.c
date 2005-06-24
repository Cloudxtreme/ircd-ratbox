/*
 * ircd-ratbox: A slightly useful ircd.
 * cluster.c: The code for handling kline clusters
 *
 * Copyright (C) 2002-2004 ircd-ratbox development team
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
#include "cluster.h"
#include "tools.h"
#include "client.h"
#include "memory.h"
#include "s_serv.h"
#include "send.h"

dlink_list cluster_list;

struct cluster *
make_cluster(void)
{
	struct cluster *clptr;
	clptr = (struct cluster *) MyMalloc(sizeof(struct cluster));
	return clptr;
}

void
free_cluster(struct cluster *clptr)
{
	s_assert(clptr != NULL);
	if(clptr == NULL)
		return;

	MyFree(clptr->name);
	MyFree((char *) clptr);
}

void
clear_clusters(void)
{
	dlink_node *ptr;
	dlink_node *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, cluster_list.head)
	{
		free_cluster(ptr->data);
		dlinkDestroy(ptr, &cluster_list);
	}
}

int
find_cluster(const char *name, int type)
{
	struct cluster *clptr;
	dlink_node *ptr;

	DLINK_FOREACH(ptr, cluster_list.head)
	{
		clptr = ptr->data;

		if(match(clptr->name, name) && clptr->type & type)
			return 1;
	}

	return 0;
}

void
propagate_generic(struct Client *source_p, const char *command,
		const char *target, int cap, const char *format, ...)
{
	char buffer[BUFSIZE];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	sendto_match_servs(source_p, target, cap, NOCAPS,
			"%s %s %s",
			command, target, buffer);
	sendto_match_servs(source_p, target, CAP_ENCAP, cap,
			"ENCAP %s %s %s",
			target, command, buffer);
}
			
void
cluster_generic(struct Client *source_p, const char *command,
		int cltype, int cap, const char *format, ...)
{
	char buffer[BUFSIZE];
	struct cluster *clptr;
	va_list args;
	dlink_node *ptr;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	DLINK_FOREACH(ptr, cluster_list.head)
	{
		clptr = ptr->data;

		if(!(clptr->type & cltype))
			continue;

		sendto_match_servs(source_p, clptr->name, cap, NOCAPS,
				"%s %s %s",
				command, clptr->name, buffer);
		sendto_match_servs(source_p, clptr->name, CAP_ENCAP, cap,
				"ENCAP %s %s %s",
				clptr->name, command, buffer);
	}
}


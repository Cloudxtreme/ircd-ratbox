/*
 * ircd-ratbox: A slightly useful ircd.
 * cluster.h: The code for handling kline clusters
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

#ifndef INCLUDED_cluster_h
#define INCLUDED_cluster_h

#include "tools.h"

struct Client;

extern dlink_list cluster_list;

struct cluster
{
	char *name;
	int type;
};

#define CLUSTER_KLINE   0x0001
#define CLUSTER_UNKLINE 0x0002
#define CLUSTER_LOCOPS  0x0004
#define CLUSTER_XLINE	0x0008
#define CLUSTER_UNXLINE	0x0010
#define CLUSTER_RESV	0x0020
#define CLUSTER_UNRESV	0x0040

#define CLUSTER_ALL	(CLUSTER_KLINE | CLUSTER_UNKLINE | CLUSTER_LOCOPS |\
			 CLUSTER_XLINE | CLUSTER_UNXLINE | CLUSTER_RESV |\
			 CLUSTER_UNRESV)

extern struct cluster *make_cluster(void);
extern void free_cluster(struct cluster *clptr);
extern void clear_clusters(void);

extern int find_cluster(const char *name, int type);

extern void cluster_kline(struct Client *source_p, int tkline_time,
			  const char *user, const char *host, const char *reason);
extern void cluster_unkline(struct Client *source_p, const char *user, const char *host);
extern void cluster_locops(struct Client *source_p, const char *message);

extern void cluster_xline(struct Client *source_p, const char *gecos,
			  int xtype, const char *reason);
extern void cluster_unxline(struct Client *source_p, const char *gecos);

extern void cluster_resv(struct Client *source_p, const char *name,
			 const char *reason);
extern void cluster_unresv(struct Client *source_p, const char *name);

#endif

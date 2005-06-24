/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * s_newconf.h: code for dealing with conf stuff like klines
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

#ifndef INCLUDED_newconf_h
#define INCLUDED_newconf_h

#include "handlers.h"
#include "tools.h"

#define CONF_RESV	0x0001
#define CONF_XLINE	0x0002
#define XLINE_WILD	0x0020
#define RESV_CHANNEL	0x0100
#define RESV_NICK	0x0200
#define RESV_NICKWILD	0x0400

#define IsResv(x)	((x)->flags & CONF_RESV)
#define IsXline(x)	((x)->flags & CONF_XLINE)
#define IsResvChannel(x)	((x)->flags & RESV_CHANNEL)
#define IsResvNick(x)		((x)->flags & RESV_NICK)

#define ENCAP_PERM	0x001

struct rxconf
{
	char *name;
	char *reason;
	int type;
	int flags;
};

struct shared
{
	char *username;
	char *host;
	char *servername;
	int flags;
};

struct encap
{
	const char *name;
	MessageHandler handler;
	int flags;
};

extern void init_conf(void);

extern dlink_list xline_list;
extern dlink_list xline_hash_list;
extern dlink_list resv_list;
extern dlink_list resv_hash_list;

extern struct rxconf *make_rxconf(const char *, const char *, int, int);
extern void add_rxconf(struct rxconf *);
extern void free_rxconf(struct rxconf *);

extern struct rxconf *find_xline(const char *);
extern void clear_xlines(void);

extern int find_channel_resv(const char *);
extern int find_nick_resv(const char *);
extern void clear_resvs(void);

extern int valid_wild_card_simple(const char *);
extern int clean_resv_nick(const char *);

extern dlink_list shared_list;
extern struct shared *make_shared(void);
extern void free_shared(struct shared *);
extern void clear_shared(void);
extern int find_shared(const char *username, const char *host, 
			const char *servername, int type);

extern dlink_list encap_list;
extern int add_encap(struct encap *enptr);
extern int del_encap(struct encap *enptr);
extern struct encap *find_encap(const char *name);

#endif


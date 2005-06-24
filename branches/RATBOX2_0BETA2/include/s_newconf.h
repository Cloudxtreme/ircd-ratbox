/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * s_newconf.h: code for dealing with conf stuff like klines
 *
 * Copyright (C) 2002 ircd-ratbox development team
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

#include "tools.h"

struct xline
{
	char *gecos;
	char *reason;
	int type;
};

struct shared
{
	char *username;
	char *host;
	char *servername;
	int flags;
};

extern void init_conf(void);

extern dlink_list xline_list;
extern struct xline *make_xline(const char *, const char *, int);
extern void free_xline(struct xline *);
extern void clear_xlines(void);
extern struct xline *find_xline(char *);

extern dlink_list shared_list;
extern struct shared *make_shared(void);
extern void free_shared(struct shared *);
extern void clear_shared(void);
extern int find_shared(const char *username, const char *host, 
												const char *servername, int type);

#endif


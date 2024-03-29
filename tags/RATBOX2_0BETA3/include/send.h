/*
 *  ircd-ratbox: A slightly useful ircd.
 *  send.h: A header for the message sending functions.
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

#ifndef INCLUDED_send_h
#define INCLUDED_send_h

#include "ircd_defs.h"
#include "config.h"		/* HAVE_STDARG_H */

/*
 * struct decls
 */
struct Client;
struct Channel;
struct dlink_list;

/* The nasty global also used in s_serv.c for server bursts */
extern unsigned long current_serial;

/* send.c prototypes */

extern void send_queued_write(int fd, void *data);

extern void send_queued_slink_write(int fd, void *data);

extern void
sendto_one(struct Client *, const char *, ...)
AFP(2, 3);

     extern void sendto_channel_butone(struct Client *one,
				       struct Client *from,
				       struct Channel *chptr,
				       const char *command, const char *pattern, ...) AFP(5, 6);

     extern void sendto_one_prefix(struct Client *, struct Client *, const char *, ...) AFP(3, 4);

     extern void sendto_common_channels_local(struct Client *, const char *, ...) AFP(2, 3);

     extern void sendto_channel_local(int type, struct Channel *, const char *, ...) AFP(3, 4);
     extern void sendto_channel_local_butone(struct Client *, int,
					     struct Channel *, const char *, ...) AFP(4, 5);

     extern void sendto_channel_remote(struct Client *one,
				       struct Client *from, int type,
				       int caps, int nocaps,
				       struct Channel *chptr, const char *pattern, ...) AFP(7, 8);

     extern void sendto_server(struct Client *one, struct Channel *chptr,
			       unsigned long caps, unsigned long nocaps,
			       const char *format, ...) AFP(5, 6);

     extern void sendto_match_butone(struct Client *, struct Client *,
				     const char *, int, const char *, ...) AFP(5, 6);

     extern void sendto_match_servs(struct Client *source_p,
				    const char *mask, int capab, const char *, ...) AFP(4, 5);

     extern void sendto_realops_flags(int, int, const char *, ...) AFP(3, 4);

     extern void sendto_wallops_flags(int, struct Client *, const char *, ...) AFP(3, 4);

     extern void ts_warn(const char *, ...) AFP(1, 2);

     extern void sendto_anywhere(struct Client *, struct Client *, const char *, ...) AFP(3, 4);

     extern void
	     kill_client(struct Client *client_p, struct Client *diedie,
			 const char *pattern, ...) AFP(3, 4);

     extern void
	     kill_client_serv_butone(struct Client *one,
				     struct Client *source_p, const char *pattern, ...) AFP(3, 4);


#define ALL_MEMBERS  0
#define NON_CHANOPS  1
#define ONLY_CHANOPS_VOICED 2
#define ONLY_CHANOPS 3
#define ONLY_SERVERS 4		/* for channel_mode.c */

#define L_ALL 	0
#define L_OPER 	1
#define L_ADMIN	2

#define NOCAPS          0	/* no caps */

/* used when sending to #mask or $mask */
#define MATCH_SERVER  1
#define MATCH_HOST    2

#endif /* INCLUDED_send_h */

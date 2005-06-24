/************************************************************************
 *
 *   IRC - Internet Relay Chat, include/vchannel.h
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#ifndef INCLUDED_vchannel_h
#define INCLUDED_vchannel_h

#ifndef INCLUDED_channel_h
#include "channel.h"
#endif

#ifndef INCLUDED_ircd_defs_h
#include "ircd_defs.h"        /* buffer sizes */
#endif

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* time_t */
#define INCLUDED_sys_types_h
#endif

struct Client;
struct Channel;

extern void	add_vchan_to_client_cache(struct Client *sptr,
					  struct Channel *base_chan,
					  struct Channel *vchan);

extern void	del_vchan_from_client_cache(struct Client *sptr,
					    struct Channel *vchan);

extern struct Channel* map_vchan(struct Channel *chptr, struct Client *sptr);
extern struct Channel* map_bchan(struct Channel *chptr, struct Client *sptr);
extern struct Channel* find_bchan(struct Channel *chptr);

extern void	show_vchans(struct Client *cptr,
			    struct Client *sptr,
			    struct Channel *chptr);

/* find a matching vchan with a !key (nick) */ 
extern struct Channel* find_vchan(struct Channel *, char *);

/* See if this client is on a sub chan already */
extern int on_sub_vchan(struct Channel *chptr, struct Client *sptr);

/* Check for an invite to any of the vchans */
extern struct Channel* vchan_invites(struct Channel *chptr,
                                     struct Client *sptr);

/* Valid to verify a channel is a subchan */
#define IsVchan(chan)	(chan->prev_vchan)

/* Only valid for top chan, i.e. only valid to determine if there are vchans
 * under hash table lookup of top level channel
 */
#define HasVchans(chan)	(chan->next_vchan)

/* Valid to determine if this is the top of a set of vchans */
#define IsVchanTop(chan)	((chan->next_vchan)&&(chan->prev_vchan==0))

#endif  /* INCLUDED_vchannel_h */


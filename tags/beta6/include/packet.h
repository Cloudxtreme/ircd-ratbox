/************************************************************************
 *   IRC - Internet Relay Chat, include/packet.h
 *   Copyright (C) 1992 Darren Reed
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
 *
 * "packet.h". - Headers file.
 *
 * $Id$
 *
 */
#ifndef INCLUDED_packet_h
#define INCLUDED_packet_h
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif
#include "fdlist.h"

/*
 * this hides in here rather than a config.h because it really shouldn't
 * be tweaked unless you *REALLY REALLY* know what you're doing!
 * Remember, messages are only anti-flooded on incoming from the client, not on
 * incoming from a server for a given client, so if you tweak this you risk
 * allowing a client to flood differently depending upon where they are on
 * the network..
 *   -- adrian
 */
#define MAX_FLOOD_PER_SEC               8
/* And the initial rate of flooding after registration... -A1kmm. */
#define MAX_FLOOD_PER_SEC_I            24

extern PF  read_ctrl_packet;
extern PF  read_packet;
extern PF  flood_recalc;

extern void client_dopacket(struct Client *, char *, size_t);

#endif /* INCLUDED_packet_h */



/*
 *  ircd-ratbox: A slightly useful ircd.
 *  supported.h: Header for 005 numeric etc...
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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

#ifndef INCLUDED_supported_h
#define INCLUDED_supported_h

#include "config.h"
#include "channel.h"
#include "ircd_defs.h"

#define FEATURES "STD=i-d"		\
		" STATUSMSG=@+"		\
                "%s%s%s"		\
                " MODES=%i"		\
                " MAXCHANNELS=%i"	\
                " MAXBANS=%i"		\
                " MAXTARGETS=%i"	\
                " NICKLEN=%i"		\
                " TOPICLEN=%i"		\
                " KICKLEN=%i"		\
		" AWAYLEN=%i"

#define FEATURESVALUES ConfigChannel.use_knock ? " KNOCK" : "", \
        ConfigChannel.use_except ? " EXCEPTS" : "", \
        ConfigChannel.use_invex ? " INVEX" : "", \
        MAXMODEPARAMS,ConfigChannel.max_chans_per_user, \
        ConfigChannel.max_bans, \
        ConfigFileEntry.max_targets,NICKLEN-1,TOPICLEN,REASONLEN,AWAYLEN

#define FEATURES2 "CHANNELLEN=%i"	\
		" CHANTYPES=#&"		\
		" PREFIX=(ov)@+" 	\
		" CHANMODES=%s%sb,k,l,imnpst"	\
		" NETWORK=%s"		\
		" CASEMAPPING=rfc1459"	\
		" CHARSET=ascii"	\
		" CALLERID"		\
		" WALLCHOPS"		\
		" ETRACE"		\
		" SAFELIST"		\
		" ELIST=U"

#define FEATURES2VALUES LOC_CHANNELLEN, \
			ConfigChannel.use_except ? "e" : "", \
                        ConfigChannel.use_invex ? "I" : "", \
                        ServerInfo.network_name

/*
 * - from mirc's versions.txt
 *
 *  mIRC now supports the numeric 005 tokens: CHANTYPES=# and
 *  PREFIX=(ohv)@%+ and can handle a dynamic set of channel and
 *  nick prefixes.
 *
 *  mIRC assumes that @ is supported on all networks, any mode
 *  left of @ is assumed to have at least equal power to @, and
 *  any mode right of @ has less power.
 *
 *  mIRC has internal support for @%+ modes.
 *
 *  $nick() can now handle all mode letters listed in PREFIX.
 *
 *  Also added support for CHANMODES=A,B,C,D token (not currently
 *  supported by any servers), which lists all modes supported
 *  by a channel, where:
 *
 *    A = modes that take a parameter, and add or remove nicks
 *        or addresses to a list, such as +bIe for the ban,
 *        invite, and exception lists.
 *
 *    B = modes that change channel settings, but which take
 *        a parameter when they are set and unset, such as
 *        +k key, and -k key.
 *
 *    C = modes that change channel settings, but which take
 *        a parameter only when they are set, such as +l N,
 *        and -l.
 *
 *    D = modes that change channel settings, such as +imnpst
 *        and take no parameters.
 *
 *  All unknown/unlisted modes are treated as type D.
 */
/* ELIST=[tokens]:
 *
 * M = mask search
 * N = !mask search
 * U = user count search (< >)
 * C = creation time search (C> C<)
 * T = topic search (T> T<)
 */
#endif /* INCLUDED_supported_h */

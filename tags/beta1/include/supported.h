/* - Internet Relay Chat, include/supported.h
 *   Copyright (C) 2000 Perry Lorier <isomer@coders.net>
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
 * 
 */
#ifndef INCLUDED_supported_h
#define INCLUDED_supported_h

#include "config.h"
#include "channel.h"
#include "ircd_defs.h"

#define FEATURES "KNOCK"\
                " VCHANS" \
                " MODES=%i" \
                " MAXCHANNELS=%i" \
                " MAXBANS=%i" \
                " MAXTARGETS=%i" \
                " NICKLEN=%i" \
                " TOPICLEN=%i" \
                " KICKLEN=%i" \
                " CHANTYPES=%s" \
                " PREFIX=%s"
                 
#define FEATURESVALUES MAXMODEPARAMS,MAXCHANNELSPERUSER,MAXBANS, \
        ConfigFileEntry.max_targets,NICKLEN,TOPICLEN,TOPICLEN,"#&","@+%"

#endif /* INCLUDED_supported_h */

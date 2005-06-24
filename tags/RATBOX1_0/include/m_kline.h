/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_kline.h: A header for the kline code.
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

#ifndef INCLUDED_m_kline_h
#define INCLUDED_m_kline_h

struct Client;

struct PKDlines
{
  struct PKDlines* next;
  struct Client*   source_p;
  struct Client*   rclient_p;
  char*            user; /* username of K/D lined user */
  char*            host; /* hostname of K/D lined user */
  char*            reason; /* reason they are K/D lined */
  char*            when; /* when the K/D line was set */
  int              type;
};

typedef struct PKDlines aPendingLine;

#endif /* INCLUDED_m_kline_h */

/************************************************************************
 *   IRC - Internet Relay Chat, src/m_kline.h
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

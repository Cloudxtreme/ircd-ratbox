/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  motd.h: A header for the MOTD functions.
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

#ifndef INCLUDED_motd_h
#define INCLUDED_motd_h
#include "ircd_defs.h"   


/* XXX really, should be mallocing this on the fly but... */
#define MESSAGELINELEN 256

typedef enum {
  USER_MOTD,
  USER_LINKS,
  OPER_MOTD
} MotdType;

struct MessageFileLine
{
  char                    line[MESSAGELINELEN + 1];
  struct MessageFileLine* next;
};

typedef struct MessageFileLine MessageFileLine;

struct MessageFile
{
  char             fileName[PATH_MAX + 1];
  MotdType         motdType;
  MessageFileLine* contentsOfFile;
  char             lastChangedDate[MAX_DATE_STRING + 1];
};

typedef struct MessageFile MessageFile;

struct Client;

void InitMessageFile(MotdType, const char *, struct MessageFile *);
int SendMessageFile(struct Client *, struct MessageFile *);
int ReadMessageFile(MessageFile *);

#endif /* INCLUDED_motd_h */

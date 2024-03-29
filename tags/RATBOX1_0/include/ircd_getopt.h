/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  ircd_getopt.h: A header for the getopt() command line option calls.
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

#ifndef __GETOPT_H_INCLUDED__
#define __GETOPT_H_INCLUDED__

struct lgetopt {
  char *opt; /* name of the argument */
  void *argloc; /* where we store the argument to it (-option argument) */
  enum {INTEGER, YESNO, STRING, USAGE, ENDEBUG} argtype;
  char *desc; /* description of the argument, usage for printing help */
};

extern struct lgetopt myopts[];

void usage(char *);
void parseargs(int *, char ***, struct lgetopt *);

#endif /* __GETOPT_H_INCLUDED__ */

/************************************************************************
 *   IRC - Internet Relay Chat, src/numeric.c
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
 *   $Id$
 */

#include <sys/types.h>
#include <assert.h>

#include "setup.h"
#include "config.h"

#ifdef USE_GETTEXT
#include <libintl.h>
#endif

#include "numeric.h"
#include "irc_string.h"
#include "common.h"     /* NULL cripes */
#include "memory.h"

#include "messages.tab"

/*
 * form_str
 *
 * inputs	- numeric
 * output	- corresponding string
 * side effects	- NONE
 */
const char* form_str(int numeric)
{

  assert(-1 < numeric);
  assert(numeric < ERR_LAST_ERR_MSG);
  assert(0 != replies[numeric]);
  
  return (const char *) _(replies[numeric]);
}



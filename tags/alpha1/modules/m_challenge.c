/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_challenge.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
#include <stdlib.h>
#include "handlers.h"
#include "client.h"
#include "ircd.h"
#include "modules.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "rsa.h"
#include "msg.h"
#include "parse.h"

#ifndef OPENSSL

/* Maybe this should be an error or something? -davidt */

void
_modinit(void)
{
  return;
}

char *_version = "20001122";

#else

/* We have openssl support, so include /CHALLENGE */
struct Message challenge_msgtab = {
  MSG_CHALLENGE, 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_challenge, m_ignore, m_challenge}
};

void
_modinit(void)
{
  mod_add_cmd(&challenge_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&challenge_msgtab);
}

/* isn't this cute? :) */
#define DESRT_IDENTITY "1024 35 129898254114702764644161311398742945367211656239843407101360565864933766487482427601420598335314129357193904532215504249652048024499002590622257138142186907550826986726417399616128993705932451404433561862389005312041126460533080690966003918873462732636475035659370143015664222562459185971059059633407429578727"

char *_version = "20001122";

/*
 * m_challenge - generate RSA challenge for wouldbe oper
 * parv[0] = sender prefix
 *
 */
int m_challenge( struct Client *cptr, struct Client *sptr, int parc, char *parv[] )
{
  char * challenge;

  if( !(sptr->user) )
    return 0;

  if( sptr->user->RSA_response )
    MyFree( sptr->user->RSA_response );

  generate_challenge( &challenge, &(sptr->user->RSA_response), DESRT_IDENTITY );
  sendto_one( sptr, form_str( RPL_RSACHALLENGE ), me.name, parv[0], challenge );
  MyFree( challenge );

  return 0;
}

#endif /* OPENSSL */

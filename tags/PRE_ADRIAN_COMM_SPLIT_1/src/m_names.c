/************************************************************************
 *   IRC - Internet Relay Chat, src/m_names.c
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

#include "handlers.h"
#include "channel.h"
#include "vchannel.h"
#include "client.h"
#include "common.h"   /* bleah */
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "send.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * m_functions execute protocol messages on this server:
 *
 *      cptr    is always NON-NULL, pointing to a *LOCAL* client
 *              structure (with an open socket connected!). This
 *              identifies the physical socket where the message
 *              originated (or which caused the m_function to be
 *              executed--some m_functions may call others...).
 *
 *      sptr    is the source of the message, defined by the
 *              prefix part of the message if present. If not
 *              or prefix not found, then sptr==cptr.
 *
 *              (!IsServer(cptr)) => (cptr == sptr), because
 *              prefixes are taken *only* from servers...
 *
 *              (IsServer(cptr))
 *                      (sptr == cptr) => the message didn't
 *                      have the prefix.
 *
 *                      (sptr != cptr && IsServer(sptr) means
 *                      the prefix specified servername. (?)
 *
 *                      (sptr != cptr && !IsServer(sptr) means
 *                      that message originated from a remote
 *                      user (not local).
 *
 *              combining
 *
 *              (!IsServer(sptr)) means that, sptr can safely
 *              taken as defining the target structure of the
 *              message in this server.
 *
 *      *Always* true (if 'parse' and others are working correct):
 *
 *      1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *      2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *              *cannot* be a local connection, unless it's
 *              actually cptr!). [MyConnect(x) should probably
 *              be defined as (x == x->from) --msa ]
 *
 *      parc    number of variable parameter strings (if zero,
 *              parv is allowed to be NULL)
 *
 *      parv    a NULL terminated list of parameter pointers,
 *
 *                      parv[0], sender (prefix string), if not present
 *                              this points to an empty string.
 *                      parv[1]...parv[parc-1]
 *                              pointers to additional parameters
 *                      parv[parc] == NULL, *always*
 *
 *              note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                      non-NULL pointers.
 */

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**      parv[0] = sender prefix
**      parv[1] = channel
**      parv[2] = root name
*/
/*
 * Modified to report possible names abuse
 * drastically modified to not show all names, just names
 * on given channel names.
 *
 * -Dianora
 */
/* maximum names para to show to opers when abuse occurs */
#define TRUNCATED_NAMES 20


int     m_names( struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{ 
  struct Channel *chptr;
  struct Channel *vchan;
  struct Client *c2ptr;
  struct SLink  *lp;
  struct Channel *ch2ptr = NULL;
  int   idx;
  int   len;
  int   mlen;
  int   reply_to_send = NO;
  char  *s, *para = parc > 1 ? parv[1] : NULL;
  int comma_count=0;
  int char_count=0;
  char buf[BUFSIZE];

  /* Don't route names, no need for it -Dianora */
  /*
  if (parc > 1 &&
      hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
    return 0;
    */

  /* And throw away non local names requests that do get here -Dianora */
  if(!MyConnect(sptr))
    return 0;

  /*
   * names is called by m_join() when client joins a channel,
   * hence I cannot easily rate limit it.. perhaps that won't
   * be necessary now that remote names is prohibited.
   *
   * -Dianora
   */

  if (!BadPtr(para))
    {
      /* Here is the lamer detection code
       * P.S. meta, GROW UP
       * -Dianora 
       */
      for(s = para; *s; s++)
        {
          char_count++;
          if(*s == ',')
            comma_count++;
          if(comma_count > 1)
            {
              if(char_count > TRUNCATED_NAMES)
                para[TRUNCATED_NAMES] = '\0';
              else
                {
                  s++;
                  *s = '\0';
                }
              sendto_realops("POSSIBLE /names abuser %s [%s]",
                             para,
                             get_client_name(sptr,FALSE));
              sendto_one(sptr, form_str(ERR_TOOMANYTARGETS),
                         me.name, sptr->name, "NAMES", 1);
              return 0;
            }
        }

      s = strchr(para, ',');
      if (s)
        *s = '\0';
      if (!check_channel_name(para))
        { 
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                     me.name, parv[0], (unsigned char *)para);
          return 0;
        }

      ch2ptr = hash_find_channel(para, NULL);
      if( ch2ptr )
	{
	  if (HasVchans(ch2ptr))
	    {
	      vchan = map_vchan(ch2ptr,sptr);
	      if(vchan == 0)
		names_on_this_channel( sptr, ch2ptr, ch2ptr->chname );
	      else
		names_on_this_channel( sptr, vchan, ch2ptr->chname );
	    }
	  else
	    {
	      names_on_this_channel( sptr, ch2ptr, ch2ptr->chname );
	    }
	  return 0;
	}
    }

  mlen = strlen(me.name) + NICKLEN + 7;
  *buf = '\0';
  
  /* 
   *
   * First, do all visible channels (public and the one user self is)
   */

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      if (!ShowChannel(sptr, chptr))
        continue; /* -- users on this are not listed */
      
      /* Find users on same channel (defined by chptr) */

      (void)strcpy(buf, "* ");
      len = strlen(chptr->chname);
      (void)strcpy(buf + 2, chptr->chname);
      (void)strcpy(buf + 2 + len, " :");

      if (PubChannel(chptr))
        *buf = '=';
      else if (SecretChannel(chptr))
        *buf = '@';
      idx = len + 4;
      reply_to_send = YES;
      for (lp = chptr->members; lp; lp = lp->next)
        {
          c2ptr = lp->value.cptr;
          if (IsInvisible(c2ptr) && !IsMember(sptr,chptr))
            continue;
          if (lp->flags & CHFL_CHANOP)
            {
              strcat(buf, "@");
              idx++;
            }
          else if (lp->flags & CHFL_VOICE)
            {
              strcat(buf, "+");
              idx++;
            }
          strncat(buf, c2ptr->name, NICKLEN);
          idx += strlen(c2ptr->name) + 1;
          reply_to_send = YES;
          strcat(buf," ");
          if (mlen + idx + NICKLEN > BUFSIZE - 3)
            {
              sendto_one(sptr, form_str(RPL_NAMREPLY),
                         me.name, parv[0], buf);
              strncpy_irc(buf, "* ", 3);
              if (parc > 2)
                strncpy_irc(buf + 2, parv[2], len + 1);
              else
                strncpy_irc(buf + 2, chptr->chname, len + 1);
              strcat(buf, " :");
              if (PubChannel(chptr))
                *buf = '=';
              else if (SecretChannel(chptr))
                *buf = '@';
              idx = len + 4;
              reply_to_send = NO;
            }
        }
      if (reply_to_send)
        sendto_one(sptr, form_str(RPL_NAMREPLY),
                   me.name, parv[0], buf);
    }

  /* Second, do all non-public, non-secret channels in one big sweep */

  strncpy_irc(buf, "* * :", 6);
  idx = 5;
  reply_to_send = NO;
  for (c2ptr = GlobalClientList; c2ptr; c2ptr = c2ptr->next)
    {
      struct Channel *ch3ptr;
      int       showflag = 0, secret = 0;

      if (!IsPerson(c2ptr) || IsInvisible(c2ptr))
        continue;
      lp = c2ptr->user->channel;
      /*
       * dont show a client if they are on a secret channel or
       * they are on a channel sptr is on since they have already
       * been show earlier. -avalon
       */
      while (lp)
        {
          ch3ptr = lp->value.chptr;
          if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
            showflag = 1;
          if (SecretChannel(ch3ptr))
            secret = 1;
          lp = lp->next;
        }
      if (showflag) /* have we already shown them ? */
        continue;
      if (secret) /* on any secret channels ? */
        continue;
      (void)strncat(buf, c2ptr->name, NICKLEN);
      idx += strlen(c2ptr->name) + 1;
      (void)strcat(buf," ");
      reply_to_send = YES;
      if (mlen + idx + NICKLEN > BUFSIZE - 3)
        {
          sendto_one(sptr, form_str(RPL_NAMREPLY),
                     me.name, parv[0], buf);
          strncpy_irc(buf, "* * :", 6);
          idx = 5;
          reply_to_send = NO;
        }
    }

  if (reply_to_send)
    sendto_one(sptr, form_str(RPL_NAMREPLY), me.name, parv[0], buf);

  sendto_one(sptr, form_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
  return(1);
}

int     ms_names( struct Client *cptr,
                 struct Client *sptr,
                 int parc,
                 char *parv[])
{ 
  struct Channel *chptr;
  struct Client *c2ptr;
  struct SLink  *lp;
  struct Channel *ch2ptr = NULL;
  int   idx, flag = 0, len, mlen;
  char  *s, *para = parc > 1 ? parv[1] : NULL;
  int comma_count=0;
  int char_count=0;
  char buf[BUFSIZE];

  /* Don't route names, no need for it -Dianora */
  /*
  if (parc > 1 &&
      hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
    return 0;
    */

  /* And throw away non local names requests that do get here -Dianora */
  if(!MyConnect(sptr))
    return 0;

  /*
   * names is called by m_join() when client joins a channel,
   * hence I cannot easily rate limit it.. perhaps that won't
   * be necessary now that remote names is prohibited.
   *
   * -Dianora
   */



  if (!BadPtr(para))
    {
      /* Here is the lamer detection code
       * P.S. meta, GROW UP
       * -Dianora 
       */
      for(s = para; *s; s++)
        {
          char_count++;
          if(*s == ',')
            comma_count++;
          if(comma_count > 1)
            {
              if(char_count > TRUNCATED_NAMES)
                para[TRUNCATED_NAMES] = '\0';
              else
                {
                  s++;
                  *s = '\0';
                }
              sendto_realops("POSSIBLE /names abuser %s [%s]",
                             para,
                             get_client_name(sptr,FALSE));
              sendto_one(sptr, form_str(ERR_TOOMANYTARGETS),
                         me.name, sptr->name, "NAMES", 1);
              return 0;
            }
        }

      s = strchr(para, ',');
      if (s)
        *s = '\0';
      if (!check_channel_name(para))
        { 
          sendto_one(sptr, form_str(ERR_BADCHANNAME),
                     me.name, parv[0], (unsigned char *)para);
          return 0;
        }

      ch2ptr = hash_find_channel(para, NULL);
    }

  *buf = '\0';

  mlen = strlen(me.name) + NICKLEN + 7;
  
  /* 
   *
   * First, do all visible channels (public and the one user self is)
   */

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    {
      if ((chptr != ch2ptr) && !BadPtr(para))
        continue; /* -- wanted a specific channel */
      if (!MyConnect(sptr) && BadPtr(para))
        continue;
      if (!ShowChannel(sptr, chptr))
        continue; /* -- users on this are not listed */
      
      /* Find users on same channel (defined by chptr) */

      (void)strcpy(buf, "* ");
      len = strlen(chptr->chname);
      (void)strcpy(buf + 2, chptr->chname);
      (void)strcpy(buf + 2 + len, " :");

      if (PubChannel(chptr))
        *buf = '=';
      else if (SecretChannel(chptr))
        *buf = '@';
      idx = len + 4;
      flag = 1;
      for (lp = chptr->members; lp; lp = lp->next)
        {
          c2ptr = lp->value.cptr;
          if (IsInvisible(c2ptr) && !IsMember(sptr,chptr))
            continue;
          if (lp->flags & CHFL_CHANOP)
            {
              strcat(buf, "@");
              idx++;
            }
          else if (lp->flags & CHFL_VOICE)
            {
              strcat(buf, "+");
              idx++;
            }
          strncat(buf, c2ptr->name, NICKLEN);
          idx += strlen(c2ptr->name) + 1;
          flag = 1;
          strcat(buf," ");
          if (mlen + idx + NICKLEN > BUFSIZE - 3)
            {
              sendto_one(sptr, form_str(RPL_NAMREPLY),
                         me.name, parv[0], buf);
              strncpy_irc(buf, "* ", 3);
              strncpy_irc(buf + 2, chptr->chname, len + 1);
              strcat(buf, " :");
              if (PubChannel(chptr))
                *buf = '=';
              else if (SecretChannel(chptr))
                *buf = '@';
              idx = len + 4;
              flag = 0;
            }
        }
      if (flag)
        sendto_one(sptr, form_str(RPL_NAMREPLY),
                   me.name, parv[0], buf);
    }
  if (!BadPtr(para))
    {
      sendto_one(sptr, form_str(RPL_ENDOFNAMES), me.name, parv[0],
                 para);
      return(1);
    }

  /* Second, do all non-public, non-secret channels in one big sweep */

  strncpy_irc(buf, "* * :", 6);
  idx = 5;
  flag = 0;
  for (c2ptr = GlobalClientList; c2ptr; c2ptr = c2ptr->next)
    {
      struct Channel *ch3ptr;
      int       showflag = 0, secret = 0;

      if (!IsPerson(c2ptr) || IsInvisible(c2ptr))
        continue;
      lp = c2ptr->user->channel;
      /*
       * dont show a client if they are on a secret channel or
       * they are on a channel sptr is on since they have already
       * been show earlier. -avalon
       */
      while (lp)
        {
          ch3ptr = lp->value.chptr;
          if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
            showflag = 1;
          if (SecretChannel(ch3ptr))
            secret = 1;
          lp = lp->next;
        }
      if (showflag) /* have we already shown them ? */
        continue;
      if (secret) /* on any secret channels ? */
        continue;
      (void)strncat(buf, c2ptr->name, NICKLEN);
      idx += strlen(c2ptr->name) + 1;
      (void)strcat(buf," ");
      flag = 1;
      if (mlen + idx + NICKLEN > BUFSIZE - 3)
        {
          sendto_one(sptr, form_str(RPL_NAMREPLY),
                     me.name, parv[0], buf);
          strncpy_irc(buf, "* * :", 6);
          idx = 5;
          flag = 0;
        }
    }

  if (flag)
    sendto_one(sptr, form_str(RPL_NAMREPLY), me.name, parv[0], buf);

  sendto_one(sptr, form_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
  return(1);
}


void names_on_this_channel( struct Client *sptr,
			    struct Channel *chptr,
			    char *name_of_channel)
{
  int len;
  int mlen;
  int idx;
  int reply_to_send = NO;
  char buf[BUFSIZE];
  struct Client *c2ptr;
  struct SLink  *lp;

  mlen = strlen(me.name) + NICKLEN + 7;

  /* Find users on same channel (defined by chptr) */

  (void)strcpy(buf, "* ");
  len = strlen(name_of_channel);
  (void)strcpy(buf + 2, name_of_channel);
  (void)strcpy(buf + 2 + len, " :");

  if (PubChannel(chptr))
    *buf = '=';
  else if (SecretChannel(chptr))
    *buf = '@';
  idx = len + 4;

  for (lp = chptr->members; lp; lp = lp->next)
    {
      c2ptr = lp->value.cptr;
      if (lp->flags & CHFL_CHANOP)
	{
	  strcat(buf, "@");
	  idx++;
	}
      else if (lp->flags & CHFL_VOICE)
	{
	  strcat(buf, "+");
	  idx++;
	}
      strncat(buf, c2ptr->name, NICKLEN);
      idx += strlen(c2ptr->name) + 1;
      strcat(buf," ");
      reply_to_send = YES;
      if (mlen + idx + NICKLEN > BUFSIZE - 3)
	{
	  reply_to_send = NO;
	  sendto_one(sptr, form_str(RPL_NAMREPLY),
		     me.name, sptr->name, buf);
	  strncpy_irc(buf, "* ", 3);
	  strncpy_irc(buf + 2, name_of_channel, len + 1);
	  strcat(buf, " :");
	  if (PubChannel(chptr))
	    *buf = '=';
	  else if (SecretChannel(chptr))
	    *buf = '@';
	  idx = len + 4;
	}
    }

  if(reply_to_send)
    sendto_one(sptr, form_str(RPL_NAMREPLY),
	       me.name, sptr->name, buf);

  sendto_one(sptr, form_str(RPL_ENDOFNAMES), me.name, sptr->name,
	     name_of_channel);
}

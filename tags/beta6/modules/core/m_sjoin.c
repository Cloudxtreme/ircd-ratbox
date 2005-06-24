/************************************************************************
 *   IRC - Internet Relay Chat, modules/m_sjoin.c
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
#include "tools.h"
#include "handlers.h"
#include "channel.h"
#include "vchannel.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "send.h"
#include "common.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "s_conf.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void ms_sjoin(struct Client*, struct Client*, int, char**);

struct Message sjoin_msgtab = {
  "SJOIN", 0, 0, 0, MFLG_SLOW, 0,
  {m_unregistered, m_ignore, ms_sjoin, m_ignore}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&sjoin_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&sjoin_msgtab);
}

char *_version = "20010102";
#endif
/*
 * ms_sjoin
 * parv[0] - sender
 * parv[1] - TS
 * parv[2] - channel
 * parv[3] - modes + n arguments (key and/or limit)
 * parv[4+n] - flags+nick list (all in one parameter)
 * 
 * process a SJOIN, taking the TS's into account to either ignore the
 * incoming modes or undo the existing ones or merge them, and JOIN
 * all the specified users while sending JOIN/MODEs to local clients
 */

static char    modebuf[MODEBUFLEN];
static char    parabuf[MODEBUFLEN];
static char    *para[MAXMODEPARAMS];
static char    *mbuf;
static int     pargs;

static void set_final_mode(struct Mode *mode,struct Mode *oldmode);
static void remove_our_modes(int type,
		      struct Channel *chptr, struct Channel *top_chptr,
		      struct Client *source_p);

static void remove_a_mode(int hide_or_not,
                          struct Channel *chptr, struct Channel *top_chptr,
                          struct Client *source_p, dlink_list *list, char flag);


static void ms_sjoin(struct Client *client_p,
                    struct Client *source_p,
                    int parc,
                    char *parv[])
{
  struct Channel *chptr;
  struct Channel *top_chptr=NULL;	/* XXX vchans */
  struct Client  *target_p, *lclient_p;
  time_t         newts;
  time_t         oldts;
  time_t         tstosend;
  static         struct Mode mode, *oldmode;
  int            args = 0;
  int            keep_our_modes = 1;
  int            keep_new_modes = 1;
  int            doesop = 0;
  int            fl;
  int            people = 0;
  int            vc_ts = 0;
  int            isnew;
  register       char *s, *s0, *sh;
  static         char buf[BUFSIZE];
  static         char sjbuf[BUFSIZE];
  static         char sjbuf_nh[BUFSIZE];
  char           *nick_pointer;
  char    *p;
  int hide_or_not;
  int i;
  dlink_node *m;

  *buf = '\0';
  *sjbuf = '\0';
  *sjbuf_nh = '\0';
  
  if (IsClient(source_p) || parc < 5)
    return;
  if (!IsChannelName(parv[2]))
    return;
  if (!check_channel_name(parv[2]))
    return;

  /* SJOIN's for local channels can't happen. */
  if(*parv[2] == '&')
    return;

  mbuf = modebuf;
  *mbuf = '\0';
  pargs = 0;
  newts = atol(parv[1]);
  memset(&mode, 0, sizeof(mode));

  s = parv[3];
  while (*s)
    switch(*(s++))
      {
      case 'i':
        mode.mode |= MODE_INVITEONLY;
        break;
      case 'n':
        mode.mode |= MODE_NOPRIVMSGS;
        break;
      case 'p':
        mode.mode |= MODE_PRIVATE;
        break;
      case 's':
        mode.mode |= MODE_SECRET;
        break;
      case 'm':
        mode.mode |= MODE_MODERATED;
        break;
      case 't':
        mode.mode |= MODE_TOPICLIMIT;
        break;
      case 'a':
        mode.mode |= MODE_HIDEOPS;
        break;
      case 'k':
        strncpy_irc(mode.key, parv[4 + args], KEYLEN);
        args++;
        if (parc < 5+args)
          return;
        break;
      case 'l':
        mode.limit = atoi(parv[4+args]);
        args++;
        if (parc < 5+args)
          return;
        break;
      }

  *parabuf = '\0';

  isnew = ChannelExists(parv[2]) ? 0 : 1;
  chptr = get_channel(source_p, parv[2], CREATE);

  /* XXX vchan cruft */
  /* vchans are encoded as "##mainchanname_timestamp" */

  if(parv[2][1] == '#' && !ConfigChannel.disable_vchans)
    {
      char *subp;

      /* possible sub vchan being sent along ? */
      if((subp = strrchr(parv[2],'_')))
	{
          vc_ts = atol(subp+1);
	  /* 
           * XXX - Could be a vchan, but we can't be _sure_
           *
           * We now test the timestamp matches below,
           * but that can still be faked.
           *
           * If there was some way to pass an extra bit of
           * information over non-hybrid-7 servers, through SJOIN,
           * we could tell other servers that it's a vchan.
           * That's probably not possible, unfortunately :(
           */

	  *subp = '\0';	/* fugly hack for now ... */

	  /* + 1 skip the extra '#' in the name */
	  if((top_chptr = hash_find_channel((parv[2] + 1), NULL)))
	    {
	      /* If the vchan is already in the vchan_list for this
	       * root, don't re-add it.
	       */
              /* Compare timestamps too */
	      if(dlinkFind(&top_chptr->vchan_list,chptr) == NULL &&
                 newts == vc_ts)
		{
		  m = make_dlink_node();
		  dlinkAdd(chptr, m, &top_chptr->vchan_list);
		  chptr->root_chptr=top_chptr;
		}
	    }
          /* check TS before creating a root channel */
	  else if(newts == vc_ts)
	    {
	      top_chptr = get_channel(source_p, (parv[2] + 1), CREATE);
	      m = make_dlink_node();
	      dlinkAdd(chptr, m, &top_chptr->vchan_list);
	      chptr->root_chptr=top_chptr;
              /* let users access it somehow... */
              chptr->vchan_id[0] = '!';
              chptr->vchan_id[1] = '\0';
	    }

	  *subp = '_';	/* fugly hack, restore '_' */
	}
    }

  oldts = chptr->channelts;

  doesop = (parv[4+args][0] == '@' || parv[4+args][1] == '@');

  oldmode = &chptr->mode;

  if (newts < 800000000)
    {
      sendto_realops_flags(FLAGS_DEBUG,
			"*** Bogus TS %lu on %s ignored from %s",
			(unsigned long) newts,
			chptr->chname,
			client_p->name);

      newts = (oldts==0) ? oldts : 800000000;
    }

  /*
   * XXX - this no doubt destroys vchans
   */
  if (isnew)
    chptr->channelts = tstosend = newts;
  /* Remote is sending users to a permanent channel.. we need to drop our version
   * and use theirs, to keep compatibility -- fl */
  else if (chptr->users == 0 && parv[4+args][0])
    {
       keep_our_modes = NO;
       chptr->channelts = tstosend = newts;
    }
  /* Theyre not sending users, lets just ignore it and carry on */
  else if (chptr->users == 0 && !parv[4+args][0])
    return;

  /* It isnt a perm channel, do normal timestamp rules */
  else if (newts == 0 || oldts == 0)
    chptr->channelts = tstosend = 0;
  else if (!newts)
    chptr->channelts = tstosend = oldts;
  else if (newts == oldts)
    tstosend = oldts;
  else if (newts < oldts)
    {
      if (ServerInfo.no_hack_ops)
	{
	  keep_our_modes = NO;
	  chptr->channelts = tstosend = newts;
	}
      else
	{
	  if (doesop)
	    keep_our_modes = NO;
	  if (chptr->opcount && !doesop)
	    tstosend = oldts;
	  else
	    chptr->channelts = tstosend = newts;
	}
    }
  else
    {
      if (chptr->opcount)
        keep_new_modes = NO;
      if (doesop && !chptr->opcount)
        {
          chptr->channelts = tstosend = newts;
        }
      else
        tstosend = oldts;
    }

  if (!keep_new_modes)
    mode = *oldmode;
  else if (keep_our_modes)
    {
      mode.mode |= oldmode->mode;
      if (oldmode->limit > mode.limit)
        mode.limit = oldmode->limit;
      if (strcmp(mode.key, oldmode->key) < 0)
        strcpy(mode.key, oldmode->key);
    }

  if(mode.mode & MODE_HIDEOPS)
    hide_or_not = ONLY_CHANOPS_HALFOPS;
  else
    hide_or_not = ALL_MEMBERS;

  if ((MODE_HIDEOPS & mode.mode) && !(MODE_HIDEOPS & oldmode->mode))
    sync_channel_oplists(chptr, 1);

  /* Don't reveal the ops, only to remove them all */
  if (keep_our_modes)
    if (!(MODE_HIDEOPS & mode.mode) && (MODE_HIDEOPS & oldmode->mode))
      sync_channel_oplists(chptr, 0);

  set_final_mode(&mode,oldmode);
  chptr->mode = mode;

  /* Lost the TS, other side wins, so remove modes on this side */
  if (!keep_our_modes)
    {
      remove_our_modes(hide_or_not, chptr, top_chptr, source_p);
    }

  if(*modebuf != '\0')
    {
      /* This _SHOULD_ be to ALL_MEMBERS
       * It contains only +aimnstlki, etc */
      if(top_chptr != NULL)
	sendto_channel_local(ALL_MEMBERS,
			     chptr, ":%s MODE %s %s %s",
			     me.name,
			     top_chptr->chname, modebuf, parabuf);
      else
	sendto_channel_local(ALL_MEMBERS,
			     chptr, ":%s MODE %s %s %s",
			     me.name,
			     chptr->chname, modebuf, parabuf);
    }

  *modebuf = *parabuf = '\0';
  if (parv[3][0] != '0' && keep_new_modes)
    channel_modes(chptr, source_p, modebuf, parabuf);
  else
    {
      modebuf[0] = '0';
      modebuf[1] = '\0';
    }

  ircsprintf(buf, ":%s SJOIN %lu %s %s %s :",
	parv[0],
	(unsigned long) tstosend,
	parv[2], modebuf, parabuf);

  mbuf = modebuf;
  para[0] = para[1] = para[2] = para[3] = "";
  pargs = 0;
  nick_pointer = sjbuf;

  *mbuf++ = '+';

  sh = sjbuf_nh;
  
  for (s = s0 = strtoken(&p, parv[args+4], " "); s;
       				s = s0 = strtoken(&p, (char *)NULL, " "))
    {
      fl = 0;

      for (i = 0; i < 2; i++)
      {
        if (*s == '@')
	{
	  fl |= MODE_CHANOP;
          if (keep_new_modes)
            *sh++ = *s;
	  s++;
	}
        else if (*s == '+')
	{
	  fl |= MODE_VOICE;
          if (keep_new_modes)
            *sh++ = *s;
	  s++;
	}
        else if (*s == '%')
        {
          fl |= MODE_HALFOP;
          if (keep_new_modes)
            *sh++ = '@';
          s++;
        }
      }

      sh += ircsprintf(sh, "%s ", s); /* Copy over the nick */

      if (!keep_new_modes)
       {
        if (fl & MODE_CHANOP)
          {
            fl = MODE_DEOPPED;
          }
        else
          {
            fl = 0;
          }
       }

      if (!(target_p = find_chasing(source_p, s, NULL)))
        continue;
      if (target_p->from != client_p)
        continue;
      if (!IsPerson(target_p))
        continue;
      
      people++;

      /* LazyLinks - Introduce unknown clients before sending the sjoin */
      if (ServerInfo.hub)
      {
        for (m = serv_list.head; m; m = m->next)
        {
          lclient_p = m->data;

          /* Hopefully, the server knows about it's own clients. */
          if (client_p == lclient_p)
            continue;

          /* Ignore non lazylinks */
          if (!IsCapable(lclient_p,CAP_LL))
            continue;

          /* Ignore servers we won't tell anyway */
          if( !(RootChan(chptr)->lazyLinkChannelExists &
                lclient_p->localClient->serverMask) )
            continue;

          /* Ignore servers that already know target_p */
          if( !(target_p->lazyLinkClientExists &
                lclient_p->localClient->serverMask) )
          {
            /* Tell LazyLink Leaf about client_p,
             * as the leaf is about to get a SJOIN */
            sendnick_TS( lclient_p, target_p );
            add_lazylinkclient(lclient_p,target_p);
          }
        }
      }
      
      if (!IsMember(target_p, chptr))
        {
          add_user_to_channel(chptr, target_p, fl);
	  /* XXX vchan stuff */

	  if( top_chptr )
	    {
	      add_vchan_to_client_cache(target_p,top_chptr, chptr);
	      sendto_channel_local(ALL_MEMBERS,chptr, ":%s!%s@%s JOIN :%s",
				   target_p->name,
				   target_p->username,
				   target_p->host,
				   top_chptr->chname);
	    }
	  else
	    {
	      sendto_channel_local(ALL_MEMBERS,chptr, ":%s!%s@%s JOIN :%s",
				   target_p->name,
				   target_p->username,
				   target_p->host,
				   parv[2]);
	    }
        }

      /* XXX if (server_nick_count >= MAXMODEPARAMS) ... 
       *  if this is ever a possibility...
       */
      if (keep_new_modes)
	ircsprintf(nick_pointer,"%s ", s0);
      else
	ircsprintf(nick_pointer,"%s ", s);

      nick_pointer += strlen(nick_pointer);

      if (fl & MODE_CHANOP)
        {
          *mbuf++ = 'o';
	  para[pargs++] = s;
        }
      else if (fl & MODE_VOICE)
        {
          *mbuf++ = 'v';
	  para[pargs++] = s;
        }
      else if (fl & MODE_HALFOP)
        {
          *mbuf++ = 'h';
          para[pargs++] = s;
        }

      if (pargs >= MAXMODEPARAMS)
        {
          *mbuf = '\0';
          sendto_channel_local(hide_or_not, chptr,
                               ":%s MODE %s %s %s %s %s %s",
                               me.name,
                               RootChan(chptr)->chname,
                               modebuf,
                               para[0],para[1],para[2],para[3]);
          mbuf = modebuf;
          *mbuf++ = '+';
          para[0] = para[1] = para[2] = para[3] = "";
          pargs = 0;
        }
    }
  
  *mbuf = '\0';
  if (pargs)
    {
      sendto_channel_local(hide_or_not, chptr,
                           ":%s MODE %s %s %s %s %s %s",
                           me.name,
                           RootChan(chptr)->chname,
                           modebuf,
                           para[0], para[1], para[2], para[3]);
    }

  /* relay the SJOIN to other servers */
  for(m = serv_list.head; m; m = m->next)
    {
      target_p = m->data;

      if (target_p == client_p->from)
        continue;

      /* skip lazylinks that don't know about this server */
      if (ServerInfo.hub && IsCapable(target_p,CAP_LL))
      {
        if( !(RootChan(chptr)->lazyLinkChannelExists &
              target_p->localClient->serverMask) )
          continue;
      }

      /* Its a blank sjoin, ugh */
      if (!parv[4+args][0])
          return;

      /* XXX - ids ? */
      if (IsCapable(target_p,CAP_HOPS))
        sendto_one(target_p, "%s %s", buf, sjbuf);
      else
        sendto_one(target_p, "%s %s", buf, sjbuf_nh);
   }
}

/*
 * set_final_mode
 *
 * inputs	- pointer to mode to setup
 *		- pointer to old mode
 * output	- NONE
 * side effects	- 
 */

struct mode_letter {
  int mode;
  char letter;
};

struct mode_letter flags[] = {
  { MODE_NOPRIVMSGS, 'n' },
  { MODE_TOPICLIMIT, 't' },
  { MODE_SECRET,     's' },
  { MODE_MODERATED,  'm' },
  { MODE_INVITEONLY, 'i' },
  { MODE_PRIVATE,    'p' },
  { MODE_HIDEOPS,    'a' },
  { 0, 0 }
};

static void set_final_mode(struct Mode *mode,struct Mode *oldmode)
{
  int what = 0;
  char numeric[16];
  char *s;
  int  i;

  for (i = 0; flags[i].letter; i++)
    {
      if ((flags[i].mode & mode->mode) && !(flags[i].mode & oldmode->mode))
	{
	  if (what != 1)
	    {
	      *mbuf++ = '+';
	      what = 1;
	    }
	  *mbuf++ = flags[i].letter;
	}
    }

  for (i = 0; flags[i].letter; i++)
    {
      if ((flags[i].mode & oldmode->mode) && !(flags[i].mode & mode->mode))
	{
	  if (what != -1)
	    {
	      *mbuf++ = '-';
	      what = -1;
	    }
	  *mbuf++ = flags[i].letter;
	}
    }

  if (oldmode->limit && !mode->limit)
    {
      if (what != -1)
        {
          *mbuf++ = '-';
          what = -1;
        }
      *mbuf++ = 'l';
    }
  if (oldmode->key[0] && !mode->key[0])
    {
      if (what != -1)
        {
          *mbuf++ = '-';
          what = -1;
        }
      *mbuf++ = 'k';
      strcat(parabuf, oldmode->key);
      strcat(parabuf, " ");
      pargs++;
    }
  if (mode->limit && oldmode->limit != mode->limit)
    {
      if (what != 1)
        {
          *mbuf++ = '+';
          what = 1;
        }
      *mbuf++ = 'l';
      ircsprintf(numeric, "%d", mode->limit);
      if ((s = strchr(numeric, ' ')))
        *s = '\0';
      strcat(parabuf, numeric);
      strcat(parabuf, " ");
      pargs++;
    }
  if (mode->key[0] && strcmp(oldmode->key, mode->key))
    {
      if (what != 1)
        {
          *mbuf++ = '+';
          what = 1;
        }
      *mbuf++ = 'k';
      strcat(parabuf, mode->key);
      strcat(parabuf, " ");
      pargs++;
    }
  *mbuf = '\0';
}

/*
 * remove_our_modes
 *
 * inputs	- hide from ops or not int flag
 *		- pointer to channel to remove modes from
 *		- if vchan basechannel pointer 
 *		- client pointer
 * output	- NONE
 * side effects	- Go through the local members, remove all their
 *		  chanop modes etc., this side lost the TS.
 */
static void remove_our_modes( int hide_or_not,
                              struct Channel *chptr, struct Channel *top_chptr,
                              struct Client *source_p)
{
  remove_a_mode(hide_or_not, chptr, top_chptr, source_p, &chptr->chanops, 'o');
  remove_a_mode(hide_or_not, chptr, top_chptr, source_p, &chptr->halfops, 'h');
  remove_a_mode(hide_or_not, chptr, top_chptr, source_p, &chptr->voiced, 'v');

  /* Move all voice/ops etc. to non opped list */
  dlinkMoveList(&chptr->chanops, &chptr->peons);
  dlinkMoveList(&chptr->halfops, &chptr->peons);
  dlinkMoveList(&chptr->voiced, &chptr->peons);

  dlinkMoveList(&chptr->locchanops, &chptr->locpeons);
  dlinkMoveList(&chptr->lochalfops, &chptr->locpeons);
  dlinkMoveList(&chptr->locvoiced, &chptr->locpeons);

  chptr->opcount = 0;
}


/*
 * remove_a_mode
 *
 * inputs	-
 * output	- NONE
 * side effects	- remove ONE mode from a channel
 */
static void remove_a_mode( int hide_or_not,
                           struct Channel *chptr, struct Channel *top_chptr,
                           struct Client *source_p, dlink_list *list, char flag)
{
  dlink_node *ptr;
  struct Client *target_p;
  char buf[BUFSIZE];
  char lmodebuf[MODEBUFLEN];
  char *lpara[MAXMODEPARAMS];
  char *chname;
  int count = 0;

  mbuf = lmodebuf;
  *mbuf++ = '-';

  lpara[0] = lpara[1] = lpara[2] = lpara[3] = "";

  chname = chptr->chname;

  if(IsVchan(chptr) && top_chptr)
    chname = top_chptr->chname;

  ircsprintf(buf,":%s MODE %s ", me.name, chname);

  for (ptr = list->head; ptr && ptr->data; ptr = ptr->next)
    {
      target_p = ptr->data;
      lpara[count++] = target_p->name;

      *mbuf++ = flag;

      if (count >= MAXMODEPARAMS)
	{
	  *mbuf   = '\0';
	  sendto_channel_local(hide_or_not, chptr,
			       ":%s MODE %s %s %s %s %s %s",
			       me.name,
			       chname,
			       lmodebuf,
			       lpara[0], lpara[1], lpara[2], lpara[3] );

	  mbuf = lmodebuf;
	  *mbuf++ = '-';
	  count = 0;
	  lpara[0] = lpara[1] = lpara[2] = lpara[3] = "";
	}
    }

  if(count != 0)
    {
      *mbuf   = '\0';
      sendto_channel_local(hide_or_not, chptr,
			   ":%s MODE %s %s %s %s %s %s",
			   me.name,
			   chname,
			   lmodebuf,
			   lpara[0], lpara[1], lpara[2], lpara[3] );

    }
}

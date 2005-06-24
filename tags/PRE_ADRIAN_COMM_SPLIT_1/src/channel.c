/************************************************************************
 *   IRC - Internet Relay Chat, src/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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
 *
 * a number of behaviours in set_mode() have been rewritten
 * These flags can be set in a define if you wish.
 *
 * OLD_P_S      - restore xor of p vs. s modes per channel
 *                currently p is rather unused, so using +p
 *                to disable "knock" seemed worth while.
 * OLD_MODE_K   - new mode k behaviour means user can set new key
 *                while old one is present, mode * -k of old key is done
 *                on behalf of user, with mode * +k of new key.
 *                /mode * -key results in the sending of a *, which
 *                can be used to resynchronize a channel.
 * OLD_NON_RED  - Current code allows /mode * -s etc. to be applied
 *                even if +s is not set. Old behaviour was not to allow
 *                mode * -p etc. if flag was clear
 *
 *
 * $Id$
 */
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_serv.h"       /* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h" /* ConfigFileEntry */
#include "vchannel.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* LazyLinks */
static void destroy_channel(struct Channel *);

#ifdef NEED_SPLITCODE

static void check_still_split();
int server_was_split=YES;
int got_server_pong;
time_t server_split_time;

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
struct Channel *empty_channel_list=(struct Channel*)NULL;
void remove_empty_channels();
#endif
#endif

struct Channel *GlobalChannelList = NullChn;

static  int     add_id (struct Client *, struct Channel *, char *, int);
static  int     del_id (struct Channel *, char *, int);
static  void    free_channel_masks(struct Channel *);
static  void    sub1_from_channel (struct Channel *);


/* static functions used in set_mode */
static char* pretty_mask(char *);
static char *fix_key(char *);
static char *fix_key_old(char *);
static void collapse_signs(char *);
static int errsent(int,int *);
static void change_chan_flag(struct Channel *, struct Client *, int );
static void set_deopped(struct Client *,struct Channel *,int);


/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */

static  char    buf[BUFSIZE];
static  char    modebuf[MODEBUFLEN], modebuf2[MODEBUFLEN];
static  char    parabuf[MODEBUFLEN], parabuf2[MODEBUFLEN];


/* 
 * return the length (>=0) of a chain of struct SLinks.
 */
static  int     list_length(struct SLink *lp)
{
  int   count = 0;

  for (; lp; lp = lp->next)
    count++;
  return count;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\0`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
static char* check_string(char* s)
{
  static char star[2] = "*";
  char* str = s;

  if (BadPtr(s))
    return star;

  for ( ; *s; ++s) {
    if (IsSpace(*s))
      {
        *s = '\0';
        break;
      }
  }
  return str;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
static char* make_nick_user_host(const char* nick, 
                                 const char* name, const char* host)
{
  static char namebuf[NICKLEN + USERLEN + HOSTLEN + 6];
  int   n;
  char* s;
  const char* p;

  s = namebuf;

  for (p = nick, n = NICKLEN; *p && n--; )
    *s++ = *p++;
  *s++ = '!';
  for(p = name, n = USERLEN; *p && n--; )
    *s++ = *p++;
  *s++ = '@';
  for(p = host, n = HOSTLEN; *p && n--; )
    *s++ = *p++;
  *s = '\0';
  return namebuf;
}

/*
 * Ban functions to work with mode +b/e/d/I
 */
/* add the specified ID to the channel.. 
   -is 8/9/00 */

static  int     add_id(struct Client *cptr, struct Channel *chptr, 
			  char *banid, int type)
{
  struct SLink  *ban;
  struct SLink  **list;

  /* dont let local clients overflow the banlist */
  if ((!IsServer(cptr)) && (chptr->num_bed >= MAXBANS))
	  if (MyClient(cptr))
	    {
	      sendto_one(cptr, form_str(ERR_BANLISTFULL),
                   me.name, cptr->name,
                   chptr->chname, banid);
	      return -1;
	    }
  
  if (MyClient(cptr))
    collapse(banid);

  switch(type) 
    {
    case CHFL_BAN:
      list = &chptr->banlist;
      break;
    case CHFL_EXCEPTION:
      list = &chptr->exceptlist;
      break;
    case CHFL_DENY:
      list = &chptr->denylist;
      break;
    case CHFL_INVEX:
      list = &chptr->invexlist;
      break;
    default:
      sendto_realops("add_id() called with unknown ban type %d!", type);
      return -1;
    }

  for (ban = *list; ban; ban = ban->next)
	  if (match(BANSTR(ban), banid))
	    return -1;
  
  ban = make_link();
  memset(ban, 0, sizeof(struct SLink));
  ban->flags = type;
  ban->next = *list;
  
  ban->value.banptr = (aBan *)MyMalloc(sizeof(aBan));
  ban->value.banptr->banstr = (char *)MyMalloc(strlen(banid)+1);
  (void)strcpy(ban->value.banptr->banstr, banid);

  if (IsPerson(cptr))
    {
      ban->value.banptr->who =
        (char *)MyMalloc(strlen(cptr->name)+
                         strlen(cptr->username)+
                         strlen(cptr->host)+3);
      ircsprintf(ban->value.banptr->who, "%s!%s@%s",
                 cptr->name, cptr->username, cptr->host);
    }
  else
    {
      ban->value.banptr->who = (char *)MyMalloc(strlen(cptr->name)+1);
      (void)strcpy(ban->value.banptr->who, cptr->name);
    }

  ban->value.banptr->when = CurrentTime;

  *list = ban;
  chptr->num_bed++;
  return 0;
}

/*
 *
 * "del_id - delete an id belonging to cptr
 * if banid is null, deleteall banids belonging to cptr."
 *
 * from orabidoo
 * modified 8/9/00 by is: now we handle add ban types here
 * (invex/excemp/deny/etc)
 */
static  int     del_id(struct Channel *chptr, char *banid, int type)
{
  register struct SLink *ban;
  register struct SLink *tmp;
  register struct SLink **list;

  if (!banid)
    return -1;

  switch(type)
    {
    case CHFL_BAN:
      list = &chptr->banlist;
      break;
    case CHFL_EXCEPTION:
      list = &chptr->exceptlist;
      break;
    case CHFL_DENY:
      list = &chptr->denylist;
      break;
    case CHFL_INVEX:
      list = &chptr->invexlist;
      break;
    default:
      sendto_realops("del_id() called with unknown ban type %d!", type);
      return -1;
    }

  for (ban = *list; ban; ban = ban->next)
    {
      if (irccmp(banid, ban->value.banptr->banstr)==0)
        {
          tmp = ban;
          *list = tmp->next;
          MyFree(tmp->value.banptr->banstr);
          MyFree(tmp->value.banptr->who);
          MyFree(tmp->value.banptr);
          free_link(tmp);
	  /* num_bed should never be < 0 */
	  if(chptr->num_bed > 0)
	    chptr->num_bed--;
	  else
	    chptr->num_bed = 0;
          break;
        }
    }
  return 0;
}

/*
 * del_matching_exception - delete an exception matching this user
 *
 * The idea is, if a +e client gets kicked for any reason
 * remove the matching exception for this client.
 * This will immediately stop channel "chatter" with scripts
 * that kick on matching ban. It will also stop apparent "desyncs."
 * It's not the long term answer, but it will tide us over.
 *
 * modified from orabidoo - Dianora
 */
static void del_matching_exception(struct Client *cptr,struct Channel *chptr)
{
  register struct SLink **ex;
  register struct SLink *tmp;
  char  s[NICKLEN + USERLEN + HOSTLEN+6];
  char  *s2;

  if (!IsPerson(cptr))
    return;

  strcpy(s, make_nick_user_host(cptr->name, cptr->username, cptr->host));
#ifdef IPV6
  s2 = make_nick_user_host(cptr->name, cptr->username,
                           mk6addrstr(&cptr->ip6));
#else
  s2 = make_nick_user_host(cptr->name, cptr->username,
                           inetntoa((char*) &cptr->ip));
#endif

  for (ex = &(chptr->exceptlist); *ex; ex = &((*ex)->next))
    {
      tmp = *ex;

      if (match(BANSTR(tmp), s) ||
          match(BANSTR(tmp), s2) )
        {

          /* code needed here to send -e to channel.
           * I will not propogate the removal,
           * This will lead to desyncs of e modes,
           * but its not going to be any worse then it is now.
           *
           * Kickee gets to see the -e removal by the server
           * since they have not yet been removed from the channel.
           * I don't think thats a biggie.
           *
           * -Dianora
           */

          sendto_channel_butserv(chptr,
                                 &me,
                                 ":%s MODE %s -e %s", 
                                 me.name,
                                 chptr->chname,
                                 BANSTR(tmp));

          *ex = tmp->next;
          MyFree(tmp->value.banptr->banstr);
          MyFree(tmp->value.banptr->who);
          MyFree(tmp->value.banptr);
          free_link(tmp);
	  /* num_bed should never be < 0 */
	  if(chptr->num_bed > 0)
	    chptr->num_bed--;
	  else
	    chptr->num_bed = 0;
          return;
        }
    }
}

/*
 * is_banned -  returns an int 0 if not banned,
 *              CHFL_BAN if banned (or +d'd)
 *              CHFL_EXCEPTION if they have a ban exception
 *
 * IP_BAN_ALL from comstud
 * always on...
 *
 * +e code from orabidoo
 */

int is_banned(struct Client *cptr,struct Channel *chptr)
{
  register struct SLink *tmp;
  register struct SLink *t2;
  char  s[NICKLEN+USERLEN+HOSTLEN+6];
  char  *s2;

  if (!IsPerson(cptr))
    return (0);

  strcpy(s, make_nick_user_host(cptr->name, cptr->username, cptr->host));
  s2 = make_nick_user_host(cptr->name, cptr->username, cptr->sockhost);

  for (tmp = chptr->banlist; tmp; tmp = tmp->next)
    {
      if (match(BANSTR(tmp), s) ||
	  match(BANSTR(tmp), s2))
	break;
    }

  if (!tmp)
    {  /* check +d list */
      for (tmp = chptr->denylist; tmp; tmp = tmp->next)
	{
	  if (match(BANSTR(tmp), cptr->info))
	    break;
	}
    }

  if (tmp)
    {
      for (t2 = chptr->exceptlist; t2; t2 = t2->next)
        if (match(BANSTR(t2), s) ||
            match(BANSTR(t2), s2))
          {
            return CHFL_EXCEPTION;
          }
    }

  /* return CHFL_BAN for +b or +d match, we really dont need to be more
     specific */
  return ((tmp?CHFL_BAN:0));
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
void    add_user_to_channel(struct Channel *chptr, struct Client *who, int flags)
{
  struct SLink *ptr;

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
  if( chptr->mode.mode & MODE_SPLIT )
    {
      /* Unmark the split mode */
      chptr->mode.mode &= ~MODE_SPLIT;

      /* remove from the empty channel double link list */
      if (chptr->last_empty_channel)
        chptr->last_empty_channel->next_empty_channel =
          chptr->next_empty_channel;
      else
        empty_channel_list = chptr->next_empty_channel;
      if (chptr->next_empty_channel)
        chptr->next_empty_channel->last_empty_channel =
          chptr->last_empty_channel;
    }
#endif

  if (who->user)
    {
      ptr = make_link();
      ptr->flags = flags;
      ptr->value.cptr = who;
      ptr->next = chptr->members;
      chptr->members = ptr;
      chptr->users++;
      if(flags & MODE_CHANOP)
        chptr->opcount++;

      /* LazyLink code */
      if(MyClient(who))
        {
          chptr->locusers++;
          chptr->locusers_last = CurrentTime;
        }

      ptr = make_link();
      ptr->value.chptr = chptr;
      ptr->next = who->user->channel;
      who->user->channel = ptr;
      who->user->joined++;
    }
}

void    remove_user_from_channel(struct Client *sptr,struct Channel *chptr,int was_kicked)
{
  struct SLink  **curr;
  struct SLink  *tmp;

  for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
    if (tmp->value.cptr == sptr)
      {
        if((tmp->flags & MODE_CHANOP) && chptr->opcount)
          chptr->opcount--;

        /* LazyLink code */
        if(MyClient(sptr) && chptr->locusers)
          {
            chptr->locusers--;
            chptr->locusers_last = CurrentTime;
	  }
        /* User was kicked, but had an exception.
         * so, to reduce chatter I'll remove any
         * matching exception now.
         */
        if(was_kicked && (tmp->flags & CHFL_EXCEPTION))
          {
            del_matching_exception(sptr,chptr);
          }
        *curr = tmp->next;
        free_link(tmp);
        break;
      }
  for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
    if (tmp->value.chptr == chptr)
      {
        *curr = tmp->next;
        free_link(tmp);
        break;
      }
  sptr->user->joined--;
  
  if (IsVchan(chptr))
    del_vchan_from_client_cache(sptr, chptr); 

  sub1_from_channel(chptr);
}

static  void    change_chan_flag(struct Channel *chptr,struct Client *cptr, int flag)
{
  struct SLink *tmp;

  if ((tmp = find_user_link(chptr->members, cptr)))
   {
    if (flag & MODE_ADD)
      {
        if (flag & MODE_CHANOP)
          {
            tmp->flags &= ~MODE_DEOPPED;
            if( !(tmp->flags & MODE_CHANOP) )
              {
                chptr->opcount++;
              }
          }
        tmp->flags |= flag & MODE_FLAGS;
      }
    else
      {
        if ((tmp->flags & MODE_CHANOP) && (flag & MODE_CHANOP))
          {
            if( chptr->opcount )
              {
                chptr->opcount--;
              }
          }
        tmp->flags &= ~flag & MODE_FLAGS;
      }
   }
}

static  void   set_deopped(struct Client *cptr, struct Channel *chptr,int flag)
{
  struct SLink  *tmp;

  if ((tmp = find_user_link(chptr->members, cptr)))
    if ((tmp->flags & flag) == 0)
      tmp->flags |= MODE_DEOPPED;
}

int     is_chan_op(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *lp;

  if (chptr)
    if ((lp = find_user_link(chptr->members, cptr)))
      return (lp->flags & CHFL_CHANOP);
  
  return 0;
}

int     is_deopped(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *lp;

  if (chptr)
    if ((lp = find_user_link(chptr->members, cptr)))
      return (lp->flags & CHFL_DEOPPED);
  
  return 0;
}

int     has_voice(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *lp;

  if (chptr)
    if ((lp = find_user_link(chptr->members, cptr)))
      return (lp->flags & CHFL_VOICE);

  return 0;
}

int     can_send(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *lp;

#ifdef JUPE_CHANNEL
  if (MyClient(cptr) && (chptr->mode.mode & MODE_JUPED))
    {
      return (MODE_JUPED);
    }
#endif

  lp = find_user_link(chptr->members, cptr);

  if (ConfigFileEntry.quiet_on_ban)
    if (is_banned(cptr, chptr))
      return MODE_BAN;

  if (chptr->mode.mode & MODE_MODERATED &&
      (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE))))
    return (MODE_MODERATED);

  if (chptr->mode.mode & MODE_NOPRIVMSGS && !lp)
    return (MODE_NOPRIVMSGS);

  return 0;
}

int     user_channel_mode(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *lp;

  if (chptr)
    if ((lp = find_user_link(chptr->members, cptr)))
      return (lp->flags);
  
  return 0;
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
void channel_modes(struct Client *cptr, char *mbuf, char *pbuf, struct Channel *chptr)
{
  *mbuf++ = '+';
  if (chptr->mode.mode & MODE_SECRET)
    *mbuf++ = 's';

  /* bug found by "is" ejb@debian.org */
#ifdef OLD_P_S
  else if (chptr->mode.mode & MODE_PRIVATE)
    *mbuf++ = 'p';
#else
  if (chptr->mode.mode & MODE_PRIVATE)
    *mbuf++ = 'p';
#endif

  if (chptr->mode.mode & MODE_MODERATED)
    *mbuf++ = 'm';
  if (chptr->mode.mode & MODE_TOPICLIMIT)
    *mbuf++ = 't';
  if (chptr->mode.mode & MODE_INVITEONLY)
    *mbuf++ = 'i';
  if (chptr->mode.mode & MODE_NOPRIVMSGS)
    *mbuf++ = 'n';
  if (chptr->mode.limit)
    {
      *mbuf++ = 'l';
      if (IsMember(cptr, chptr) || IsServer(cptr))
        ircsprintf(pbuf, "%d ", chptr->mode.limit);
    }
  if (*chptr->mode.key)
    {
      *mbuf++ = 'k';
      if (IsMember(cptr, chptr) || IsServer(cptr))
        (void)strcat(pbuf, chptr->mode.key);
    }
  *mbuf++ = '\0';
  return;
}

/*
 * only used to send +b and +e now, +d/+a too.
 * 
 */

static  void    send_mode_list(struct Client *cptr,
                               char *chname,
                               struct SLink *top,
                               int mask,
                               char flag)
{
  struct SLink  *lp;
  char  *cp, *name;
  int   count = 0, send = 0;
  
  cp = modebuf + strlen(modebuf);
  if (*parabuf) /* mode +l or +k xx */
    count = 1;
  for (lp = top; lp; lp = lp->next)
    {
      if (!(lp->flags & mask))
        continue;
      name = BANSTR(lp);
        
      if (strlen(parabuf) + strlen(name) + 10 < (size_t) MODEBUFLEN)
        {
          (void)strcat(parabuf, " ");
          (void)strcat(parabuf, name);
          count++;
          *cp++ = flag;
          *cp = '\0';
        }
      else if (*parabuf)
        send = 1;
      if (count == 3)
        send = 1;
      if (send)
        {
          sendto_one(cptr, ":%s MODE %s %s %s",
                     me.name, chname, modebuf, parabuf);
          send = 0;
          *parabuf = '\0';
          cp = modebuf;
          *cp++ = '+';
          if (count != 3)
            {
              (void)strcpy(parabuf, name);
              *cp++ = flag;
            }
          count = 0;
          *cp = '\0';
        }
    }
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void send_channel_modes(struct Client *cptr, struct Channel *chptr)
{
  struct SLink  *l, *anop = NULL, *skip = NULL;
  int   n = 0;
  char  *t;

  if (*chptr->chname != '#')
    return;

  *modebuf = *parabuf = '\0';
  channel_modes(cptr, modebuf, parabuf, chptr);

  if (*parabuf)
    strcat(parabuf, " ");
  ircsprintf(buf, ":%s SJOIN %lu %s %s %s:", me.name,
          chptr->channelts, chptr->chname, modebuf, parabuf);
  t = buf + strlen(buf);
  for (l = chptr->members; l && l->value.cptr; l = l->next)
    if (l->flags & MODE_CHANOP)
      {
        anop = l;
        break;
      }
  /* follow the channel, but doing anop first if it's defined
  **  -orabidoo
  */
  l = NULL;
  for (;;)
    {
      if (anop)
        {
          l = skip = anop;
          anop = NULL;
        }
      else 
        {
          if (l == NULL || l == skip)
            l = chptr->members;
          else
            l = l->next;
          if (l && l == skip)
            l = l->next;
          if (l == NULL)
            break;
        }
      if (l->flags & MODE_CHANOP)
        *t++ = '@';
      if (l->flags & MODE_VOICE)
        *t++ = '+';
      strcpy(t, l->value.cptr->name);
      t += strlen(t);
      *t++ = ' ';
      n++;
      if (t - buf > BUFSIZE - 80)
        {
          *t++ = '\0';
          if (t[-1] == ' ') t[-1] = '\0';
          sendto_one(cptr, "%s", buf);
          ircsprintf(buf, ":%s SJOIN %lu %s 0 :",
                  me.name, chptr->channelts,
                  chptr->chname);
          t = buf + strlen(buf);
          n = 0;
        }
    }
      
  if (n)
    {
      *t++ = '\0';
      if (t[-1] == ' ') t[-1] = '\0';
      sendto_one(cptr, "%s", buf);
    }
  *parabuf = '\0';
  *modebuf = '+';
  modebuf[1] = '\0';
  send_mode_list(cptr, chptr->chname, chptr->banlist, CHFL_BAN,'b');

  if (modebuf[1] || *parabuf)
    sendto_one(cptr, ":%s MODE %s %s %s",
               me.name, chptr->chname, modebuf, parabuf);

  if(!IsCapable(cptr,CAP_EX))
    return;

  *parabuf = '\0';
  *modebuf = '+';
  modebuf[1] = '\0';
  send_mode_list(cptr, chptr->chname, chptr->exceptlist, CHFL_EXCEPTION,'e');

  if (modebuf[1] || *parabuf)
    sendto_one(cptr, ":%s MODE %s %s %s",
               me.name, chptr->chname, modebuf, parabuf);

  if(!IsCapable(cptr,CAP_DE))
      return;
  *parabuf = '\0';
  *modebuf = '+';
  modebuf[1] = '\0';
  send_mode_list(cptr, chptr->chname, chptr->denylist, CHFL_DENY,'d');
  
  if (modebuf[1] || *parabuf)
    sendto_one(cptr, ":%s MODE %s %s %s",
               me.name, chptr->chname, modebuf, parabuf);

  if (!IsCapable(cptr,CAP_IE))
		return;
	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_mode_list(cptr, chptr->chname, chptr->invexlist, CHFL_INVEX, 'I');

  if (modebuf[1] || *parabuf)
    sendto_one(cptr, ":%s MODE %s %s %s",
               me.name, chptr->chname, modebuf, parabuf);
}

/* stolen from Undernet's ircd  -orabidoo
 *
 */

static char* pretty_mask(char* mask)
{
  register char* cp = mask;
  register char* user;
  register char* host;

  if ((user = strchr(cp, '!')))
    *user++ = '\0';
  if ((host = strrchr(user ? user : cp, '@')))
    {
      *host++ = '\0';
      if (!user)
        return make_nick_user_host("*", check_string(cp), check_string(host));
    }
  else if (!user && strchr(cp, '.'))
    return make_nick_user_host("*", "*", check_string(cp));
  return make_nick_user_host(check_string(cp), check_string(user), 
                             check_string(host));
}

static  char    *fix_key(char *arg)
{
  u_char        *s, *t, c;

  for (s = t = (u_char *)arg; (c = *s); s++)
    {
      c &= 0x7f;
      if (c != ':' && c > ' ')
        *t++ = c;
    }
  *t = '\0';
  return arg;
}

/*
 * Here we attempt to be compatible with older non-hybrid servers.
 * We can't back down from the ':' issue however.  --Rodder
 */
static  char    *fix_key_old(char *arg)
{
  u_char        *s, *t, c;

  for (s = t = (u_char *)arg; (c = *s); s++)
    { 
      c &= 0x7f;
      if ((c != 0x0a) && (c != ':'))
        *t++ = c;
    }
  *t = '\0';
  return arg;
}

/*
 * like the name says...  take out the redundant signs in a modechange list
 */
static  void    collapse_signs(char *s)
{
  char  plus = '\0', *t = s, c;
  while ((c = *s++))
    {
      if (c != plus)
        *t++ = c;
      if (c == '+' || c == '-')
        plus = c;
    }
  *t = '\0';
}

/* little helper function to avoid returning duplicate errors */
static  int     errsent(int err, int *errs)
{
  if (err & *errs)
    return 1;
  *errs |= err;
  return 0;
}

/* bitmasks for various error returns that set_mode should only return
 * once per call  -orabidoo
 */

#define SM_ERR_NOTS             0x00000001      /* No TS on channel */
#define SM_ERR_NOOPS            0x00000002      /* No chan ops */
#define SM_ERR_UNKNOWN          0x00000004
#define SM_ERR_RPL_C            0x00000008
#define SM_ERR_RPL_B            0x00000010
#define SM_ERR_RPL_E            0x00000020
#define SM_ERR_NOTONCHANNEL     0x00000040      /* Not on channel */
#define SM_ERR_RESTRICTED       0x00000080      /* Restricted chanop */
#define SM_ERR_RPL_I						0x00000100
#define SM_ERR_RPL_D            0x00000200

/*
** Apply the mode changes passed in parv to chptr, sending any error
** messages and MODE commands out.  Rewritten to do the whole thing in
** one pass, in a desperate attempt to keep the code sane.  -orabidoo
*/
/*
 * rewritten to remove +h/+c/z 
 * in spirit with the one pass idea, I've re-written how "imnspt"
 * handling was done
 *
 * I've also left some "remnants" of the +h code in for possible
 * later addition.
 * For example, isok could be replaced witout half ops, with ischop() or
 * chan_op depending.
 *
 * -Dianora
 */

void set_channel_mode(struct Client *cptr,
                      struct Client *sptr,
                      struct Channel *chptr,
                      int parc,
                      char *parv[],
		      char *real_name)
{
  int   errors_sent = 0, opcnt = 0, len = 0, tmp, nusers;
  int   keychange = 0, limitset = 0;
  int   whatt = MODE_ADD, the_mode = 0;
#ifdef OLD_P_S
  int   done_s_or_p = NO;
#else
  int   done_s = NO, done_p = NO;
#endif
  int   done_i = NO, done_m = NO, done_n = NO, done_t = NO;
  struct Client *who;
  struct SLink  *lp;
  char  *curr = parv[0], c, *arg, plus = '+', *tmpc;
  char  numeric[16];
  /* mbufw gets the param-less mode chars, always with their sign
   * mbuf2w gets the paramed mode chars, always with their sign
   * pbufw gets the params, in ID form whenever possible
   * pbuf2w gets the params, no ID's
   */
  /* no ID code at the moment
   * pbufw gets the params, no ID's
   * grrrr for now I'll stick the params into pbufw without ID's
   * -Dianora
   */
  /* *sigh* FOR YOU Roger, and ONLY for you ;-)
   * lets stick mode/params that only the newer servers will understand
   * into modebuf_new/parabuf_new 
   * even worse!  nodebuf_newer/parabuf_newer <-- for CAP_DE       
	 * "£$"£*(!!! <-- CAP_IE
   */

  char  modebuf_invex[MODEBUFLEN];
  char  parabuf_invex[MODEBUFLEN];

  char  modebuf_newer[MODEBUFLEN];
  char  parabuf_newer[MODEBUFLEN];

  char  modebuf_new[MODEBUFLEN];
  char  parabuf_new[MODEBUFLEN];

  char  *mbufw = modebuf, *mbuf2w = modebuf2;
  char  *pbufw = parabuf, *pbuf2w = parabuf2;

  char  *mbufw_new = modebuf_new;
  char  *pbufw_new = parabuf_new;

  char  *mbufw_newer = modebuf_newer;
  char  *pbufw_newer = parabuf_newer;

  char  *mbufw_invex = modebuf_invex;
  char  *pbufw_invex = parabuf_invex;

  int   ischop;
  int   isok;
  int   isdeop;
  int   chan_op;
  int   self_lose_ops;
  int   user_mode;

  self_lose_ops = 0;

  user_mode = user_channel_mode(sptr, chptr);
  chan_op = (user_mode & CHFL_CHANOP);

  /* has ops or is a server */
  ischop = IsServer(sptr) || chan_op;

  /* is client marked as deopped */
  isdeop = !ischop && !IsServer(sptr) && (user_mode & CHFL_DEOPPED);

  /* is an op or server or remote user on a TS channel */
  isok = ischop || (!isdeop && IsServer(cptr) && chptr->channelts);

  if(isok)
    chptr->keep_their_modes = YES;

  /* isok_c calculated later, only if needed */

  /* parc is the number of _remaining_ args (where <0 means 0);
  ** parv points to the first remaining argument
  */
  parc--;
  parv++;

  for ( ; ; )
    {
      if (BadPtr(curr))
        {
          /*
           * Deal with mode strings like "+m +o blah +i"
           */
          if (parc-- > 0)
            {
              curr = *parv++;
              continue;
            }
          break;
        }
      c = *curr++;

      switch (c)
        {
        case '+' :
          whatt = MODE_ADD;
          plus = '+';
          continue;
          /* NOT REACHED */
          break;

        case '-' :
          whatt = MODE_DEL;
          plus = '-';
          continue;
          /* NOT REACHED */
          break;

        case '=' :
          whatt = MODE_QUERY;
          plus = '=';   
          continue;
          /* NOT REACHED */
          break;

        case 'o' :
        case 'v' :
          if (MyClient(sptr))
            {
              if(!IsMember(sptr, chptr))
                {
                  if(!errsent(SM_ERR_NOTONCHANNEL, &errors_sent))
                    sendto_one(sptr, form_str(ERR_NOTONCHANNEL),
                               me.name, sptr->name, real_name);
                  /* eat the parameter */
                  parc--;
                  parv++;
                  break;
                }
#ifdef LITTLE_I_LINES
              else
                {
                  if(IsRestricted(sptr) && (whatt == MODE_ADD))
                    {
                      if(!errsent(SM_ERR_RESTRICTED, &errors_sent))
                        {
                          sendto_one(sptr,
            ":%s NOTICE %s :*** Notice -- You are restricted and cannot chanop others",
                                 me.name,
                                 sptr->name);
                        }
                      /* eat the parameter */
                      parc--;
                      parv++;
                      break;
                    }
                }
#endif
            }
          if (whatt == MODE_QUERY)
            break;
          if (parc-- <= 0)
            break;
          arg = check_string(*parv++);

          if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;

          if (!(who = find_chasing(sptr, arg, NULL)))
            break;

          /* there is always the remote possibility of picking up
           * a bogus user, be nasty to core for that. -Dianora
           */

          if (!who->user)
            break;

          /* no more of that mode bouncing crap */
          if (!IsMember(who, chptr))
            {
              if (MyClient(sptr))
                sendto_one(sptr, form_str(ERR_USERNOTINCHANNEL), me.name, 
                           sptr->name, arg, real_name);
              break;
            }

          if ((who == sptr) && (c == 'o'))
            {
              if(whatt == MODE_ADD)
                break;
              
              if(whatt == MODE_DEL)
                self_lose_ops = 1;
              }

          /* ignore server-generated MODE +-ovh */
          if (IsServer(sptr))
            {
              ts_warn( "MODE %c%c on %s for %s from server %s (ignored)", 
                       (whatt == MODE_ADD ? '+' : '-'), c, real_name, 
                       who->name,sptr->name);
              break;
            }

          if (c == 'o')
            the_mode = MODE_CHANOP;
          else if (c == 'v')
            the_mode = MODE_VOICE;

          if (isdeop && (c == 'o') && whatt == MODE_ADD)
            set_deopped(who, chptr, the_mode);

          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }
        
          tmp = strlen(arg);
          if (len + tmp + 2 >= MODEBUFLEN)
            break;

          *mbufw++ = plus;
          *mbufw++ = c;
          strcpy(pbufw, who->name);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          len += tmp + 1;
          opcnt++;

          change_chan_flag(chptr, who, the_mode|whatt);

          if (self_lose_ops)
            isok = 0;

          break;

        case 'k':
          if (whatt == MODE_QUERY)
            break;
          if (parc-- <= 0)
            {
              /* allow arg-less mode -k */
              if (whatt == MODE_DEL)
                arg = "*";
              else
                break;
            }
          else
            {
              if (whatt == MODE_DEL)
                {
                  arg = check_string(*parv++);
                }
              else
                {
                  if MyClient(sptr)
                    arg = fix_key(check_string(*parv++));
                  else
                    arg = fix_key_old(check_string(*parv++));
                }
            }

          if (keychange++)
            break;
          /*      if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;
            */
          if (!*arg)
            break;

          if (!isok)
            {
              if (!errsent(SM_ERR_NOOPS, &errors_sent) && MyClient(sptr))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

#ifdef OLD_MODE_K
          if ((whatt == MODE_ADD) && (*chptr->mode.key))
            {
              sendto_one(sptr, form_str(ERR_KEYSET), me.name, 
                         sptr->name, real_name);
              break;
            }
#endif
          if ( (tmp = strlen(arg)) > KEYLEN)
            {
              arg[KEYLEN] = '\0';
              tmp = KEYLEN;
            }

          if (len + tmp + 2 >= MODEBUFLEN)
            break;

#ifndef OLD_MODE_K
          /* if there is already a key, and the client is adding one
           * remove the old one, then add the new one
           */

          if((whatt == MODE_ADD) && *chptr->mode.key)
            {
              /* If the key is the same, don't do anything */

              if(!strcmp(chptr->mode.key,arg))
                break;

              sendto_channel_butserv(chptr, sptr, ":%s MODE %s -k %s", 
                                     sptr->name, real_name,
                                     chptr->mode.key);

              sendto_match_servs(chptr, cptr, ":%s MODE %s -k %s",
                                 sptr->name, real_name,
                                 chptr->mode.key);
            }
#endif
          if (whatt == MODE_DEL)
            {
              if( (arg[0] == '*') && (arg[1] == '\0'))
                arg = chptr->mode.key;
              else
                {
                  if(strcmp(arg,chptr->mode.key))
                    break;
		}
	    }

          *mbufw++ = plus;
          *mbufw++ = 'k';
          strcpy(pbufw, arg);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          len += tmp + 1;
          /*      opcnt++; */

          if (whatt == MODE_DEL)
            {
              *chptr->mode.key = '\0';
            }
          else
            {
              /*
               * chptr was zeroed
               */
              strncpy_irc(chptr->mode.key, arg, KEYLEN);
            }

          break;

          /* There is a nasty here... I'm supposed to have
           * CAP_IE before I can send exceptions to bans to a server.
           * But that would mean I'd have to keep two strings
           * one for local clients, and one for remote servers,
           * one with the 'I' strings, one without.
           * I added another parameter buf and mode buf for "new"
           * capabilities.
           *
           * -Dianora
           */

        case 'I':
          if (whatt == MODE_QUERY || parc-- <= 0)
            {
              if (!MyClient(sptr))
                break;
              if (errsent(SM_ERR_RPL_I, &errors_sent))
                break;
              /* don't allow a non chanop to see the invex list
               */
              if(isok)
                {
                  for (lp = chptr->invexlist; lp; lp = lp->next)
                    sendto_one(cptr, form_str(RPL_INVITELIST),
                               me.name, cptr->name,
                               real_name,
                               lp->value.banptr->banstr,
                               lp->value.banptr->who,
                               lp->value.banptr->when);

                  sendto_one(sptr, form_str(RPL_ENDOFINVITELIST),
                             me.name, sptr->name, 
                             real_name);
                }
              break;
            }
          arg = check_string(*parv++);

          if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;

          if (!isok)
            {
              if (!errsent(SM_ERR_NOOPS, &errors_sent) && MyClient(sptr))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                           me.name, sptr->name, 
                           real_name);
              break;
            }
          
          if(MyClient(sptr))
            chptr->keep_their_modes = YES;
          else if(!chptr->keep_their_modes)
            {
              parc--;
              parv++;
              break;
            }

          /* user-friendly ban mask generation, taken
          ** from Undernet's ircd  -orabidoo
          */
          if (MyClient(sptr))
            arg = collapse(pretty_mask(arg));

          if(*arg == ':')
            {
              parc--;
              parv++;
              break;
            }

          tmp = strlen(arg);
          if (len + tmp + 2 >= MODEBUFLEN)
            break;

          if (!(((whatt & MODE_ADD) && !add_id(sptr, chptr, arg, CHFL_INVEX)) ||
                ((whatt & MODE_DEL) && !del_id(chptr, arg, CHFL_INVEX))))
            break;

          /* This stuff can go back in when all servers understand +e 
           * with the pbufw_new nonsense removed -Dianora
           */

          /*
          *mbufw++ = plus;
          *mbufw++ = 'I';
          strcpy(pbufw, arg);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          */
          len += tmp + 1;
          opcnt++;

          *mbufw_invex++ = plus;
          *mbufw_invex++ = 'I';
          strcpy(pbufw_invex, arg);
          pbufw_invex += strlen(pbufw_invex);
          *pbufw_invex++ = ' ';

          break;

          /* There is a nasty here... I'm supposed to have
           * CAP_EX before I can send exceptions to bans to a server.
           * But that would mean I'd have to keep two strings
           * one for local clients, and one for remote servers,
           * one with the 'e' strings, one without.
           * I added another parameter buf and mode buf for "new"
           * capabilities.
           *
           * -Dianora
           */

        case 'e':
          if (whatt == MODE_QUERY || parc-- <= 0)
            {
              if (!MyClient(sptr))
                break;
              if (errsent(SM_ERR_RPL_E, &errors_sent))
                break;
              /* don't allow a non chanop to see the exception list
               * suggested by Matt on operlist nov 25 1998
               */
              if(isok)
                {
                  for (lp = chptr->exceptlist; lp; lp = lp->next)
                    sendto_one(cptr, form_str(RPL_EXCEPTLIST),
                               me.name, cptr->name,
                               real_name,
                               lp->value.banptr->banstr,
                               lp->value.banptr->who,
                               lp->value.banptr->when);

                  sendto_one(sptr, form_str(RPL_ENDOFEXCEPTLIST),
                             me.name, sptr->name, 
                             real_name);
                }
              else
                {
                  sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                               sptr->name, real_name);
                }
              break;
            }
          arg = check_string(*parv++);

          if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;

          if (!isok)
            {
              if (!errsent(SM_ERR_NOOPS, &errors_sent) && MyClient(sptr))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                           me.name, sptr->name, 
                           real_name);
              break;
            }
          
          if(MyClient(sptr))
            chptr->keep_their_modes = YES;
          else if(!chptr->keep_their_modes)
            {
              parc--;
              parv++;
              break;
            }

          /* user-friendly ban mask generation, taken
          ** from Undernet's ircd  -orabidoo
          */
          if (MyClient(sptr))
            arg = collapse(pretty_mask(arg));

          if(*arg == ':')
            {
              parc--;
              parv++;
              break;
            }

          tmp = strlen(arg);
          if (len + tmp + 2 >= MODEBUFLEN)
            break;

          if (!(((whatt & MODE_ADD) && !add_id(sptr, chptr, arg, CHFL_EXCEPTION)) ||
                ((whatt & MODE_DEL) && !del_id(chptr, arg, CHFL_EXCEPTION))))
            break;

          /* This stuff can go back in when all servers understand +e 
           * with the pbufw_new nonsense removed -Dianora
           */

          /*
          *mbufw++ = plus;
          *mbufw++ = 'e';
          strcpy(pbufw, arg);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          */
          len += tmp + 1;
          opcnt++;

          *mbufw_new++ = plus;
          *mbufw_new++ = 'e';
          strcpy(pbufw_new, arg);
          pbufw_new += strlen(pbufw_new);
          *pbufw_new++ = ' ';

          break;


          /* There is a nasty here... I'm supposed to have
           * CAP_DE before I can send exceptions to bans to a server.
           * But that would mean I'd have to keep two strings
           * one for local clients, and one for remote servers,
           * one with the 'd' strings, one without.
           * I added another parameter buf and mode buf for "new"
           * capabilities.
           *
           * -Dianora
           */

        case 'd':
          if (whatt == MODE_QUERY || parc-- <= 0)
            {
              if (!MyClient(sptr))
                break;
              if (errsent(SM_ERR_RPL_D, &errors_sent))
                break;
                  for (lp = chptr->denylist; lp; lp = lp->next)
                    sendto_one(cptr, form_str(RPL_BANLIST),
                               me.name, cptr->name,
                               real_name,
                               lp->value.banptr->banstr,
                               lp->value.banptr->who,
                               lp->value.banptr->when);
                  sendto_one(sptr, form_str(RPL_ENDOFBANLIST),
                             me.name, sptr->name, 
                             real_name);
                  break;
            }
          arg = check_string(*parv++);

          if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;

          if (!isok)
            {
              if (!errsent(SM_ERR_NOOPS, &errors_sent) && MyClient(sptr))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                           me.name, sptr->name, 
                           real_name);
              break;
            }
          
          if(MyClient(sptr))
            chptr->keep_their_modes = YES;
          else if(!chptr->keep_their_modes)
            {
              parc--;
              parv++;
              break;
            }

          if(*arg == ':')
            {
              parc--;
              parv++;
              break;
            }

          tmp = strlen(arg);
          if (len + tmp + 2 >= MODEBUFLEN)
            break;

          if (!(((whatt & MODE_ADD) && !add_id(sptr, chptr, arg, CHFL_DENY)) ||
                ((whatt & MODE_DEL) && !del_id(chptr, arg, CHFL_DENY))))
            break;

          /* This stuff can go back in when all servers understand +e 
           * with the pbufw_new nonsense removed -Dianora
           */

          /*
          *mbufw++ = plus;
          *mbufw++ = 'e';
          strcpy(pbufw, arg);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          */
          len += tmp + 1;
          opcnt++;

          *mbufw_newer++ = plus;
          *mbufw_newer++ = 'd';
          strcpy(pbufw_newer, arg);
          pbufw_newer += strlen(pbufw_newer);
          *pbufw_newer++ = ' ';

          break;

        case 'b':
          if (whatt == MODE_QUERY || parc-- <= 0)
            {
              if (!MyClient(sptr))
                break;

              if (errsent(SM_ERR_RPL_B, &errors_sent))
                break;
              for (lp = chptr->banlist; lp; lp = lp->next)
                sendto_one(cptr, form_str(RPL_BANLIST),
                           me.name, cptr->name,
                           real_name,
                           lp->value.banptr->banstr,
                           lp->value.banptr->who,
                           lp->value.banptr->when);

              sendto_one(sptr, form_str(RPL_ENDOFBANLIST),
                         me.name, sptr->name, 
                         real_name);
              break;
            }

          if(MyClient(sptr))
            chptr->keep_their_modes = YES;
          else if(!chptr->keep_their_modes)
            {
              parc--;
              parv++;
              break;
            }

          arg = check_string(*parv++);

          if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
            break;

          if (!isok)
            {
              if (!errsent(SM_ERR_NOOPS, &errors_sent) && MyClient(sptr))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED),
                           me.name, sptr->name, 
                           real_name);
              break;
            }


          /* Ignore colon at beginning of ban string.
           * Unfortunately, I can't ignore all such strings,
           * because otherwise the channel could get desynced.
           * I can at least, stop local clients from placing a ban
           * with a leading colon.
           *
           * Roger uses check_string() combined with an earlier test
           * in his TS4 code. The problem is, this means on a mixed net
           * one can't =remove= a colon prefixed ban if set from
           * an older server.
           * His code is more efficient though ;-/ Perhaps
           * when we've all upgraded this code can be moved up.
           *
           * -Dianora
           */

          /* user-friendly ban mask generation, taken
          ** from Undernet's ircd  -orabidoo
          */
          if (MyClient(sptr))
            {
              if( (*arg == ':') && (whatt & MODE_ADD) )
                {
                  parc--;
                  parv++;
                  break;
                }
              arg = collapse(pretty_mask(arg));
            }

          tmp = strlen(arg);
          if (len + tmp + 2 >= MODEBUFLEN)
            break;

          if (!(((whatt & MODE_ADD) && !add_id(sptr, chptr, arg, CHFL_BAN)) ||
                ((whatt & MODE_DEL) && !del_id(chptr, arg, CHFL_BAN))))
            break;

          *mbufw++ = plus;
          *mbufw++ = 'b';
          strcpy(pbufw, arg);
          pbufw += strlen(pbufw);
          *pbufw++ = ' ';
          len += tmp + 1;
          opcnt++;

          break;

        case 'l':
          if (whatt == MODE_QUERY)
            break;
          if (!isok || limitset++)
            {
              if (whatt == MODE_ADD && parc-- > 0)
                parv++;
              break;
            }

          if (whatt == MODE_ADD)
            {
              if (parc-- <= 0)
                {
                  if (MyClient(sptr))
                    sendto_one(sptr, form_str(ERR_NEEDMOREPARAMS),
                               me.name, sptr->name, "MODE +l");
                  break;
                }
              
              arg = check_string(*parv++);
              /*              if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
                break; */
              if ((nusers = atoi(arg)) <= 0)
                break;
              ircsprintf(numeric, "%d", nusers);
              if ((tmpc = strchr(numeric, ' ')))
                *tmpc = '\0';
              arg = numeric;

              tmp = strlen(arg);
              if (len + tmp + 2 >= MODEBUFLEN)
                break;

              chptr->mode.limit = nusers;
              chptr->mode.mode |= MODE_LIMIT;

              *mbufw++ = '+';
              *mbufw++ = 'l';
              strcpy(pbufw, arg);
              pbufw += strlen(pbufw);
              *pbufw++ = ' ';
              len += tmp + 1;
              /*              opcnt++;*/
            }
          else
            {
              chptr->mode.limit = 0;
              chptr->mode.mode &= ~MODE_LIMIT;
              *mbufw++ = '-';
              *mbufw++ = 'l';
            }

          break;

          /* Traditionally, these are handled separately
           * but I decided to combine them all into this one case
           * statement keeping it all sane
           *
           * The disadvantage is a lot more code duplicated ;-/
           *
           * -Dianora
           */

        case 'i' :
          if (whatt == MODE_QUERY)      /* shouldn't happen. */
            break;
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          if(MyClient(sptr))
            {
              if(done_i)
                break;
              else
                done_i = YES;

              /*              if ( opcnt >= MAXMODEPARAMS)
                break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_INVITEONLY))
#endif
                {
                  chptr->mode.mode |= MODE_INVITEONLY;
                  *mbufw++ = '+';
                  *mbufw++ = 'i';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;

              while ( (lp = chptr->invites) )
                del_invite(lp->value.cptr, chptr);

#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_INVITEONLY)
#endif
                {
                  chptr->mode.mode &= ~MODE_INVITEONLY;
                  *mbufw++ = '-';
                  *mbufw++ = 'i';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

          /* Un documented for now , I have no idea how this got here ;-) */
#ifdef JUPE_CHANNEL
        case 'j':
          if(MyConnect(sptr) && IsAnOper(sptr))
            {
              if (whatt == MODE_ADD)
                {
                  chptr->mode.mode |= MODE_JUPED;
                  sendto_realops("%s!%s@%s locally juping channel %s",
                                 sptr->name, sptr->username,
                                 sptr->host, real_name);
                }
              else if(whatt == MODE_DEL)
                {
                  chptr->mode.mode &= ~MODE_JUPED;
                  sendto_realops("%s!%s@%s locally unjuping channel %s",
                                 sptr->name, sptr->username,
                                 sptr->host, real_name);
                }
            }
          break;
#endif

        case 'm' :
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          if(MyClient(sptr))
            {
              if(done_m)
                break;
              else
                done_m = YES;

              /*              if ( opcnt >= MAXMODEPARAMS)
                              break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_MODERATED))
#endif
                {
                  chptr->mode.mode |= MODE_MODERATED;
                  *mbufw++ = '+';
                  *mbufw++ = 'm';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_MODERATED)
#endif
                {
                  chptr->mode.mode &= ~MODE_MODERATED;
                  *mbufw++ = '-';
                  *mbufw++ = 'm';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

        case 'n' :
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          if(MyClient(sptr))
            {
              if(done_n)
                break;
              else
                done_n = YES;

              /*              if ( opcnt >= MAXMODEPARAMS)
                              break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_NOPRIVMSGS))
#endif
                {
                  chptr->mode.mode |= MODE_NOPRIVMSGS;
                  *mbufw++ = '+';
                  *mbufw++ = 'n';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_NOPRIVMSGS)
#endif
                {
                  chptr->mode.mode &= ~MODE_NOPRIVMSGS;
                  *mbufw++ = '-';
                  *mbufw++ = 'n';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

        case 'p' :
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          if(MyClient(sptr))
            {
#ifdef OLD_P_S
              if(done_s_or_p)
                break;
              else
                done_s_or_p = YES;
#else
              if(done_p)
                break;
              else
                done_p = YES;
#endif
              /*              if ( opcnt >= MAXMODEPARAMS)
                break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_P_S
              if(chptr->mode.mode & MODE_SECRET)
                {
                  if (len + 2 >= MODEBUFLEN)
                    break;
                  *mbufw++ = '-';
                  *mbufw++ = 's';
                  len += 2;
                  chptr->mode.mode &= ~MODE_SECRET;
                }
#endif
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_PRIVATE))
#endif
                {
                  chptr->mode.mode |= MODE_PRIVATE;
                  *mbufw++ = '+';
                  *mbufw++ = 'p';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_PRIVATE)
#endif
                {
                  chptr->mode.mode &= ~MODE_PRIVATE;
                  *mbufw++ = '-';
                  *mbufw++ = 'p';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

        case 's' :
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          /* ickity poo, traditional +p-s nonsense */

          if(MyClient(sptr))
            {
#ifdef OLD_P_S
              if(done_s_or_p)
                break;
              else
                done_s_or_p = YES;
#else
              if(done_s)
                break;
              else
                done_s = YES;
#endif
              /*              if ( opcnt >= MAXMODEPARAMS)
                              break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_P_S
              if(chptr->mode.mode & MODE_PRIVATE)
                {
                  if (len + 2 >= MODEBUFLEN)
                    break;
                  *mbufw++ = '-';
                  *mbufw++ = 'p';
                  len += 2;
                  chptr->mode.mode &= ~MODE_PRIVATE;
                }
#endif
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_SECRET))
#endif
                {
                  chptr->mode.mode |= MODE_SECRET;
                  *mbufw++ = '+';
                  *mbufw++ = 's';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_SECRET)
#endif
                {
                  chptr->mode.mode &= ~MODE_SECRET;
                  *mbufw++ = '-';
                  *mbufw++ = 's';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

        case 't' :
          if (!isok)
            {
              if (MyClient(sptr) && !errsent(SM_ERR_NOOPS, &errors_sent))
                sendto_one(sptr, form_str(ERR_CHANOPRIVSNEEDED), me.name, 
                           sptr->name, real_name);
              break;
            }

          if(MyClient(sptr))
            {
              if(done_t)
                break;
              else
                done_t = YES;

              /*              if ( opcnt >= MAXMODEPARAMS)
                              break; */
            }

          if(whatt == MODE_ADD)
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(!(chptr->mode.mode & MODE_TOPICLIMIT))
#endif
                {
                  chptr->mode.mode |= MODE_TOPICLIMIT;
                  *mbufw++ = '+';
                  *mbufw++ = 't';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          else
            {
              if (len + 2 >= MODEBUFLEN)
                break;
#ifdef OLD_NON_RED
              if(chptr->mode.mode & MODE_TOPICLIMIT)
#endif
                {
                  chptr->mode.mode &= ~MODE_TOPICLIMIT;
                  *mbufw++ = '-';
                  *mbufw++ = 't';
                  len += 2;
                  /*              opcnt++; */
                }
            }
          break;

        default:
          if (whatt == MODE_QUERY)
            break;

          /* only one "UNKNOWNMODE" per mode... we don't want
          ** to generate a storm, even if it's just to a 
          ** local client  -orabidoo
          */
          if (MyClient(sptr) && !errsent(SM_ERR_UNKNOWN, &errors_sent))
            sendto_one(sptr, form_str(ERR_UNKNOWNMODE), me.name, sptr->name, c);
          break;
        }
    }

  /*
  ** WHEW!!  now all that's left to do is put the various bufs
  ** together and send it along.
  */

  *mbufw = *mbuf2w = *pbufw = *pbuf2w = *mbufw_new = *pbufw_new = 
  *mbufw_newer = *pbufw_newer = *mbufw_invex = *pbufw_invex = '\0';

  collapse_signs(modebuf);
/*  collapse_signs(modebuf2); */
  collapse_signs(modebuf_new);
  collapse_signs(modebuf_newer);

  if(*modebuf)
    {
      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", 
                           sptr->name, real_name,
                           modebuf, parabuf);

      sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s",
                         sptr->name, chptr->chname,
                         modebuf, parabuf);
    }

  if(*modebuf_new)
    {
      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", 
                             sptr->name, real_name,
                             modebuf_new, parabuf_new);

      sendto_match_cap_servs(chptr, cptr, CAP_EX, ":%s MODE %s %s %s",
                             sptr->name, chptr->chname,
                             modebuf_new, parabuf_new);
    }
  if(*modebuf_newer)
    {
      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
                             sptr->name, real_name,
                             modebuf_newer, parabuf_newer);
      sendto_match_cap_servs(chptr, cptr, CAP_DE, ":%s MODE %s %s %s",
                             sptr->name, chptr->chname,
                             modebuf_newer, parabuf_newer);
    }
  if(*modebuf_invex)
    {
      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			     sptr->name, real_name,
			     modebuf_invex, parabuf_invex);
      sendto_match_cap_servs(chptr, cptr, CAP_IE, ":%s MODE %s %s %s",
			     sptr->name, chptr->chname,
			     modebuf_invex, parabuf_invex);
    }	
                     
  return;
}

int     can_join(struct Client *sptr, struct Channel *chptr, char *key, int *flags)
{
  struct SLink  *lp;
  int ban_or_exception;

#ifdef JUPE_CHANNEL
  if( chptr->mode.mode & MODE_JUPED )
    {
      sendto_ops_flags(FLAGS_SPY,
             "%s %s (%s@%s) is attempting to join locally juped channel %s",
                     IsOper(sptr) ? "Oper" : "User", sptr->name,
                     sptr->username, sptr->host,chptr->chname);
      return (ERR_BADCHANNAME);
    }
#endif

  if ( (ban_or_exception = is_banned(sptr, chptr)) == CHFL_BAN)
    return (ERR_BANNEDFROMCHAN);
  else
    *flags |= ban_or_exception; /* Mark this client as "charmed" */

  if (chptr->mode.mode & MODE_INVITEONLY)
    {
      for (lp = sptr->user->invited; lp; lp = lp->next)
        if (lp->value.chptr == chptr)
          break;
      if (!lp)
        return (ERR_INVITEONLYCHAN);
    }

  if (*chptr->mode.key && (BadPtr(key) || irccmp(chptr->mode.key, key)))
    return (ERR_BADCHANNELKEY);

  if (chptr->mode.limit && chptr->users >= chptr->mode.limit)
    return (ERR_CHANNELISFULL);

  return 0;
}

/*
 * check_channel_name - check channel name for invalid characters
 * return true (1) if name ok, false (0) otherwise
 */
int check_channel_name(const char* name)
{
  assert(0 != name);
  
  for ( ; *name; ++name) {
    if (!IsChanChar(*name))
      return 0;
  }
  return 1;
}

/*
**  Get Channel block for chname (and allocate a new channel
**  block, if it didn't exist before).
*/
struct Channel* get_channel(struct Client *cptr, char *chname, int flag)
{
  struct Channel *chptr;
  int   len;

  if (BadPtr(chname))
    return NULL;

  len = strlen(chname);
  if (MyClient(cptr) && len > CHANNELLEN)
    {
      len = CHANNELLEN;
      *(chname + CHANNELLEN) = '\0';
    }
  if ((chptr = hash_find_channel(chname, NULL)))
    return (chptr);

  /*
   * If a channel is created during a split make sure its marked
   * as created locally 
   * Also make sure a created channel has =some= timestamp
   * even if it get over-ruled later on. Lets quash the possibility
   * an ircd coder accidentally blasting TS on channels. (grrrrr -db)
   *
   * Actually, it might be fun to make the TS some impossibly huge value (-db)
   */

  if (flag == CREATE)
    {
      chptr = (struct Channel*) MyMalloc(sizeof(struct Channel) + len + 1);
      memset(chptr, 0, sizeof(struct Channel));
      /*
       * NOTE: strcpy ok here, we have allocated strlen + 1
       */
      strcpy(chptr->chname, chname);
      if (GlobalChannelList)
        GlobalChannelList->prevch = chptr;
      chptr->prevch = NULL;
      chptr->nextch = GlobalChannelList;
      GlobalChannelList = chptr;
      if (Count.myserver == 0)
        chptr->locally_created = YES;
      chptr->keep_their_modes = YES;
      chptr->channelts = CurrentTime;     /* doesn't hurt to set it here */
      add_to_channel_hash_table(chname, chptr);
      Count.chan++;
    }
  return chptr;
}

void    add_invite(struct Client *cptr,struct Channel *chptr)
{
  struct SLink  *inv, **tmp;

  del_invite(cptr, chptr);
  /*
   * delete last link in chain if the list is max length
   */
  if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
    {
      del_invite(cptr, cptr->user->invited->value.chptr);
    }
  /*
   * add client to channel invite list
   */
  inv = make_link();
  inv->value.cptr = cptr;
  inv->next = chptr->invites;
  chptr->invites = inv;
  /*
   * add channel to the end of the client invite list
   */
  for (tmp = &(cptr->user->invited); *tmp; tmp = &((*tmp)->next))
    ;
  inv = make_link();
  inv->value.chptr = chptr;
  inv->next = NULL;
  (*tmp) = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void del_invite(struct Client *cptr,struct Channel *chptr)
{
  struct SLink  **inv, *tmp;

  for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
    if (tmp->value.cptr == cptr)
      {
        *inv = tmp->next;
        free_link(tmp);
        break;
      }

  for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
    if (tmp->value.chptr == chptr)
      {
        *inv = tmp->next;
        free_link(tmp);
        break;
      }
}

/*
**  Subtract one user from channel (and free channel
**  block, if channel became empty).
*/
static  void    sub1_from_channel(struct Channel *chptr)
{
  struct SLink *tmp;
  struct Channel *root_chptr;

  if (--chptr->users <= 0)
    {
      chptr->users = 0; /* if chptr->users < 0, make sure it sticks at 0
                         * It should never happen but...
                         */

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
      if(server_was_split && (chptr->chname[0] != '&'))
        {
          /*
           * Now, find all invite links from channel structure
           */
          /* The idea here is, not to "forget" the channel mode
           * the ban list, exception lists, and not to release
           * the channel at this time.
           * The invite list should be forgotten now, as well
           * as the flooder lists
           * -db
           */

          while ((tmp = chptr->invites))
            del_invite(tmp->value.cptr, chptr);

#ifdef FLUD
          free_fluders(NULL, chptr);
#endif
          /* flag the channel as split */
          chptr->mode.mode |= MODE_SPLIT;

          /* Add to double linked empty channel list */
          if(empty_channel_list)
            empty_channel_list->last_empty_channel = chptr;
          chptr->last_empty_channel = (struct Channel *)NULL;
          chptr->next_empty_channel = empty_channel_list;
          empty_channel_list = chptr;
        }
      else
#endif
#ifdef JUPE_CHANNEL
        if( chptr->mode.mode & MODE_JUPED )
          {
            while ((tmp = chptr->invites))
              del_invite(tmp->value.cptr, chptr);
            
#ifdef FLUD
            free_fluders(NULL, chptr);
#endif
          }
        else
#endif
       {
          /*
           * Now, find all invite links from channel structure
           */
          while ((tmp = chptr->invites))
            del_invite(tmp->value.cptr, chptr);

	  /* free all bans/exceptions/denies */
	  free_channel_masks( chptr );

          /* free topic_info */
          MyFree(chptr->topic_info);            

#ifdef FLUD
          free_fluders(NULL, chptr);
#endif

          /* Is this the top level channel? 
	   * If so, don't remove if it has sub vchans
	   * top level chan always has prev_chan == NULL
	   */
          if (!IsVchan(chptr))
            {
              if (!HasVchans(chptr))
		{
		  if (chptr->prevch)
		    chptr->prevch->nextch = chptr->nextch;
		  else
		    GlobalChannelList = chptr->nextch;
		  if (chptr->nextch)
		    chptr->nextch->prevch = chptr->prevch;
		  del_from_channel_hash_table(chptr->chname, chptr);
		  MyFree((char*) chptr);
		  Count.chan--;
		}
	    }
          /* if this is a subchan take it out the linked list */
          else
            {
              /* find it's base chan, incase we can remove that after */
              root_chptr = find_bchan(chptr);
	      /* remove from vchan double link list */
              chptr->prev_vchan->next_vchan = chptr->next_vchan;
              if (chptr->next_vchan)
                chptr->next_vchan->prev_vchan = chptr->prev_vchan;

	      /* remove from global chan double link list and hash */
	      if (chptr->prevch)
		chptr->prevch->nextch = chptr->nextch;
	      else
		GlobalChannelList = chptr->nextch;
	      if (chptr->nextch)
		chptr->nextch->prevch = chptr->prevch;
              del_from_channel_hash_table(chptr->chname, chptr);
              MyFree((char*) chptr);
              Count.chan--; /* is this line needed for subchans? yes -db */

              if (!HasVchans(root_chptr) && (root_chptr->users == 0))
                {
                  chptr = root_chptr;
                  if (chptr->prevch)
                    chptr->prevch->nextch = chptr->nextch;
                  else
                    GlobalChannelList = chptr->nextch;
                  if (chptr->nextch)
                    chptr->nextch->prevch = chptr->prevch;
                  del_from_channel_hash_table(chptr->chname, chptr);
                  MyFree((char*) chptr);   
                  Count.chan--;
                }
            }
        }
    }
}

/*
 * clear_bans_exceptions_denies
 *
 * I could have re-written del_banid/del_exceptid to do this
 *
 * still need a bit of cleanup on the MODE -b stuff...
 * -Dianora
 */

void clear_bans_exceptions_denies(struct Client *sptr, struct Channel *chptr)
{
  static char modebuf[MODEBUFLEN];
  register struct SLink *ban;
  char *b1,*b2,*b3,*b4;
  char *mp;

  b1="";
  b2="";
  b3="";
  b4="";

  mp= modebuf;
  *mp = '\0';

  for(ban = chptr->banlist; ban; ban = ban->next)
    {
      if(!*b1)
        {
          b1 = BANSTR(ban);
          *mp++ = '-';
          *mp++ = 'b';
          *mp = '\0';
        }
      else if(!*b2)
        {
          b2 = BANSTR(ban);
          *mp++ = 'b';
          *mp = '\0';
        }
      else if(!*b3)
        {
          b3 = BANSTR(ban);
          *mp++ = 'b';
          *mp = '\0';
        }
      else if(!*b4)
        {
          b4 = BANSTR(ban);
          *mp++ = 'b';
          *mp = '\0';

          sendto_channel_butserv(chptr, &me,
                                 ":%s MODE %s %s %s %s %s %s",
                                 sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);
          b1="";
          b2="";
          b3="";
          b4="";

          mp = modebuf;
          *mp = '\0';
        }
    }

  if(*modebuf)
    sendto_channel_butserv(chptr, &me,
                           ":%s MODE %s %s %s %s %s %s",
                           sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);
  b1="";
  b2="";
  b3="";
  b4="";

  mp= modebuf;
  *mp = '\0';

  for(ban = chptr->exceptlist; ban; ban = ban->next)
    {
      if(!*b1)
        {
          b1 = BANSTR(ban);
          *mp++ = '-';
          *mp++ = 'e';
          *mp = '\0';
        }
      else if(!*b2)
        {
          b2 = BANSTR(ban);
          *mp++ = 'e';
          *mp = '\0';
        }
      else if(!*b3)
        {
          b3 = BANSTR(ban);
          *mp++ = 'e';
          *mp = '\0';
        }
      else if(!*b4)
        {
          b4 = BANSTR(ban);
          *mp++ = 'e';
          *mp = '\0';

          sendto_channel_butserv(chptr, &me,
                                 ":%s MODE %s %s %s %s %s %s",
                                 sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);
          b1="";
          b2="";
          b3="";
          b4="";
          mp = modebuf;
          *mp = '\0';
        }
    }

  if(*modebuf)
    sendto_channel_butserv(chptr, &me,
                           ":%s MODE %s %s %s %s %s %s",
                           sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);

  b1="";
  b2="";
  b3="";
  b4="";

  mp= modebuf;
  *mp = '\0';

  for(ban = chptr->denylist; ban; ban = ban->next)
    {
      if(!*b1)
        {
          b1 = BANSTR(ban);
          *mp++ = '-';
          *mp++ = 'd';
          *mp = '\0';
        }
      else if(!*b2)
        {
          b2 = BANSTR(ban);
          *mp++ = 'd';
          *mp = '\0';
        }
      else if(!*b3)
        {
          b3 = BANSTR(ban);
          *mp++ = 'd';
          *mp = '\0';
        }
      else if(!*b4)
        {
          b4 = BANSTR(ban);
          *mp++ = 'd';
          *mp = '\0';

          sendto_channel_butserv(chptr, &me,
                                 ":%s MODE %s %s %s %s %s %s",
                                 sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);
          b1="";
          b2="";
          b3="";
          b4="";
          mp = modebuf;
          *mp = '\0';
        }
    }

  if(*modebuf)
    sendto_channel_butserv(chptr, &me,
                           ":%s MODE %s %s %s %s %s %s",
                           sptr->name,chptr->chname,modebuf,b1,b2,b3,b4);

  /* free all bans/exceptions/denies */
  free_channel_masks(chptr);
}

/*
 * free_channel_masks
 *
 * inputs	- pointer to channel structure
 * output	- none
 * side effects	- all bans/exceptions denies are freed for channel
 */

static void free_channel_masks(struct Channel *chptr)
{
  struct SLink *ban;
  struct SLink *next_ban;

  for(ban = chptr->banlist; ban; ban = next_ban)
    {
      next_ban = ban->next;
      MyFree(ban->value.banptr->banstr);
      MyFree(ban->value.banptr->who);
      MyFree(ban->value.banptr);
      free_link(ban);
    }

  for(ban = chptr->exceptlist; ban; ban = next_ban)
    {
      next_ban = ban->next;
      MyFree(ban->value.banptr->banstr);
      MyFree(ban->value.banptr->who);
      MyFree(ban->value.banptr);
      free_link(ban);
    }

  for(ban = chptr->denylist; ban; ban = next_ban)
    {
      next_ban = ban->next;
      MyFree(ban->value.banptr->banstr);
      MyFree(ban->value.banptr->who);
      MyFree(ban->value.banptr);
      free_link(ban);
    }

  for(ban = chptr->invexlist; ban; ban = next_ban)
    {
      next_ban = ban->next;
      MyFree(ban->value.banptr->banstr);
      MyFree(ban->value.banptr->who);
      MyFree(ban->value.banptr);
      free_link(ban);
    }

  chptr->banlist = chptr->exceptlist = chptr->denylist = chptr->invexlist = 
    NULL;
  chptr->num_bed = 0;
}


#ifdef NEED_SPLITCODE

/*
 * check_still_split()
 *
 * inputs       -NONE
 * output       -NONE
 * side effects -
 * Check to see if the server split timer has expired, if so
 * check to see if there are now a decent number of servers connected
 * and users present, so I can consider this split over.
 *
 * -Dianora
 */

static void check_still_split()
{
  if((server_split_time + GlobalSetOptions.server_split_recovery_time)
     < CurrentTime)
    {
      if((Count.server >= GlobalSetOptions.split_smallnet_size) &&
#ifdef SPLIT_PONG
         (got_server_pong == YES) &&
#endif
         (Count.total >= GlobalSetOptions.split_smallnet_users))
        {
          /* server hasn't been split for a while.
           * -Dianora
           */
          server_was_split = NO;
          sendto_ops("Net Rejoined, split-mode deactivated");
          cold_start = NO;
#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
          remove_empty_channels();
#endif
        }
      else
        {
          server_split_time = CurrentTime; /* still split */
          server_was_split = YES;
        }
    }
}
#endif

#if defined(PRESERVE_CHANNEL_ON_SPLIT) || defined(NO_JOIN_ON_SPLIT)
/*
 * remove_empty_channels
 *
 * inputs       - none
 * output       - none
 * side effects - remove all channels on empty_channel_list that have
 * 
 * Any channel struct on this link list, is here because it had
 * no members, hence normally would not exist through a split.
 * If after the split is over, there are any channels left in this
 * list, they must be removed. Whenever a channel gains a member
 * whether locally or from a remote SJOIN it is removed from this list.
 */

void remove_empty_channels()
{
  struct SLink *tmp;
  struct SLink  *obtmp;
  struct Channel *next_empty_channel;

  for(;empty_channel_list;
      empty_channel_list = next_empty_channel )
    {
      next_empty_channel = empty_channel_list->next_empty_channel;

      if(empty_channel_list->users)             /* sanity test */
        {
	  /* This is an oddity, rather than an out and out error
	   * if this happens, a client managed to join the channel
	   * making it non zero users, and I didn't notice. That means
	   * strictly speaking its an error. However, if this entry is
	   * ignored, its a non fatal one.
	   */
#if 0
          sendto_ops("non zero user count in remove_empty_channels");
          sendto_ops("Please report to the hybrid team! ircd-hybrid@the-project.org");
#endif
          empty_channel_list->next_empty_channel = (struct Channel *)NULL;
          empty_channel_list->last_empty_channel = (struct Channel *)NULL;
          continue;
        }

      /*
       * Now, find all invite links from channel structure
       */
      while ((tmp = empty_channel_list->invites))
        del_invite(tmp->value.cptr, empty_channel_list);

      free_channel_masks(empty_channel_list);      

      if (empty_channel_list->prevch)
        empty_channel_list->prevch->nextch = empty_channel_list->nextch;
      else
        channel = empty_channel_list->nextch;
      if (empty_channel_list->nextch)
        empty_channel_list->nextch->prevch = empty_channel_list->prevch;

      MyFree(chptr->topic_info);

#ifdef FLUD
      free_fluders(NULL, empty_channel_list);
#endif
      del_from_channel_hash_table(empty_channel_list->chname, 
                                        empty_channel_list);
      MyFree((char*) empty_channel_list);
      Count.chan--;
    }
}
#endif


int     count_channels(struct Client *sptr)
{
  struct Channel      *chptr;
  int   count = 0;

  for (chptr = GlobalChannelList; chptr; chptr = chptr->nextch)
    count++;
  return (count);
}

/* Only leaves need to remove channels that have no local members */

void cleanup_channels(void)
{
   struct Channel *chptr;
   struct Channel *next_chptr;
 
   if(!serv_cptr_list)
     {
       sendto_ops_flags(FLAGS_DEBUG,
          "**** Cannot clean channels, waiting to link to my uplink.");
       return;
     }

   if(!IsCapable(serv_cptr_list, CAP_LL))
     return;

   sendto_ops_flags(FLAGS_DEBUG, "*** Cleaning up local channels...");
   
   next_chptr = NULL;

   for(chptr = GlobalChannelList; chptr; chptr = next_chptr)
     {
       next_chptr = chptr->nextch;

       if((CurrentTime - chptr->locusers_last >= CLEANUP_CHANNELS_TIME) && !chptr->locusers)
         {
           sendto_one(serv_cptr_list,":%s DROP %s",
                  me.name, chptr->chname);
           if(!chptr->locusers)
             destroy_channel(chptr);
         }
     }
}    

/*
 * destroy_channel
 * inputs       - channel pointer
 * output       - none
 * side effects - walk through this channel, and destroy it.
 */

static void destroy_channel(struct Channel *chptr)
{
  struct SLink  *tmp;

  struct SLink  **current;
  struct SLink  **nextCurrent;
  struct SLink  *tmpCurrent;

  struct SLink  **currentClient;
  struct SLink  **nextCurrentClient;
  struct SLink  *tmpCurrentClient;

  struct Client *sptr;

  /* Walk through all the struct SLink's pointing to members of this chanel,
   * then walk through each client found from each SLink, removing
   * any reference it has to this channel.
   * Finally, free now unused SLink's
   */
  for (current = &chptr->members;
        (tmpCurrent = *current);
          current = nextCurrent )
    {
      nextCurrent = &tmpCurrent->next;
      sptr = tmpCurrent->value.cptr;

      for (currentClient = &sptr->user->channel;
            (tmpCurrentClient = *currentClient);
              currentClient = nextCurrentClient )
        {
          nextCurrentClient = &tmpCurrentClient->next;

          if( tmpCurrentClient->value.chptr == chptr)
            {
              sptr->user->joined--;
              *currentClient = tmpCurrentClient->next;
              free_link(tmpCurrentClient);
            }
        }
      *current = tmpCurrent->next;
      free_link(tmpCurrent);
    }

  while ((tmp = chptr->invites))
    del_invite(tmp->value.cptr, chptr);

  /* free all bans/exceptions/denies */
  free_channel_masks( chptr );

  if (chptr->prevch)
    chptr->prevch->nextch = chptr->nextch;
  else
    GlobalChannelList = chptr->nextch;
  if (chptr->nextch)
    chptr->nextch->prevch = chptr->prevch;

  MyFree(chptr->topic_info);

#ifdef FLUD
  free_fluders(NULL, chptr);
#endif

  del_from_channel_hash_table(chptr->chname, chptr);
  MyFree((char*) chptr);
  Count.chan--;
  /* Wheee */
}


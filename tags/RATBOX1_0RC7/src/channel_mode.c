/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  channel_mode.c: Controls modes on channels.
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

#include "stdinc.h"
#include "setup.h"

#include "tools.h"
#include "channel.h"
#include "channel_mode.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"             /* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"             /* ConfigFileEntry, ConfigChannel */
#include "event.h"
#include "memory.h"
#include "balloc.h"


#include "s_log.h"

static int add_id(struct Client *, struct Channel *, char *, int);
static int del_id(struct Channel *, char *, int);

static void change_channel_membership(struct Channel *chptr,
                                     dlink_list *to_list,
                                     dlink_list *loc_to_list,
                                     struct Client *who);

/* some small utility functions */
static char *check_string(char *s);
static char *pretty_mask(char *);
static char *fix_key(char *);
static char *fix_key_old(char *);


static void chm_nosuch(struct Client *, struct Client *,
                       struct Channel *, int, int *, char **, int *, int,
                       int, char, void *, const char *chname);

static void chm_simple(struct Client *, struct Client *, struct Channel *,
                       int, int *, char **, int *, int, int, char, void *,
                       const char *chname);

static void chm_limit(struct Client *, struct Client *, struct Channel *,
                      int, int *, char **, int *, int, int, char, void *,
                      const char *chname);

static void chm_key(struct Client *, struct Client *, struct Channel *,
                    int, int *, char **, int *, int, int, char, void *,
                    const char *chname);

static void chm_op(struct Client *, struct Client *, struct Channel *, int,
                   int *, char **, int *, int, int, char, void *,
                   const char *chname);

static void chm_voice(struct Client *, struct Client *, struct Channel *,
                      int, int *, char **, int *, int, int, char, void *,
                      const char *chname);

static void chm_ban(struct Client *, struct Client *, struct Channel *, int,
                    int *, char **, int *, int, int, char, void *,
                    const char *chname);

static void chm_except(struct Client *, struct Client *, struct Channel *,
                       int, int *, char **, int *, int, int, char, void *,
                       const char *chname);

static void chm_invex(struct Client *, struct Client *, struct Channel *,
                      int, int *, char **, int *, int, int, char, void *,
                      const char *chname);

#ifdef ANONOPS
static void chm_hideops(struct Client *, struct Client *, struct Channel *,
                        int, int *, char **, int *, int, int, char, void *,
                        const char *chname);

static void send_oplist(const char *, struct Client *, dlink_list *,
                        char *, int);
static void mode_get_status(struct Channel *, struct Client *, 
                            int *, int *, int);
#endif

static void send_cap_mode_changes(struct Client *, struct Client *,
                                  struct Channel *, int, int);

static void send_mode_changes(struct Client *, struct Client *,
                              struct Channel *, char *chname);

static void update_channel_info(struct Channel *);

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */

static char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
static char mask_buf[BUFSIZE];
static int mask_pos;

static struct ChModeChange mode_changes[BUFSIZE];
static int mode_count;

#ifdef ANONOPS
static int hideops_changed;
#endif

static int mode_limit;

static int channel_capabs[] = { CAP_AOPS, CAP_EX, CAP_IE, CAP_UID };

#define NCHCAPS         (sizeof(channel_capabs)/sizeof(int))
#define NCHCAP_COMBOS   (1 << NCHCAPS)

static struct ChCapCombo chcap_combos[NCHCAP_COMBOS];

extern BlockHeap *ban_heap;

/*
 * check_string
 *
 * inputs       - string to check
 * output       - pointer to modified string
 * side effects - Fixes a string so that the first white space found
 *                becomes an end of string marker (`\0`).
 *                returns the 'fixed' string or "*" if the string
 *                was NULL length or a NULL pointer.
 */
static char *
check_string(char *s)
{
  char *str = s;

  if (!(s && *s))
    return "*";

  for (; *s; ++s)
  {
    if (IsSpace(*s))
    {
      *s = '\0';
      break;
    }
  }
  return str;
}

/*
 * Ban functions to work with mode +b/e/d/I
 */
/* add the specified ID to the channel.. 
 *   -is 8/9/00 
 */

static int
add_id(struct Client *client_p, struct Channel *chptr, char *banid, int type)
{
  dlink_list *list;
  dlink_node *ban;
  struct Ban *actualBan;

  /* dont let local clients overflow the banlist */
  if(MyClient(client_p))
  {
    if(chptr->num_mask >= ConfigChannel.max_bans)
    {
      sendto_one(client_p, form_str(ERR_BANLISTFULL),
                 me.name, client_p->name, chptr->chname, banid);
      return 0;
    }

    collapse(banid);
  }

  switch (type)
  {
    case CHFL_BAN:
      list = &chptr->banlist;
      break;
    case CHFL_EXCEPTION:
      list = &chptr->exceptlist;
      break;
    case CHFL_INVEX:
      list = &chptr->invexlist;
      break;
    default:
      sendto_realops_flags(UMODE_ALL, L_ALL,
                           "add_id() called with unknown ban type %d!", type);
      return 0;
  }

  DLINK_FOREACH(ban, list->head)
  {
    actualBan = ban->data;
    if (match(actualBan->banstr, banid))
      return 0;
  }


  actualBan = (struct Ban *)BlockHeapAlloc(ban_heap);
  DupString(actualBan->banstr, banid);

  if (IsPerson(client_p))
  {
    actualBan->who =
      (char *)MyMalloc(strlen(client_p->name) +
                       strlen(client_p->username) +
                       strlen(client_p->host) + 3);
    ircsprintf(actualBan->who, "%s!%s@%s",
               client_p->name, client_p->username, client_p->host);
  }
  else
  {
    DupString(actualBan->who, client_p->name);
  }

  actualBan->when = CurrentTime;

  dlinkAddAlloc(actualBan, list);

  chptr->num_mask++;
  return 1;
}

/*
 *
 * "del_id - delete an id belonging to client_p
 * if banid is null, deleteall banids belonging to client_p."
 *
 * from orabidoo
 * modified 8/9/00 by is: now we handle add ban types here
 * (invex/excemp/etc)
 */
static int
del_id(struct Channel *chptr, char *banid, int type)
{
  dlink_list *list;
  dlink_node *ban;
  struct Ban *banptr;

  if (!banid)
    return 0;

  switch (type)
  {
    case CHFL_BAN:
      list = &chptr->banlist;
      break;
    case CHFL_EXCEPTION:
      list = &chptr->exceptlist;
      break;
    case CHFL_INVEX:
      list = &chptr->invexlist;
      break;
    default:
      sendto_realops_flags(UMODE_ALL, L_ALL,
                           "del_id() called with unknown ban type %d!", type);
      return 0;
  }

  DLINK_FOREACH(ban, list->head)
  {
    banptr = ban->data;

    if (irccmp(banid, banptr->banstr) == 0)
    {
      MyFree(banptr->banstr);
      MyFree(banptr->who);
      BlockHeapFree(ban_heap, banptr);

      /* num_mask should never be < 0 */
      if (chptr->num_mask > 0)
        chptr->num_mask--;
      else
        chptr->num_mask = 0;

      dlinkDestroy(ban,list);
      return 1;
    }
  }
  return 0;
}

/*
 * change_channel_membership
 *
 * inputs       - pointer to channel
 *              - pointer to membership list of given channel to modify
 *              - pointer to membership list for local clients to modify 
 *              - pointer to client struct being modified
 * side effects - change given user "who" from whichever membership list
 *                it is on, to the given membership list in to_list.
 *                
 */
static void
change_channel_membership(struct Channel *chptr,
                          dlink_list * to_list,
                          dlink_list * loc_to_list, struct Client *who)
{
  dlink_node *ptr;
  int x;
  dlink_list *loclists[] = {
  	&chptr->locpeons,
  	&chptr->locvoiced,
  	&chptr->locchanops,
  	&chptr->locchanops_voiced,
	NULL  
  };

  dlink_list *lists[] = {
  	&chptr->peons,
  	&chptr->voiced,
  	&chptr->chanops,
  	&chptr->chanops_voiced,
  	NULL
  };
  /* local clients need to be moved from local list too */
  if (MyClient(who))
  {
    for(x = 0; loclists[x] != NULL; x++)
    {
    	ptr = find_user_link(loclists[x], who);
    	if(ptr != NULL)
        {
          if(loclists[x] != loc_to_list)
	  {
            dlinkMoveNode(ptr, loclists[x], loc_to_list);
          }
          break;
    	} 
    }
  }

  for(x = 0; lists[x] != NULL; x++)
  {
    ptr = find_user_link(lists[x], who);
    if(ptr != NULL)
    {
      if(lists[x] != to_list)
      {
	dlinkMoveNode(ptr, lists[x], to_list);
      }
      break;
    }
  }
  
  dlinkFindDestroy(&chptr->deopped, who);

}

/*
 * channel_modes
 * inputs       - pointer to channel
 *              - pointer to client
 *              - pointer to mode buf
 *              - pointer to parameter buf
 * output       - NONE
 * side effects - write the "simple" list of channel modes for channel
 * chptr onto buffer mbuf with the parameters in pbuf.
 *
 * NOTE: m_join.c uses this function and depends on the trailing spaces
 *       in pbuf if there is a key or limit.
 */
void
channel_modes(struct Channel *chptr, struct Client *client_p,
              char *mbuf, char *pbuf)
{
  int len;
  *mbuf++ = '+';
  *pbuf = '\0';

  if (chptr->mode.mode & MODE_SECRET)
    *mbuf++ = 's';
  if (chptr->mode.mode & MODE_PRIVATE)
    *mbuf++ = 'p';
  if (chptr->mode.mode & MODE_MODERATED)
    *mbuf++ = 'm';
  if (chptr->mode.mode & MODE_TOPICLIMIT)
    *mbuf++ = 't';
  if (chptr->mode.mode & MODE_INVITEONLY)
    *mbuf++ = 'i';
  if (chptr->mode.mode & MODE_NOPRIVMSGS)
    *mbuf++ = 'n';
#ifdef ANONOPS
  if (chptr->mode.mode & MODE_HIDEOPS)
    *mbuf++ = 'a';
#endif
  if (chptr->mode.limit)
  {
    *mbuf++ = 'l';
    if (IsMember(client_p, chptr) || IsServer(client_p))
    {
      len = ircsprintf(pbuf, "%d ", chptr->mode.limit);
      pbuf += len;
    }
  }
  if (*chptr->mode.key)
  {
    *mbuf++ = 'k';
    if (IsMember(client_p, chptr) || IsServer(client_p))
      ircsprintf(pbuf, "%s ", chptr->mode.key);
  }

  *mbuf++ = '\0';
  return;
}

/* static char *
 * pretty_mask(char *mask);
 *
 * Input: A mask.
 * Output: A "user-friendly" version of the mask, in mask_buf.
 * Side-effects: mask_buf is appended to. mask_pos is incremented.
 * Notes: The following transitions are made:
 *  x!y@z =>  x!y@z
 *  y@z   =>  *!y@z
 *  x!y   =>  x!y@*
 *  x     =>  x!*@*
 *  z.d   =>  *!*@z.d
 *
 * If either nick/user/host are > than their respective limits, they are
 * chopped
 */
static char *
pretty_mask(char *mask)
{
  int old_mask_pos;
  char *nick = "*", *user = "*", *host = "*";
  char *t, *at, *ex;
  char ne = 0, ue = 0, he = 0; /* save values at nick[NICKLEN], et all */
  mask = check_string(mask);

  if (BUFSIZE - mask_pos < strlen(mask) + 5)
    return NULL;

  old_mask_pos = mask_pos;

  at = ex = NULL;
  if ((t = strchr(mask, '@')) != NULL)
  {
    at = t;
    *t++ = '\0';
    if (*t != '\0')
      host = t;

    if ((t = strchr(mask, '!')) != NULL)
    {
      ex = t;
      *t++ = '\0';
      if (*t != '\0')
	user = t;
      if (*mask != '\0')
	nick = mask;
    }
    else
    {
      if (*mask != '\0')
	user = mask;
    }
  }
  else if ((t = strchr(mask, '!')) != NULL)
  {
    ex = t;
    *t++ = '\0';
    if (*mask != '\0')
      nick = mask;
    if (*t != '\0')
      user = t;
  }
  else if (strchr(mask, '.') != NULL && strchr(mask, ':') != NULL)
  {
    if (*mask != '\0')
      host = mask;
  }
  else
  {
    if (*mask != '\0')
      nick = mask;
  }

  /* truncate values to max lengths */
  if (strlen(nick) > NICKLEN - 1)
  {
    ne = nick[NICKLEN - 1];
    nick[NICKLEN - 1] = '\0';
  }
  if (strlen(user) > USERLEN)
  {
    ue = user[USERLEN];
    user[USERLEN] = '\0';
  }
  if (strlen(host) > HOSTLEN)
  {
    he = host[HOSTLEN];
    host[HOSTLEN] = '\0';
  }
    
  mask_pos += ircsprintf(mask_buf + mask_pos, "%s!%s@%s", nick, user, host)
    + 1;

  /* restore mask, since we may need to use it again later */
  if (at)
    *at = '@';
  if (ex)
    *ex = '!';
  if (ne)
    nick[NICKLEN - 1] = ne;
  if (ue)
    user[USERLEN] = ue;
  if (he)
    host[HOSTLEN] = he;

  return mask_buf + old_mask_pos;
}

/*
 * fix_key
 * 
 * inputs       - pointer to key to clean up
 * output       - pointer to cleaned up key
 * side effects - input string is modified
 *
 * stolen from Undernet's ircd  -orabidoo
 */
static char *
fix_key(char *arg)
{
  u_char *s, *t, c;

  for (s = t = (u_char *) arg; (c = *s); s++)
  {
    c &= 0x7f;
    if (c != ':' && c > ' ')
      *t++ = c;
  }
  *t = '\0';
  return arg;
}

/*
 * fix_key_old
 * 
 * inputs       - pointer to key to clean up
 * output       - pointer to cleaned up key
 * side effects - input string is modifed 
 *
 * Here we attempt to be compatible with older non-hybrid servers.
 * We can't back down from the ':' issue however.  --Rodder
 */
static char *
fix_key_old(char *arg)
{
  u_char *s, *t, c;

  for (s = t = (u_char *) arg; (c = *s); s++)
  {
    c &= 0x7f;
    if ((c != 0x0a) && (c != ':') && (c != 0x0d))
      *t++ = c;
  }
  *t = '\0';
  return arg;
}

/* bitmasks for various error returns that set_channel_mode should only return
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
#define SM_ERR_RPL_I            0x00000100
#define SM_ERR_RPL_D            0x00000200

/* Now lets do some stuff to keep track of what combinations of
 * servers exist...
 * Note that the number of combinations doubles each time you add
 * something to this list. Each one is only quick if no servers use that
 * combination, but if the numbers get too high here MODE will get too
 * slow. I suggest if you get more than 7 here, you consider getting rid
 * of some and merging or something. If it wasn't for irc+cs we would
 * probably not even need to bother about most of these, but unfortunately
 * we do. -A1kmm
 */

/* void init_chcap_usage_counts(void)
 *
 * Inputs	- none
 * Output	- none
 * Side-effects	- Initialises the usage counts to zero. Fills in the
 *                chcap_yes and chcap_no combination tables.
 */
void
init_chcap_usage_counts(void)
{
  unsigned long m, c, y, n;

  memset(chcap_combos, 0, sizeof(chcap_combos));

  /* For every possible combination */
  for (m = 0; m < NCHCAP_COMBOS; m++)
  {
    /* Check each capab */
    for (c = y = n = 0; c < NCHCAPS; c++)
    {
      if ((m & (1 << c)) == 0)
        n |= channel_capabs[c];
      else
        y |= channel_capabs[c];
    }
    chcap_combos[m].cap_yes = y;
    chcap_combos[m].cap_no  = n;
  }
}

/* void set_chcap_usage_counts(struct Client *serv_p)
 * Input: serv_p; The client whose capabs to register.
 * Output: none
 * Side-effects: Increments the usage counts for the correct capab
 *               combination.
 */
void
set_chcap_usage_counts(struct Client *serv_p)
{
  int n;

  for (n = 0; n < NCHCAP_COMBOS; n++)
  {
    if (((serv_p->localClient->caps & chcap_combos[n].cap_yes) ==
         chcap_combos[n].cap_yes) &&
        ((serv_p->localClient->caps & chcap_combos[n].cap_no) == 0))
    {
      chcap_combos[n].count++;
      return;
    }
  }

  /* This should be impossible -A1kmm. */
  assert(0);
}

/* void set_chcap_usage_counts(struct Client *serv_p)
 *
 * Inputs	- serv_p; The client whose capabs to register.
 * Output	- none
 * Side-effects	- Decrements the usage counts for the correct capab
 *                combination.
 */

void
unset_chcap_usage_counts(struct Client *serv_p)
{
  int n;

  for (n = 0; n < NCHCAP_COMBOS; n++)
  {
    if ((serv_p->localClient->caps & chcap_combos[n].cap_yes) ==
        chcap_combos[n].cap_yes &&
        (serv_p->localClient->caps & chcap_combos[n].cap_no) == 0)
    {
      /* Hopefully capabs can't change dynamically or anything... */
      assert(chcap_combos[n].count > 0);
      chcap_combos[n].count--;
      return;
    }
  }

  /* This should be impossible -A1kmm. */
  assert(0);
}

/* Mode functions handle mode changes for a particular mode... */
static void
chm_nosuch(struct Client *client_p, struct Client *source_p,
           struct Channel *chptr, int parc, int *parn,
           char **parv, int *errors, int alev, int dir, char c, void *d,
           const char *chname)
{
  if (*errors & SM_ERR_UNKNOWN)
    return;
  *errors |= SM_ERR_UNKNOWN;
  sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
}

static void
chm_simple(struct Client *client_p, struct Client *source_p,
           struct Channel *chptr, int parc, int *parn,
           char **parv, int *errors, int alev, int dir, char c, void *d,
           const char *chname)
{
  long mode_type;

  mode_type = (long)d;

  if(alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  /* +ntspmaikl == 9 + MAXMODEPARAMS (4 * +o) */
  if (MyClient(source_p) && (++mode_limit > (9 + MAXMODEPARAMS)))
    return;

  /* setting + */
  if ((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
  {
    chptr->mode.mode |= mode_type;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count++].arg = NULL;
  }
  else if ((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
  {
    /* setting - */

    chptr->mode.mode &= ~mode_type;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = NULL;
  }
}

#ifdef ANONOPS
static void
chm_hideops(struct Client *client_p, struct Client *source_p,
            struct Channel *chptr, int parc, int *parn,
            char **parv, int *errors, int alev, int dir, char c, void *d,
            const char *chname)
{
  /* if we dont support it, dont send it anywhere.  well only
   * have the capab if we support it, so we should never get
   * here for a remote server if we dont support it..
   */
  if(!ConfigChannel.use_anonops)
    return;

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if (dir == MODE_ADD && !(chptr->mode.mode & MODE_HIDEOPS))
  {
    chptr->mode.mode |= MODE_HIDEOPS;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = CAP_AOPS;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count++].arg = NULL;
  }
  else if (dir == MODE_DEL && (chptr->mode.mode & MODE_HIDEOPS))
  {
    chptr->mode.mode &= ~MODE_HIDEOPS;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = CAP_AOPS;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count++].arg = NULL;
  }
}
#endif

static void
chm_ban(struct Client *client_p, struct Client *source_p,
        struct Channel *chptr, int parc, int *parn,
        char **parv, int *errors, int alev, int dir, char c, void *d,
        const char *chname)
{
  char *mask;
  char *raw_mask;
  dlink_node *ptr;
  struct Ban *banptr;

  if (dir == 0 || parc <= *parn)
  {
    if ((*errors & SM_ERR_RPL_B) != 0)
      return;
    *errors |= SM_ERR_RPL_B;

#ifdef ANONOPS
    if ((chptr->mode.mode & MODE_HIDEOPS) && (alev < CHACCESS_CHANOP))
      DLINK_FOREACH(ptr, chptr->banlist.head)
      {
        banptr = ptr->data;
        sendto_one(client_p, form_str(RPL_BANLIST),
                   me.name, client_p->name, chname,
                   banptr->banstr, me.name, banptr->when);
      }
    else
#endif
      DLINK_FOREACH(ptr, chptr->banlist.head)
      {
        banptr = ptr->data;
        sendto_one(client_p, form_str(RPL_BANLIST),
                   me.name, client_p->name, chname,
                   banptr->banstr, banptr->who, banptr->when);
      }
    sendto_one(source_p, form_str(RPL_ENDOFBANLIST), me.name,
               source_p->name, chname);
    return;
  }

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if (MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
    return;

  raw_mask = parv[(*parn)++];
  
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);

  /* we'd have problems parsing this, hyb6 does it too */
  if(strlen(mask) > (MODEBUFLEN - 2))
    return;

  /* if we're adding a NEW id */
  if (dir == MODE_ADD) 
  {
    add_id(source_p, chptr, mask, CHFL_BAN);

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_BAN) == 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if(del_id(chptr, raw_mask, CHFL_BAN))
        mask = raw_mask;
    }

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
}

static void
chm_except(struct Client *client_p, struct Client *source_p,
           struct Channel *chptr, int parc, int *parn,
           char **parv, int *errors, int alev, int dir, char c, void *d,
           const char *chname)
{
  dlink_node *ptr;
  struct Ban *banptr;
  char *mask, *raw_mask;

  /* if we have +e disabled, allow local clients to do anything but
   * set the mode.  This prevents the abuse of +e when just a few
   * servers support it. --fl
   */
  if(!ConfigChannel.use_except && MyClient(source_p) && 
    ((dir == MODE_ADD) && (parc > *parn)))
  {
    if ((*errors & SM_ERR_RPL_E) != 0)
      return;

    *errors |= SM_ERR_RPL_E;
    return;
  }

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_QUERY) || parc <= *parn)
  {
    if ((*errors & SM_ERR_RPL_E) != 0)
      return;
    *errors |= SM_ERR_RPL_E;

    DLINK_FOREACH(ptr, chptr->exceptlist.head)
    {
      banptr = ptr->data;
      sendto_one(client_p, form_str(RPL_EXCEPTLIST),
                 me.name, client_p->name, chname,
                 banptr->banstr, banptr->who, banptr->when);
    }
    sendto_one(source_p, form_str(RPL_ENDOFEXCEPTLIST), me.name,
               source_p->name, chname);
    return;
  }

  if (MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
    return;

  raw_mask = parv[(*parn)++];
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);

  if(strlen(mask) > (MODEBUFLEN - 2))
    return;

  /* If we're adding a NEW id */
  if (dir == MODE_ADD)
  {
    add_id(source_p, chptr, mask, CHFL_EXCEPTION);

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = CAP_EX;
    mode_changes[mode_count].nocaps = 0;

    if(ConfigChannel.use_except)
      mode_changes[mode_count].mems = ONLY_CHANOPS;
    else
      mode_changes[mode_count].mems = ONLY_SERVERS;

    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_EXCEPTION) == 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if(del_id(chptr, raw_mask, CHFL_EXCEPTION))
        mask = raw_mask;
    }

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = CAP_EX;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ONLY_CHANOPS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
}

static void
chm_invex(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  char *mask, *raw_mask;
  dlink_node *ptr;
  struct Ban *banptr;

  /* if we have +I disabled, allow local clients to do anything but
   * set the mode.  This prevents the abuse of +I when just a few
   * servers support it --fl
   */
  if(!ConfigChannel.use_invex && MyClient(source_p) && 
    (dir == MODE_ADD) && (parc > *parn))
  {
    if((*errors & SM_ERR_RPL_I) != 0)
      return;
    
    *errors |= SM_ERR_RPL_I;
    return;
  }

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_QUERY) || parc <= *parn)
  {
    if ((*errors & SM_ERR_RPL_I) != 0)
      return;
    *errors |= SM_ERR_RPL_I;

    DLINK_FOREACH(ptr, chptr->invexlist.head)
    {
      banptr = ptr->data;
      sendto_one(client_p, form_str(RPL_INVITELIST), me.name,
                 client_p->name, chname, banptr->banstr,
                 banptr->who, banptr->when);
    }
    sendto_one(source_p, form_str(RPL_ENDOFINVITELIST), me.name,
               source_p->name, chname);
    return;
  }

  if (MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
    return;

  raw_mask = parv[(*parn)++];
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);

  if(strlen(mask) > (MODEBUFLEN - 2))
    return;

  if(dir == MODE_ADD)
  {
    add_id(source_p, chptr, mask, CHFL_INVEX);

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = CAP_IE;
    mode_changes[mode_count].nocaps = 0;
    
    if(ConfigChannel.use_invex)
      mode_changes[mode_count].mems = ONLY_CHANOPS;
    else
      mode_changes[mode_count].mems = ONLY_SERVERS;

    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_INVEX) == 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if(del_id(chptr, raw_mask, CHFL_INVEX))
        mask = raw_mask;
    }

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = CAP_IE;
    mode_changes[mode_count].nocaps = 0;

    if(ConfigChannel.use_invex)
      mode_changes[mode_count].mems = ONLY_CHANOPS;
    else
      mode_changes[mode_count].mems = ONLY_SERVERS;

    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = mask;
  }
}

static void
chm_op(struct Client *client_p, struct Client *source_p,
       struct Channel *chptr, int parc, int *parn,
       char **parv, int *errors, int alev, int dir, char c, void *d,
       const char *chname)
{
  char *opnick;
  struct Client *targ_p;

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_QUERY) || (parc <= *parn))
    return;

  if(IsRestricted(source_p) && (dir == MODE_ADD))
  {
    if(!(*errors & SM_ERR_RESTRICTED))
      sendto_one(source_p, 
                ":%s NOTICE %s :*** Notice -- You are restricted and cannot chanop others",
		me.name, source_p->name);
    
    *errors |= SM_ERR_RESTRICTED;
    return;
  }

  opnick = parv[(*parn)++];

  if ((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
  {
    return;
  }

  if (!IsMember(targ_p, chptr))
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL), me.name,
                 source_p->name, chname, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
    return;
  }

  if (MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
    return;

  if (dir == MODE_ADD)
  {
    if(targ_p == source_p)
      return;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ONLY_CHANOPS;
    mode_changes[mode_count].id = targ_p->user->id;
    mode_changes[mode_count].arg = targ_p->name;
    mode_changes[mode_count++].client = targ_p;
  }
  else
  {
    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ONLY_CHANOPS;
    mode_changes[mode_count].id = targ_p->user->id;
    mode_changes[mode_count].arg = targ_p->name;
    mode_changes[mode_count++].client = targ_p;
  }
}

static void
chm_voice(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  char *opnick;
  struct Client *targ_p;

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_QUERY) || parc <= *parn)
    return;

  opnick = parv[(*parn)++];

  if ((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
  {
    return;
  }

  if (!IsMember(targ_p, chptr))
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL), me.name,
                 source_p->name, chname, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
    return;
  }

  if (MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
    return;

  if (dir == MODE_ADD)
  {
    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ONLY_CHANOPS;
    mode_changes[mode_count].id = targ_p->user->id;
    mode_changes[mode_count].arg = targ_p->name;
    mode_changes[mode_count++].client = targ_p;
  }
  else
  {
    mode_changes[mode_count].letter = 'v';
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ONLY_CHANOPS;
    mode_changes[mode_count].id = targ_p->user->id;
    mode_changes[mode_count].arg = targ_p->name;
    mode_changes[mode_count++].client = targ_p;
  }
}

static void
chm_limit(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  int limit;
  char *lstr;

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if (dir == MODE_QUERY)
    return;

  if ((dir == MODE_ADD) && parc > *parn)
  {
    lstr = parv[(*parn)++];

    if ((limit = strtoul(lstr, NULL, 10)) <= 0)
      return;

    ircsprintf(lstr, "%d", limit);

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = lstr;

    chptr->mode.limit = limit;
  }
  else if (dir == MODE_DEL)
  {
    if (!chptr->mode.limit)
      return;

    chptr->mode.limit = 0;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = NULL;
  }
}

static void
chm_key(struct Client *client_p, struct Client *source_p,
        struct Channel *chptr, int parc, int *parn,
        char **parv, int *errors, int alev, int dir, char c, void *d,
        const char *chname)
{
  char *key;

  if (alev < CHACCESS_CHANOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if (dir == MODE_QUERY)
    return;

  if ((dir == MODE_ADD) && parc > *parn)
  {
    key = parv[(*parn)++];

    if (MyClient(source_p))
      fix_key(key);
    else
      fix_key_old(key);

    assert(key[0] != ' ');
    strlcpy(chptr->mode.key, key, sizeof(chptr->mode.key));

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_ADD;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = chptr->mode.key;
  }
  else if (dir == MODE_DEL)
  {
    if (!(*chptr->mode.key))
      return;

    *chptr->mode.key = 0;

    mode_changes[mode_count].letter = c;
    mode_changes[mode_count].dir = MODE_DEL;
    mode_changes[mode_count].caps = 0;
    mode_changes[mode_count].nocaps = 0;
    mode_changes[mode_count].mems = ALL_MEMBERS;
    mode_changes[mode_count].id = NULL;
    mode_changes[mode_count++].arg = "*";
  }
}

struct ChannelMode
{
  void (*func) (struct Client *client_p, struct Client *source_p,
                struct Channel *chptr, int parc, int *parn, char **parv,
                int *errors, int alev, int dir, char c, void *d,
                const char *chname);
  void *d;
};
/* *INDENT-OFF* */
static struct ChannelMode ModeTable[255] =
{
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},                             /* A */
  {chm_nosuch, NULL},                             /* B */
  {chm_nosuch, NULL},                             /* C */
  {chm_nosuch, NULL},                             /* D */
  {chm_nosuch, NULL},                             /* E */
  {chm_nosuch, NULL},                             /* F */
  {chm_nosuch, NULL},                             /* G */
  {chm_nosuch, NULL},                             /* H */
  {chm_invex, NULL},                              /* I */
  {chm_nosuch, NULL},                             /* J */
  {chm_nosuch, NULL},                             /* K */
  {chm_nosuch, NULL},                             /* L */
  {chm_nosuch, NULL},                             /* M */
  {chm_nosuch, NULL},                             /* N */
  {chm_nosuch, NULL},                             /* O */
  {chm_nosuch, NULL},                             /* P */
  {chm_nosuch, NULL},                             /* Q */
  {chm_nosuch, NULL},                             /* R */
  {chm_nosuch, NULL},                             /* S */
  {chm_nosuch, NULL},                             /* T */
  {chm_nosuch, NULL},                             /* U */
  {chm_nosuch, NULL},                             /* V */
  {chm_nosuch, NULL},                             /* W */
  {chm_nosuch, NULL},                             /* X */
  {chm_nosuch, NULL},                             /* Y */
  {chm_nosuch, NULL},                             /* Z */
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},
  {chm_nosuch, NULL},
#ifdef ANONOPS
  {chm_hideops, NULL},                            /* a */
#else
  {chm_nosuch, NULL},				  /* a */
#endif
  {chm_ban, NULL},                                /* b */
  {chm_nosuch, NULL},                             /* c */
  {chm_nosuch, NULL},                             /* d */
  {chm_except, NULL},                             /* e */
  {chm_nosuch, NULL},                             /* f */
  {chm_nosuch, NULL},                             /* g */
  {chm_nosuch, NULL},				  /* h */
  {chm_simple, (void *) MODE_INVITEONLY},         /* i */
  {chm_nosuch, NULL},                             /* j */
  {chm_key, NULL},                                /* k */
  {chm_limit, NULL},                              /* l */
  {chm_simple, (void *) MODE_MODERATED},          /* m */
  {chm_simple, (void *) MODE_NOPRIVMSGS},         /* n */
  {chm_op, NULL},                                 /* o */
  {chm_simple, (void *) MODE_PRIVATE},            /* p */
  {chm_nosuch, NULL},                             /* q */
  {chm_nosuch, NULL},                             /* r */
  {chm_simple, (void *) MODE_SECRET},             /* s */
  {chm_simple, (void *) MODE_TOPICLIMIT},         /* t */
  {chm_nosuch, NULL},                             /* u */
  {chm_voice, NULL},                              /* v */
  {chm_nosuch, NULL},                             /* w */
  {chm_nosuch, NULL},                             /* x */
  {chm_nosuch, NULL},                             /* y */
  {chm_nosuch, NULL},                             /* z */
};
/* *INDENT-ON* */

/* int get_channel_access(struct Client *source_p, struct Channel *chptr)
 * Input: The client, the channel
 * Output: MODE_CHANOP if we should let them have chanop level access,
 *         MODE_PEON for peon level access.
 * Side-effects: None.
 */
  static int
get_channel_access(struct Client *source_p, struct Channel *chptr)
{
  /* Let hacked servers in for now... */

  if (!MyClient(source_p))
    return CHACCESS_CHANOP;

  if (is_chan_op(chptr, source_p))
    return CHACCESS_CHANOP;

  return CHACCESS_PEON;
}

/* void send_cap_mode_changes(struct Client *client_p,
 *                        struct Client *source_p,
 *                        struct Channel *chptr, int cap, int nocap)
 * Input: The client sending(client_p), the source client(source_p),
 *        the channel to send mode changes for(chptr)
 * Output: None.
 * Side-effects: Sends the appropriate mode changes to capable servers.
 *
 * Reverted back to my original design, except that we now keep a count
 * of the number of servers which each combination as an optimisation, so
 * the capabs combinations which are not needed are not worked out. -A1kmm
 */
static void
send_cap_mode_changes(struct Client *client_p, struct Client *source_p,
                      struct Channel *chptr, int cap, int nocap)
{
  int i, mbl, pbl, nc, mc;
  char *arg;
  int dir;

  mc = 0;
  nc = 0;
  pbl = 0;
  parabuf[0] = 0;
  dir = MODE_QUERY;

  if ((cap & CAP_UID) && source_p->user &&
      (source_p->user->id[0] == '.'))
    mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->user->id,
                     chptr->chname);
  else
    mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->name,
                     chptr->chname);

  /* loop the list of - modes we have */
  for (i = 0; i < mode_count; i++)
  {
    /* if they dont support the cap we need, or they do support a cap they
     * cant have, then dont add it to the modebuf.. that way they wont see
     * the mode
     */
    if ((mode_changes[i].letter == 0) ||
        ((cap & mode_changes[i].caps) != mode_changes[i].caps)
        || ((nocap & mode_changes[i].nocaps) != mode_changes[i].nocaps))
      continue;

    arg = "";
    if ((cap & CAP_UID) && mode_changes[i].id)
      arg = mode_changes[i].id;
    if (!*arg)
      arg = mode_changes[i].arg;

    /* if we're creeping past the buf size, we need to send it and make
     * another line for the other modes
     * XXX - this could give away server topology with uids being
     * different lengths, but not much we can do, except possibly break
     * them as if they were the longest of the nick or uid at all times,
     * which even then won't work as we don't always know the uid -A1kmm.
     */
    if ((arg != NULL) && ((mc == MAXMODEPARAMS) ||
                        ((strlen(arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (nc != 0)
        sendto_server(client_p, chptr, cap, nocap,
                      "%s %s", modebuf, parabuf);
      nc = 0;
      mc = 0;

      if ((cap & CAP_UID) && source_p->user &&
          (source_p->user->id[0] == '.'))
        mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->user->id,
                         chptr->chname);
      else
        mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->name,
                         chptr->chname);

      pbl = 0;
      parabuf[0] = 0;
      dir = MODE_QUERY;
    }

    if(dir != mode_changes[i].dir)
    {
      modebuf[mbl++] = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
      dir = mode_changes[i].dir;
    }

    modebuf[mbl++] = mode_changes[i].letter;
    modebuf[mbl] = 0;
    nc++;

    if (arg != NULL)
    {
      pbl = strlcat(parabuf, arg, MODEBUFLEN);
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
      mc++;
    }
  }

  if (pbl && parabuf[pbl - 1] == ' ')
    parabuf[pbl - 1] = 0;

  if (nc != 0)
    sendto_server(client_p, chptr, cap, nocap,
                  "%s %s", modebuf, parabuf);
}

/* void send_mode_changes(struct Client *client_p,
 *                        struct Client *source_p,
 *                        struct Channel *chptr)
 * Input: The client sending(client_p), the source client(source_p),
 *        the channel to send mode changes for(chptr),
 *        mode change globals.
 * Output: None.
 * Side-effects: Sends the appropriate mode changes to other clients
 *               and propagates to servers.
 */
static void
send_mode_changes(struct Client *client_p, struct Client *source_p,
                  struct Channel *chptr, char *chname)
{
  int pbl, mbl, nc, mc;
  int i, st;
  int dir = MODE_QUERY;

  /* bail out if we have nothing to do... */
  if (!mode_count)
    return;

  /* Send all mode changes to the chanops, and even peons if -a */
#ifdef ANONOPS
  st = (chptr->mode.mode & MODE_HIDEOPS) ? ONLY_CHANOPS : ALL_MEMBERS;
#else
  st = ALL_MEMBERS;
#endif

  if (IsServer(source_p))
    mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
  else
    mbl = ircsprintf(modebuf, ":%s!%s@%s MODE %s ", source_p->name,
                     source_p->username, source_p->host, chname);

  pbl = 0;
  parabuf[0] = '\0';
  nc = 0;
  mc = 0;

  for (i = 0; i < mode_count; i++)
  {
    if (mode_changes[i].letter == 0 ||
        mode_changes[i].mems == NON_CHANOPS ||
        mode_changes[i].mems == ONLY_SERVERS)
      continue;

    if (mode_changes[i].arg != NULL &&
        ((mc == MAXMODEPARAMS)  || 
        ((strlen(mode_changes[i].arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (mbl && modebuf[mbl - 1] == '-')
        modebuf[mbl - 1] = '\0';

      if (nc != 0)
        sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

      nc = 0;
      mc = 0;

      if (IsServer(source_p))
        mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
      else
        mbl = ircsprintf(modebuf, ":%s!%s@%s MODE %s ", source_p->name,
                   source_p->username, source_p->host, chname);

      pbl = 0;
      parabuf[0] = '\0';
      dir = MODE_QUERY;
    }

    if(dir != mode_changes[i].dir)
    {
      modebuf[mbl++] = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
      dir = mode_changes[i].dir;
    }

    modebuf[mbl++] = mode_changes[i].letter;
    modebuf[mbl] = '\0';
    nc++;

    if (mode_changes[i].arg != NULL)
    {
      mc++;
      pbl = strlen(strcat(parabuf, mode_changes[i].arg));
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (pbl && parabuf[pbl - 1] == ' ')
    parabuf[pbl - 1] = 0;

  if (nc != 0)
    sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

  nc = 0;
  mc = 0;

#ifdef ANONOPS
  /* If peons were missed out above, send to them now... */
  if (chptr->mode.mode & MODE_HIDEOPS)
  {
    st = NON_CHANOPS;
    dir = MODE_QUERY;
    mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
    pbl = 0;
    parabuf[0] = '\0';

    for (i = 0; i < mode_count; i++)
    {
      if (mode_changes[i].letter == 0 ||
          mode_changes[i].mems == ONLY_SERVERS)
        continue;

      if (mode_changes[i].mems != ALL_MEMBERS)
      {
        if (mode_changes[i].letter == 'v' &&
	    MyConnect(mode_changes[i].client) &&
	    !is_chan_op(chptr, mode_changes[i].client))
	  sendto_one(mode_changes[i].client, ":%s MODE %s -v %s",
	             me.name, chname, mode_changes[i].arg);
	continue;
      }

      if (mode_changes[i].arg != NULL && ((mc == MAXMODEPARAMS) ||
           ((strlen(mode_changes[i].arg) + mbl + pbl + 2) > BUFSIZE)))
      {
        if (mbl && modebuf[mbl - 1] == '-')
          modebuf[mbl - 1] = '\0';

        if (nc != 0)
          sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

        nc = 0;
        mc = 0;
        
        mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
        pbl = 0;
        parabuf[0] = '\0';
      }

      if(dir != mode_changes[i].dir)
      {
        modebuf[mbl++] = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
        dir = mode_changes[i].dir;
      }

      modebuf[mbl++] = mode_changes[i].letter;
      modebuf[mbl] = '\0';
      nc++;

      if (mode_changes[i].arg != NULL)
      {
        mc++;
        pbl = strlen(strcat(parabuf, mode_changes[i].arg));
        parabuf[pbl++] = ' ';
        parabuf[pbl] = '\0';
      }
    }

    if (pbl && parabuf[pbl - 1] == ' ')
      parabuf[pbl - 1] = 0;
    if (mbl && modebuf[mbl - 1] == '+')
      modebuf[mbl - 1] = 0;

    if (nc != 0)
      sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);
  }
#endif

  /* Now send to servers... */
  for (i = 0; i < NCHCAP_COMBOS; i++)
    if (chcap_combos[i].count != 0)
      send_cap_mode_changes(client_p, source_p, chptr,
                            chcap_combos[i].cap_yes,
                            chcap_combos[i].cap_no);
}

/* void set_channel_mode(struct Client *client_p, struct Client *source_p,
 *               struct Channel *chptr, int parc, char **parv,
 *               char *chname)
 * Input: The client we received this from, the client this originated
 *        from, the channel, the parameter count starting at the modes,
 *        the parameters, the channel name.
 * Output: None.
 * Side-effects: Changes the channel membership and modes appropriately,
 *               sends the appropriate MODE messages to the appropriate
 *               clients.
 */
void
set_channel_mode(struct Client *client_p, struct Client *source_p,
                 struct Channel *chptr, int parc, char *parv[], char *chname)
{
  int dir = MODE_ADD;
  int parn = 1;
  int alevel, errors = 0;
  char *ml = parv[0], c;
  int table_position;

  mask_pos = 0;
  mode_count = 0;
#ifdef ANONOPS
  hideops_changed = (chptr->mode.mode & MODE_HIDEOPS);
#endif
  mode_limit = 0;

  alevel = get_channel_access(source_p, chptr);

  for (; (c = *ml) != 0; ml++)
    switch (c)
    {
      case '+':
        dir = MODE_ADD;
        break;
      case '-':
        dir = MODE_DEL;
        break;
      case '=':
        dir = MODE_QUERY;
        break;
      default:
        if (c < 'A' || c > 'z')
          table_position = 0;
        else
          table_position = c - 'A' + 1;
        ModeTable[table_position].func(client_p, source_p, chptr,
                                       parc, &parn,
                                       parv, &errors, alevel, dir, c,
                                       ModeTable[table_position].d,
                                       chname);
        break;
    }

  update_channel_info(chptr);

  send_mode_changes(client_p, source_p, chptr, chname);
}

/*
 * set_channel_mode_flags
 *
 * inputs       - pointer to array of strings for chanops, voiced,
 *                peons
 *              - pointer to channel
 *              - pointer to source
 * output       - none
 * side effects -
 */
void
set_channel_mode_flags(char flags_ptr[NUMLISTS][2], struct Channel *chptr,
                       struct Client *source_p)
{
#ifdef ANONOPS
  if (chptr->mode.mode & MODE_HIDEOPS && !is_chan_op(chptr, source_p))
  {
    flags_ptr[0][0] = '\0';
    flags_ptr[1][0] = '\0';
    flags_ptr[2][0] = '\0';
    flags_ptr[3][0] = '\0';
  }
  else
#endif
  {
    flags_ptr[0][0] = '@';
    flags_ptr[1][0] = '+';
    flags_ptr[2][0] = '\0';
    flags_ptr[3][0] = '@';

    flags_ptr[0][1] = '\0';
    flags_ptr[1][1] = '\0';
    flags_ptr[3][1] = '\0';
  }
}


#ifdef ANONOPS
/*
 * sync_oplists
 *
 * inputs       - pointer to channel
 *              - pointer to client
 * output       - none
 * side effects - Sends MODE +o/+h/+v list to user
 *                (for +a channels)
 */
static void
sync_oplists(struct Channel *chptr, struct Client *target_p,
             int dir, const char *name)
{
  send_oplist(name, target_p, &chptr->chanops, "o", dir);
  send_oplist(name, target_p, &chptr->chanops_voiced, "o", dir);
  send_oplist(name, target_p, &chptr->voiced, "v", dir);
  send_oplist(name, target_p, &chptr->chanops_voiced, "v", dir);
}

static void
send_oplist(const char *chname, struct Client *client_p, dlink_list * list,
            char *prefix, int dir)
{
  dlink_node *ptr;
  int cur_modes = 0;            /* no of chars in modebuf */
  struct Client *target_p;
  int data_to_send = 0;
  char mcbuf[6] = "";
  char opbuf[MODEBUFLEN];
  char *t;

  *mcbuf = *opbuf = '\0';
  t = opbuf;

  DLINK_FOREACH(ptr, list->head)
  {
    target_p = ptr->data;
    if (dir == MODE_DEL && *prefix == 'v' && target_p == client_p)
      continue;

    if (cur_modes == 0)
    {
      mcbuf[cur_modes++] = ((dir == MODE_ADD) ? '+' : '-');
    }

    mcbuf[cur_modes++] = *prefix;

    t += ircsprintf(t, "%s ", target_p->name);
    data_to_send = 1;

    if (cur_modes == (MAXMODEPARAMS + 1))       /* '+' and modes */
    {
      *t = '\0';
      mcbuf[cur_modes] = '\0';
      sendto_one(client_p, ":%s MODE %s %s %s", me.name,
                 chname, mcbuf, opbuf);

      cur_modes = 0;
      *mcbuf = *opbuf = '\0';
      t = opbuf;
      data_to_send = 0;
    }
  }

  if (data_to_send)
  {
    *t = '\0';
    mcbuf[cur_modes] = '\0';
    sendto_one(client_p, ":%s MODE %s %s %s", me.name, chname, mcbuf, opbuf);
  }
}

void
sync_channel_oplists(struct Channel *chptr, int dir)
{
  dlink_node *ptr;

  DLINK_FOREACH(ptr, chptr->locpeons.head)
  {
    sync_oplists(chptr, ptr->data, MODE_ADD, chptr->chname);
  }
  DLINK_FOREACH(ptr, chptr->locvoiced.head)
  {
    sync_oplists(chptr, ptr->data, MODE_ADD, chptr->chname);
  }
}

#endif

/* mode_get_status()
 *
 * Used by chm_op and others instead of calling is_chan_op and is_voiced.
 * Since member status is now changed *after* processing all
 * modes, we need a special tool to keep track of who is opped, voiced etc.
 */
#ifdef ANONOPS
static void mode_get_status(struct Channel *chptr, struct Client *target_p,
                            int *t_op, int *t_voice, int need_check)
{
  int i;

  if (need_check)
  {
    *t_op = is_chan_op(chptr, target_p);
    *t_voice = is_voiced(chptr, target_p);
  }
  else
  {
    *t_op = 0;
  }

  for (i = 0; i < mode_count; i++)
  {
    if (mode_changes[i].client == target_p)
    {
      if (mode_changes[i].letter == 'o')
        *t_op = (mode_changes[i].dir == MODE_ADD) ? 1 : 0;
      else if (mode_changes[i].letter == 'v')
        *t_voice = (mode_changes[i].dir == MODE_ADD) ? 1 : 0;
    }
  }
}
#endif

static void update_channel_info(struct Channel *chptr)
{
  int i;
#ifdef ANONOPS
  int t_voice, t_op;
  dlink_node *ptr;
  dlink_node *ptr_next;

  /* hideops_changed is set to chptr->mode.mode & MODE_HIDEOPS at
   * the beginning..
   */
  if (hideops_changed != (chptr->mode.mode & MODE_HIDEOPS))
  {
    if(chptr->mode.mode & MODE_HIDEOPS)
    {
      DLINK_FOREACH(ptr, chptr->locpeons.head)
      {
        mode_get_status(chptr, ptr->data, &t_op, &t_voice, 0);
        if (!t_op)
          sync_oplists(chptr, ptr->data, MODE_DEL, chptr->chname);
      }

      DLINK_FOREACH(ptr, chptr->locvoiced.head)
      {
        mode_get_status(chptr, ptr->data, &t_op, &t_voice, 0);
        if (!t_op)
          sync_oplists(chptr, ptr->data, MODE_DEL, chptr->chname);
      }

      DLINK_FOREACH(ptr, chptr->locchanops_voiced.head)
      {
        mode_get_status(chptr, ptr->data, &t_op, &t_voice, 1);
        if(!t_op)
          sync_oplists(chptr, ptr->data, MODE_DEL, chptr->chname);
      }

      DLINK_FOREACH(ptr, chptr->locchanops.head)
      {
        mode_get_status(chptr, ptr->data, &t_op, &t_voice, 1);
        if(!t_op)
          sync_oplists(chptr, ptr->data, MODE_DEL, chptr->chname);
      }
    }
    else
    {
      sync_channel_oplists(chptr, MODE_ADD);
      chptr->mode.mode &= ~MODE_HIDEOPS;
    }
  }

  /* +/-a hasnt changed but we're still +a, sync those who have changed
   * status
   */
  else if(chptr->mode.mode & MODE_HIDEOPS)
  {
    dlink_list deopped = {NULL, NULL};
    dlink_list opped = {NULL, NULL};

    for (i = 0; i < mode_count; i++)
    {
      if(mode_changes[i].dir == MODE_DEL)
      {
        if((mode_changes[i].letter == 'o' || mode_changes[i].letter == 'h') &&
           MyConnect(mode_changes[i].client))
        {
          if((ptr = dlinkFind(&opped, mode_changes[i].client)) == NULL)
          {
            dlinkAddAlloc(mode_changes[i].client, &deopped);
          }
          else
          {
            dlinkDestroy(ptr, &opped);
          }
        }
      }
      else
      {
        if((mode_changes[i].letter == 'o' || mode_changes[i].letter == 'h') &&
           MyConnect(mode_changes[i].client))
        {
          if((ptr = dlinkFind(&deopped, mode_changes[i].client)) == NULL)
          {
            dlinkAddAlloc(mode_changes[i].client, &opped);
          }
          else
          {
            dlinkDestroy(ptr, &deopped);
          }
        }
      }
    }

    /* ..and send a resync to them */
    DLINK_FOREACH_SAFE(ptr, ptr_next, deopped.head)
    {
      sync_oplists(chptr, ptr->data, MODE_DEL, chptr->chname);
      free_dlink_node(ptr);
    }

    DLINK_FOREACH_SAFE(ptr, ptr_next, opped.head)
    {
      sync_oplists(chptr, ptr->data, MODE_ADD, chptr->chname);
      free_dlink_node(ptr);
    }
  }
#endif

  /* Update channel members lists. */
  for (i = 0; i < mode_count; i++)
  {
    if (mode_changes[i].letter == 'o')
    {
      if (is_voiced(chptr, mode_changes[i].client))
      {
        if(mode_changes[i].dir == MODE_DEL)
        {
          change_channel_membership(chptr, &chptr->voiced, &chptr->locvoiced,
	                            mode_changes[i].client);
        }
        else
        {
          change_channel_membership(chptr, &chptr->chanops_voiced,
                                    &chptr->locchanops_voiced, 
                                    mode_changes[i].client);
        }
      }
      else
      {
        if(mode_changes[i].dir == MODE_DEL)
        {
          change_channel_membership(chptr, &chptr->peons, &chptr->locpeons,
	                            mode_changes[i].client);
        }
        else
        {
          change_channel_membership(chptr, &chptr->chanops, &chptr->locchanops,
                                    mode_changes[i].client);
        }
      }
    }
    else if (mode_changes[i].letter == 'v')
    {
      if (is_chan_op(chptr, mode_changes[i].client))
      {
        if(mode_changes[i].dir == MODE_DEL)
        {
          change_channel_membership(chptr, &chptr->chanops, &chptr->locchanops,
	                            mode_changes[i].client);
        }
        else
        {
          change_channel_membership(chptr, &chptr->chanops_voiced,
                                    &chptr->locchanops_voiced, 
                                    mode_changes[i].client);
        }
      }
      else
      {
        if(mode_changes[i].dir == MODE_DEL)
        {
          change_channel_membership(chptr, &chptr->peons, &chptr->locpeons,
	                            mode_changes[i].client);
        }
        else
        {
          change_channel_membership(chptr, &chptr->voiced, &chptr->locvoiced,
                                    mode_changes[i].client);
        }
      }
    }
  }
}


/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  channel_mode.c: Controls modes on channels.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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

#include "tools.h"
#include "channel.h"
#include "channel_mode.h"
#include "vchannel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "list.h"
#include "numeric.h"
#include "s_serv.h"             /* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"             /* ConfigFileEntry, ConfigChannel */
#include "event.h"
#include "memory.h"
#include "balloc.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "s_log.h"

static int add_id(struct Client *, struct Channel *, char *, int);
static int del_id(struct Channel *, char *, int);

static void send_oplist(const char *, struct Client *, dlink_list *,
                        char *, int);

static int change_channel_membership(struct Channel *chptr,
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

static void chm_halfop(struct Client *, struct Client *, struct Channel *,
                       int, int *, char **, int *, int, int, char, void *,
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

static void chm_hideops(struct Client *, struct Client *, struct Channel *,
                        int, int *, char **, int *, int, int, char, void *,
                        const char *chname);

static void send_cap_mode_changes(struct Client *, struct Client *,
                                  struct Channel *, int, int);

static void send_mode_changes(struct Client *, struct Client *,
                              struct Channel *, char *chname);

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */

static char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
static char mask_buf[BUFSIZE];
static int mask_pos;

static struct ChModeChange mode_changes_plus[BUFSIZE];
static struct ChModeChange mode_changes_minus[BUFSIZE];
static struct ChModeBounce mode_bounces[BUFSIZE];
static struct ChResyncOp resync_ops[BUFSIZE];
static int mode_count_plus, mode_count_minus, bounce_count, resync_count;
static int mode_limit;

static int channel_capabs[] = { CAP_HOPS, CAP_AOPS, CAP_EX, CAP_IE, CAP_UID };

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
  if ((!IsServer(client_p)) && (chptr->num_mask >= ConfigChannel.max_bans))
  {
    if (MyClient(client_p))
    {
      sendto_one(client_p, form_str(ERR_BANLISTFULL),
                 me.name, client_p->name, chptr->chname, banid);
      return -1;
    }
  }

  if (MyClient(client_p))
    collapse(banid);

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
      sendto_realops_flags(FLAGS_ALL, L_ALL,
                           "add_id() called with unknown ban type %d!", type);
      return -1;
  }

  for (ban = list->head; ban; ban = ban->next)
  {
    actualBan = ban->data;
    if (match(actualBan->banstr, banid))
      return -1;
  }

  ban = make_dlink_node();

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

  dlinkAdd(actualBan, ban, list);

  chptr->num_mask++;
  return 0;
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
    return -1;

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
      sendto_realops_flags(FLAGS_ALL, L_ALL,
                           "del_id() called with unknown ban type %d!", type);
      return -1;
  }

  for (ban = list->head; ban; ban = ban->next)
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

      dlinkDelete(ban, list);
      free_dlink_node(ban);

      return 0;
    }
  }
  return -1;
}

/*
 * change_channel_membership
 *
 * inputs       - pointer to channel
 *              - pointer to membership list of given channel to modify
 *              - pointer to membership list for local clients to modify 
 *              - pointer to client struct being modified
 * output       - int success 1 or 0 if failure
 * side effects - change given user "who" from whichever membership list
 *                it is on, to the given membership list in to_list.
 *                
 */
static int
change_channel_membership(struct Channel *chptr,
                          dlink_list * to_list,
                          dlink_list * loc_to_list, struct Client *who)
{
  dlink_node *ptr;
  int ok = 1;

  /* local clients need to be moved from local list too */
  if (MyClient(who))
  {
    if ((ptr = find_user_link(&chptr->locpeons, who)))
    {
      if (to_list != &chptr->locpeons)
      {
        dlinkDelete(ptr, &chptr->locpeons);
        dlinkAdd(who, ptr, loc_to_list);
      }
    }
    else if ((ptr = find_user_link(&chptr->locvoiced, who)))
    {
      if (to_list != &chptr->locvoiced)
      {
        dlinkDelete(ptr, &chptr->locvoiced);
        dlinkAdd(who, ptr, loc_to_list);
      }
    }
    else if ((ptr = find_user_link(&chptr->lochalfops, who)))
    {
      if (to_list != &chptr->lochalfops)
      {
        dlinkDelete(ptr, &chptr->lochalfops);
        dlinkAdd(who, ptr, loc_to_list);
      }
    }
    else if ((ptr = find_user_link(&chptr->locchanops, who)))
    {
      if (to_list != &chptr->locchanops)
      {
        dlinkDelete(ptr, &chptr->locchanops);
        dlinkAdd(who, ptr, loc_to_list);
      }
    }
#ifdef REQUIRE_OANDV
    else if ((ptr = find_user_link(&chptr->locchanops_voiced, who)))
    {
      if (to_list != &chptr->locchanops_voiced)
      {
        dlinkDelete(ptr, &chptr->locchanops_voiced);
        dlinkAdd(who, ptr, loc_to_list);
      }
    }
#endif
    else
      ok = 0;
  }

  if ((ptr = find_user_link(&chptr->peons, who)))
  {
    if (to_list != &chptr->peons)
    {
      dlinkDelete(ptr, &chptr->peons);
      dlinkAdd(who, ptr, to_list);
    }
  }
  else if ((ptr = find_user_link(&chptr->voiced, who)))
  {
    if (to_list != &chptr->voiced)
    {
      dlinkDelete(ptr, &chptr->voiced);
      dlinkAdd(who, ptr, to_list);
    }
  }
  else if ((ptr = find_user_link(&chptr->halfops, who)))
  {
    if (to_list != &chptr->halfops)
    {
      dlinkDelete(ptr, &chptr->halfops);
      dlinkAdd(who, ptr, to_list);
    }
  }
  else if ((ptr = find_user_link(&chptr->chanops, who)))
  {
    if (to_list != &chptr->chanops)
    {
      dlinkDelete(ptr, &chptr->chanops);
      dlinkAdd(who, ptr, to_list);
    }
  }
#ifdef REQUIRE_OANDV
  else if ((ptr = find_user_link(&chptr->chanops_voiced, who)))
  {
    if (to_list != &chptr->chanops_voiced)
    {
      dlinkDelete(ptr, &chptr->chanops_voiced);
      dlinkAdd(who, ptr, to_list);
    }
  }
#endif
  else
    ok = 0;

  return ok;
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
  if (chptr->mode.mode & MODE_HIDEOPS)
    *mbuf++ = 'a';
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
      ircsprintf(pbuf, "%s", chptr->mode.key);
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
    host = t;

    if ((t = strchr(mask, '!')) != NULL)
    {
      ex = t;
      *t++ = '\0';
      user = t;
      nick = mask;
    }
    else
      user = mask;
  }
  else if ((t = strchr(mask, '!')) != NULL)
  {
    ex = t;
    *t++ = '\0';
    nick = mask;
    user = t;
  }
  else if (strchr(mask, '.') != NULL && strchr(mask, ':') != NULL)
    host = mask;
  else
    nick = mask;

  /* truncate values to max lengths */
  if (strlen(nick) > NICKLEN)
  {
    ne = nick[NICKLEN];
    nick[NICKLEN] = '\0';
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
    nick[NICKLEN] = ne;
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
  int i;

  mode_type = (long)d;
 
  /* dont allow halfops to set +-p, as this controls whether they can set
   * +-h or not.. all other simple modes are ok
   */
  if((alev < CHACCESS_HALFOP) ||
    ((mode_type == MODE_PRIVATE) && (alev < CHACCESS_CHANOP)))
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  /* setting + */
  if ((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
  {
    chptr->mode.mode |= mode_type;

    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c)
      {
        /* + and - make 0, so return... */
        mode_changes_minus[i].letter = 0;
        return;
      }
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = 0;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus].mems = ALL_MEMBERS;
    mode_changes_plus[mode_count_plus++].arg = NULL;
  }
  else if ((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
  {
    /* setting - */

    chptr->mode.mode &= ~mode_type;

    for (i = 0; i < mode_count_plus; i++)
      if (mode_changes_plus[i].letter == c)
      {
        /* - and + make 0, so return... */
        mode_changes_plus[i].letter = 0;
        return;
      }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = 0;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ALL_MEMBERS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = NULL;
  }
}

static void
chm_hideops(struct Client *client_p, struct Client *source_p,
            struct Channel *chptr, int parc, int *parn,
            char **parv, int *errors, int alev, int dir, char c, void *d,
            const char *chname)
{
  int i;

  if (alev < CHACCESS_HALFOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_ADD) && !(chptr->mode.mode & MODE_HIDEOPS))
  {
    chptr->mode.mode |= MODE_HIDEOPS;
    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c)
      {
        /* + and - make 0, so return... */
        mode_changes_minus[i].letter = 0;
        for (i = 0; i < resync_count; i++)
        {
          if (resync_ops[i].whole_chan)
          {
            resync_ops[i].client_p = NULL;
            resync_ops[i].whole_chan = 0;
          }
        }
        return;
      }
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = CAP_AOPS;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ALL_MEMBERS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = NULL;

    resync_ops[resync_count].client_p = NULL;
    resync_ops[resync_count].dir = MODE_DEL;
    resync_ops[resync_count].c = c;
    resync_ops[resync_count].send = 0;
    resync_ops[resync_count].whole_chan = 1;
    resync_ops[resync_count++].sync = 0;
  }
  else if ((dir == MODE_DEL) && (chptr->mode.mode & MODE_HIDEOPS))
  {
    chptr->mode.mode &= ~MODE_HIDEOPS;
    
    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c)
      {
        /* + and - make 0, so return... */
        mode_changes_plus[i].letter = 0;
        for (i = 0; i < resync_count; i++)
        {
          if (resync_ops[i].whole_chan)
          {
            resync_ops[i].client_p = NULL;
            resync_ops[i].whole_chan = 0;
          }
        }
        return;
      }
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = CAP_AOPS;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ALL_MEMBERS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = NULL;

    resync_ops[resync_count].client_p = NULL;
    resync_ops[resync_count].dir = MODE_ADD;
    resync_ops[resync_count].c = c;
    resync_ops[resync_count].send = 0;
    resync_ops[resync_count].whole_chan = 1;
    resync_ops[resync_count++].sync = 0;
  }
}

static void
chm_ban(struct Client *client_p, struct Client *source_p,
        struct Channel *chptr, int parc, int *parn,
        char **parv, int *errors, int alev, int dir, char c, void *d,
        const char *chname)
{
  int i;
  char *mask;
  char *raw_mask;
  dlink_node *ptr;
  struct Ban *banptr;

  if (dir == 0 || parc <= *parn)
  {
    if ((*errors & SM_ERR_RPL_B) != 0)
      return;
    *errors |= SM_ERR_RPL_B;

    if ((chptr->mode.mode & MODE_HIDEOPS) && (alev < CHACCESS_HALFOP))
      for (ptr = chptr->banlist.head; ptr; ptr = ptr->next)
      {
        banptr = ptr->data;
        sendto_one(client_p, form_str(RPL_BANLIST),
                   me.name, client_p->name, chname,
                   banptr->banstr, me.name, banptr->when);
      }
    else
      for (ptr = chptr->banlist.head; ptr; ptr = ptr->next)
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

  if (alev < CHACCESS_HALFOP)
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if (MyClient(source_p) && mode_limit++ > 4)
    return;

  raw_mask = parv[(*parn)++];
  
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);
    
  /* Cant do this - older servers dont.. it WILL cause a desync, but should
   * be limited by our input buffer anyway --fl_
   * 
   * if (strlen(mask) > HOSTLEN+NICKLEN+USERLEN)
   *   return;
   */

  /* if we're adding a NEW id */
  if ((dir == MODE_ADD) && (add_id(source_p, chptr, mask, CHFL_BAN) == 0))
  {
    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c &&
          !irccmp(mode_changes_minus[i].arg, mask))
      {
        mode_changes_minus[i].letter = 0;
        return;
      }
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = 0;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ALL_MEMBERS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_BAN) != 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if (del_id(chptr, raw_mask, CHFL_BAN) != 0)
      {
        /* nope */
        return;
      }
      mask = raw_mask;
    }

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c &&
          !irccmp(mode_changes_plus[i].arg, mask))
      {
        mode_changes_plus[i].letter = 0;
        return;
      }
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = 0;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ALL_MEMBERS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = mask;
  }
}

static void
chm_except(struct Client *client_p, struct Client *source_p,
           struct Channel *chptr, int parc, int *parn,
           char **parv, int *errors, int alev, int dir, char c, void *d,
           const char *chname)
{
  int i;
  dlink_node *ptr;
  struct Ban *banptr;
  char *mask, *raw_mask;

  if (alev < CHACCESS_HALFOP)
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

    for (ptr = chptr->exceptlist.head; ptr; ptr = ptr->next)
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

  if (MyClient(source_p) && mode_limit++ > 4)
    return;

  raw_mask = parv[(*parn)++];
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);

  /* will cause a desync, cant be done */
#if 0
  if (strlen(mask) > NICKLEN+USERLEN+HOSTLEN)
    return;
#endif    

  /* If we're adding a NEW id */
  if ((dir == MODE_ADD) && add_id(source_p, chptr, mask, CHFL_EXCEPTION) == 0)
  {
    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c &&
          !irccmp(mode_changes_minus[i].arg, mask))
      {
        mode_changes_minus[i].letter = 0;
        return;
      }
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = CAP_EX;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ONLY_CHANOPS_HALFOPS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_EXCEPTION) != 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if (del_id(chptr, raw_mask, CHFL_EXCEPTION) != 0)
      {
        /* nope */
        return;
      }
      mask = raw_mask;
    }

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c &&
          !irccmp(mode_changes_plus[i].arg, mask))
      {
        mode_changes_plus[i].letter = 0;
        return;
      }
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = CAP_EX;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = mask;
  }
}

static void
chm_invex(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  int i;
  char *mask, *raw_mask;
  dlink_node *ptr;
  struct Ban *banptr;

  if (alev < CHACCESS_HALFOP)
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

    for (ptr = chptr->invexlist.head; ptr; ptr = ptr->next)
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

  if (MyClient(source_p) && mode_limit++ > 4)
    return;

  raw_mask = parv[(*parn)++];
  if (IsServer(client_p))
    mask = raw_mask;
  else
    mask = pretty_mask(raw_mask);

  /* will cause a desync, cant be done */
#if 0  
  if (strlen(mask) > NICKLEN+USERLEN+HOSTLEN)
    return;
#endif    

  if ((dir == MODE_ADD) && add_id(source_p, chptr, mask, CHFL_INVEX) == 0)
  {
    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c &&
          !irccmp(mode_changes_minus[i].arg, mask))
      {
        /* It cancels out, just return... */
        mode_changes_minus[i].letter = 0;
        return;
      }
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = CAP_IE;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ONLY_CHANOPS_HALFOPS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = mask;
  }
  else if (dir == MODE_DEL)
  {
    if (del_id(chptr, mask, CHFL_INVEX) != 0)
    {
      /* mask isn't a valid ban, check raw_mask */
      if (del_id(chptr, raw_mask, CHFL_INVEX) != 0)
      {
        /* nope */
        return;
      }
      mask = raw_mask;
    }

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c &&
          !irccmp(mode_changes_plus[i].arg, mask))
      {
        /* It cancels out, so just return... */
        mode_changes_plus[i].letter = 0;
        return;
      }
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = CAP_IE;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = mask;
  }
}

static void
chm_op(struct Client *client_p, struct Client *source_p,
       struct Channel *chptr, int parc, int *parn,
       char **parv, int *errors, int alev, int dir, char c, void *d,
       const char *chname)
{
  int i;

  /* Note on was_opped etc...
   * The was_opped variable is set to 1 if they were set -o in this mode,
   * was implies that previously they were +o, so we should not send a
   * +o out. wasnt_opped is set to 1 if they were +o in this mode, which
   * implies they were previously -o so we don't send -o out. Note that
   * was_hopped and was_voiced are along with is_half_op/is_voiced to
   * decide if we need to -h/-v first to support servers/clients that
   * allow more than one +h/+v/+o at a time.
   * -A1kmm.
   */

  int was_opped = 0, was_hopped = 0, was_voiced = 0, wasnt_opped = 0;
  int wasnt_hopped = 0, wasnt_voiced = 0, t_op = 0, t_voice = 0, t_hop = 0;
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

  opnick = parv[(*parn)++];

  if ((targ_p = find_client(opnick)) == NULL)
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
                 source_p->name, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
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

  if ((t_op = is_chan_op(chptr, targ_p)) != 0)
    ;
  else if ((t_hop = is_half_op(chptr, targ_p)) != 0)
    ;
#ifndef REQUIRE_OANDV
  else
#endif
    t_voice = is_voiced(chptr, targ_p);

  if (((dir == MODE_ADD) && t_op) ||
      ((dir == MODE_DEL) && !t_op))
    return;

  if (MyClient(source_p) && (mode_limit++ > 4))
    return;

  /* Cancel mode changes... */

  for (i = 0; i < mode_count_plus; i++)
  {
    if ((mode_changes_plus[i].letter == 'o' ||
         mode_changes_plus[i].letter == 'h'
#ifndef REQUIRE_OANDV
         || mode_changes_plus[i].letter == 'v'
#endif
       )
        && !irccmp(mode_changes_plus[i].arg, opnick))
    {
      if (mode_changes_plus[i].letter == 'o')
        wasnt_opped = 1;
      else if (mode_changes_plus[i].letter == 'h')
        wasnt_hopped = 1;
      else if (mode_changes_plus[i].letter == 'v')
        wasnt_voiced = 1;
#ifdef REQUIRE_OANDV
      if (mode_changes_plus[i].letter != 'v')
#endif
        mode_changes_plus[i].letter = 0;
    }
  }

  for (i = 0; i < mode_count_minus; i++)
  {
    if ((mode_changes_minus[i].letter == 'o' ||
         mode_changes_minus[i].letter == 'h'
#ifndef REQUIRE_OANDV
         || mode_changes_minus[i].letter == 'v'
#endif
        )
        && !irccmp(mode_changes_minus[i].arg, opnick))
    {
      if (mode_changes_minus[i].letter == 'o')
      {
        mode_changes_minus[i].letter = 0; /* clear the mode */
        was_opped = 1;
      }
      else if (mode_changes_minus[i].letter == 'v')
        was_voiced = 1;
      else if (mode_changes_minus[i].letter == 'h')
        was_hopped = 1;
    }
  }

  if (dir == MODE_ADD)
  {
#ifndef REQUIRE_OANDV
    if (!wasnt_voiced && t_voice)
    {
      mode_changes_minus[mode_count_minus].letter = 'v';
      mode_changes_minus[mode_count_minus].caps = 0;
      mode_changes_minus[mode_count_minus].nocaps = 0;
      mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;
    }
    else
#endif
    if (!wasnt_hopped && t_hop)
    {
      mode_changes_minus[mode_count_minus].letter = 'h';
      mode_changes_minus[mode_count_minus].caps = CAP_HOPS;
      mode_changes_minus[mode_count_minus].nocaps = 0;
      mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;
    }

#ifdef REQUIRE_OANDV
    if (t_voice)
      change_channel_membership(chptr, &chptr->chanops_voiced,
                                &chptr->locchanops_voiced, targ_p);
    else
#endif
      change_channel_membership(chptr, &chptr->chanops,
                                &chptr->locchanops, targ_p);

    if (!was_opped)
    {
      mode_changes_plus[mode_count_plus].letter = c;
      mode_changes_plus[mode_count_plus].caps = 0;
      mode_changes_plus[mode_count_plus].nocaps = 0;
      mode_changes_plus[mode_count_plus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_plus[mode_count_plus].id = targ_p->user->id;
      mode_changes_plus[mode_count_plus++].arg = targ_p->name;
    }

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
      {
        if (resync_ops[i].dir == dir || resync_ops[i].c == c)
          resync_ops[i].client_p = NULL;
        else
          resync_ops[i].sync = 0;
        resync_ops[i].send = 1;
      }
    }

    if (was_opped == 0 && MyConnect(targ_p) &&
        chptr->mode.mode & MODE_HIDEOPS)
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].send = 0;
      resync_ops[resync_count].whole_chan = 0;
      /* -h+o or the implicit form of this shouldn't resync... */
      resync_ops[resync_count++].sync = (was_hopped || t_hop) ? 0 : 1;
    }
  }
  else
  {
    /* Converting -o to -h... */
    if (t_hop)
    {
      c = 'h';

      /* This code previously only allowed us to convert if it was a local
       * client, however, we may convert halfops to ops when sending to
       * old servers (assuming the target isn't on the older server),
       * so we should accept this from other servers too.
       *
       * -David-T
       */

      /* check if this just canceled out an earlier mode we cleared */
      if (!wasnt_hopped)
      {
        mode_changes_minus[mode_count_minus].letter = 'h';
        mode_changes_minus[mode_count_minus].caps = CAP_HOPS;
        mode_changes_minus[mode_count_minus].nocaps = 0;
        mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
        mode_changes_minus[mode_count_minus].id = NULL;
        mode_changes_minus[mode_count_minus++].arg = targ_p->name;

#ifdef BOUNCE_BAD_HOPS
        mode_changes_minus[mode_count_minus].letter = 'o';
        mode_changes_minus[mode_count_minus].caps = 0;
        mode_changes_minus[mode_count_minus].nocaps = CAP_HOPS;
        mode_changes_minus[mode_count_minus].mems = ONLY_SERVERS;
        mode_changes_minus[mode_count_minus].id = NULL;
        mode_changes_minus[mode_count_minus++].arg = targ_p->name;
#endif
      }
    }
    else
    {
      /* check if this just canceled out an earlier mode we cleared */
      if (!wasnt_opped)
      {
        mode_changes_minus[mode_count_minus].letter = c;
        mode_changes_minus[mode_count_minus].caps = 0;
        mode_changes_minus[mode_count_minus].nocaps = 0;
        mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
        mode_changes_minus[mode_count_minus].id = NULL;
        mode_changes_minus[mode_count_minus++].arg = targ_p->name;
      }
    }

#ifdef REQUIRE_OANDV
    if (!t_hop && t_voice)
      change_channel_membership(chptr, &chptr->voiced,
                                &chptr->locvoiced, targ_p);
    else
#endif
      change_channel_membership(chptr, &chptr->peons,
                                &chptr->locpeons, targ_p);

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
        resync_ops[i].client_p = NULL;
    }
    if (!wasnt_opped && MyClient(targ_p) &&
        chptr->mode.mode & MODE_HIDEOPS)
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].send = 1;
      resync_ops[resync_count].whole_chan = 0;
      resync_ops[resync_count++].sync = 1;
    }
  }
}

static void
chm_halfop(struct Client *client_p, struct Client *source_p,
           struct Channel *chptr, int parc, int *parn,
           char **parv, int *errors, int alev, int dir, char c, void *d,
           const char *chname)
{
  int i, was_opped = 0, was_hopped = 0, was_voiced = 0, wasnt_hopped = 0;
  int wasnt_voiced = 0, t_voice = 0, t_op = 0, t_hop = 0;
  char *opnick;
  struct Client *targ_p;

/* *sigh* - dont allow halfops to set +/-h, they could fully control a
 * channel if there were no ops - it doesnt solve anything.. MODE_PRIVATE
 * when used with MODE_SECRET is paranoid - cant use +p
 *
 * it needs to be optional per channel - but not via +p, that or remove
 * paranoid.. -- fl_
 *
 * +p means paranoid, it is useless for anything else on modern IRC, as
 * list isn't really usable. If you want to have a private channel these
 * days, you set it +s. Halfops can no longer remove simple modes when
 * +p is set(although they can set +p) so it is safe to use this to
 * control whether they can (de)halfop...
 */
  if (alev <
      ((chptr->mode.mode & MODE_PRIVATE) ?
        CHACCESS_CHANOP : CHACCESS_HALFOP))
  {
    if (!(*errors & SM_ERR_NOOPS))
      sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
                 source_p->name, chname);
    *errors |= SM_ERR_NOOPS;
    return;
  }

  if ((dir == MODE_QUERY) || parc <= *parn)
    return;

  if (MyClient(source_p) && alev < CHACCESS_CHANOP && mode_limit++ > 4)
    return;

  opnick = parv[(*parn)++];

  if ((targ_p = find_client(opnick)) == NULL)
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
                 source_p->name, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
    return;
  }

  if ((t_op = is_chan_op(chptr, targ_p)))
    ;
  else if ((t_hop = is_half_op(chptr, targ_p)))
    ;
#ifdef REQUIRE_OANDV
  else
#endif
    t_voice = is_voiced(chptr, targ_p);

  /* Ignore +/-h on ops, from local clients.. */
  if (MyClient(source_p) && t_op)
    return;

  if (!IsMember(targ_p, chptr))
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL), me.name,
                 source_p->name, chname, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
    return;
  }

  if (((dir == MODE_ADD) && (t_hop || t_op)) ||
      ((dir == MODE_DEL) && !t_hop))
    return;

#ifdef BOUNCE_BAD_HOPS
  if (!MyClient(targ_p) && !IsCapable(targ_p->from, CAP_HOPS))
  {
    if (IsServer(client_p))
    {
      mode_bounces[bounce_count].letter = 'h';
      mode_bounces[bounce_count].dir = MODE_DEL;
      mode_bounces[bounce_count].id = targ_p->user->id;
      mode_bounces[bounce_count++].arg = targ_p->name;
    }

    if (IsClient(source_p))
      sendto_one(source_p,
                 ":%s NOTICE %s :Unable to halfop %s"
                 " - server does not support halfops.",
                 me.name, source_p->name, targ_p->name);
    return;
  }
#endif

  /* Cancel out all other mode changes... */
  for (i = 0; i < mode_count_plus; i++)
  {
    if ((mode_changes_plus[i].letter == 'o' ||
         mode_changes_plus[i].letter == 'h' ||
         mode_changes_plus[i].letter == 'v')
        && !irccmp(mode_changes_plus[i].arg, opnick))
    {
      if (mode_changes_plus[i].letter == 'h')
      {
        wasnt_hopped = 1;
        if (dir == MODE_ADD)
          mode_changes_plus[i].letter = 0;
      }
      if (mode_changes_plus[i].letter == 'v')
        wasnt_voiced = 1;
      /* +h-h is the only case we should get here, wipe it... */
      mode_changes_plus[i].letter = 0;
    }
  }

  for (i = 0; i < mode_count_minus; i++)
  {
    if ((mode_changes_minus[i].letter == 'o' ||
         mode_changes_minus[i].letter == 'h' ||
         mode_changes_minus[i].letter == 'v')
        && !irccmp(mode_changes_minus[i].arg, opnick))
    {
      if (mode_changes_minus[i].letter == 'o')
        was_opped = 1;
      else if (mode_changes_minus[i].letter == 'h')
        was_hopped = 1;
      else if (mode_changes_minus[i].letter == 'v')
        was_voiced = 1;
      /* -h+h or -v+h or +v+h should be wiped, -o+h should not. */
      if (mode_changes_minus[i].letter != 'o')
        mode_changes_minus[i].letter = 0;
    }
  }

  if (dir == MODE_ADD)
  {
    if (!wasnt_voiced && t_voice)
    {
      mode_changes_minus[mode_count_minus].letter = 'v';
      mode_changes_minus[mode_count_minus].caps = 0;
      mode_changes_minus[mode_count_minus].nocaps = 0;
      mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;
    }

    if (!was_hopped)
    {
      mode_changes_plus[mode_count_plus].letter = c;
      mode_changes_plus[mode_count_plus].caps = CAP_HOPS;
      mode_changes_plus[mode_count_plus].nocaps = 0;
      mode_changes_plus[mode_count_plus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_plus[mode_count_plus].id = targ_p->user->id;
      mode_changes_plus[mode_count_plus++].arg = targ_p->name;

#ifdef BOUNCE_BAD_HOPS
      mode_changes_plus[mode_count_plus].letter = 'o';
      mode_changes_plus[mode_count_plus].caps = 0;
      mode_changes_plus[mode_count_plus].nocaps = CAP_HOPS;
      mode_changes_plus[mode_count_plus].mems = ONLY_SERVERS;
      mode_changes_plus[mode_count_plus].id = targ_p->user->id;
      mode_changes_plus[mode_count_plus++].arg = targ_p->name;
#endif
      change_channel_membership(chptr, &chptr->halfops,
                                &chptr->lochalfops, targ_p);
    }

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
      {
        if (resync_ops[i].dir == dir || resync_ops[i].c == c)
          resync_ops[i].client_p = NULL;
        else
          resync_ops[i].sync = 0;
        resync_ops[i].send = 0;
      }
    }

    if (!was_hopped && MyClient(targ_p) && (chptr->mode.mode & MODE_HIDEOPS))
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].send = 0;
      resync_ops[resync_count].whole_chan = 0;
      /* -o+h shouldn't send massdeop... */
      resync_ops[resync_count++].sync = was_opped ? 0 : 1;
    }
  }
  else 
  { /* MODE_DEL */
    if (wasnt_hopped == 0)
    {
      mode_changes_minus[mode_count_minus].letter = 'h';
      mode_changes_minus[mode_count_minus].caps = CAP_HOPS;
      mode_changes_minus[mode_count_minus].nocaps = 0;
      mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;

#ifdef BOUNCE_BAD_HOPS
      mode_changes_minus[mode_count_minus].letter = 'o';
      mode_changes_minus[mode_count_minus].caps = 0;
      mode_changes_minus[mode_count_minus].nocaps = CAP_HOPS;
      mode_changes_minus[mode_count_minus].mems = ONLY_SERVERS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;
#endif
    }

    change_channel_membership(chptr, &chptr->peons, &chptr->locpeons, targ_p);


    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
        resync_ops[i].client_p = NULL;
    }
    if (!wasnt_hopped && MyClient(targ_p) &&
        chptr->mode.mode & MODE_HIDEOPS)
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].send = 1;
      resync_ops[resync_count].whole_chan = 0;
      resync_ops[resync_count++].sync = 1;
    }
  }
}

static void
chm_voice(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  int i, was_hopped = 0, was_opped = 0, was_voiced = 0, wasnt_voiced = 0;
  int t_op = 0, t_voice = 0, t_hop = 0;
  char *opnick;
  struct Client *targ_p;

  if (alev < CHACCESS_HALFOP)
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

  if ((targ_p = find_client(opnick)) == NULL)
  {
    if (!(*errors & SM_ERR_NOTONCHANNEL))
      sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
                 source_p->name, opnick);
    *errors |= SM_ERR_NOTONCHANNEL;
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

  if ((t_op = is_chan_op(chptr, targ_p)) != 0)
    t_op = 1;
  else if ((t_hop = is_half_op(chptr, targ_p)) != 0)
    t_hop = 1;
#ifndef REQUIRE_OANDV
  else
#endif
    t_voice = is_voiced(chptr, targ_p);

  /* Ignore +/-v on op... Now we need not worry about limiting the
   * number of +/-v...
   */
#if 0
  if (MyClient(source_p) && mode_limit++ > 4)
    return;
#endif

  if (
#ifndef REQUIRE_OANDV
      t_op ||
#endif
      t_hop ||
      (dir == MODE_ADD && t_voice) ||
      (dir == MODE_DEL && !t_voice))
    return;

  /* Remove other changes... */

  for (i = 0; i < mode_count_plus; i++)
  {
    if ((mode_changes_plus[i].letter == 'o' ||
         mode_changes_plus[i].letter == 'h' ||
         mode_changes_plus[i].letter == 'v')
        && !irccmp(mode_changes_plus[i].arg, opnick))
    {
      if (mode_changes_plus[i].letter == 'v')
      {
        wasnt_voiced = 1;
      }
#ifdef REQUIRE_OANDV
      if (mode_changes_plus[i].letter != 'o')
#endif
        mode_changes_plus[i].letter = 0;
    }
  }

  for (i = 0; i < mode_count_minus; i++)
  {
    if ((mode_changes_minus[i].letter == 'o' ||
         mode_changes_minus[i].letter == 'h' ||
         mode_changes_minus[i].letter == 'v')
        && !irccmp(mode_changes_minus[i].arg, opnick))
    {
      if (mode_changes_minus[i].letter == 'o')
        was_opped = 1;
      if (mode_changes_minus[i].letter == 'h')
        was_hopped = 1;
      if (mode_changes_minus[i].letter == 'v')
      {
        mode_changes_minus[i].letter = 0;
        was_voiced = 1;
      }
      /* No assignment to 0 here needed... -A1kmm */
    }
  }

  if (dir == MODE_ADD)
  {
#ifdef REQUIRE_OANDV
    if (t_op)
      change_channel_membership(chptr, &chptr->chanops_voiced,
                                &chptr->locchanops_voiced, targ_p);
    else
#endif
      change_channel_membership(chptr, &chptr->voiced,
                                &chptr->locvoiced, targ_p);
    if (was_voiced == 0)
    {
      mode_changes_plus[mode_count_plus].letter = c;
      mode_changes_plus[mode_count_plus].caps = 0;
      mode_changes_plus[mode_count_plus].nocaps = 0;
      mode_changes_plus[mode_count_plus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_plus[mode_count_plus].id = targ_p->user->id;
      mode_changes_plus[mode_count_plus++].arg = targ_p->name;
    }

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
      {
        if (resync_ops[i].dir == dir || resync_ops[i].c == c)
          resync_ops[i].client_p = NULL;
      }
    }

    if (was_voiced == 0 && MyConnect(targ_p) &&
        chptr->mode.mode & MODE_HIDEOPS)
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].sync = 0;
      resync_ops[resync_count].whole_chan = 0;
      /* voiced users don't see mode changes, so send even on +v... */
      resync_ops[resync_count++].send = 1;
    }
  }
  else
  {
#ifdef REQUIRE_OANDV
    if (t_op)
      change_channel_membership(chptr, &chptr->chanops,&chptr->locchanops,
                                targ_p);
    else
#endif
      change_channel_membership(chptr, &chptr->peons, &chptr->locpeons,
                                targ_p);

    /* Don't send redundant modes on +v-v nick nick */

    if (!wasnt_voiced)
    {
      mode_changes_minus[mode_count_minus].letter = 'v';
      mode_changes_minus[mode_count_minus].caps = 0;
      mode_changes_minus[mode_count_minus].nocaps = 0;
      mode_changes_minus[mode_count_minus].mems = ONLY_CHANOPS_HALFOPS;
      mode_changes_minus[mode_count_minus].id = targ_p->user->id;
      mode_changes_minus[mode_count_minus++].arg = targ_p->name;
    }

    resync_ops[resync_count].sync = 0;

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == targ_p)
        resync_ops[i].client_p = NULL;
    }

    if (was_voiced == 0 && MyConnect(targ_p) &&
        chptr->mode.mode & MODE_HIDEOPS)
    {
      resync_ops[resync_count].client_p = targ_p;
      resync_ops[resync_count].dir = dir;
      resync_ops[resync_count].c = c;
      resync_ops[resync_count].send = 1;
      resync_ops[resync_count].whole_chan = 0;
      resync_ops[resync_count++].sync = 0;
    }
  }
}

static void
chm_limit(struct Client *client_p, struct Client *source_p,
          struct Channel *chptr, int parc, int *parn,
          char **parv, int *errors, int alev, int dir, char c, void *d,
          const char *chname)
{
  int i, limit;
  char *lstr;

  if (alev < CHACCESS_HALFOP)
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

    for (i = 0; i < mode_count_minus; i++)
      if (mode_changes_minus[i].letter == c)
        mode_changes_minus[i].letter = 0;

    for (i = 0; i < mode_count_plus; i++)
      if (mode_changes_plus[i].letter == c)
        mode_changes_plus[i].letter = 0;

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = 0;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ALL_MEMBERS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = lstr;

    chptr->mode.limit = limit;
  }
  else if (dir == MODE_DEL)
  {
    if (!chptr->mode.limit)
      return;

    chptr->mode.limit = 0;

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c)
      {
        mode_changes_plus[i].letter = 0;
        return;
      }
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = 0;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ALL_MEMBERS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = NULL;
  }
}

static void
chm_key(struct Client *client_p, struct Client *source_p,
        struct Channel *chptr, int parc, int *parn,
        char **parv, int *errors, int alev, int dir, char c, void *d,
        const char *chname)
{
  int i;
  char *key;

  if (alev < CHACCESS_HALFOP)
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
    strncpy_irc(chptr->mode.key, key, KEYLEN - 1)[KEYLEN - 1] = 0;

    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == c)
        mode_changes_minus[i].letter = 0;
    }

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c)
        mode_changes_plus[i].letter = 0;
    }

    mode_changes_plus[mode_count_plus].letter = c;
    mode_changes_plus[mode_count_plus].caps = 0;
    mode_changes_plus[mode_count_plus].nocaps = 0;
    mode_changes_plus[mode_count_plus].mems = ALL_MEMBERS;
    mode_changes_plus[mode_count_plus].id = NULL;
    mode_changes_plus[mode_count_plus++].arg = key;
  }
  else if (dir == MODE_DEL)
  {
    if (!(*chptr->mode.key))
      return;

    *chptr->mode.key = 0;

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == c)
        mode_changes_plus[i].letter = 0;
    }

    mode_changes_minus[mode_count_minus].letter = c;
    mode_changes_minus[mode_count_minus].caps = 0;
    mode_changes_minus[mode_count_minus].nocaps = 0;
    mode_changes_minus[mode_count_minus].mems = ALL_MEMBERS;
    mode_changes_minus[mode_count_minus].id = NULL;
    mode_changes_minus[mode_count_minus++].arg = "*";
  }
}

struct ChannelMode
{
  void (*func) (struct Client *client_p, struct Client *source_p,
                struct Channel *chptr, int parc, int *parn, char **parv,
                int *errors, int alev, int dir, char c, void *d,
                const char *chname);
  void *d;
}
/* *INDENT-OFF* */
ModeTable[255] =
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
  {chm_hideops, NULL},                            /* a */
  {chm_ban, NULL},                                /* b */
  {chm_nosuch, NULL},                             /* c */
  {chm_nosuch, NULL},                             /* d */
  {chm_except, NULL},                             /* e */
  {chm_nosuch, NULL},                             /* f */
  {chm_nosuch, NULL},                             /* g */
  {chm_halfop, NULL},                             /* h */
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
 *         MODE_HALFOP for halfop level access,
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

  if (is_half_op(chptr, source_p))
    return CHACCESS_HALFOP;

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
 * send_cap_mode_changes() will loop the server list itself, because
 * at this point in time we have 4 capabs for channels, CAP_IE, CAP_EX,
 * CAP_AOPS, CAP_HOPS, and a server could support any number of these..
 * so we make the modebufs per server, tailoring them to each servers
 * specific demand.  Its not very pretty, but its one of the few realistic
 * ways to handle having this many capabs for channel modes.. --fl_
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

  mc = 0;
  nc = 0;
  pbl = 0;
  parabuf[0] = 0;
  if ((cap & CAP_UID) && source_p->user &&
      (source_p->user->id[0] == '.'))
    mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->user->id,
                     chptr->chname);
  else
    mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->name,
                     chptr->chname);

  if (mode_count_minus > 0)
  {
    modebuf[mbl++] = '-';
    modebuf[mbl] = 0;
  }

  /* loop the list of - modes we have */
  for (i = 0; i < mode_count_minus; i++)
  {
    /* if they dont support the cap we need, or they do support a cap they
     * cant have, then dont add it to the modebuf.. that way they wont see
     * the mode
     */
    if ((mode_changes_minus[i].letter == 0) ||
        ((cap & mode_changes_minus[i].caps) != mode_changes_minus[i].caps)
        || ((nocap & mode_changes_minus[i].nocaps)
            != mode_changes_minus[i].nocaps))
      continue;
    arg = "";
    if ((cap & CAP_UID) && mode_changes_minus[i].id)
      arg = mode_changes_minus[i].id;
    if (!*arg)
      arg = mode_changes_minus[i].arg;
    nc++;
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
        sendto_server(client_p, source_p, chptr, cap, nocap,
                      LL_ICHAN | LL_ICLIENT, "%s %s", modebuf, parabuf);
      nc = 0;
      mc = 0;
      if ((cap & CAP_UID) && source_p->user &&
          (source_p->user->id[0] == '.'))
        mbl = ircsprintf(modebuf, ":%s MODE %s -", source_p->user->id,
                         chptr->chname);
      else
        mbl = ircsprintf(modebuf, ":%s MODE %s -", source_p->name,
                         chptr->chname);
      pbl = 0;
      parabuf[0] = 0;
    }

    modebuf[mbl++] = mode_changes_minus[i].letter;
    modebuf[mbl] = 0;
    mc++;

    if (arg != NULL)
    {
      pbl = strlcat(parabuf, arg, MODEBUFLEN);
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (mode_count_plus > 0)
  {
    if (mbl > 0 && modebuf[mbl - 1] == '-')
      modebuf[mbl - 1] = '+';
    else
      modebuf[mbl++] = '+';

    modebuf[mbl] = 0;
  }

  /* loop the + modes */
  for (i = 0; i < mode_count_plus; i++)
  {
    /* same as above, check they support needed capabs, and dont have
     * capabs we dont want..
     */
    if (mode_changes_plus[i].letter == 0 ||
        (cap & mode_changes_plus[i].caps) != mode_changes_plus[i].caps
        || ((nocap & mode_changes_plus[i].nocaps) !=
            mode_changes_plus[i].nocaps))
      continue;

    arg = mode_changes_plus[i].arg;
    if ((cap & CAP_UID) && mode_changes_plus[i].id != NULL)
      arg = mode_changes_plus[i].id;
    nc++;

    if ((arg != NULL) && ((mc == MAXMODEPARAMS) || 
                          ((strlen(arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (mbl && modebuf[mbl - 1] == '+')
        modebuf[mbl - 1] = '\0';

      if (nc != 0)
        sendto_server(client_p, source_p, chptr, cap, nocap,
                      LL_ICLIENT, "%s %s", modebuf, parabuf);
      nc = 0;
      mc = 0;
      if ((cap & CAP_UID) && source_p->user &&
          (source_p->user->id[0] == '.'))
        mbl = ircsprintf(modebuf, ":%s MODE %s +", source_p->user->id,
                         chptr->chname);
      else
        mbl = ircsprintf(modebuf, ":%s MODE %s +", source_p->name,
                         chptr->chname);
      pbl = 0;
      parabuf[0] = 0;
    }

    modebuf[mbl++] = mode_changes_plus[i].letter;
    modebuf[mbl] = 0;
    mc++;

    if (mode_changes_plus[i].arg != NULL)
    {
      pbl = strlcat(parabuf, mode_changes_plus[i].arg, MODEBUFLEN);
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (mbl && modebuf[mbl - 1] == '+')
    modebuf[mbl - 1] = 0;

  if (nc != 0)
    sendto_server(client_p, source_p, chptr, cap, nocap,
                  LL_ICLIENT, "%s %s", modebuf, parabuf);
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
  int i, st, dir = MODE_QUERY;

  /* bail out if we have nothing to do... */
  if (!(mode_count_plus || mode_count_minus || bounce_count || resync_count))
    return;
  
  /* Send any resyncs that we need to send... */
  /* XXX - setting -a removes hideops, yet still needs a resync.. */

    for (i = 0; i < resync_count; i++)
    {
      if (resync_ops[i].client_p == NULL)
      {
        if (resync_ops[i].whole_chan)
          sync_channel_oplists(chptr, resync_ops[i].dir);
        else
          continue;
      }
      if (resync_ops[i].sync)
      {
        sync_oplists(chptr, resync_ops[i].client_p,
                     resync_ops[i].dir, chname);
      }
      if (resync_ops[i].dir == MODE_ADD && resync_ops[i].c == 'v')
        sendto_one(resync_ops[i].client_p, ":%s MODE %s +v %s", me.name,
                   chname, resync_ops[i].client_p->name);
      else if (resync_ops[i].send)
        sendto_one(resync_ops[i].client_p, ":%s MODE %s -%c %s", me.name,
                   chname, resync_ops[i].c, resync_ops[i].client_p->name);
    }

  /* Send all mode changes to the chanops/halfops, and even peons if
   * we are not +a...
   */
  st = (chptr->mode.mode & MODE_HIDEOPS) ? ONLY_CHANOPS_HALFOPS : ALL_MEMBERS;

  if (IsServer(source_p))
    mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
  else
    mbl = ircsprintf(modebuf, ":%s!%s@%s MODE %s ", source_p->name,
                     source_p->username, source_p->host, chname);

  pbl = 0;
  parabuf[0] = '\0';
  nc = 0;
  mc = 0;

  if (mode_count_minus > 0)
  {
    modebuf[mbl++] = '-';
    modebuf[mbl] = '\0';
  }

  for (i = 0; i < mode_count_minus; i++)
  {
    if (mode_changes_minus[i].letter == 0 ||
        mode_changes_minus[i].mems == NON_CHANOPS ||
        mode_changes_minus[i].mems == ONLY_SERVERS)
      continue;
    nc++;
    if (mode_changes_minus[i].arg != NULL &&
        ((mc == MAXMODEPARAMS)  || 
        ((strlen(mode_changes_minus[i].arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (mbl && modebuf[mbl - 1] == '-')
        modebuf[mbl - 1] = '\0';

      if (nc != 0)
        sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

      nc = 0;
      mc = 0;

      if (IsServer(source_p))
        mbl = ircsprintf(modebuf, ":%s MODE %s -", me.name, chname);
      else
        mbl = ircsprintf(modebuf, ":%s!%s@%s MODE %s -", source_p->name,
                   source_p->username, source_p->host, chname);

      pbl = 0;
      parabuf[0] = '\0';
    }

    modebuf[mbl++] = mode_changes_minus[i].letter;
    modebuf[mbl] = '\0';
    mc++;

    if (mode_changes_minus[i].arg != NULL)
    {
      pbl = strlen(strcat(parabuf, mode_changes_minus[i].arg));
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (mode_count_plus > 0)
  {
    if (mbl > 0 && modebuf[mbl - 1] == '-')
      modebuf[mbl - 1] = '+';
    else
      modebuf[mbl++] = '+';
    modebuf[mbl] = '\0';
  }

  for (i = 0; i < mode_count_plus; i++)
  {
    if (mode_changes_plus[i].letter == 0 ||
        mode_changes_plus[i].mems == NON_CHANOPS ||
        mode_changes_plus[i].mems == ONLY_SERVERS)
      continue;
    nc++;
    if (mode_changes_plus[i].arg != NULL &&
        ((mc == MAXMODEPARAMS) ||
         ((strlen(mode_changes_plus[i].arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (mbl && modebuf[mbl - 1] == '+')
        modebuf[mbl - 1] = '\0';

      if (nc != 0)
        sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

      nc = 0;
      mc = 0;

      if (IsServer(source_p))
        mbl = ircsprintf(modebuf, ":%s MODE %s +", me.name, chname);
      else
        mbl = ircsprintf(modebuf, ":%s!%s@%s MODE %s +", source_p->name,
                   source_p->username, source_p->host, chname);

      pbl = 0;
      parabuf[0] = '\0';
    }

    modebuf[mbl++] = mode_changes_plus[i].letter;
    modebuf[mbl] = '\0';
    mc++;

    if (mode_changes_plus[i].arg != NULL)
    {
      pbl = strlen(strcat(parabuf, mode_changes_plus[i].arg));
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (pbl && parabuf[pbl - 1] == '+')
    parabuf[pbl - 1] = '\0';

  if (nc != 0)
    sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

  nc = 0;
  mc = 0;

  /* If peons were missed out above, send to them now... */
  if (chptr->mode.mode & MODE_HIDEOPS)
  {
    st = NON_CHANOPS;
    mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);
    pbl = 0;
    parabuf[0] = '\0';

    if (mode_count_minus > 0)
    {
      modebuf[mbl++] = '-';
      modebuf[mbl] = '\0';
    }

    for (i = 0; i < mode_count_minus; i++)
    {
      if (mode_changes_minus[i].letter == 0 ||
          mode_changes_minus[i].mems != ALL_MEMBERS ||
          mode_changes_minus[i].mems == ONLY_SERVERS)
        continue;

      nc++;

      if (mode_changes_minus[i].arg != NULL &&
          ((mc == MAXMODEPARAMS) ||
           ((strlen(mode_changes_minus[i].arg) + mbl + pbl + 2) > BUFSIZE)))
      {
        if (mbl && modebuf[mbl - 1] == '-')
          modebuf[mbl - 1] = '\0';

        if (nc != 0)
          sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

        nc = 0;
        mc = 0;
        
        mbl = ircsprintf(modebuf, ":%s MODE %s -", me.name, chname);
        pbl = 0;
        parabuf[0] = '\0';
      }

      modebuf[mbl++] = mode_changes_minus[i].letter;
      modebuf[mbl] = '\0';
      mc++;

      if (mode_changes_minus[i].arg != NULL)
      {
        pbl = strlen(strcat(parabuf, mode_changes_minus[i].arg));
        parabuf[pbl++] = ' ';
        parabuf[pbl] = '\0';
      }
    }

    if (mode_count_plus > 0)
    {
      if (mbl > 0 && modebuf[mbl - 1] == '-')
        modebuf[mbl - 1] = '+';
      else
        modebuf[mbl++] = '+';

      modebuf[mbl] = '\0';
    }

    for (i = 0; i < mode_count_plus; i++)
    {
      if (mode_changes_plus[i].letter == 0 ||
          mode_changes_plus[i].mems != ALL_MEMBERS ||
          mode_changes_plus[i].mems == ONLY_SERVERS)
        continue;

      nc++;

      if (mode_changes_plus[i].arg != NULL &&
          ((mc == MAXMODEPARAMS) ||
           ((strlen(mode_changes_plus[i].arg) + mbl + pbl + 2) > BUFSIZE)))
      {
        if (mbl && modebuf[mbl - 1] == '+')
          modebuf[mbl - 1] = '\0';

        if (nc != 0)
          sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);

        nc = 0;
        mc = 0;
        mbl = ircsprintf(modebuf, ":%s MODE %s +", me.name, chname);
        pbl = 0;
        parabuf[0] = '\0';
      }

      modebuf[mbl++] = mode_changes_plus[i].letter;
      modebuf[mbl] = '\0';
      mc++;

      if (mode_changes_plus[i].arg != NULL)
      {
        pbl = strlen(strcat(parabuf, mode_changes_plus[i].arg));
        parabuf[pbl++] = ' ';
        parabuf[pbl] = '\0';
      }
    }

    if (pbl && parabuf[pbl - 1] == '+')
      parabuf[pbl - 1] = '\0';

    if (nc != 0)
      sendto_channel_local(st, chptr, "%s %s", modebuf, parabuf);
  }

  /* Bounce modes to client_p */
  nc = 0;
  if (bounce_count)
  {
    mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);

    pbl = 0;
    parabuf[0] = '\0';
    mc = 0;
    dir = MODE_QUERY;
  }

  for (i = 0; i < bounce_count; i++)
  {
    if (mode_bounces[i].letter == 0)
      continue;

    nc++;
    if (mode_bounces[i].arg != NULL &&
        ((mc == MAXMODEPARAMS)  || 
        ((strlen(mode_bounces[i].arg) + mbl + pbl + 2) > BUFSIZE)))
    {
      if (nc != 0)
        sendto_one(client_p, "%s %s", modebuf, parabuf);

      nc = 0;
      mc = 0;

      mbl = ircsprintf(modebuf, ":%s MODE %s ", me.name, chname);

      pbl = 0;
      parabuf[0] = '\0';
    }

    if (dir != mode_bounces[i].dir)
    {
      modebuf[mbl++] = (mode_bounces[i].dir == MODE_ADD) ? '+' : '-';
      dir = mode_bounces[i].dir;
    }
    modebuf[mbl++] = mode_bounces[i].letter;
    modebuf[mbl] = '\0';
    mc++;

    if (mode_bounces[i].arg != NULL)
    {
      pbl = strlen(strcat(parabuf, mode_bounces[i].arg));
      parabuf[pbl++] = ' ';
      parabuf[pbl] = '\0';
    }
  }

  if (nc != 0)
    sendto_one(client_p, "%s %s", modebuf, parabuf);

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
  mode_count_plus = 0;
  mode_count_minus = 0;
  bounce_count = 0;
  resync_count = 0;
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

  send_mode_changes(client_p, source_p, chptr, chname);
}

/*
 * set_channel_mode_flags
 *
 * inputs       - pointer to array of strings for chanops, voiced,
 *                halfops,peons
 *              - pointer to channel
 *              - pointer to source
 * output       - none
 * side effects -
 */
void
set_channel_mode_flags(char flags_ptr[NUMLISTS][2], struct Channel *chptr,
                       struct Client *source_p)
{
  if (chptr->mode.mode & MODE_HIDEOPS && !is_any_op(chptr, source_p))
  {
    flags_ptr[0][0] = '\0';
    flags_ptr[1][0] = '\0';
    flags_ptr[2][0] = '\0';
    flags_ptr[3][0] = '\0';
#ifdef REQUIRE_OANDV
    flags_ptr[4][0] = '\0';
#endif
  }
  else
  {
    flags_ptr[0][0] = '@';
    flags_ptr[1][0] = '%';
    flags_ptr[2][0] = '+';
    flags_ptr[3][0] = '\0';
#ifdef REQUIRE_OANDV
    flags_ptr[4][0] = '@';
#endif

    flags_ptr[0][1] = '\0';
    flags_ptr[1][1] = '\0';
    flags_ptr[2][1] = '\0';
#ifdef REQUIRE_OANDV
    flags_ptr[4][1] = '\0';
#endif
  }
}


/*
 * sync_oplists
 *
 * inputs       - pointer to channel
 *              - pointer to client
 * output       - none
 * side effects - Sends MODE +o/+h/+v list to user
 *                (for +a channels)
 */
void
sync_oplists(struct Channel *chptr, struct Client *target_p,
             int dir, const char *name)
{
  send_oplist(name, target_p, &chptr->chanops, "o", dir);
  send_oplist(name, target_p, &chptr->halfops, "h", dir);
  send_oplist(name, target_p, &chptr->voiced, "v", dir);
}

static void
send_oplist(const char *chname, struct Client *client_p,
            dlink_list * list, char *prefix, int dir)
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

  for (ptr = list->head; ptr && ptr->data; ptr = ptr->next)
  {
    if (cur_modes == 0)
    {
      mcbuf[cur_modes++] = ((dir == MODE_ADD) ? '+' : '-');
    }

    target_p = ptr->data;
    /* Don't include this client... */
    if (target_p == client_p)
      continue;

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
  dlink_list *list;
  struct Client *target_p;

  list = &chptr->locpeons;
  for (ptr = list->head; ptr && ptr->data; ptr = ptr->next)
  {
    target_p = ptr->data;
    
    sync_oplists(chptr, target_p, dir, RootChan(chptr)->chname);
  }
  
  list = &chptr->locvoiced;
  for (ptr = list->head; ptr && ptr->data; ptr = ptr->next)
  {
    target_p = ptr->data;
    
    sync_oplists(chptr, target_p, dir, RootChan(chptr)->chname);
  }
}

#ifdef INTENSIVE_DEBUG
/* void do_channel_integrity_check(void)
 * Input: None.
 * Output: None.
 * Side-effects: Asserts a number of fundamental assumptions.
 * Note: This is a cpu intensive debug function. Call only when doing
 *       debugging of the channel code, and only on fairly small networks.
 */
void
do_channel_integrity_check(void)
{
  dlink_node *ptr = NULL;
  struct Client *cl;
  struct Channel *ch;
  for (cl=GlobalClientList; cl; cl=cl->next)
  {
    if (!IsRegisteredUser(cl) || IsDead(cl))
      continue;
    for (ptr=cl->user->channel.head; ptr; ptr=ptr->next)
    {
      dlink_node *ptr2;
      int matched = 0, matched_local;
      ch = (struct Channel*)ptr->data;
      if (!MyConnect(cl))
        matched_local = -1;
      else
        matched_local = 0;
      /* Make sure that they match once, and only once... */
#define SEARCH_LIST(listname) \
      for (ptr2=ch->listname.head; ptr2; ptr2=ptr2->next) \
        if (ptr2->data == cl) \
        { \
          assert(matched == 0); \
          matched = -1; \
        } \
      for (ptr2=ch->loc ## listname.head; ptr2; ptr2=ptr2->next) \
        if (ptr2->data == cl) \
        { \
          assert(matched_local == 0); \
          matched_local = -1; \
        }
      SEARCH_LIST(chanops)
#ifdef REQUIRE_OANDV
      SEARCH_LIST(chanops_voiced)
#endif
      SEARCH_LIST(halfops)
      SEARCH_LIST(voiced)
      SEARCH_LIST(peons)
#undef SEARCH_LIST
      assert(matched);
      assert(matched_local);
    }
  }
  for (ch=GlobalChannelList; ch; ch=NULL /*ch->nextch */)
  {
#define SEARCH_LIST(listname) \
    for (ptr=ch->listname.head; ptr; ptr=ptr->next) \
    { \
      int matched = 0; \
      dlink_node *ptr2; \
      cl = (struct Client*)ptr->data; \
      for (ptr2=cl->user->channel.head; ptr2; ptr2=ptr2->next) \
        if (ptr2->data == ch) \
        { \
          assert(matched == 0); \
          matched = -1; \
        } \
      assert(matched); \
    }
    SEARCH_LIST(chanops)
    SEARCH_LIST(halfops)
#ifdef REQUIRE_OANDV
    SEARCH_LIST(chanops_voiced)
#endif
    SEARCH_LIST(voiced)
    SEARCH_LIST(peons)
  }
}
#endif

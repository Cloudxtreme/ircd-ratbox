/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel_mode.c: Controls modes on channels.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center 
 *  Copyright (C) 1996-2002 Hybrid Development Team 
 *  Copyright (C) 2002-2004 ircd-ratbox development team 
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
#include "s_serv.h"		/* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"		/* ConfigFileEntry, ConfigChannel */
#include "event.h"
#include "memory.h"
#include "balloc.h"
#include "s_log.h"

static int add_id(struct Client *, struct Channel *, char *, int);
static int del_id(struct Channel *, char *, int);

static void change_channel_membership(struct membership *, unsigned int, unsigned int);

/* some small utility functions */
static const char *check_string(char *s);
static char *pretty_mask(char *);
static char *fix_key(char *);
static char *fix_key_old(char *);


static void chm_nosuch(struct Client *, struct Client *,
		       struct Channel *, int, int *, char **, int *, int,
		       int, char, void *, const char *chname);

static void chm_simple(struct Client *, struct Client *, struct Channel *,
		       int, int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_limit(struct Client *, struct Client *, struct Channel *,
		      int, int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_key(struct Client *, struct Client *, struct Channel *,
		    int, int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_op(struct Client *, struct Client *, struct Channel *, int,
		   int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_voice(struct Client *, struct Client *, struct Channel *,
		      int, int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_ban(struct Client *, struct Client *, struct Channel *, int,
		    int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_except(struct Client *, struct Client *, struct Channel *,
		       int, int *, char **, int *, int, int, char, void *, const char *chname);

static void chm_invex(struct Client *, struct Client *, struct Channel *,
		      int, int *, char **, int *, int, int, char, void *, const char *chname);

static void send_cap_mode_changes(struct Client *, struct Client *, struct Channel *, int, int);

static void send_mode_changes(struct Client *, struct Client *, struct Channel *, char *chname);

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */

static char modebuf[BUFSIZE], parabuf[BUFSIZE];
static char mask_buf[BUFSIZE];
static int mask_pos;

static struct ChModeChange mode_changes[BUFSIZE];
static int mode_count;

static int mode_limit;

static int channel_capabs[] = { CAP_EX, CAP_IE };

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
static const char *
check_string(char *s)
{
	char *str = s;

	if(!(s && *s))
		return "*";

	for (; *s; ++s)
	{
		if(IsSpace(*s))
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

		/* local clients cant set redundant bans.. */
		DLINK_FOREACH(ban, list->head)
		{
			actualBan = ban->data;
			if(match(actualBan->banstr, banid))
				return 0;
		}
	}
	/* remote stuff cant set duplicate bans */
	else
	{
		DLINK_FOREACH(ban, list->head)
		{
			actualBan = ban->data;
			if(!irccmp(actualBan->banstr, banid))
				return 0;
		}
	}

	actualBan = (struct Ban *) BlockHeapAlloc(ban_heap);
	DupString(actualBan->banstr, banid);

	if(IsPerson(client_p))
	{
		actualBan->who =
			(char *) MyMalloc(strlen(client_p->name) +
					  strlen(client_p->username) + strlen(client_p->host) + 3);
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

	if(!banid)
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

		if(irccmp(banid, banptr->banstr) == 0)
		{
			MyFree(banptr->banstr);
			MyFree(banptr->who);
			BlockHeapFree(ban_heap, banptr);

			/* num_mask should never be < 0 */
			if(chptr->num_mask > 0)
				chptr->num_mask--;
			else
				chptr->num_mask = 0;

			dlinkDestroy(ban, list);
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
change_channel_membership(struct membership *msptr, unsigned int add_flag,
			  unsigned int del_flag)
{
	s_assert(msptr != NULL);
	if(msptr == NULL)
		return;

	msptr->flags |= add_flag;
	msptr->flags &= ~del_flag;
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
channel_modes(struct Channel *chptr, struct Client *client_p, char *mbuf, char *pbuf)
{
	int len;
	*mbuf++ = '+';
	*pbuf = '\0';

	if(chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	if(chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if(chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if(chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if(chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if(chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';

	if(chptr->mode.limit)
	{
		*mbuf++ = 'l';
		if(IsMember(client_p, chptr) || IsServer(client_p))
		{
			len = ircsprintf(pbuf, "%d ", chptr->mode.limit);
			pbuf += len;
		}
	}
	if(*chptr->mode.key)
	{
		*mbuf++ = 'k';
		if(IsMember(client_p, chptr) || IsServer(client_p))
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
	static char star[] = "*";
	char *nick = star, *user = star, *host = star;
	char *t, *at, *ex;
	char ne = 0, ue = 0, he = 0;	/* save values at nick[NICKLEN], et all */
	mask = (char *)check_string(mask); /* XXX discards const!!! */

	if((size_t)(BUFSIZE - mask_pos) < strlen(mask) + 5)
		return NULL;

	old_mask_pos = mask_pos;

	at = ex = NULL;
	if((t = strchr(mask, '@')) != NULL)
	{
		at = t;
		*t++ = '\0';
		if(*t != '\0')
			host = t;

		if((t = strchr(mask, '!')) != NULL)
		{
			ex = t;
			*t++ = '\0';
			if(*t != '\0')
				user = t;
			if(*mask != '\0')
				nick = mask;
		}
		else
		{
			if(*mask != '\0')
				user = mask;
		}
	}
	else if((t = strchr(mask, '!')) != NULL)
	{
		ex = t;
		*t++ = '\0';
		if(*mask != '\0')
			nick = mask;
		if(*t != '\0')
			user = t;
	}
	else if(strchr(mask, '.') != NULL && strchr(mask, ':') != NULL)
	{
		if(*mask != '\0')
			host = mask;
	}
	else
	{
		if(*mask != '\0')
			nick = mask;
	}

	/* truncate values to max lengths */
	if(strlen(nick) > NICKLEN - 1)
	{
		ne = nick[NICKLEN - 1];
		nick[NICKLEN - 1] = '\0';
	}
	if(strlen(user) > USERLEN)
	{
		ue = user[USERLEN];
		user[USERLEN] = '\0';
	}
	if(strlen(host) > HOSTLEN)
	{
		he = host[HOSTLEN];
		host[HOSTLEN] = '\0';
	}

	mask_pos += ircsprintf(mask_buf + mask_pos, "%s!%s@%s", nick, user, host) + 1;

	/* restore mask, since we may need to use it again later */
	if(at)
		*at = '@';
	if(ex)
		*ex = '!';
	if(ne)
		nick[NICKLEN - 1] = ne;
	if(ue)
		user[USERLEN] = ue;
	if(he)
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
		if(c != ':' && c > ' ')
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
		if((c != 0x0a) && (c != ':') && (c != 0x0d))
			*t++ = c;
	}
	*t = '\0';
	return arg;
}

/* bitmasks for various error returns that set_channel_mode should only return
 * once per call  -orabidoo
 */

#define SM_ERR_NOTS             0x00000001	/* No TS on channel */
#define SM_ERR_NOOPS            0x00000002	/* No chan ops */
#define SM_ERR_UNKNOWN          0x00000004
#define SM_ERR_RPL_C            0x00000008
#define SM_ERR_RPL_B            0x00000010
#define SM_ERR_RPL_E            0x00000020
#define SM_ERR_NOTONCHANNEL     0x00000040	/* Not on channel */
#define SM_ERR_RESTRICTED       0x00000080	/* Restricted chanop */
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
			if((m & (1 << c)) == 0)
				n |= channel_capabs[c];
			else
				y |= channel_capabs[c];
		}
		chcap_combos[m].cap_yes = y;
		chcap_combos[m].cap_no = n;
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
		if(((serv_p->localClient->caps & chcap_combos[n].cap_yes) ==
		    chcap_combos[n].cap_yes) &&
		   ((serv_p->localClient->caps & chcap_combos[n].cap_no) == 0))
		{
			chcap_combos[n].count++;
			return;
		}
	}

	/* This should be impossible -A1kmm. */
	s_assert(0);
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
		if((serv_p->localClient->caps & chcap_combos[n].cap_yes) ==
		   chcap_combos[n].cap_yes &&
		   (serv_p->localClient->caps & chcap_combos[n].cap_no) == 0)
		{
			/* Hopefully capabs can't change dynamically or anything... */
			s_assert(chcap_combos[n].count > 0);
			chcap_combos[n].count--;
			return;
		}
	}

	/* This should be impossible -A1kmm. */
	s_assert(0);
}

/* Mode functions handle mode changes for a particular mode... */
static void
chm_nosuch(struct Client *client_p, struct Client *source_p,
	   struct Channel *chptr, int parc, int *parn,
	   char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	if(*errors & SM_ERR_UNKNOWN)
		return;
	*errors |= SM_ERR_UNKNOWN;
	sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
}

static void
chm_simple(struct Client *client_p, struct Client *source_p,
	   struct Channel *chptr, int parc, int *parn,
	   char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	long mode_type;

	mode_type = (long) d;

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	/* +ntspmaikl == 9 + MAXMODEPARAMS (4 * +o) */
	if(MyClient(source_p) && (++mode_limit > (9 + MAXMODEPARAMS)))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		/* setting - */

		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
}

static void
chm_ban(struct Client *client_p, struct Client *source_p,
	struct Channel *chptr, int parc, int *parn,
	char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	char *mask;
	char *raw_mask;
	dlink_node *ptr;
	struct Ban *banptr;

	if(dir == 0 || parc <= *parn)
	{
		if((*errors & SM_ERR_RPL_B) != 0)
			return;
		*errors |= SM_ERR_RPL_B;

		DLINK_FOREACH(ptr, chptr->banlist.head)
		{
			banptr = ptr->data;
			sendto_one(client_p, form_str(RPL_BANLIST),
				   me.name, client_p->name, chname,
				   banptr->banstr, banptr->who, banptr->when);
		}
		sendto_one(source_p, form_str(RPL_ENDOFBANLIST), me.name, source_p->name, chname);
		return;
	}

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	raw_mask = parv[(*parn)++];

	/* empty ban, or starts with ':' (messes up s2s traffic on burst) */
	if(EmptyString(raw_mask) || *raw_mask == ':')
		return;

	if(IsServer(client_p))
		mask = raw_mask;
	else
		mask = pretty_mask(raw_mask);

	/* we'd have problems parsing this, hyb6 does it too */
	if(strlen(mask) > (MODEBUFLEN - 2))
		return;

	/* if we're adding a NEW id */
	if(dir == MODE_ADD)
	{
		/* dont allow local clients to overflow the banlist, and dont
		 * let servers do redundant +b's, as it wastes bandwidth on
		 * a netjoin.
		 */
		if(!add_id(source_p, chptr, mask, CHFL_BAN))
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = mask;
	}
	else if(dir == MODE_DEL)
	{
		if(del_id(chptr, mask, CHFL_BAN) == 0)
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
		mode_changes[mode_count++].arg = mask;
	}
}

static void
chm_except(struct Client *client_p, struct Client *source_p,
	   struct Channel *chptr, int parc, int *parn,
	   char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	dlink_node *ptr;
	struct Ban *banptr;
	char *mask, *raw_mask;

	/* if we have +e disabled, allow local clients to do anything but
	 * set the mode.  This prevents the abuse of +e when just a few
	 * servers support it. --fl
	 */
	if(!ConfigChannel.use_except && MyClient(source_p) && ((dir == MODE_ADD) && (parc > *parn)))
	{
		if((*errors & SM_ERR_RPL_E) != 0)
			return;

		*errors |= SM_ERR_RPL_E;
		return;
	}

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if((dir == MODE_QUERY) || parc <= *parn)
	{
		if((*errors & SM_ERR_RPL_E) != 0)
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

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	raw_mask = parv[(*parn)++];

	/* empty ban, or starts with ':' (messes up s2s traffic on burst) */
	if(EmptyString(raw_mask) || *raw_mask == ':')
		return;

	if(IsServer(client_p))
		mask = raw_mask;
	else
		mask = pretty_mask(raw_mask);

	if(strlen(mask) > (MODEBUFLEN - 2))
		return;

	/* If we're adding a NEW id */
	if(dir == MODE_ADD)
	{
		/* dont allow local clients to overflow the banlist, and dont
		 * let servers do redundant +e's, as it wastes bandwidth on
		 * a netjoin.
		 */
		if(!add_id(source_p, chptr, mask, CHFL_EXCEPTION))
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = CAP_EX;
		mode_changes[mode_count].nocaps = 0;

		if(ConfigChannel.use_except)
			mode_changes[mode_count].mems = ONLY_CHANOPS;
		else
			mode_changes[mode_count].mems = ONLY_SERVERS;

		mode_changes[mode_count++].arg = mask;
	}
	else if(dir == MODE_DEL)
	{
		if(del_id(chptr, mask, CHFL_EXCEPTION) == 0)
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
		mode_changes[mode_count++].arg = mask;
	}
}

static void
chm_invex(struct Client *client_p, struct Client *source_p,
	  struct Channel *chptr, int parc, int *parn,
	  char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	char *mask, *raw_mask;
	dlink_node *ptr;
	struct Ban *banptr;

	/* if we have +I disabled, allow local clients to do anything but
	 * set the mode.  This prevents the abuse of +I when just a few
	 * servers support it --fl
	 */
	if(!ConfigChannel.use_invex && MyClient(source_p) && (dir == MODE_ADD) && (parc > *parn))
	{
		if((*errors & SM_ERR_RPL_I) != 0)
			return;

		*errors |= SM_ERR_RPL_I;
		return;
	}

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if((dir == MODE_QUERY) || parc <= *parn)
	{
		if((*errors & SM_ERR_RPL_I) != 0)
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

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	raw_mask = parv[(*parn)++];

	/* empty ban, or starts with ':' (messes up s2s traffic on burst) */
	if(EmptyString(raw_mask) || *raw_mask == ':')
		return;

	if(IsServer(client_p))
		mask = raw_mask;
	else
		mask = pretty_mask(raw_mask);

	if(strlen(mask) > (MODEBUFLEN - 2))
		return;

	if(dir == MODE_ADD)
	{
		/* dont allow local clients to overflow the banlist, and dont
		 * let servers do redundant +I's, as it wastes bandwidth on
		 * a netjoin.
		 */
		if(!add_id(source_p, chptr, mask, CHFL_INVEX))
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = CAP_IE;
		mode_changes[mode_count].nocaps = 0;

		if(ConfigChannel.use_invex)
			mode_changes[mode_count].mems = ONLY_CHANOPS;
		else
			mode_changes[mode_count].mems = ONLY_SERVERS;

		mode_changes[mode_count++].arg = mask;
	}
	else if(dir == MODE_DEL)
	{
		if(del_id(chptr, mask, CHFL_INVEX) == 0)
		{
			/* mask isn't a valid ban, check raw_mask */
			if(del_id(chptr, raw_mask, CHFL_INVEX))
				mask = raw_mask;
		}

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = CAP_IE;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ONLY_CHANOPS;
		mode_changes[mode_count++].arg = mask;
	}
}

static void
chm_op(struct Client *client_p, struct Client *source_p,
       struct Channel *chptr, int parc, int *parn,
       char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	struct membership *msptr;
	char *opnick;
	struct Client *targ_p;

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if((dir == MODE_QUERY) || (parc <= *parn))
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

	/* empty, bleh */
	if(EmptyString(opnick))
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK),
			   me.name, source_p->name, "*");
		return;
	}

	if((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
	{
		return;
	}

	msptr = find_channel_membership(chptr, targ_p);
	if(msptr == NULL)
	{
		if(!(*errors & SM_ERR_NOTONCHANNEL))
			sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL), me.name,
				   source_p->name, opnick, chname);
		*errors |= SM_ERR_NOTONCHANNEL;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	if(dir == MODE_ADD)
	{
		if(targ_p == source_p)
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		change_channel_membership(msptr, CHFL_CHANOP, CHFL_DEOPPED);
	}
	else
	{
		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		change_channel_membership(msptr, 0, CHFL_CHANOP);
	}
}

static void
chm_voice(struct Client *client_p, struct Client *source_p,
	  struct Channel *chptr, int parc, int *parn,
	  char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	struct membership *msptr;
	char *opnick;
	struct Client *targ_p;

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if((dir == MODE_QUERY) || parc <= *parn)
		return;

	opnick = parv[(*parn)++];

	/* empty, bleh */
	if(EmptyString(opnick))
	{
		sendto_one(source_p, form_str(ERR_NOSUCHNICK),
			   me.name, source_p->name, "*");
		return;
	}

	if((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
	{
		return;
	}

	msptr = find_channel_membership(chptr, targ_p);
	if(msptr == NULL)
	{
		if(!(*errors & SM_ERR_NOTONCHANNEL))
			sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL), me.name,
				   source_p->name, opnick, chname);
		*errors |= SM_ERR_NOTONCHANNEL;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	if(dir == MODE_ADD)
	{
		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		change_channel_membership(msptr, CHFL_VOICE, 0);
	}
	else
	{
		mode_changes[mode_count].letter = 'v';
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		change_channel_membership(msptr, 0, CHFL_VOICE);
	}
}

static void
chm_limit(struct Client *client_p, struct Client *source_p,
	  struct Channel *chptr, int parc, int *parn,
	  char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	int limit;
	char *lstr;

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(dir == MODE_QUERY)
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		lstr = parv[(*parn)++];

		if(EmptyString(lstr) || (limit = atoi(lstr)) <= 0)
			return;

		ircsprintf(lstr, "%d", limit);

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = lstr;

		chptr->mode.limit = limit;
	}
	else if(dir == MODE_DEL)
	{
		if(!chptr->mode.limit)
			return;

		chptr->mode.limit = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
}

static void
chm_key(struct Client *client_p, struct Client *source_p,
	struct Channel *chptr, int parc, int *parn,
	char **parv, int *errors, int alev, int dir, char c, void *d, const char *chname)
{
	char *key;

	if(alev != CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(dir == MODE_QUERY)
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		key = parv[(*parn)++];

		if(EmptyString(key))
			return;

		if(MyClient(source_p))
			fix_key(key);
		else
			fix_key_old(key);

		s_assert(key[0] != ' ');
		strlcpy(chptr->mode.key, key, sizeof(chptr->mode.key));

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = chptr->mode.key;
	}
	else if(dir == MODE_DEL)
	{
		if (parc > *parn)
			(*parn)++;

		if(!(*chptr->mode.key))
			return;

		*chptr->mode.key = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = "*";
	}
}

struct ChannelMode
{
	void (*func) (struct Client * client_p, struct Client * source_p,
		      struct Channel * chptr, int parc, int *parn, char **parv,
		      int *errors, int alev, int dir, char c, void *d, const char *chname);
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
  {chm_nosuch, NULL},				  /* a */
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
get_channel_access(struct Client *source_p, struct membership *msptr)
{
	/* Let hacked servers in for now... */
	if(!MyClient(source_p) || is_chanop(msptr))
		return CHFL_CHANOP;

	return CHFL_PEON;
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
	int i, mbl, pbl, nc, mc, arglen, preflen, len;
	const char *arg;
	char *pbuf;
	int dir;

	mc = 0;
	nc = 0;
	pbl = 0;
	pbuf = parabuf;
	parabuf[0] = 0;
	dir = MODE_QUERY;

	preflen = mbl = ircsprintf(modebuf, ":%s MODE %s ", source_p->name, chptr->chname);

	/* loop the list of - modes we have */
	for (i = 0; i < mode_count; i++)
	{
		/* if they dont support the cap we need, or they do support a cap they
		 * cant have, then dont add it to the modebuf.. that way they wont see
		 * the mode
		 */
		if((mode_changes[i].letter == 0) ||
		   ((cap & mode_changes[i].caps) != mode_changes[i].caps)
		   || ((nocap & mode_changes[i].nocaps) != mode_changes[i].nocaps))
			continue;

		arg = mode_changes[i].arg;

		if(arg)
		{
			arglen = strlen(arg);

			if(arglen > MODEBUFLEN-5)
				continue;
		}

		/* if we're creeping past the buf size, we need to send it and make
		 * another line for the other modes
		 * XXX - this could give away server topology with uids being
		 * different lengths, but not much we can do, except possibly break
		 * them as if they were the longest of the nick or uid at all times,
		 * which even then won't work as we don't always know the uid -A1kmm.
		 */
		if((arg != NULL) && ((mc == MAXMODEPARAMS) ||
		   (arglen + pbl + 4) > sizeof(parabuf) ||
		   (arglen + pbl + mbl + 4) > BUFSIZE-5))
		{
			if(nc != 0)
				sendto_server(client_p, chptr, cap, nocap,
					      "%s %s", modebuf, parabuf);
			nc = 0;
			mc = 0;

			mbl = preflen;
			pbl = 0;
			pbuf = parabuf;
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

		if(arg != NULL)
		{
			len = ircsprintf(pbuf, "%s ", arg);
			pbl += len;
			pbuf += len;
			mc++;
		}
	}

	if(pbl && parabuf[pbl - 1] == ' ')
		parabuf[pbl - 1] = 0;

	if(nc != 0)
		sendto_server(client_p, chptr, cap, nocap, "%s %s", modebuf, parabuf);
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
	int cur_len, mlen, len;
	int paralen;
	int paracount;
	int arglen;
	char *mbuf;
	char *pbuf;
	int i, j, flags;
	int dir;

	/* bail out if we have nothing to do... */
	if(!mode_count)
		return;

	if(IsServer(source_p))
		mlen = ircsprintf(modebuf, ":%s MODE %s ", 
				  me.name, chname);
	else
		mlen = ircsprintf(modebuf, ":%s!%s@%s MODE %s ", 
				  source_p->name, source_p->username, 
				  source_p->host, chname);

	/* loop twice, once for ALL_MEMBERS, once for ONLY_CHANOPS */
	for(j = 0, flags = ALL_MEMBERS; j < 2; j++, flags = ONLY_CHANOPS)
	{
		cur_len = mlen;
		mbuf = modebuf + mlen;
		pbuf = parabuf;
		parabuf[0] = '\0';
		paracount = paralen = 0;
		dir = MODE_QUERY;

		/* loop and output all the changes that everyone can see */
		for (i = 0; i < mode_count; i++)
		{
			if(mode_changes[i].letter == 0 ||
			   mode_changes[i].mems != flags)
				continue;

			if(mode_changes[i].arg != NULL)
			{
				arglen = strlen(mode_changes[i].arg);

				if(arglen > MODEBUFLEN-5)
					continue;
			}
			else
				arglen = 0;

			if(mode_changes[i].arg != NULL &&
			   ((paracount == MAXMODEPARAMS) ||
			    ((paralen + arglen + 4) > sizeof(parabuf)) ||
			    ((paralen + arglen + cur_len + 4) > BUFSIZE-4)))
			{
				*mbuf = '\0';

				if(cur_len > mlen)
					sendto_channel_local(flags, chptr, "%s %s", modebuf, parabuf);
				else
					continue;

				paracount = paralen = 0;
				cur_len = mlen;
				mbuf = modebuf + mlen;
				pbuf = parabuf;
				parabuf[0] = '\0';
				dir = MODE_QUERY;
			}

			if(dir != mode_changes[i].dir)
			{
				*mbuf++ = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
				cur_len++;
				dir = mode_changes[i].dir;
			}

			*mbuf++ = mode_changes[i].letter;
			cur_len++;

			if(mode_changes[i].arg != NULL)
			{
				paracount++;
				len = ircsprintf(pbuf, "%s ", mode_changes[i].arg);
				pbuf += len;
				paralen += len;
			}
		}

		if(paralen && parabuf[paralen - 1] == ' ')
			parabuf[paralen - 1] = '\0';

		*mbuf = '\0';
		if(cur_len > mlen)
			sendto_channel_local(flags, chptr, "%s %s", modebuf, parabuf);
	}

	/* only attempt to propagate if we have to */
	if(MyClient(source_p) || dlink_list_length(&serv_list) > 1)
	{
		/* Now send to servers... */
		for (i = 0; i < NCHCAP_COMBOS; i++)
			if(chcap_combos[i].count != 0)
				send_cap_mode_changes(client_p, source_p, chptr,
						      chcap_combos[i].cap_yes, chcap_combos[i].cap_no);
	}
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
		 struct Channel *chptr, struct membership *msptr,
		 int parc, char *parv[], char *chname)
{
	int dir = MODE_ADD;
	int parn = 1;
	int alevel, errors = 0;
	char *ml = parv[0], c;
	int table_position;

	mask_pos = 0;
	mode_count = 0;
	mode_limit = 0;

	alevel = get_channel_access(source_p, msptr);

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
			if(c < 'A' || c > 'z')
				table_position = 0;
			else
				table_position = c - 'A' + 1;
			ModeTable[table_position].func(client_p, source_p, chptr,
						       parc, &parn,
						       parv, &errors, alevel, dir, c,
						       ModeTable[table_position].d, chname);
			break;
		}

	send_mode_changes(client_p, source_p, chptr, chname);
}


/************************************************************************
 *   IRC - Internet Relay Chat, src/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
#include "parse.h"
#include "client.h"
#include "channel.h"
#include "handlers.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_stats.h"
#include "send.h"
#include "s_debug.h"
#include "ircd_handler.h"
#include "msg.h"
#include "s_conf.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static  char    *para[MAXPARA+1];
static  int     cancel_clients (struct Client *, struct Client *, char *);
static  void    remove_unknown (struct Client *, char *, char *);

static  char    sender[HOSTLEN+1];
static  int     cancel_clients (struct Client *, struct Client *, char *);
static  void    remove_unknown (struct Client *, char *, char *);

static int do_numeric (char [], struct Client *,
                         struct Client *, int, char **);

static struct Message *do_msg_tree(MESSAGE_TREE *, char *, struct Message *);
static struct Message *tree_parse(char *);

static char buffer[1024];  /* ZZZ must this be so big? must it be here? */

struct MessageTree* msg_tree_root;

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other functions!
 */
int parse(struct Client *cptr, char *buffer, char *bufend)
{
  struct Client*  from = cptr;
  char*           ch;
  char*           s;
  int             i;
  int             paramcount;
  int             handle_idx = -1;	/* Handler index */
  char*           numeric = 0;
  struct Message* mptr;
  MessageHandler  handler = 0;

  Debug((DEBUG_DEBUG, "Parsing %s:", buffer));

  if (IsDead(cptr))
    return -1;

  s = sender;
  *s = '\0';

  for (ch = buffer; *ch == ' '; ch++)   /* skip spaces */
    /* null statement */ ;

  para[0] = from->name;
  if (*ch == ':')
    {
      ch++;

      /*
      ** Copy the prefix to 'sender' assuming it terminates
      ** with SPACE (or NULL, which is an error, though).
      */
      for (i = 0; *ch && *ch != ' '; i++ )
	{
	  if (i < (sizeof(sender)-1))
	    *s++ = *ch++; /* leave room for NULL */
	}
      *s = '\0';
      i = 0;

      /*
      ** Actually, only messages coming from servers can have
      ** the prefix--prefix silently ignored, if coming from
      ** a user client...
      **
      ** ...sigh, the current release "v2.2PL1" generates also
      ** null prefixes, at least to NOTIFY messages (e.g. it
      ** puts "sptr->nickname" as prefix from server structures
      ** where it's null--the following will handle this case
      ** as "no prefix" at all --msa  (": NOTICE nick ...")
      */
      if (*sender && IsServer(cptr))
        {
          from = find_client(sender, (struct Client *) NULL);
          if (!from || !match(from->name, sender))
            from = find_server(sender);

          para[0] = sender;
          
          /* Hmm! If the client corresponding to the
           * prefix is not found--what is the correct
           * action??? Now, I will ignore the message
           * (old IRC just let it through as if the
           * prefix just wasn't there...) --msa
           */
          if (!from)
            {
              Debug((DEBUG_ERROR, "Unknown prefix (%s)(%s) from (%s)",
                     sender, buffer, cptr->name));
              ServerStats->is_unpf++;

              remove_unknown(cptr, sender, buffer);

              return -1;
            }
          if (from->from != cptr)
            {
              ServerStats->is_wrdi++;
              Debug((DEBUG_ERROR, "Message (%s) coming from (%s)",
                     buffer, cptr->name));

              return cancel_clients(cptr, from, buffer);
            }
        }
      while (*ch == ' ')
        ch++;
    }

  if (*ch == '\0')
    {
      ServerStats->is_empt++;
      Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
             cptr->name, from->name));
      return(-1);
    }

  /*
  ** Extract the command code from the packet.  Point s to the end
  ** of the command code and calculate the length using pointer
  ** arithmetic.  Note: only need length for numerics and *all*
  ** numerics must have parameters and thus a space after the command
  ** code. -avalon
  *
  * ummm???? - Dianora
  */

  if( *(ch + 3) == ' ' && /* ok, lets see if its a possible numeric.. */
      IsDigit(*ch) && IsDigit(*(ch + 1)) && IsDigit(*(ch + 2)) )
    {
      mptr = (struct Message *)NULL;
      numeric = ch;
      paramcount = MAXPARA;
      ServerStats->is_num++;
      s = ch + 3;       /* I know this is ' ' from above if */
      *s++ = '\0';      /* blow away the ' ', and point s to next part */
    }
  else
    {
      s = strchr(ch, ' ');      /* moved from above,now need it here */
      if (s)
        *s++ = '\0';

      mptr = tree_parse(ch);

      if (!mptr || !mptr->cmd)
        {
          /*
          ** Note: Give error message *only* to recognized
          ** persons. It's a nightmare situation to have
          ** two programs sending "Unknown command"'s or
          ** equivalent to each other at full blast....
          ** If it has got to person state, it at least
          ** seems to be well behaving. Perhaps this message
          ** should never be generated, though...  --msa
          ** Hm, when is the buffer empty -- if a command
          ** code has been found ?? -Armin
          */
          if (buffer[0] != '\0')
            {
              if (IsPerson(from))
                sendto_one(from,
                           ":%s %d %s %s :Unknown command",
                           me.name, ERR_UNKNOWNCOMMAND,
                           from->name, ch);
              Debug((DEBUG_ERROR,"Unknown (%s) from %s",
                     ch, get_client_name(cptr, TRUE)));
            }
          ServerStats->is_unco++;
          return(-1);
        }

      paramcount = mptr->parameters;
      i = bufend - ((s) ? s : ch);
      mptr->bytes += i;

      /* Allow only 1 msg per 2 seconds
       * (on average) to prevent dumping.
       * to keep the response rate up,
       * bursts of up to 5 msgs are allowed
       * -SRB
       * Opers can send 1 msg per second, burst of ~20
       * -Taner
       */
      if ((mptr->flags & 1) && !(IsServer(cptr)))
        {
	  if (ConfigFileEntry.no_oper_flood) {
            if (IsAnOper(cptr))
              /* "randomly" (weighted) increase the since */
              cptr->since += (cptr->receiveM % 5) ? 1 : 0;
            else
            cptr->since += (2 + i / 120);
	  }
        }
    }
  /*
  ** Must the following loop really be so devious? On
  ** surface it splits the message to parameters from
  ** blank spaces. But, if paramcount has been reached,
  ** the rest of the message goes into this last parameter
  ** (about same effect as ":" has...) --msa
  **
  ** changed how this works - now paramcount is simply the
  ** required number of arguments for a command.  imo the
  ** previous behavior isn't needed --is
  */

  /* Note initially true: s==NULL || *(s-1) == '\0' !! */

  i = 1;

  if (s)   /* redone by is, aug 2000 */
    {
      if (paramcount > MAXPARA)
        paramcount = MAXPARA;
      
      {
	char *longarg = NULL;
	char *ap;
	
	longarg = s;
	
	if(strsep(&longarg,":")) /* Tear off short args */
	  
	  if(longarg)
	    *(longarg-2) = '\0';
	
	while((ap = strsep(&s, " ")) != NULL) 
	  {
	    if(*ap != '\0') 
	      {
		para[i] = ap;
		if(i < MAXPARA)
		  ++i;
		else
		  break;
	      }
	  }
	
	if(longarg) 
	  {
	    para[i] = longarg;
	    i++;
	  }
      }
    }
  
  para[i] = NULL;

  if (mptr == (struct Message *)NULL)
    return (do_numeric(numeric, cptr, from, i, para));

  mptr->count++;

  /* New patch to avoid server flooding from unregistered connects
     - Pie-Man 07/27/2000 */

  if (!IsRegistered(cptr))
  {
      /* if its from a possible server connection
       * ignore it.. more than likely its a header thats sneaked through
       */

      if((IsHandshake(cptr) || IsConnecting(cptr) || IsServer(cptr))
	   && !(mptr->flags & MFLG_UNREG))
        return -1;
  }

  /* Again, instead of function address comparing, see if
   * this function resets idle time as given from mptr
   * if IDLE_FROM_MSG is undefined, the sense of the flag is reversed.
   * i.e. if the flag is 0, then reset idle time, otherwise don't reset it.
   *
   * - Dianora
   */

#if 0
#ifdef IDLE_FROM_MSG
  if (IsRegisteredUser(cptr) && mptr->reset_idle)
    from->user->last = CurrentTime;
#else
  if (IsRegisteredUser(cptr) && !mptr->reset_idle)
    from->user->last = CurrentTime;
#endif
#endif
  
  /* don't allow other commands while a list is blocked. since we treat
     them specially with respect to sendq. */
#if 0
  if ((IsDoingList(cptr)) && (*mptr->func != m_list))
      return -1;
#endif

  /* Determine the class of this connection and assign it one of four handler types to
      fit in with the handler table - Pie-Man 07/24/2000 */
  switch (cptr->status)
  { 
     case STAT_CONNECTING:
     case STAT_HANDSHAKE:
     case STAT_ME:
     case STAT_UNKNOWN:
       handle_idx = UNREGISTERED_HANDLER;
       break;
     case STAT_SERVER:
       handle_idx = SERVER_HANDLER;
       break;
     case STAT_CLIENT:
       handle_idx = IsAnyOper(cptr) ? OPER_HANDLER : CLIENT_HANDLER;
       break;
     default:
  /* Todo: An error should be logged here, unable to determine the class of connection.
     Should never happen and something we need to fix if it does - Pie-Man */
       return -1;
  }

 if (handle_idx != cptr->handler)
        log(L_ERROR,"Handler for client '%s' performing '%s' is incorrect"
         ,cptr->name,mptr->cmd);             

  handler = mptr->handlers[handle_idx];
  /* check right amount of params is passed... --is */
  
  if (i < mptr->parameters) {
	  sendto_one(cptr, form_str(ERR_NEEDMOREPARAMS),
                 me.name, para[0], mptr->cmd);
	  return 0;
  }
  return (*handler)(cptr, from, i, para);
}

/* for qsort'ing the msgtab in place -orabidoo */
static int mcmp(struct Message *m1, struct Message *m2)
{
  return strcmp(m1->cmd, m2->cmd);
}

/*
 * init_tree_parse()
 *
 * inputs               - pointer to msg_table defined in msg.h 
 * output       - NONE
 * side effects - MUST MUST be called at startup ONCE before
 *                any other keyword hash routine is used.
 *
 *      -Dianora, orabidoo
 */
/* Initialize the msgtab parsing tree -orabidoo
 */
void init_tree_parse(struct Message *mptr)
{
  int i;
  struct Message *mpt = mptr;

  for (i=0; mpt->cmd; mpt++)
    i++;
  qsort((void *)mptr, i, sizeof(struct Message), 
                (int (*)(const void *, const void *)) mcmp);
  msg_tree_root = (MESSAGE_TREE *)MyMalloc(sizeof(MESSAGE_TREE));
  mpt = do_msg_tree(msg_tree_root, "", mptr);

  /*
   * this happens if one of the msgtab entries included characters
   * other than capital letters  -orabidoo
   */
  if (mpt->cmd)
    {
      log(L_CRIT, "bad msgtab entry: ``%s''\n", mpt->cmd);
      exit(1);
    }
}

/*  Recursively make a prefix tree out of the msgtab -orabidoo
 */
static struct Message *do_msg_tree(MESSAGE_TREE *mtree, char *prefix,
                                struct Message *mptr)
{
  char newpref[64];  /* must be longer than any command */
  int c, c2, lp;
  MESSAGE_TREE *mtree1;

  lp = strlen(prefix);
  if (!lp || !strncmp(mptr->cmd, prefix, lp))
    {
      if (!mptr[1].cmd || (lp && strncmp(mptr[1].cmd, prefix, lp)))
        {
          /* non ambiguous -> make a final case */
          mtree->final = mptr->cmd + lp;
          mtree->msg = mptr;
          for (c=0; c<=25; c++)
            mtree->pointers[c] = NULL;
          return mptr+1;
        }
      else
        {
          /* ambigous -> make new entries for each of the letters that match */
          if (!irccmp(mptr->cmd, prefix))
            {
              /* fucking OPERWALL blows me */
              mtree->final = (void *)1;
              mtree->msg = mptr;
              mptr++;
            }
          else
            mtree->final = NULL;

      for (c='A'; c<='Z'; c++)
        {
          if (mptr->cmd[lp] == c)
            {
              mtree1 = (MESSAGE_TREE *)MyMalloc(sizeof(MESSAGE_TREE));
              mtree1->final = NULL;
              mtree->pointers[c-'A'] = mtree1;
              strcpy(newpref, prefix);
              newpref[lp] = c;
              newpref[lp+1] = '\0';
              mptr = do_msg_tree(mtree1, newpref, mptr);
              if (!mptr->cmd || strncmp(mptr->cmd, prefix, lp))
                {
                  for (c2=c+1-'A'; c2<=25; c2++)
                    mtree->pointers[c2] = NULL;
                  return mptr;
                }
            } 
          else
            {
              mtree->pointers[c-'A'] = NULL;
            }
        }
      return mptr;
        }
    } 
  else
    {
      assert(0);
      exit(1);
    }
}

struct Message msgtab[] = {
  {MSG_PRIVMSG, 0, 1, MFLG_SLOW | MFLG_UNREG, 0L,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_privmsg, ms_privmsg, mo_privmsg }
  },
  {MSG_NOTICE, 0, 1, MFLG_SLOW | MFLG_UNREG, 0L,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_notice, ms_notice, mo_notice }
  },
  {MSG_CBURST, 0, 1, MFLG_SLOW | MFLG_UNREG, 0L,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_error, ms_cburst, m_error }
  },
  {MSG_DROP, 0, 2, MFLG_SLOW | MFLG_UNREG, 0L,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_error, ms_drop, m_error }
  },
  {MSG_LLJOIN, 0, 3, MFLG_SLOW | MFLG_UNREG, 0L,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_error, ms_lljoin, m_error }
  },
  {MSG_JOIN, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_join, ms_join, m_join }
  },
  {MSG_CJOIN, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_cjoin, m_error, m_cjoin }
  },
  {MSG_MODE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_mode, ms_mode, m_mode }
  },
  {MSG_QUIT, 0, 0, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_quit, m_quit, ms_quit, mo_quit }
  },
  {MSG_PART, 1, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_part, ms_part, mo_part }
  },
  {MSG_KNOCK, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
   /* can knock be recvd from a server? i think not --is */
    { m_unregistered, m_knock, m_ignore, m_knock }
  },
  {MSG_TOPIC, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_topic, ms_topic, m_topic}
  },
  {MSG_INVITE, 0, 2, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_invite, ms_invite, m_invite }
  },
  {MSG_KICK, 0, 2, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_kick, ms_kick, m_kick }
  },
  {MSG_WALLOPS, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_wallops, mo_wallops }
  },
  {MSG_PING, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_ping, ms_ping, m_ping }
  },
  {MSG_PONG, 0, 1, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { mr_pong, m_ignore, ms_pong, m_ignore }
  },
  {MSG_ERROR, 0, 0, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { mr_error, m_ignore, ms_error, m_ignore }
  },
  {MSG_KILL, 0, 2, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_kill, mo_kill }
  },
  {MSG_USER, 0, 4, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_user, m_registered, m_ignore, m_registered }
  },
  {MSG_NICK, 0, 1, MFLG_SLOW, 0,
    { m_nick, m_nick, m_nick, m_nick }
  },
  {MSG_AWAY, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_away, ms_away, m_away }
  },
  {MSG_ISON, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_ison, m_ignore, m_ison }
  },
  {MSG_SERVER, 0, 3, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { mr_server, m_registered, ms_server, m_registered }
  },
  {MSG_SQUIT, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_squit, mo_squit }
  },
  {MSG_WHOIS, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_whois, m_whois, m_whois }
  },
  {MSG_WHO, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_who, m_ignore, m_who }
  },
  {MSG_WHOWAS, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_whowas, m_whowas, m_whowas }
  },
  {MSG_LIST, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_list, m_ignore, m_list }
  },
  {MSG_NAMES, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_names, ms_names, m_names }
  },
  {MSG_USERHOST, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_userhost, m_ignore, m_userhost }
  },
#if 0
  {MSG_USERIP, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_userip, m_ignore, m_userip }
  },
#endif
  {MSG_TRACE, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_trace, ms_trace, mo_trace }
  },
  {MSG_PASS, 0, 1, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_pass, m_registered, m_ignore, m_registered }
  },
  {MSG_LUSERS, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_lusers, ms_lusers, m_lusers }
  },
  {MSG_TIME, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_time, m_time, m_time }
  },
  {MSG_OPER, 0, 2, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_oper, ms_oper, mo_oper }
  },
  {MSG_CONNECT, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_connect, mo_connect }
  },
  {MSG_VERSION, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
        /* why allow version command from unreg'd users? no point --is */
    { m_unregistered, m_version, ms_version, m_version }
  },
  {MSG_STATS, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_stats, ms_stats, mo_stats }
  },
  {MSG_LINKS, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_links, ms_links, m_links }
  },
  {MSG_ADMIN, 0, 0, MFLG_SLOW | MFLG_UNREG, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_admin, m_admin, ms_admin, m_admin }
  },
  {MSG_HELP, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_help, m_ignore, m_help }
  },
  {MSG_INFO, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_info, ms_info, mo_info }
  },
  {MSG_MOTD, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_motd, m_motd, m_motd }
  },
  {MSG_SJOIN, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
   /* there's absolutely no reason for a user to send sjoin, ignore it --is
    *  */
    { m_unregistered, m_ignore, ms_sjoin, m_ignore }
  },
  {MSG_CAPAB, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
   /* ditto */
    { m_unregistered, m_error, ms_capab, m_error }
  },
  {MSG_OPERWALL, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_operwall, mo_operwall }
  },
  {MSG_CLOSE, 0, MAXPARA, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_ignore, mo_close }
  },
  {MSG_KLINE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_kline, mo_kline }
  },
  {MSG_UNKLINE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_error, mo_unkline }
  },
  {MSG_DLINE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_error, mo_dline }
  },
  {MSG_GLINE, 0, 2, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, ms_gline, mo_gline }
  },
  {MSG_HASH, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_hash, m_hash }
  },
  {MSG_DNS, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_dns, m_dns, m_dns }
  },
  {MSG_REHASH, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_ignore, mo_rehash }
  },
  {MSG_RESTART, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_ignore, mo_restart }
  },
  {MSG_DIE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_ignore, mo_die }
  },
  {MSG_HTM, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_ignore, mo_htm }
  },
  {MSG_SET, 0, 0, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_error, mo_set }
  },
  {MSG_TESTLINE, 0, 1, MFLG_SLOW, 0,
    /* UNREG, CLIENT, SERVER, OPER */
    { m_unregistered, m_not_oper, m_error, mo_testline }
  },
  { 0 }
}; 

/*
 * tree_parse()
 * 
 * inputs       - pointer to command in upper case
 * output       - NULL pointer if not found
 *                struct Message pointer to command entry if found
 * side effects - NONE
 *
 *      -Dianora, orabidoo
 */

static struct Message *tree_parse(char *cmd)
{
  char r;
  MESSAGE_TREE *mtree = msg_tree_root;

  while ((r = *cmd++))
    {
      r &= 0xdf;  /* some touppers have trouble w/ lowercase, says Dianora */
      if (r < 'A' || r > 'Z')
        return NULL;
      mtree = mtree->pointers[r - 'A'];
      if (!mtree)
        return NULL;
      if (mtree->final == (void *)1)
        {
          if (!*cmd)
            return mtree->msg;
        }
      else
        if (mtree->final && !irccmp(mtree->final, cmd))
          return mtree->msg;
    }
  return ((struct Message *)NULL);
}

static  int     cancel_clients(struct Client *cptr,
                               struct Client *sptr,
                               char *cmd)
{
  /*
   * kill all possible points that are causing confusion here,
   * I'm not sure I've got this all right...
   * - avalon
   *
   * knowing avalon, probably not.
   */

  /*
  ** with TS, fake prefixes are a common thing, during the
  ** connect burst when there's a nick collision, and they
  ** must be ignored rather than killed because one of the
  ** two is surviving.. so we don't bother sending them to
  ** all ops everytime, as this could send 'private' stuff
  ** from lagged clients. we do send the ones that cause
  ** servers to be dropped though, as well as the ones from
  ** non-TS servers -orabidoo
  */
  /*
   * Incorrect prefix for a server from some connection.  If it is a
   * client trying to be annoying, just QUIT them, if it is a server
   * then the same deal.
   */
  if (IsServer(sptr) || IsMe(sptr))
    {
      sendto_realops_flags(FLAGS_DEBUG, "Message for %s[%s] from %s",
                         sptr->name, sptr->from->name,
                         get_client_name(cptr, TRUE));
      if (IsServer(cptr))
        {
          sendto_realops_flags(FLAGS_DEBUG,
                             "Not dropping server %s (%s) for Fake Direction",
                             cptr->name, sptr->name);
          return -1;
        }

      if (IsClient(cptr))
        sendto_realops_flags(FLAGS_DEBUG,
                           "Would have dropped client %s (%s@%s) [%s from %s]",
                           cptr->name, cptr->username, cptr->host,
                           cptr->user->server, cptr->from->name);
      return -1;

      /*
        return exit_client(cptr, cptr, &me, "Fake Direction");
        */
    }
  /*
   * Ok, someone is trying to impose as a client and things are
   * confused.  If we got the wrong prefix from a server, send out a
   * kill, else just exit the lame client.
   */
  if (IsServer(cptr))
   {
    /*
    ** If the fake prefix is coming from a TS server, discard it
    ** silently -orabidoo
	**
	** all servers must be TS these days --is
    */
	   if (sptr->user)
		   sendto_realops_flags(FLAGS_DEBUG,
								"Message for %s[%s@%s!%s] from %s (TS, ignored)",
								sptr->name, sptr->username, sptr->host,
								sptr->from->name, get_client_name(cptr, TRUE));
	   return 0;
   }
  return exit_client(cptr, cptr, &me, "Fake prefix");
}

static  void    remove_unknown(struct Client *cptr,
                               char *sender,
                               char *buffer)
{
  if (!IsRegistered(cptr))
    return;

  if (IsClient(cptr))
    {
      sendto_realops_flags(FLAGS_DEBUG,
                 "Weirdness: Unknown client prefix (%s) from %s, Ignoring %s",
                         buffer,
                         get_client_name(cptr, FALSE), sender);
      return;
    }

  /*
   * Not from a server so don't need to worry about it.
   */
  if (!IsServer(cptr))
    return;
  /*
   * Do kill if it came from a server because it means there is a ghost
   * user on the other server which needs to be removed. -avalon
   * Tell opers about this. -Taner
   */
  if (!strchr(sender, '.'))
    sendto_one(cptr, ":%s KILL %s :%s (%s(?) <- %s)",
               me.name, sender, me.name, sender,
               get_client_name(cptr, FALSE));
  else
    {
      sendto_realops_flags(FLAGS_DEBUG,
        "Unknown prefix (%s) from %s, Squitting %s", buffer, get_client_name(cptr, FALSE), sender);
      sendto_one(cptr, ":%s SQUIT %s :(Unknown prefix (%s) from %s)",
                 me.name, sender, buffer, get_client_name(cptr, FALSE));
    }
}



/*
** DoNumeric (replacement for the old do_numeric)
**
**      parc    number of arguments ('sender' counted as one!)
**      parv[0] pointer to 'sender' (may point to empty string) (not used)
**      parv[1]..parv[parc-1]
**              pointers to additional parameters, this is a NULL
**              terminated list (parv[parc] == NULL).
**
** *WARNING*
**      Numerics are mostly error reports. If there is something
**      wrong with the message, just *DROP* it! Don't even think of
**      sending back a neat error message -- big danger of creating
**      a ping pong error message...
*/
static int     do_numeric(
                   char numeric[],
                   struct Client *cptr,
                   struct Client *sptr,
                   int parc,
                   char *parv[])
{
  struct Client *acptr;
  struct Channel *chptr;
  char  *nick, *p;
  int   i;

  if (parc < 1 || !IsServer(sptr))
    return 0;

  /* Remap low number numerics. */
  if(numeric[0] == '0')
    numeric[0] = '1';

  /*
  ** Prepare the parameter portion of the message into 'buffer'.
  ** (Because the buffer is twice as large as the message buffer
  ** for the socket, no overflow can occur here... ...on current
  ** assumptions--bets are off, if these are changed --msa)
  ** Note: if buffer is non-empty, it will begin with SPACE.
  */
  buffer[0] = '\0';
  if (parc > 1)
    {
      for (i = 2; i < (parc - 1); i++)
        {
          (void)strcat(buffer, " ");
          (void)strcat(buffer, parv[i]);
        }
      (void)strcat(buffer, " :");
      (void)strcat(buffer, parv[parc-1]);
    }
  for (; (nick = strtoken(&p, parv[1], ",")); parv[1] = NULL)
    {
      if ((acptr = find_client(nick, (struct Client *)NULL)))
        {
          /*
          ** Drop to bit bucket if for me...
          ** ...one might consider sendto_ops
          ** here... --msa
          ** And so it was done. -avalon
          ** And regretted. Dont do it that way. Make sure
          ** it goes only to non-servers. -avalon
          ** Check added to make sure servers don't try to loop
          ** with numerics which can happen with nick collisions.
          ** - Avalon
          */
          if (!IsMe(acptr) && IsPerson(acptr))
            sendto_prefix_one(acptr, sptr,":%s %s %s%s",
                              parv[0], numeric, nick, buffer);
          else if (IsServer(acptr) && acptr->from != cptr)
            sendto_prefix_one(acptr, sptr,":%s %s %s%s",
                              parv[0], numeric, nick, buffer);
        }
      else if ((acptr = find_server(nick)))
        {
          if (!IsMe(acptr) && acptr->from != cptr)
            sendto_prefix_one(acptr, sptr,":%s %s %s%s",
                              parv[0], numeric, nick, buffer);
        }
      else if ((chptr = hash_find_channel(nick, (struct Channel *)NULL)))
        sendto_channel_butone(cptr,sptr,chptr,":%s %s %s%s",
                              parv[0],
                              numeric, chptr->chname, buffer);
    }
  return 0;
}


/* stick these here for now, to get it to compile
 * will go into m_defaults.c just like ircu (thanks bleep!)
 */
int m_not_oper(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  sendto_one(sptr, form_str(ERR_NOPRIVILEGES), me.name, parv[0]);
  return 0;
}

int m_unregistered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  sendto_one(cptr, ":%s %d * %s :Register first.",
             me.name, ERR_NOTREGISTERED, parv[0]);
  return 0;
}

int m_registered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  /*  return send_error_to_client(sptr, ERR_ALREADYREGISTRED); */
  sendto_one(cptr, ":%s NOTICE %s :Already registered",
	     me.name,cptr->name);
  return 0;
}

int m_ignore(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return 0;
}


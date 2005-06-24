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

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

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
#include "vchannel.h"
#include "memory.h"

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static  char    *sender;
static  char    *para[MAXPARA+1];
static  int     cancel_clients (struct Client *, struct Client *, char *);
static  void    remove_unknown (struct Client *, char *, char *);

static  int     cancel_clients (struct Client *, struct Client *, char *);
static  void    remove_unknown (struct Client *, char *, char *);

static int do_numeric (char [], struct Client *,
                         struct Client *, int, char **);

static int handle_command(struct Message *, struct Client *, struct Client *, int, char **);

static int hash(char *p);
static struct Message *hash_parse(char *);

/* XXX - jdc: hopefully the fd-walkover bug is gone now... */
/* struct MessageHash *msg_hash_table[MAX_MSG_HASH+10]; */

struct MessageHash *msg_hash_table[MAX_MSG_HASH];

static char buffer[1024];

/* turn a string into a parc/parv pair
 */

static void
string_to_array(char *string, int mpara, int paramcount,
                char *end, int *parc, char *parv[MAXPARA])
{
  char *ap;
  char *p=NULL;
	
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
  ** ok, now we do support it, for ISON brokenness among
  ** other things. --is
  */
	
  /* Note initially true: s==NULL || *(s-1) == '\0' !! */

  /* redone by is, aug 2000 */
  if (paramcount > MAXPARA)
    paramcount = MAXPARA;
	
/*  while((ap = strsep(&string, " ")) != NULL)  */
  for(ap = strtoken(&p,string," "); ap; ap = strtoken(&p, NULL, " "))
    if(*ap != '\0') 
      {
	parv[(*parc)] = ap;
	
	if (ap[0] == ':' || (mpara && (*parc >= mpara))) {
	  char *tendp = ap;
				
	  while (*tendp++)
	    ;
	  
	  if ( tendp < end ) /* more tokens to follow */
	    ap [ strlen (ap) ] = ' '; 
	  
	  if (ap[0] == ':')
	    ap++;
				
	  parv[(*parc)++] = ap;
	  break;
	}
			
	if(*parc < MAXPARA)
	  ++(*parc);
	else
	  break;
      }
	
  parv[(*parc)] = NULL;
}

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other functions!
 */
int parse(struct Client *cptr, char *pbuffer, char *bufend)
{
  struct Client*  from = cptr;
  char*           ch;
  char*           s;
  char*           end;
  int             i;
  int             paramcount, mpara=0;
  char*           numeric = 0;
  struct Message* mptr;
  int status;

  Debug((DEBUG_DEBUG, "Parsing %s:", pbuffer));

  if (IsDead(cptr))
    return (CLIENT_EXITED);
  
  /* XXX kludgy test, can it be combined into IsDead() ? */
  if (cptr->fd < 0)
    return (CLIENT_EXITED);

  for (ch = pbuffer; *ch == ' '; ch++)   /* skip spaces */
    /* null statement */ ;

  para[0] = from->name;
  if (*ch == ':')
    {
      ch++;

      /*
      ** Copy the prefix to 'sender' assuming it terminates
      ** with SPACE (or NULL, which is an error, though).
      */

      sender = ch;

      if( (s = strchr(ch, ' ')))
	{
	  *s = '\0';
	  s++;
	  ch = s;
	}
		  
      i = 0;

      if (*sender && IsServer(cptr))
        {
          from = find_client(sender, (struct Client *) NULL);
          if (from == NULL)
            from = find_server(sender);

          para[0] = from->name;
          
          /* Hmm! If the client corresponding to the
           * prefix is not found--what is the correct
           * action??? Now, I will ignore the message
           * (old IRC just let it through as if the
           * prefix just wasn't there...) --msa
           */
          if (!from)
            {
              Debug((DEBUG_ERROR, "Unknown prefix (%s)(%s) from (%s)",
                     sender, pbuffer, cptr->name));
              ServerStats->is_unpf++;

              remove_unknown(cptr, sender, pbuffer);

              return (CLIENT_PARSE_ERROR);
            }
          if (from->from != cptr)
            {
              ServerStats->is_wrdi++;
              Debug((DEBUG_ERROR, "Message (%s) coming from (%s)",
                     pbuffer, cptr->name));

              return cancel_clients(cptr, from, pbuffer);
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
      return (CLIENT_PARSE_ERROR);
    }

  /*
  ** Extract the command code from the packet.  Point s to the end
  ** of the command code and calculate the length using pointer
  ** arithmetic.  Note: only need length for numerics and *all*
  ** numerics must have parameters and thus a space after the command
  ** code. -avalon
  *
  * ummm????
  */

  /* EOB is 3 chars long but is not a numeric */

  if( *(ch + 3) == ' '  && /* ok, lets see if its a possible numeric.. */
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
      int ii = 0;

      if( (s = strchr(ch, ' ')) )
        *s++ = '\0';

      mptr = hash_parse(ch);

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
          if (pbuffer[0] != '\0')
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
          return (CLIENT_PARSE_ERROR);
        }

      paramcount = mptr->parameters;
	  mpara = mptr->maxpara;
	  
      ii = bufend - ((s) ? s : ch);
      mptr->bytes += ii;
    }

  end = bufend - 1;
  
  /* XXX this should be done before parse() is called */
  if(*end == '\n') *end-- = '\0';
  if(*end == '\r') *end = '\0';

  i = 1;
  
  if (s)
    string_to_array(s, mpara, paramcount, end, &i, para);
   
  if (mptr == (struct Message *)NULL)
    return do_numeric(numeric, cptr, from, i, para);

  status = handle_command(mptr, cptr, from, i, para);

  if (cptr->fd < 0)
    return(CLIENT_EXITED);

  return status;
}

static int
handle_command(struct Message *mptr, struct Client *cptr, struct Client *from, int i, char *hpara[MAXPARA])
{
  MessageHandler handler = 0;
	
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
	
  handler = mptr->handlers[cptr->handler];
	
  /* check right amount of params is passed... --is */
  if (i < mptr->parameters)
    {
      if(IsServer(cptr))
         sendto_realops_flags(FLAGS_ALL, 
                "Not enough parameters for command %s from servers %s! (%d < %d)",
                mptr->cmd, cptr->name, i, mptr->parameters);
       sendto_one(cptr, form_str(ERR_NEEDMOREPARAMS),
                  me.name, BadPtr(hpara[0]) ? "*" : hpara[0], mptr->cmd);
       return 0;
    }

  return (*handler)(cptr, from, i, hpara);
}


/*
 * init_hash_parse()
 *
 * inputs       -
 * output       - NONE
 * side effects - MUST MUST be called at startup ONCE before
 *                any other keyword hash routine is used.
 *
 */
void clear_hash_parse()
{
  memset(msg_hash_table,0,sizeof(msg_hash_table));
}

/* mod_add_cmd
 *
 * inputs	- command name
 *		- pointer to struct Message
 * output	- none
 * side effects - load this one command name
 *		  msg->count msg->bytes is modified in place, in
 *		  modules address space. Might not want to do that...
 */
void
mod_add_cmd(struct Message *msg)
{
  struct MessageHash *ptr;
  struct MessageHash *last_ptr = NULL;
  struct MessageHash *new_ptr;
  int    msgindex;

  assert(msg != NULL);

  msgindex = hash(msg->cmd);

  for(ptr = msg_hash_table[msgindex]; ptr; ptr = ptr->next )
    {
      if (strcasecmp(msg->cmd,ptr->cmd) == 0)
	return;				/* Its already added */
      last_ptr = ptr;
    }

  new_ptr = (struct MessageHash *)MyMalloc(sizeof(struct MessageHash));

  new_ptr->next = NULL;
  DupString(new_ptr->cmd,msg->cmd);
  new_ptr->msg = msg;

  msg->count = 0;
  msg->bytes = 0;

  if(last_ptr == NULL)
    msg_hash_table[msgindex] = new_ptr;
  else
    last_ptr->next = new_ptr;
}

/* mod_del_cmd
 *
 * inputs	- command name
 * output	- none
 * side effects - unload this one command name
 */
void mod_del_cmd(struct Message *msg)
{
  struct MessageHash *ptr;
  struct MessageHash *last_ptr = NULL;
  int    msgindex;

  assert(msg != NULL);

  msgindex = hash(msg->cmd);

  for(ptr = msg_hash_table[msgindex]; ptr; ptr = ptr->next )
    {
      if(strcasecmp(msg->cmd,ptr->cmd) == 0)
	{
	  MyFree(ptr->cmd);
	  if(last_ptr != NULL)
	    last_ptr->next = ptr->next;
	  else
	    msg_hash_table[msgindex] = ptr->next;
	  MyFree(ptr);
	  return;
	}
    }
}

/* hash_parse
 *
 * inputs	- command name
 * output	- pointer to struct Message
 * side effects - 
 */
struct Message *hash_parse(char *cmd)
{
  struct MessageHash *ptr;
  int    msgindex;

  msgindex = hash(cmd);

  for(ptr = msg_hash_table[msgindex]; ptr; ptr = ptr->next )
    {
      if(strcasecmp(cmd,ptr->cmd) == 0)
	{
	  return(ptr->msg);
	}
    }
  return NULL;
}

/*
 * hash
 *
 * inputs	- char string
 * output	- hash index
 * side effects - NONE
 *
 * BUGS		- This a HORRIBLE hash function
 */
static int
hash(char *p)
{
  int hash_val=0;

  while(*p)
    {
      hash_val += ((int)(*p)&0xDF);
      p++;
    }

  return(hash_val % MAX_MSG_HASH);
}

/*
 * report_messages
 *
 * inputs	- pointer to client to report to
 * output	- NONE
 * side effects	- NONE
 */
void report_messages(struct Client *sptr)
{
  int i;
  struct MessageHash *ptr;

  for (i = 0; i < MAX_MSG_HASH; i++)
    {
      for (ptr = msg_hash_table[i]; ptr; ptr = ptr->next)
	{
	  assert(ptr->msg != NULL);
	  assert(ptr->cmd != NULL);
	  
	  sendto_one(sptr, form_str(RPL_STATSCOMMANDS),
		     me.name, sptr->name, ptr->cmd,
		     ptr->msg->count, ptr->msg->bytes);
	}
    }
}

/*
 * cancel_clients
 *
 * inputs	- 
 * output	- 
 * side effects	- 
 */
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

/*
 * remove_unknown
 *
 * inputs	- 
 * output	- 
 * side effects	- 
 */
static  void    remove_unknown(struct Client *cptr,
                               char *lsender,
                               char *lbuffer)
{
  if (!IsRegistered(cptr))
    return;

  if (IsClient(cptr))
    {
      sendto_realops_flags(FLAGS_DEBUG,
                 "Weirdness: Unknown client prefix (%s) from %s, Ignoring %s",
                         lbuffer,
                         get_client_name(cptr, FALSE), lsender);
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
  /* '.something'      is an ID      (KILL)
   * 'nodots'          is a nickname (KILL)
   * 'no.dot.at.start' is a server   (SQUIT)
   */
  if ((lsender[0] == '.') || !strchr(lsender, '.'))
    sendto_one(cptr, ":%s KILL %s :%s (%s(?) <- %s)",
               me.name, lsender, me.name, lsender,
               get_client_name(cptr, FALSE));
  else
    {
      sendto_realops_flags(FLAGS_DEBUG,
                           "Unknown prefix (%s) from %s, Squitting %s",
                           lbuffer, get_client_name(cptr, FALSE), lsender);
      sendto_one(cptr, ":%s SQUIT %s :(Unknown prefix (%s) from %s)",
                 me.name, lsender, lbuffer, get_client_name(cptr, FALSE));
    }
}



/*
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

  if (parc < 1 || !IsServer(sptr))
    return CLIENT_PARSE_ERROR;

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
  if (parc > 1)
    {
      char *t = buffer; /* Current position within the buffer */
      int i;
      int   tl;	/* current length of presently being built string in t */
      for (i = 2; i < (parc - 1); i++)
        {
          tl = ircsprintf(t," %s", parv[i]);
	  t += tl;
        }
      ircsprintf(t," :%s", parv[parc-1]);
    }
  if ((acptr = find_client(parv[1], (struct Client *)NULL)))
    {
      if (IsMe(acptr)) 
        {
          /*
           * We shouldn't get numerics sent to us,
           * any numerics we do get indicate a bug somewhere..
           */
          sendto_realops_flags(FLAGS_ALL,
                               "*** %s(via %s) sent a %s numeric to me?!?",
                               sptr->name, cptr->name, numeric);
          return 0;
        }
      else if (acptr->from == cptr) 
        {
          /* This message changed direction (nick collision?)
           * ignore it.
           */
          return 0;
        }
      /* Fake it for server hiding, if its our client */
      if(GlobalSetOptions.hide_server && MyClient(acptr) && !IsOper(acptr))
	sendto_one(acptr, ":%s %s %s%s", me.name, numeric, parv[1], buffer);
      else
        sendto_one(acptr, ":%s %s %s%s", sptr->name, numeric, parv[1], buffer);
      return 0;
      }
      else if ((chptr = hash_find_channel(parv[1], (struct Channel *)NULL)))
        sendto_channel_local(ALL_MEMBERS, chptr,
                             ":%s %s %s %s",
                              sptr->name,
                              numeric, RootChan(chptr)->chname, buffer);
  return 0;
}


/* 
 * m_not_oper
 * inputs	- 
 * output	-
 * side effects	- just returns a nastyogram to given user
 */
int m_not_oper(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  sendto_one(sptr, form_str(ERR_NOPRIVILEGES), me.name, parv[0]);
  return 0;
}

int m_unregistered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  /* bit of a hack.
   * I don't =really= want to waste a bit in a flag
   * number_of_nick_changes is only really valid after the client
   * is fully registered..
   */

  if( cptr->localClient->number_of_nick_changes == 0 )
    {
      sendto_one(cptr, ":%s %d * %s :Register first.",
		 me.name, ERR_NOTREGISTERED, parv[0]);
      cptr->localClient->number_of_nick_changes++;
    }
  return 0;
}

int m_registered(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  sendto_one(cptr, form_str(ERR_ALREADYREGISTRED),   
             me.name, parv[0]); 
  return 0;
}

int m_ignore(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  return 0;
}


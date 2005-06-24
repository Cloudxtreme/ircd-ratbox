/************************************************************************
 *   IRC - Internet Relay Chat, src/mtrie_conf.c
 *   Copyright (C) 1999 Diane Bruce db@db.net
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
 * mtrie_conf.c
 *
 * This is a modified trie, i.e. instead of a node for each character
 * it is a node on each part of a domain name.
 * This turns out to be a reasonable model for handling I lines and K lines
 * within one tree.
 * The tree will have degenerate portions if long domain names are added. 
 * i.e. I:NOMATCH::*@*.this.is.a.long.host.name.com
 * This shouldn't be a major problem.
 *
 *
 * What the code does, is utilize a stack. The stack keeps
 * track of "pieces" of the domain hostname seen as its parsed.
 * i.e. "*.koruna.varner.com" is broken up into pieces of
 *
 * *            - on stack
 * koruna       - on stack
 * varner       - on stack
 * com          - on stack
 *
 * by the time the host string is parsed, its broken up into pieces
 * on the stack with the TLD on the top of the stack.
 *
 *
 * Diane Bruce -db (db@db.net)
 *
 * $Id$
 */
#include "mtrie_conf.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "dline_conf.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "send.h"
#include "memory.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

static int stack_pointer;              /* dns piece stack */
static char *dns_stack[MAX_TLD_STACK];

static DOMAIN_LEVEL *trie_list=(DOMAIN_LEVEL *)NULL;
static DOMAIN_LEVEL *first_kline_trie_list=(DOMAIN_LEVEL *)NULL;
static int saved_stack_pointer;

static struct ConfItem *last_found_iline_aconf=(struct ConfItem *)NULL;

static struct ConfItem *unsortable_list_ilines = (struct ConfItem *)NULL;
static struct ConfItem *unsortable_list_klines = (struct ConfItem *)NULL;
static struct ConfItem *wild_card_ilines = (struct ConfItem *)NULL;
static struct ConfItem *ip_i_lines=(struct ConfItem *)NULL;

/* internally defined functions */

static int sortable(char *,char *);
static void tokenize_and_stack(char* tokenized_out, const char* host);
static void create_sub_mtrie(DOMAIN_LEVEL *,struct ConfItem *,int,char *);
static struct ConfItem *find_sub_mtrie(DOMAIN_LEVEL *, const char* host,
                                 const char* user, int);

static DOMAIN_PIECE *find_or_add_host_piece(DOMAIN_LEVEL *,int,char *);
static DOMAIN_PIECE *find_host_piece(DOMAIN_LEVEL *,int,char *,
                                     const char* host);
static struct ConfItem *find_wild_host_piece(DOMAIN_LEVEL *,int,char *, 
                                       const char* user);
static void find_or_add_user_piece(DOMAIN_PIECE *,struct ConfItem *,
				   int,char *);
static struct ConfItem *find_user_piece(DOMAIN_PIECE *,int,char *,
					const char* user);

static struct ConfItem* look_in_unsortable_ilines(const char* host,
						  const char* user);
static struct ConfItem* look_in_unsortable_klines(const char* host,
						  const char* user);
static struct ConfItem* find_wild_card_iline(const char* user);

static void report_sub_mtrie(struct Client *sptr,int,DOMAIN_LEVEL *);
static void clear_sub_mtrie(DOMAIN_LEVEL *);
static struct ConfItem *find_matching_ip_i_line(struct irc_inaddr *);

/* add_mtrie_conf_entry
 *
 * inputs       - 
 * output       - NONE
 * side effects -
 */
void add_mtrie_conf_entry(struct ConfItem *aconf,int flags)
{
  char tokenized_host[HOSTLEN+1];

  /* Sanity tests are always good */
  if(!aconf->host || !aconf->user)
    {
      free_conf(aconf);
      return;
    }

  stack_pointer = 0;

  switch(sortable(tokenized_host,aconf->host))
    {
    case 0:
    case 1:

      if(aconf->status & CONF_CLIENT)
        {
          if(unsortable_list_ilines)
            {
              aconf->next = unsortable_list_ilines;
              unsortable_list_ilines = aconf;
            }
          else
            unsortable_list_ilines = aconf;
        }
      else
        {
          if(unsortable_list_klines)
            {
              aconf->next = unsortable_list_klines;
              unsortable_list_klines = aconf;
            }
          else
            unsortable_list_klines = aconf;
        }
      return;
      break;

    case -2:
      if(aconf->status & CONF_CLIENT)
        {
          if(wild_card_ilines)
            {
              aconf->next = wild_card_ilines;
              wild_card_ilines = aconf;
            }
          else
            wild_card_ilines = aconf;
        }
      else
        {
          if(unsortable_list_klines)
            {
              aconf->next = unsortable_list_klines;
              unsortable_list_klines = aconf;
            }
          else
            unsortable_list_klines = aconf;
        }
      return;
      break;

    case -1:
      break;
    }

  if(trie_list == (DOMAIN_LEVEL *)NULL)
    {
      trie_list = (DOMAIN_LEVEL *)MyMalloc(sizeof(DOMAIN_LEVEL));
      memset((void *)trie_list,0,sizeof(DOMAIN_LEVEL));
    }

  /* now, start generating the sub mtrie tree */

  create_sub_mtrie(trie_list,aconf,flags,aconf->host);
}

/*
 * create_sub_mtrie
 *
 * inputs       - DOMAIN_LEVEL pointer
 *              - flags as integer
 *              - full hostname
 *              - username
 *              - reason (if kline)
 *
 * create a sub mtrie tree entry
 */

static void create_sub_mtrie(DOMAIN_LEVEL *cur_level,
                             struct ConfItem *aconf,
                             int flags,
                             char *host)
{
  char *cur_dns_piece;
  DOMAIN_PIECE *last_piece;
  DOMAIN_PIECE *cur_piece;

  cur_dns_piece = dns_stack[--stack_pointer];
  cur_piece = find_or_add_host_piece(cur_level,flags,cur_dns_piece);

  if(stack_pointer == 0)
    {
      (void)find_or_add_user_piece(cur_piece, aconf, flags, cur_dns_piece);
      return;
    }

  last_piece = cur_piece;

  cur_level = last_piece->next_level;

  if(cur_level == (DOMAIN_LEVEL *)NULL)
    {
      cur_level = (DOMAIN_LEVEL *)MyMalloc(sizeof(DOMAIN_LEVEL));
      memset((void *)cur_level,0,sizeof(DOMAIN_LEVEL));
      last_piece->next_level = cur_level;
    }
  create_sub_mtrie(cur_level,aconf,flags,host);
}


/* find_or_add_host_piece
 *
 * inputs       - pointer to current level 
 *              - piece of domain name being looked for
 *              - username
 * output       - pointer to next DOMAIN_PIECE to use
 * side effects -
 *
 */

static DOMAIN_PIECE *find_or_add_host_piece(DOMAIN_LEVEL *level_ptr,
                                     int flags,char *host_piece)
{
  DOMAIN_PIECE *piece_ptr;
  DOMAIN_PIECE *cur_piece;
  DOMAIN_PIECE *new_ptr;
  DOMAIN_PIECE *last_ptr;
  DOMAIN_PIECE *ptr;
  int pieceindex;

  pieceindex = *host_piece&(MAX_PIECE_LIST-1);
  piece_ptr = level_ptr->piece_list[pieceindex];

  if(piece_ptr == (DOMAIN_PIECE *)NULL)
    {
      cur_piece = (DOMAIN_PIECE *)MyMalloc(sizeof(DOMAIN_PIECE));
      memset((void *)cur_piece,0,sizeof(DOMAIN_PIECE));
      DupString(cur_piece->host_piece,host_piece);
      level_ptr->piece_list[pieceindex] = cur_piece;
      cur_piece->flags |= flags;
      return(cur_piece);
    }

  last_ptr = (DOMAIN_PIECE *)NULL;

  for(ptr=piece_ptr; ptr; ptr = ptr->next_piece)
    {
      if(!irccmp(ptr->host_piece,host_piece))
        {
          ptr->flags |= flags;
          return(ptr);
        }
      last_ptr = ptr;
    }

  if(last_ptr)
    {
      new_ptr = (DOMAIN_PIECE *)MyMalloc(sizeof(DOMAIN_PIECE));
      memset((void *)new_ptr,0,sizeof(DOMAIN_PIECE));
      DupString(new_ptr->host_piece,host_piece);

      last_ptr->next_piece = new_ptr;
      new_ptr->flags |= flags;
      return(new_ptr);
    }
  sendto_realops_flags(FLAGS_ALL,"Bug: in find_or_add_host_piece. yay.");
  return(NULL);
  /* NOT REACHED */
}

/* find_or_add_user_piece
 *
 * inputs       - pointer to current level 
 *              - piece of domain name being looked for
 *              - flags
 *              - aconf pointer to an struct ConfItem 
 * output       - pointer to next DOMAIN_LEVEL to use
 * side effects -
 *
 */

static void find_or_add_user_piece(DOMAIN_PIECE *piece_ptr,
                                   struct ConfItem *aconf,
                                   int flags,
                                   char *host_piece)
{
  DOMAIN_PIECE *ptr;
  DOMAIN_PIECE *new_ptr;
  DOMAIN_PIECE *last_ptr;
  struct ConfItem *found_aconf;
  char *user;

  last_ptr = (DOMAIN_PIECE *)NULL;
  user = aconf->user;

  if((user[0] == '*') && (user[1] == '\0') &&
    (host_piece[0] != '*'))
    {
  /* This is the case of *@*.some.host.tld */

      if(!(piece_ptr->wild_conf_ptr))
         {
      /* its empty, so add the given aconf, and return. done */

           aconf->status |= flags;
           piece_ptr->wild_conf_ptr = aconf;
           piece_ptr->flags |= flags;
           return;
         }
      else      /* not empty.... */
        {
          found_aconf = piece_ptr->wild_conf_ptr;

          if(found_aconf->flags & CONF_FLAGS_E_LINED)
            {
              /* if requested kline aconf =exactly=
               * matches an already present aconf
               * discard the requested K line
               */

              free_conf(aconf); /* toss it in the garbage */

              found_aconf->status |= flags;
              found_aconf->status &= ~CONF_KILL;
              piece_ptr->flags |= flags;
              piece_ptr->flags &= ~CONF_KILL;
              return;
            }
          else if(flags & CONF_KILL)
            {
              if(found_aconf->clients)
                found_aconf->status |= CONF_ILLEGAL;
              else
                free_conf(found_aconf);
              piece_ptr->wild_conf_ptr = aconf;
              piece_ptr->flags |= flags;
              return;
            }
          else if(flags & CONF_CLIENT)  /* I line replacement */
            {
              /* another I line/CONF_CLIENT exactly matching this
               * toss the new one into the garbage
               */
              free_conf(aconf); 
              found_aconf->status |= flags;
              piece_ptr->flags |= flags;
              return;
            }
        }
    }

  /*
   * if the piece_ptr->conf_ptr is NULL, then its the first piece_ptr
   * being added. The flags in piece_ptr will have already been set
   * but OR them in again anyway. aconf->status will also already have
   * the right flags. hint, these are optimization places for later.
   */

  for( ptr = piece_ptr; ptr; ptr = ptr->next_piece)
    {
      if(!ptr->conf_ptr)
        {
          aconf->status |= flags;       /* redundant -db */
          piece_ptr->flags |= flags;    /* redundant -db */
          ptr->conf_ptr = aconf;
          return;
        }

      found_aconf=ptr->conf_ptr;

      if( (match(ptr->host_piece,host_piece)) &&
          (!irccmp(found_aconf->user,user)) )
        {
          found_aconf->status |= flags;
          piece_ptr->flags |= flags;

          if(found_aconf->flags & CONF_FLAGS_E_LINED)
            {
              free_conf(aconf);         /* toss it in the garbage */
              found_aconf->status &= ~CONF_KILL;
              piece_ptr->flags &= ~CONF_KILL;
            }
          else if(found_aconf->status & CONF_CLIENT)
            {
              if(flags & CONF_CLIENT)
                {
                  free_conf(aconf);     /* toss new I line into the garbage */
                }
              else
                {
                  /* Its a K line */
                  if(found_aconf->clients)
                    found_aconf->status |= CONF_ILLEGAL;
                  else
                    free_conf(found_aconf);
                  ptr->conf_ptr = aconf;
                }
            }
          return;
        }
      last_ptr = ptr;
   }

  if(last_ptr)
    {
      new_ptr = (DOMAIN_PIECE *)MyMalloc(sizeof(DOMAIN_PIECE));
      memset((void *)new_ptr,0,sizeof(DOMAIN_PIECE));
      DupString(new_ptr->host_piece,host_piece);
      new_ptr->conf_ptr = aconf;
      last_ptr->next_piece = new_ptr;
    }
  else
    {
      sendto_realops_flags(FLAGS_ALL,"Bug in mtrie_conf.c last_ptr found NULL");
    }

  return;
}

/* find_matching_mtrie_conf
 *
 * inputs       - host name
 *              - user name
 * output       - pointer to struct ConfItem that corresponds to user/host pair
 *                or NULL if not found
 * side effects - NONE

 */

struct ConfItem* find_matching_mtrie_conf(const char* host, const char* user,
                                    struct irc_inaddr *ip)
{
  struct ConfItem *iline_aconf_unsortable = NULL;
  struct ConfItem *iline_aconf = NULL;
  struct ConfItem *kline_aconf = NULL;
  char tokenized_host[HOSTLEN + 1];
  int top_of_stack = 0;
  
  last_found_iline_aconf = NULL;
  
  /* Look in the unsortable i line list first, to find
   * special cases like *@*ppp* first
   */
  
  iline_aconf_unsortable = look_in_unsortable_ilines(host,user);
  
  /* an E lined I line is always accepted first
   * there is no point checking for a k-line
   */
  
  if(iline_aconf_unsortable &&
     (iline_aconf_unsortable->flags & CONF_FLAGS_E_LINED))
    return(iline_aconf_unsortable);
  
  if(trie_list)
    {
      stack_pointer = 0;
      tokenize_and_stack(tokenized_host, host);
      top_of_stack = stack_pointer;
      saved_stack_pointer = -1;
      first_kline_trie_list = (DOMAIN_LEVEL *)NULL;
      
      iline_aconf = find_sub_mtrie(trie_list, host, user, CONF_CLIENT);
    }
  if (iline_aconf)
    {
      if(iline_aconf->flags & CONF_FLAGS_E_LINED)
	return iline_aconf;
    }
  else
    {
      if (ip)
	{
	  iline_aconf = find_matching_ip_i_line(ip);

	  if (iline_aconf)
	    {
	      if (iline_aconf->flags & CONF_FLAGS_E_LINED)
		return iline_aconf;
	    }
	}
    }

  /* always default to an I line found in the unsortable list */

  if(iline_aconf_unsortable)
    iline_aconf = iline_aconf_unsortable;

  if (!iline_aconf)
    iline_aconf = find_wild_card_iline(user);

  /* If there is no I line, there is no point checking for a K line now
   * is there?
   */

  if(!iline_aconf)
    return((struct ConfItem *)NULL);
  else
    {
      if (iline_aconf->flags & CONF_FLAGS_E_LINED)
	return iline_aconf;
    }

  /* I have an I line, now I have to see if it gets
   * over-ruled by a K line somewhere else in the tree.
   * Note, that if first_kline_trie_list is non NULL
   * then trie_list had to have been non NULL as well.
   * call me paranoid.
   * Remember again, if any of the I lines
   * found also had an E line, I've already returned it
   * and not bothering with the K line search
   */

  /* ok, if there is a trie to use...
   * and if a possible branch was found the first time
   * I'll have a first_kline_trie_list saved.
   * its possible there won't be a branch of possible klines
   * in which case, I will have to start from the top of the tree again.
   */

  kline_aconf = (struct ConfItem *)NULL;

  if(trie_list)
    {
      if(first_kline_trie_list)
        {
          stack_pointer = saved_stack_pointer;
          kline_aconf = find_sub_mtrie(first_kline_trie_list,host,user,
                                       CONF_KILL);
        }
      else
        {
          stack_pointer = top_of_stack;
          kline_aconf = find_sub_mtrie(trie_list,host,user,CONF_KILL);
        }
    }

  /* I didn't find a kline in the mtrie, I'll try the unsortable list */

  if(!kline_aconf)
    kline_aconf = look_in_unsortable_klines(host,user);

  /* Try an IP hostname ban */

  if(!kline_aconf)
    kline_aconf = match_ip_Kline(ip,user);

  /* If this client isn't k-lined return the I line found */

  if(kline_aconf)
    return(kline_aconf);
  return(iline_aconf);
}

/*
 * find_sub_mtrie 
 * inputs       - pointer to current domain level 
 *              - hostname piece
 *              - username
 *              - flags flags to match for
 * output       - pointer to struct ConfItem or NULL
 * side effects -
 */

static struct ConfItem *find_sub_mtrie(DOMAIN_LEVEL *cur_level,
                                 const char* host, const char* user,int flags)
{
  DOMAIN_PIECE *cur_piece;
  char *cur_dns_piece;
  struct ConfItem *aconf=(struct ConfItem *)NULL;
  struct ConfItem *aconf_user=(struct ConfItem *)NULL;

  cur_dns_piece = dns_stack[--stack_pointer];

  if(!cur_dns_piece)
    return((struct ConfItem *)NULL);

  if(flags & CONF_KILL)
    {
      /* looking for CONF_KILL, look first for a kline at this level */
      /* This handles: "*foobar.com" type of kline "*.bar.com" type of kline
       */

      aconf = find_wild_host_piece(cur_level,flags,cur_dns_piece,user);
      if(aconf && aconf->status & CONF_KILL)
        return(aconf);

      /* no k-line yet, so descend deeper yet if possible */
      cur_piece = find_host_piece(cur_level,flags,cur_dns_piece,user);
      if(!cur_piece)
        return((struct ConfItem *)NULL);
    }
  else
    {
      aconf = find_wild_host_piece(cur_level,flags,cur_dns_piece,user);
      if(aconf)
        last_found_iline_aconf = aconf;

      /* looking for CONF_CLIENT, so descend deeper */
      cur_piece = find_host_piece(cur_level,flags,cur_dns_piece,user);

      if(!cur_piece)
	return last_found_iline_aconf;
    }

  if((cur_piece->flags & CONF_KILL) && (!first_kline_trie_list))
    {
      first_kline_trie_list = cur_level;
      saved_stack_pointer = stack_pointer+1;
    }

  if(stack_pointer == 0)
    {
      aconf_user=find_user_piece(cur_piece,flags,cur_dns_piece,user);
      return(aconf_user ? aconf_user : last_found_iline_aconf);
    }

  if(cur_piece->next_level)
    {
      cur_level = cur_piece->next_level;
      return(find_sub_mtrie(cur_level,host,user,flags));
    }
  else
    {
      if((aconf = find_user_piece(cur_piece,flags,cur_dns_piece,user)))
	if (match(aconf->host, host))
	  return aconf;
      if((aconf = find_wild_host_piece(cur_level,flags,cur_dns_piece,user)))
        return(aconf);
      return(last_found_iline_aconf);
    }

  /* NOT REACHED */
  return((struct ConfItem *)NULL);
}

/* find_user_piece
 *
 * inputs       - pointer to current level 
 *              - int flags
 *              - piece of domain name being looked for
 *              - username
 * output       - pointer to next DOMAIN_LEVEL to use
 * side effects -
 *
 */

static struct ConfItem *find_user_piece(DOMAIN_PIECE *piece_ptr, int flags,
                     char *host_piece, const char* user)
{
  DOMAIN_PIECE *ptr;
  struct ConfItem *aconf=NULL;
  struct ConfItem *first_aconf=NULL;
  struct ConfItem *wild_aconf=NULL;

  wild_aconf = piece_ptr->wild_conf_ptr;

  for(ptr=piece_ptr; ptr; ptr=ptr->next_piece)
    {
      if((aconf=ptr->conf_ptr))
        {
          if( (match(ptr->host_piece,host_piece)) &&
              (aconf->status & flags) )
            {
              if(match(aconf->user,user))
                {
                  first_aconf = aconf;
                  if(first_aconf->status & CONF_ELINE)
                    break;
                }

            }
        }
    }

  /* Propogate a kill "downwards" from *@*.host.tld if found,
   * unless an aconf is found with an E line 
   */
  if(wild_aconf)
    {
      if (wild_aconf->status & CONF_KILL)
        {
          if(first_aconf && (first_aconf->status & CONF_ELINE))
            {
              return(first_aconf);
            }
        }
      /* Ditto with E line.
       * Propogate an E line "downwards" from *@*.host.tld if found.
       */
      else if(wild_aconf->status & CONF_ELINE)
        {
          if(first_aconf && (first_aconf->status & CONF_KILL))
            {
              first_aconf->status &= ~CONF_KILL;
              first_aconf->status |= CONF_ELINE;
              return(first_aconf);
            }
        }
      return(wild_aconf);
    }
  /* its up to first_aconf, since wild_aconf is NULL */
  return(first_aconf);
}

/* find_host_piece
 *
 * inputs       - pointer to current level 
 *              - piece of domain name being looked for
 *              - usename
 * output       - pointer to next DOMAIN_LEVEL to use
 * side effects -
 *
 */

static DOMAIN_PIECE *find_host_piece(DOMAIN_LEVEL *level_ptr,int flags,
                                     char *host_piece, const char* user)
{
  DOMAIN_PIECE *ptr;
  DOMAIN_PIECE *piece_ptr;
  int pieceindex;

  if(!level_ptr)
    return((DOMAIN_PIECE *)NULL);
  
  pieceindex = *host_piece&(MAX_PIECE_LIST-1);
  piece_ptr = level_ptr->piece_list[pieceindex];

  for(ptr=piece_ptr;ptr;ptr=ptr->next_piece)
    {
      if(!irccmp(ptr->host_piece,host_piece) && (ptr->flags & flags))
        {
          return(ptr);
        }
    }

  return((DOMAIN_PIECE *)NULL);
}

/* find_wild_host_piece
 *
 * inputs       - pointer to current level 
 *              - piece of domain name being looked for
 *              - usename
 * output       - struct ConfItem or NULL
 * side effects -
 * 
 * Eventually the mtrie code could be extended to deal with
 * such cases as "*foo*.some.host.com" ,
 * the mtrie handling the sortable portion down to the "*foo*"
 * portion, this would reduce the length of the unsortable link list,
 * speeding up this code. I'll do that later, or someone else can.
 * This would necessitate logic changes in sortable()
 *
 */
static struct ConfItem *find_wild_host_piece(DOMAIN_LEVEL *level_ptr,int flags,
                                     char *host_piece, const char* user)
{
  struct ConfItem *first_aconf=NULL;
  struct ConfItem *wild_aconf=NULL;
  struct ConfItem *aconf=NULL;
  DOMAIN_PIECE *ptr;
  DOMAIN_PIECE *pptr;
  DOMAIN_PIECE *piece_ptr;
  int pieceindex;
  
  pieceindex = '*'&(MAX_PIECE_LIST-1);
  piece_ptr = level_ptr->piece_list[pieceindex];
  
  for(ptr=piece_ptr;ptr;ptr=ptr->next_piece)
    {
      if(match(ptr->host_piece,host_piece) && (ptr->flags & flags))
        {
          first_aconf = (struct ConfItem *)NULL;
          wild_aconf = (struct ConfItem *)NULL;

          for(pptr = ptr; pptr; pptr=pptr->next_piece)
            {
              if(pptr->conf_ptr)
                {
                  aconf= pptr->conf_ptr;
                  if( (aconf->status & flags) &&
                      (match(pptr->host_piece,host_piece)) )

                    {
                      if(match(aconf->user,user))
                        first_aconf = aconf;
                    }
                }

              if(pptr->wild_conf_ptr)
                {
                  aconf= pptr->wild_conf_ptr;
                  if( (aconf->status & flags) &&
                      (match(pptr->host_piece,host_piece)) )
                      
		    {
                       wild_aconf = aconf;
		    }
		}
            }
        }
    }
  
  /* Propogate a kill "downwards" from *@*.host.tld if found,
   * unless an aconf is found with an E line 
   */
  if(wild_aconf)
    {
      if (wild_aconf->status & CONF_KILL)
        {
          if(first_aconf && (first_aconf->status & CONF_ELINE))
            {
              return(first_aconf);
            }
        }
      /* Ditto with E line.
       * Propogate an E line "downwards" from *@*.host.tld if found.
       */
      else if(wild_aconf->status & CONF_ELINE)
        {
          if(first_aconf)
            {
              first_aconf->status &= ~CONF_KILL;
              first_aconf->status |= CONF_ELINE;
              return(first_aconf);
            }
        }
      return(wild_aconf);
    }
  /* its up to first_aconf since wild_aconf is NULL */

  return(first_aconf);
}



/*
 * This function decides whether a string may be used with ordered lists.
 * -1 means the string has to be reversed. A string that can't be put in
 * an ordered list yields 0 (yes, a piece of Soleil)
 *
 * a little bit rewritten, and yes. I tested it. it is faster.
 *
 * modified for use with mtrie_conf.c
 */
static int sortable(char *tokenized,char *p)
{
  int  state=0;
  char *d;              /* destination */

  if (!p)
    return(0);                  /* NULL patterns aren't allowed in ordered
                                 * lists (will have to use linear functions)
                                 * -Sol
                                 *
                                 * uh, if its NULL then nothing can be done
                                 * with it
                                 */

  if (strchr(p, '?'))
    return(0);                  /* reject strings with '?' as non-sortable
                                 *  whoever uses '?' patterns anyway ? -Sol
                                 */

  d = tokenized;

  if((*p == '*') && (*(p+1) == '\0'))   /* special case a single'*' */
    return(-2); 


  FOREVER
    {
      switch(state)
        {
        case 0:
          if(*p == '*')
            {
              *d = *p;
              state = 1;        /* Go into state 1 if first char is '*' */
            }
          else
            {
              *d = *p;          /* Go into state 2 if first char is not '*' */
              state = 2;
            }
          break;

        case 1:                 /* state 1, sit here until '\0' or '*' seen */
          if(*p == '\0')        
            {
              *d = '\0';
              dns_stack[stack_pointer++] = tokenized;
              return(-1);       /* followed by null terminator is sortable */
            }
          else if(*p == '*')    /* '*' followed by another '*' is unsortable */
            return(0);
          else if(*p == '.')    /* this is a "*.foo" type kline */
            {
              *d = '\0';
              dns_stack[stack_pointer++] = tokenized;
              tokenized = d+1;
            }
          else
            *d = *p;            /* just keep copying, building this token */
          break;
         
        case 2:                 /* state 2, sit here if no '*' seen and */
          if(*p == '\0')        
            {
              *d = '\0';        /* if null terminator seen, its sortable */
              dns_stack[stack_pointer++] = tokenized;
              return(-1);       
            }
          else if(*p == '*')    /* its "blah*blah" or "blah*"
                                   which is not sortable */
            {

              return(0);
            }
          else if(*p == '.')    /* push another piece on stack */
            {
              *d = '\0';
              dns_stack[stack_pointer++] = tokenized;
              tokenized = d+1;
            }
          else
            *d = *p;            /* just keep copying, building this token */
          break;
         
        default:
          return(0);
          break;
        }
      d++;
      p++;
    }
  return 0;
}

/*
 * tokenize_and_stack
 *
 * inputs       - pointer to tokenized output
 * output       - none
 * side effects -
 * This function tokenizes the input, reversing it onto
 * a dns stack. Basically what sortable() does, but without
 * scanning for sortability.
 */

static void tokenize_and_stack(char* tokenized, const char* p)
{
  char* d = tokenized;
  assert(0 != d);

  if (!p)
    return;

  *d = '\0';

  while (*p)
    {
      if(*p == '.')
        {
          *d = '\0';
          dns_stack[stack_pointer++] = tokenized;
          tokenized = d+1;
        }
      else
        *d = *p;

      d++;
      p++;
    }
  *d = '\0';
  dns_stack[stack_pointer++] = tokenized;
}


/*
 * find_wild_card_iline()
 *
 * inputs       - username
 * output       - struct ConfItem pointer or NULL
 * side effects -
 *
 * scan the link list of top level domain *
 */

static struct ConfItem* find_wild_card_iline(const char* user)
{
  struct ConfItem *found_conf;

  for(found_conf=wild_card_ilines;found_conf;found_conf=found_conf->next)
    {
      if(match(found_conf->user,user))
        return(found_conf);
    }
  return((struct ConfItem *)NULL);
}

/*
 * report_mtrie_conf_links()
 *
 * inputs       - struct Client pointer
 *              - flags type either CONF_KILL or CONF_CLIENT
 * output       - none
 * side effects - report I lines/K lines found in the mtrie
 */

void report_mtrie_conf_links(struct Client *sptr, int flags)
{
  struct ConfItem *found_conf;
  char *name, *host, *pass, *user, *classname;
  int  port;

  if(trie_list)
    report_sub_mtrie(sptr,flags,trie_list);

  /* If requesting I lines do this */
  if(flags & CONF_CLIENT)
    {
      for(found_conf = unsortable_list_ilines;
          found_conf;found_conf=found_conf->next)
        {
          /* Non local opers do not need to know about
           * I lines that do spoofing 
           */
          if(!(MyConnect(sptr) && IsOper(sptr)) &&
             IsConfDoSpoofIp(found_conf))
            continue;

          get_printable_conf(found_conf, &name, &host, &pass, &user, &port,
			     &classname);

          sendto_one(sptr, form_str(RPL_STATSILINE), me.name,
                     sptr->name,
                     'I',
                     name,
                     show_iline_prefix(sptr,found_conf,user),
                     host,
                     port,
                     classname);
        }

      for(found_conf = wild_card_ilines;
          found_conf;found_conf=found_conf->next)
        {
          get_printable_conf(found_conf, &name, &host, &pass, &user, &port,
			     &classname);

          sendto_one(sptr, form_str(RPL_STATSILINE), me.name,
                     sptr->name,
                     'I',
                     name,
                     show_iline_prefix(sptr,found_conf,user),
                     host,
                     port,
                     classname);
        }

      for(found_conf = ip_i_lines;
          found_conf;found_conf=found_conf->next)
        {
          get_printable_conf(found_conf, &name, &host, &pass, &user, &port,
			     &classname );

          if(!(found_conf->status&CONF_CLIENT))
            continue;

          sendto_one(sptr, form_str(RPL_STATSILINE), me.name,
                     sptr->name,
                     'I',
                     name,
                     show_iline_prefix(sptr,found_conf,name),
                     host,
                     port,
                     classname);
        }
    }
  else
    {
      report_ip_Klines(sptr);

      for(found_conf = unsortable_list_klines;
          found_conf;found_conf=found_conf->next)
        {
          get_printable_conf(found_conf, &name, &host, &pass, &user, &port,
			     &classname);

          sendto_one(sptr, form_str(RPL_STATSKLINE), me.name,
                     sptr->name, 'K', host,
                     user, pass);
        }
    }
}

/*
 * show_iline_prefix()
 *
 * inputs       - pointer to struct Client requesting output
 *              - pointer to struct ConfItem 
 *              - name to which iline prefix will be prefixed to
 * output       - pointer to static string with prefixes listed in ascii form
 * side effects - NONE
 */
char *show_iline_prefix(struct Client *sptr,struct ConfItem *aconf,char *name)
{
  static char prefix_of_host[MAXPREFIX];
  char *prefix_ptr;

  prefix_ptr = prefix_of_host;
 
  if (IsNoTilde(aconf))
    *prefix_ptr++ = '-';
  if (IsLimitIp(aconf))
    *prefix_ptr++ = '!';
  if (IsNeedIdentd(aconf))
    *prefix_ptr++ = '+';
  if (IsPassIdentd(aconf))
    *prefix_ptr++ = '$';
  if (IsNoMatchIp(aconf))
    *prefix_ptr++ = '%';
  if (IsConfDoSpoofIp(aconf))
    *prefix_ptr++ = '=';

  if(IsOper(sptr))
    if (IsConfElined(aconf))
      *prefix_ptr++ = '^';

  if(IsOper(sptr))
    if (IsConfFlined(aconf))
      *prefix_ptr++ = '>';

  if(IsOper(sptr)) 
    if (IsConfIdlelined(aconf))
      *prefix_ptr++ = '<';

  *prefix_ptr = '\0';

  strncat(prefix_of_host,name,MAXPREFIX);
  return(prefix_of_host);
}

/*
 * report_sub_mtrie()
 * inputs       - pointer to DOMAIN_LEVEL (mtrie subtree)
 * output       - none
 * side effects -
 * report sub mtrie entries recursively
 */

static void report_sub_mtrie(struct Client *sptr, int flags, DOMAIN_LEVEL *dl_ptr)
{
  DOMAIN_PIECE *dp_ptr;
  struct ConfItem *aconf;
  int i;
  char *name, *host, *pass, *user, *classname;
  int  port;

  if(!dl_ptr)
    return;

  for(i=0; i < MAX_PIECE_LIST; i++)
    {
      for(dp_ptr=dl_ptr->piece_list[i];dp_ptr; dp_ptr = dp_ptr->next_piece)
        {
          report_sub_mtrie(sptr,flags,dp_ptr->next_level);
          if(dp_ptr->conf_ptr)
            {
              /* Only show desired I/K lines */
              aconf = dp_ptr->conf_ptr;

              if(aconf->status & flags)
                {
                  get_printable_conf(aconf, &name, &host, &pass, &user,
                                        &port, &classname);

                  if (aconf->status == CONF_KILL)
                    {
                      sendto_one(sptr, form_str(RPL_STATSKLINE),
                                 me.name,
                                 sptr->name,
                                 'K',
                                 host,
                                 user,
                                 pass);
                    }
                  else
                    {
                      /* Non local opers do not need to know about
                       * I lines that do spoofing
                       */
                      if(!(MyConnect(sptr) && IsOper(sptr))
                         && IsConfDoSpoofIp(aconf))
                        continue;

                      sendto_one(sptr, form_str(RPL_STATSILINE),
                                 me.name,
                                 sptr->name,
                                 'I',
                                 name,
                                 show_iline_prefix(sptr,aconf,user),
                                 host,
                                 port,
                                 classname);
                    }
                }
            }

          if(dp_ptr->wild_conf_ptr)
            {
              aconf = dp_ptr->wild_conf_ptr;

              if(aconf->status & flags)
                {
                  get_printable_conf(aconf, &name, &host, &pass,
                                        &user, &port, &classname);

                  if (aconf->status == CONF_KILL)
                    {
                      sendto_one(sptr, form_str(RPL_STATSKLINE),
                                 me.name,
                                 sptr->name,
                                 'K',
                                 host,
                                 user,
                                 pass);
                    }
                  else
                    {
                      /* Non local opers do not need to know about
                       * I lines that do spoofing
                       */
                      if(!(MyConnect(sptr) && IsOper(sptr))
                         && IsConfDoSpoofIp(aconf))
                        continue;

                      sendto_one(sptr, form_str(RPL_STATSILINE),
                                 me.name,
                                 sptr->name,
                                 'I',
                                 name,
                                 show_iline_prefix(sptr,aconf,user),
                                 host,
                                 port,
                                 classname);
                    }
                }
            }
        }
    }
}

/*
 * clear_mtrie_conf_links()
 *
 * inputs       - NONE
 * output       - NONE
 * side effects -
 * Clear out the mtrie list and the unsortable list (recursively)
 */

void clear_mtrie_conf_links()
{
  struct ConfItem *found_conf;
  struct ConfItem *found_conf_next;

  if(trie_list)
    {
      clear_sub_mtrie(trie_list);
      trie_list = (DOMAIN_LEVEL *)NULL;
    }

  for(found_conf=unsortable_list_ilines;
      found_conf;found_conf=found_conf_next)
    {
      found_conf_next = found_conf->next;

      /* this is an I line list */

      if(found_conf->clients)
        found_conf->status |= CONF_ILLEGAL;
      else
        free_conf(found_conf);
    }
  unsortable_list_ilines = (struct ConfItem *)NULL;

  for(found_conf=unsortable_list_klines;
      found_conf;found_conf=found_conf_next)
    {
      found_conf_next = found_conf->next;
      free_conf(found_conf);
    }
  unsortable_list_klines = (struct ConfItem *)NULL;

  for(found_conf=wild_card_ilines;
      found_conf;found_conf=found_conf_next)
    {
      found_conf_next = found_conf->next;
      if (found_conf->clients)
        found_conf->status |= CONF_ILLEGAL;
      else
        free_conf(found_conf);
    }
  wild_card_ilines = (struct ConfItem *)NULL;

  for(found_conf = ip_i_lines; found_conf;
      found_conf = found_conf_next)
    {
      found_conf_next = found_conf->next;

      /* The aconf's pointed to by each ip entry here,
       * have already been cleared out of the mtrie tree above.
       */
      if(found_conf->clients)
        found_conf->status |= CONF_ILLEGAL;
      else
        free_conf(found_conf);
    }
  ip_i_lines = (struct ConfItem *)NULL;
}

/*
 * clear_sub_mtrie
 *
 * inputs       - DOMAIN_LEVEL pointer
 * output       - none
 * side effects - this portion of the mtrie is cleared
 */

static void clear_sub_mtrie(DOMAIN_LEVEL *dl_ptr)
{
  DOMAIN_PIECE *dp_ptr;
  DOMAIN_PIECE *next_dp_ptr;
  struct ConfItem *conf_ptr;
  int i;

  if(!dl_ptr)
    return;

  for(i=0; i < MAX_PIECE_LIST; i++)
    {
      dp_ptr = dl_ptr->piece_list[i];
      dl_ptr->piece_list[i] = NULL;

      for(;dp_ptr; dp_ptr = next_dp_ptr)
        {
          clear_sub_mtrie(dp_ptr->next_level);

          if(dp_ptr->wild_conf_ptr)
            {
              conf_ptr = dp_ptr->wild_conf_ptr;
              if( (conf_ptr->status & CONF_CLIENT) && conf_ptr->clients)
                conf_ptr->status |= CONF_ILLEGAL;
              else
                free_conf(conf_ptr);
            }

          if(dp_ptr->conf_ptr)
            {
              conf_ptr = dp_ptr->conf_ptr;
              if( (conf_ptr->status & CONF_CLIENT) && conf_ptr->clients)
                conf_ptr->status |= CONF_ILLEGAL;
              else
                free_conf(conf_ptr);
            }
            
          next_dp_ptr = dp_ptr->next_piece;
          MyFree(dp_ptr->host_piece);
          MyFree(dp_ptr);
        }
    }
  MyFree(dl_ptr);
}

/*
 * find_matching_ip_i_line()
 * 
 * inputs       - unsigned long IP in host order
 * output       - struct ConfItem pointer if found, NULL if not
 * side effects -
 * search the ip_i_line link list
 * looking for a match, return struct ConfItem pointer if found 
 */

static struct ConfItem *find_matching_ip_i_line(struct irc_inaddr *host_ip)
{
  struct ConfItem *aconf;
  /* XXX: This is broken for IPv6 */
  
  for( aconf = ip_i_lines; aconf; aconf = aconf->next)
    {
      if(( ((struct sockaddr_in *)host_ip)->sin_addr.s_addr & aconf->ip_mask) == aconf->ip)
        return(aconf);
    }
  return((struct ConfItem *)NULL);
}

/*
 * add_ip_Iline()
 *
 * inputs       -
 * output       - NONE
 * side effects -
 */
void add_ip_Iline( struct ConfItem *aconf )
{
  aconf->next = ip_i_lines;
  ip_i_lines = aconf;
}

/*
 * look_in_unsortable_ilines()
 *
 * inputs       - host name
 *              - username
 * output       - struct ConfItem pointer or NULL
 * side effects - scan the link list of unsortable iline patterns
 */

static struct ConfItem *look_in_unsortable_ilines(const char* host, const char* user)
{
  struct ConfItem *found_conf;

  for(found_conf=unsortable_list_ilines;found_conf;found_conf=found_conf->next)
    {
      if(match(found_conf->host,host) &&
         match(found_conf->user,user))
        {
            return(found_conf);
        }
    }
  return((struct ConfItem *)NULL);
}

/*
 * look_in_unsortable_klines()
 *
 * inputs       - host name
 *              - username
 * output       - struct ConfItem pointer or NULL
 * side effects - scan the link list of unsortable kline patterns
 */

static struct ConfItem *look_in_unsortable_klines(const char* host, const char* user)
{
  struct ConfItem *found_conf;

  for(found_conf=unsortable_list_klines;found_conf;found_conf=found_conf->next)
    {
      if(match(found_conf->host,host) &&
         match(found_conf->user,user))
        return(found_conf);
    }
  return((struct ConfItem *)NULL);
}

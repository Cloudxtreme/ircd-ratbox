/*
 *  ircd-hybrid: an advanced Internet Relay Chat Daemon(ircd).
 *  tools.h: Header for the various tool functions.
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

#ifndef __TOOLS_H__
#define __TOOLS_H__


/*
 * double-linked-list stuff
 */
typedef struct _dlink_node dlink_node;
typedef struct _dlink_list dlink_list;

struct _dlink_node {
    void *data;
    dlink_node *prev;
    dlink_node *next;

};
  
struct _dlink_list {
    dlink_node *head;
    dlink_node *tail;
};

void
dlinkAdd(void *data, dlink_node * m, dlink_list * list);

void
dlinkAddBefore(dlink_node *b, void *data, dlink_node *m, dlink_list *list);

void
dlinkAddTail(void *data, dlink_node *m, dlink_list *list);

void
dlinkDelete(dlink_node *m, dlink_list *list);

void
dlinkMoveList(dlink_list *from, dlink_list *to);

int
dlink_list_length(dlink_list *m);

dlink_node *
dlinkFind(dlink_list *m, void *data);

#ifndef NDEBUG
void mem_frob(void *data, int len);
#else
#define mem_frob(x, y) 
#endif

/*
 * The functions below are included for the sake of inlining
 * hopefully this will speed up things just a bit
 * 
 */


/* 
 * dlink_ routines are stolen from squid, except for dlinkAddBefore,
 * which is mine.
 *   -- adrian
 */
extern inline void
dlinkAdd(void *data, dlink_node * m, dlink_list * list)
{
 m->data = data;
 m->next = list->head;
 /* Assumption: If list->tail != NULL, list->head != NULL */
 if (list->head != NULL)
   list->head->prev = m;
 else if (list->tail == NULL)
   list->tail = m;
 list->head = m;
}

extern inline void
dlinkAddBefore(dlink_node *b, void *data, dlink_node *m, dlink_list *list)
{
    /* Shortcut - if its the first one, call dlinkAdd only */
    if (b == list->head)
        dlinkAdd(data, m, list);
    else {
        m->data = data;
        b->prev->next = m;
        m->prev = b->prev;
        b->prev = m; 
        m->next = b;
    }
}

extern inline void
dlinkAddTail(void *data, dlink_node *m, dlink_list *list)
{
 m->data = data;
 m->next = NULL;
 m->prev = list->tail;
 /* Assumption: If list->tail != NULL, list->head != NULL */
 if (list->tail != NULL)
   list->tail->next = m;
 else if (list->head == NULL)
   list->head = m;
 list->tail = m;
}

/* Execution profiles show that this function is called the most
 * often of all non-spontaneous functions. So it had better be
 * efficient. */
extern inline void
dlinkDelete(dlink_node *m, dlink_list *list)
{
 /* Assumption: If m->next == NULL, then list->tail == m
  *      and:   If m->prev == NULL, then list->head == m
  */
 if (m->next)
   m->next->prev = m->prev;
 else
   list->tail = m->prev;
 if (m->prev)
   m->prev->next = m->next;
 else
   list->head = m->next;
 /* XXX - does this ever matter? */
 m->next = m->prev = NULL;
}


/* 
 * dlink_list_length
 * inputs	- pointer to a dlink_list
 * output	- return the length (>=0) of a chain of links.
 * side effects	-
 */
extern inline int dlink_list_length(dlink_list *list)
{
  dlink_node *ptr;
  int   count = 0;

  for (ptr = list->head; ptr; ptr = ptr->next)
    count++;
  return count;
}

/*
 * dlinkFind
 * inputs	- list to search 
 *		- data
 * output	- pointer to link or NULL if not found
 * side effects	- Look for ptr in the linked listed pointed to by link.
 */
extern inline dlink_node *dlinkFind(dlink_list *list, void * data )
{
  dlink_node *ptr;

  for (ptr = list->head; ptr; ptr = ptr->next)
    {
      if (ptr->data == data)
	return (ptr);
    }
  return (NULL);
}

extern inline void
dlinkMoveList(dlink_list *from, dlink_list *to)
{
  /* There are three cases */
  /* case one, nothing in from list */

    if(from->head == NULL)
      return;

  /* case two, nothing in to list */
  /* actually if to->head is NULL and to->tail isn't, thats a bug */

    if(to->head == NULL) {
       to->head = from->head;
       to->tail = from->tail;
       from->head = from->tail = NULL;
       return;
    }

  /* third case play with the links */

    from->tail->next = to->head;
    from->head->prev = to->head->prev;
    to->head->prev = from->tail;
    to->head = from->head;
    from->head = from->tail = NULL;

  /* I think I got that right */
}


#endif /* __TOOLS_H__ */

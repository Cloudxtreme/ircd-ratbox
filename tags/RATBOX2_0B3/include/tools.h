/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.h: Header for the various tool functions.
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

#ifndef __TOOLS_H__
#define __TOOLS_H__


/*
 * double-linked-list stuff
 */
typedef struct _dlink_node dlink_node;
typedef struct _dlink_list dlink_list;

struct _dlink_node
{
	void *data;
	dlink_node *prev;
	dlink_node *next;

};

struct _dlink_list
{
	dlink_node *head;
	dlink_node *tail;
	unsigned long length;
};

dlink_node *make_dlink_node(void);
void free_dlink_node(dlink_node * lp);
void init_dlink_nodes(void);

#ifndef NDEBUG
void mem_frob(void *data, int len);
#else
#define mem_frob(x, y)
#endif

/* This macros are basically swiped from the linux kernel
 * they are simple yet effective
 */

/*
 * Walks forward of a list.  
 * pos is your node
 * head is your list head
 */
#define DLINK_FOREACH(pos, head) for (pos = (head); pos != NULL; pos = pos->next)

/*
 * Walks forward of a list safely while removing nodes 
 * pos is your node
 * n is another list head for temporary storage
 * head is your list head
 */
#define DLINK_FOREACH_SAFE(pos, n, head) for (pos = (head), n = pos ? pos->next : NULL; pos != NULL; pos = n, n = pos ? pos->next : NULL)

#define DLINK_FOREACH_PREV(pos, head) for (pos = (head); pos != NULL; pos = pos->prev)


/* Returns the list length */
#define dlink_list_length(list) (list)->length
#define dlink_move_list(oldlist, newlist, node)

#define dlinkAddAlloc(data, list) dlinkAdd(data, make_dlink_node(), list)
#define dlinkAddTailAlloc(data, list) dlinkAddTail(data, make_dlink_node(), list)
#define dlinkDestroy(node, list) do { dlinkDelete(node, list); free_dlink_node(node); } while(0)


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

static inline void
dlinkMoveNode(dlink_node * m, dlink_list * oldlist, dlink_list * newlist)
{
	/* Assumption: If m->next == NULL, then list->tail == m
	 *      and:   If m->prev == NULL, then list->head == m
	 */
	assert(m != NULL);
	assert(oldlist != NULL);
	assert(newlist != NULL);

	if(m->next)
		m->next->prev = m->prev;
	else
		oldlist->tail = m->prev;

	if(m->prev)
		m->prev->next = m->next;
	else
		oldlist->head = m->next;

	m->prev = NULL;
	m->next = newlist->head;
	if(newlist->head != NULL)
		newlist->head->prev = m;
	else if(newlist->tail == NULL)
		newlist->tail = m;
	newlist->head = m;

	oldlist->length--;
	newlist->length++;
}

static inline void
dlinkAdd(void *data, dlink_node * m, dlink_list * list)
{
	assert(data != NULL);
	assert(m != NULL);
	assert(list != NULL);
	m->data = data;
	m->next = list->head;

	/* Assumption: If list->tail != NULL, list->head != NULL */
	if(list->head != NULL)
		list->head->prev = m;
	else if(list->tail == NULL)
		list->tail = m;

	list->head = m;
	list->length++;
}

static inline void
dlinkAddBefore(dlink_node * b, void *data, dlink_node * m, dlink_list * list)
{
	assert(b != NULL);
	assert(data != NULL);
	assert(m != NULL);
	assert(list != NULL);

	/* Shortcut - if its the first one, call dlinkAdd only */
	if(b == list->head)
	{
		dlinkAdd(data, m, list);
	}
	else
	{
		m->data = data;
		b->prev->next = m;
		m->prev = b->prev;
		b->prev = m;
		m->next = b;
		list->length++;
	}
}

static inline void
dlinkAddTail(void *data, dlink_node * m, dlink_list * list)
{
	assert(m != NULL);
	assert(list != NULL);
	assert(data != NULL);
	m->data = data;
	m->next = NULL;
	m->prev = list->tail;

	/* Assumption: If list->tail != NULL, list->head != NULL */
	if(list->tail != NULL)
		list->tail->next = m;
	else if(list->head == NULL)
		list->head = m;

	list->tail = m;
	list->length++;
}

/* Execution profiles show that this function is called the most
 * often of all non-spontaneous functions. So it had better be
 * efficient. */
static inline void
dlinkDelete(dlink_node * m, dlink_list * list)
{
	assert(m != NULL);
	assert(list != NULL);

	/* Assumption: If m->next == NULL, then list->tail == m
	 *      and:   If m->prev == NULL, then list->head == m
	 */
	if(m->next)
		m->next->prev = m->prev;
	else
		list->tail = m->prev;

	if(m->prev)
		m->prev->next = m->next;
	else
		list->head = m->next;

	m->next = m->prev = NULL;
	list->length--;
}

static inline dlink_node *
dlinkFindDelete(dlink_list * list, void *data)
{
	dlink_node *m;
	assert(list != NULL);
	assert(data != NULL);

	DLINK_FOREACH(m, list->head)
	{
		if(m->data != data)
			continue;

		if(m->next)
			m->next->prev = m->prev;
		else
			list->tail = m->prev;

		if(m->prev)
			m->prev->next = m->next;
		else
			list->head = m->next;

		m->next = m->prev = NULL;
		list->length--;
		return m;
	}

	return NULL;
}

static inline int
dlinkFindDestroy(dlink_list * list, void *data)
{
	void *ptr;

	assert(list != NULL);
	assert(data != NULL);

	ptr = dlinkFindDelete(list, data);

	if(ptr != NULL)
	{
		free_dlink_node(ptr);
		return 1;
	}
	return 0;
}

/*
 * dlinkFind
 * inputs	- list to search 
 *		- data
 * output	- pointer to link or NULL if not found
 * side effects	- Look for ptr in the linked listed pointed to by link.
 */
static inline dlink_node *
dlinkFind(dlink_list * list, void *data)
{
	dlink_node *ptr;
	assert(list != NULL);
	assert(data != NULL);

	DLINK_FOREACH(ptr, list->head)
	{
		if(ptr->data == data)
			return (ptr);
	}
	return (NULL);
}

static inline void
dlinkMoveList(dlink_list * from, dlink_list * to)
{
	assert(from != NULL);
	assert(to != NULL);

	/* There are three cases */
	/* case one, nothing in from list */
	if(from->head == NULL)
		return;

	/* case two, nothing in to list */
	if(to->head == NULL)
	{
		to->head = from->head;
		to->tail = from->tail;
		from->head = from->tail = NULL;
		to->length = from->length;
		from->length = 0;
		return;
	}

	/* third case play with the links */
	from->tail->next = to->head;
	to->head->prev = from->tail;
	to->head = from->head;
	from->head = from->tail = NULL;
	to->length += from->length;
	from->length = 0;
}

#endif /* __TOOLS_H__ */

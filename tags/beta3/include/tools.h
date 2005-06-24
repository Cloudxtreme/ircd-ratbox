/*
 * tools.h
 *
 * Definitions/prototypes for src/tools.c
 *
 * Adrian Chadd <adrian@creative.net.au>
 *
 * $Id$
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

void mem_frob(void *data, int len);

#endif

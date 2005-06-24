/* 
 *
 * fdlist.c   maintain lists of file descriptors
 *
 *
 * $Id$
 */
#include "fdlist.h"
#include "client.h"  /* struct Client */
#include "event.h"
#include "ircd.h"    /* GlobalSetOptions */
#include "s_bsd.h"   /* highest_fd */
#include "config.h"  /* option settings */
#include "send.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

fde_t *fd_table = NULL;

static void fdlist_update_biggest(int fd, int opening);

/* Highest FD and number of open FDs .. */
int highest_fd = -1; /* Its -1 because we haven't started yet -- adrian */
int number_fd = 0;

static void
fdlist_update_biggest(int fd, int opening)
{ 
    if (fd < highest_fd)
        return;
    assert(fd < MAXCONNECTIONS);
    if (fd > highest_fd) {
        /*  
         * assert that we are not closing a FD bigger than
         * our known biggest FD
         */
        assert(opening);
        highest_fd = fd;
        return;
    }
    /* if we are here, then fd == Biggest_FD */
    /*
     * assert that we are closing the biggest FD; we can't be
     * re-opening it
     */
    assert(!opening);
    while (highest_fd >= 0 && !fd_table[highest_fd].flags.open)
        highest_fd--;
}


void fdlist_init(void)
{
  static int initialized = 0;
  assert(0 == initialized);
  if (!initialized) {
    /* Since we're doing this once .. */
    fd_table = calloc(MAXCONNECTIONS + 1, sizeof(fde_t));
    /* XXXX I HATE THIS CHECK. Can someone please fix? */
    if (!fd_table)
        exit(69);
    initialized = 1;
  }
}

/* Called to open a given filedescriptor */
void
fd_open(int fd, unsigned int type, const char *desc)
{
    fde_t *F = &fd_table[fd];
    assert(fd >= 0);
    if (F->flags.open) {
#ifdef NOTYET
        debug(51, 1) ("WARNING: Closing open FD %4d\n", fd);
#endif
        fd_close(fd);
    }
    assert(!F->flags.open);
#ifdef NOTYET
    debug(51, 3) ("fd_open FD %d %s\n", fd, desc);
#endif
    F->fd = fd;
    F->type = type;
    F->flags.open = 1;
#ifdef NOTYET
    F->defer.until = 0;
    F->defer.n = 0;
    F->defer.handler = NULL;
#endif
    fdlist_update_biggest(fd, 1);
    F->comm_index = -1;
    F->list = FDLIST_NONE;
    if (desc)
        strncpy(F->desc, desc, FD_DESC_SZ);
    number_fd++;
}


/* Called to close a given filedescriptor */
void
fd_close(int fd)
{
    fde_t *F = &fd_table[fd];
    assert(F->flags.open);
    /* All disk fd's MUST go through file_close() ! */
    assert(F->type != FD_FILE);
    if (F->type == FD_FILE) {
        assert(F->read_handler == NULL);
        assert(F->write_handler == NULL);
    }
#ifdef NOTYET
    debug(51, 3) ("fd_close FD %d %s\n", fd, F->desc);
#endif
    comm_setselect(fd, FDLIST_NONE, COMM_SELECT_WRITE|COMM_SELECT_READ,
      NULL, NULL, 0);
    F->flags.open = 0;
    fdlist_update_biggest(fd, 0);
    number_fd--;
    memset(F, '\0', sizeof(fde_t));
    F->timeout = 0;
    /* Unlike squid, we're actually closing the FD here! -- adrian */
    close(fd);
}


/*
 * fd_dump() - dump the list of active filedescriptors
 */
void
fd_dump(struct Client *sptr)
{
    int i;

    for (i = 0; i <= highest_fd; i++) {
        if (!fd_table[i].flags.open)
            continue;

        sendto_one(sptr, ":%s NOTICE %s :*** fd %d, desc '%s'", me.name,
          sptr->name, i, fd_table[i].desc);
    }
    sendto_one(sptr, ":%s NOTICE %s :*** Finished", me.name, sptr->name);
}


/*
 * fd_note() - set the fd note
 */
void
fd_note(int fd, const char *desc)
{
    if (desc)
        strncpy(fd_table[fd].desc, desc, FD_DESC_SZ);
    else
        fd_table[fd].desc[0] = '\0';
}


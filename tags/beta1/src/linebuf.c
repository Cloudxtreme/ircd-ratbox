/*
 * linebuf.c - a replacement message buffer mechanism
 *
 * Adrian Chadd <adrian@creative.net.au>
 *
 * The idea here is that we should really be maintaining pre-munged
 * buffer "lines" which we can later refcount to save needless copies.
 *
 * $Id$
 */

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"
#include "linebuf.h"
#include "blalloc.h"
#include "memory.h"

#ifdef STRING_WITH_STRINGS
# include <string.h>
# include <strings.h>
#else
# ifdef HAVE_STRING_H
#  include <string.h>
# else
#  ifdef HAVE_STRINGS_H
#   include <strings.h>
#  endif
# endif
#endif

static int linebuf_initialised = 0;
static BlockHeap *linebuf_bl = NULL;


/*
 * linebuf_init
 *
 * Initialise the linebuf mechanism
 */
void
linebuf_init(void)
{
    assert(!linebuf_initialised);

    linebuf_bl = BlockHeapCreate(sizeof(buf_line_t), BUF_BLOCK_SIZE);

    linebuf_initialised = 1;
}


/*
 * linebuf_new_line
 *
 * Create a new line, and link it to the given linebuf.
 * It will be initially empty.
 */
static buf_line_t *
linebuf_new_line(buf_head_t *bufhead)
{
    buf_line_t *bufline;

    bufline = (buf_line_t *)BlockHeapAlloc(linebuf_bl);

    /* XXX Zero data, I'm being paranoid! -- adrian */
    bzero(bufline, sizeof(buf_line_t));

#if 0
    bufline->len = 0;
    bufline->terminated = 0;
    bufline->overflow = 0;
    bufline->flushing = 0;
#endif

    /* Stick it at the end of the buf list */
    dlinkAddTail(bufline, &bufline->node, &bufhead->list);

    /* And finally, update the allocated size */
    bufhead->alloclen += BUF_DATA_SIZE;
    bufhead->numlines ++;

    return bufline;
}


/*
 * linebuf_done_line
 *
 * We've finished with the given line, so deallocate it
 */
static void
linebuf_done_line(buf_head_t *bufhead, buf_line_t *bufline)
{
    /* Remove it from the linked list */
    dlinkDelete(&bufline->node, &bufhead->list);

    /* Update the allocated size */
    bufhead->alloclen -= BUF_DATA_SIZE;
    bufhead->len -= bufline->len;
    assert(bufhead->len >= 0);
    bufhead->numlines --;

    /* and finally, deallocate the buf */
    BlockHeapFree(linebuf_bl, bufline);
}


/*
 * skip to end of line or the crlfs, return the number of bytes ..
 */
static int
linebuf_skip_crlf(char *ch, int len)
{
     int cpylen = 0;

     /* First, skip until the first non-CRLF */
     while (len && (*ch != '\n') && (*ch != '\r')) {
         ch++;
	 assert(len > 0);
         len--;
         cpylen++;
     }

     /* Then, skip until the last CRLF */
     while (len && ((*ch == '\n') || (*ch == '\r'))) {
         ch++;
	 assert(len > 0);
         len--;
         cpylen++;
     }
     
     return cpylen;
}


/*
 * attach a CR/LF/NUL to the end of the line, and update the length.
 */
static void
linebuf_terminate_crlf(buf_head_t *bufhead, buf_line_t *bufline)
{
    bufline->buf[bufline->len++] = '\r';
    bufline->buf[bufline->len++] = '\n';
    bufline->buf[bufline->len] = '\0';
    bufhead->len += 2;
}


/*
 * linebuf_newbuf
 *
 * Initialise the new buffer
 */
void
linebuf_newbuf(buf_head_t *bufhead)
{
    /* not much to do right now :) */
    bzero(bufhead, sizeof(buf_head_t));
}


/*
 * linebuf_donebuf
 *
 * Flush all the lines associated with this buffer
 */
void
linebuf_donebuf(buf_head_t *bufhead)
{
    while (bufhead->list.head != NULL) {
        linebuf_done_line(bufhead, (buf_line_t *)bufhead->list.head->data);
    }
}



/*
 * linebuf_copy_line
 *
 * copy data into the given line. Return the number of bytes copied.
 * It will try to squeeze what it can into the given buffer.
 * If it hits an end-of-buffer before it hits a CRLF, it will flag
 * an overflow, and skip to the next CRLF. If it hits a CRLF before
 * filling the buffer, it will flag it terminated, and skip past
 * the CRLF.
 *
 * Just remember these:
 *
 * - we will *always* return skipped past a CRLF.
 * - if we hit an end-of-buffer before a CRLF is reached, we tag it as
 *   overflowed.
 * - if we hit a CRLF before an end-of-buffer, we terminate it and
 *   skip the CRLF
 * - we *always* null-terminate the buffer! 
 * - My definition of a CRLF is one of CR, LF, CRLF, LFCR. Hrm.
 *   We will attempt to skip multiple CRLFs in a row ..
 *
 * This routine probably isn't as optimal as it could be, but hey .. :)
 *   -- adrian
 */
static int
linebuf_copy_line(buf_head_t *bufhead, buf_line_t *bufline,
  char *data, int len)
{
    int cpylen = 0;	/* how many bytes we've copied */
    char *ch = data;	/* Pointer to where we are in the read data */
    char *bufch = &bufline->buf[bufline->len];
			/* Start of where to put new data */

    /* If its full or terminated, ignore it */
    if ((bufline->len == BUF_DATA_SIZE) || (bufline->terminated == 1))
        return 0;

    /* Next, lets enter the copy loop */
    for(;;) {
        /* Are we out of data? */
        if (len == 0) {
            /* Yes, so we mark this buffer as unterminated, and return */
            bufline->terminated = 0; /* XXX it should be anyway :) */
            goto finish;
        }

	/* Are we out of space to PUT this ? */
	if (bufline->len == BUF_DATA_SIZE) {
            /*
             * ok, we CR/LF/NUL terminate, set overflow, and loop until the
             * next CRLF. We then skip that, and return.
             */
            bufline->overflow = 1;
            cpylen += linebuf_skip_crlf(ch, len);
            linebuf_terminate_crlf(bufhead, bufline);
            /* NOTE: We're finishing, so ignore updating len */
            bufline->terminated = 1;
            goto finish;
        }

        /* Is this a CR or LF? */
        if ((*ch == '\012') || (*ch == '\015')) {
            /* Skip */
            cpylen += linebuf_skip_crlf(ch, len);
            /* Terminate the line */
            linebuf_terminate_crlf(bufhead, bufline);
            /* NOTE: We're finishing, so ignore updating len */
            bufline->terminated = 1;
            goto finish;
        }

        /*
         * phew! we can copy a byte. Do it, and update the counters.
         * this definitely blows our register sets on most sane archs,
         * but hey, someone can recode this later on if they want to.
	 * (ppc has 32 gp registers! -db)
         */
        *bufch = *ch; 
        bufch++;
        ch++;
        cpylen++;
	assert(len > 0);
        len--;
        bufline->len++;
        bufhead->len++;
    }
finish:
    /* I hate gotos, but this is nice and common .. */
    /* terminate string and then return number of bytes copied .. */
    *bufch = '\000';
    return cpylen;
}


/*
 * linebuf_parse
 *
 * Take a given buffer and break out as many buffers as we can.
 * If we find a CRLF, we terminate that buffer and create a new one.
 * If we don't find a CRLF whilst parsing a buffer, we don't mark it
 * 'finished', so the next loop through we can continue appending ..
 *
 * A few notes here, which you'll need to understand before continuing.
 *
 * - right now I'm only dealing with single sized buffers. Later on,
 *   I might consider chaining buffers together to get longer "lines"
 *   but seriously, I don't see the advantage right now.
 *
 * - This *is* designed to turn into a reference-counter-protected setup
 *   to dodge copious copies.
 */
int
linebuf_parse(buf_head_t *bufhead, char *data, int len)
{
    buf_line_t *bufline;
    int cpylen;
    int linecnt = 0;

    /* First, if we have a partial buffer, try to squeze data into it */
    if (bufhead->list.tail != NULL) {
        /* Check we're doing the partial buffer thing */
        bufline = bufhead->list.tail->data;
        assert(!bufline->flushing);
        /* just try, the worst it could do is *reject* us .. */
        cpylen = linebuf_copy_line(bufhead, bufline, data, len);
        linecnt++;
        /* If we've copied the same as what we've got, quit now */
        if (cpylen == len)
            return linecnt; /* all the data done so soon? */

        /* Skip the data and update len .. */
        len -= cpylen;
	assert(len >= 0);
        data += cpylen;
    }
    /* Next, the loop */
    while (len > 0) {
        /* We obviously need a new buffer, so .. */
        bufline = linebuf_new_line(bufhead);
        
        /* And parse */
        cpylen = linebuf_copy_line(bufhead, bufline, data, len);
        len -= cpylen;
	assert(len >= 0);
        data += cpylen;
        linecnt++;
    }
    return linecnt;
}


/*
 * linebuf_get
 *
 * get the next buffer from our line. For the time being it will copy
 * data into the given buffer and free the underlying linebuf.
 */
int
linebuf_get(buf_head_t *bufhead, char *buf, int buflen)
{
    buf_line_t *bufline;
    int cpylen;

    /* make sure we have a line */
    if (bufhead->list.head == NULL)
        return 0; /* Obviously not.. hrm. */

    bufline = bufhead->list.head->data; 

    /* make sure that the buffer was actually *terminated */
    if (!bufline->terminated)
        return 0;  /* Wait for more data! */

    /* make sure we've got the space, including the NULL */
    cpylen = bufline->len;
    assert(cpylen + 1 <= buflen);

    /* Copy it */
    memcpy(buf, bufline->buf, cpylen);

    /* Deallocate the line */
    linebuf_done_line(bufhead, bufline);

    /* return how much we copied */
    return cpylen;
}

/*
 * linebuf_put
 *
 * put some *unparsed* data in a buffer. This is used when linebufs are
 * being utilised for outbound messages, where you don't want to worry
 * about the overhead in linebuf_parse() which is a little much when you
 * DO know you are getting a single line.
 */
void
linebuf_put(buf_head_t *bufhead, char *buf, int buflen)
{
    buf_line_t *bufline;

    /* make sure the previous line is terminated */
    if (bufhead->list.tail) {
        bufline = bufhead->list.tail->data;
        assert(bufline->terminated);
    }

    /* Create a new line */
    bufline = linebuf_new_line(bufhead);

    /* Truncate the data if required */
    if (buflen > BUF_DATA_SIZE) {
        buflen = BUF_DATA_SIZE;
        bufline->overflow = 1;
    }

    /* Chop trailing CRLF's .. */
    assert(buf[buflen] == '\0');
    buflen--;
    while ((buf[buflen] == '\r') || (buf[buflen] == '\n'))
      buflen--;

    /*
     * Bump up the length to be the real length, not the pointer to the last
     * char ..
     */
    buflen++;

    /* Copy the data */
    memcpy(bufline->buf, buf, buflen);

    /* Make sure we terminate it! */
    bufline->buf[buflen + 1] = '\0';
    bufline->len = buflen;
    bufline->terminated = 1;

    /* update the line length */
    bufhead->len += buflen;

    /* And now, CRLF-NUL terminate it .. */
    linebuf_terminate_crlf(bufhead, bufline);
}


/*
 * linebuf_flush
 *
 * Flush data to the buffer. It tries to write as much data as possible
 * to the given socket. Any return values are passed straight through.
 * If there is no data in the socket, EWOULDBLOCK is set as an errno
 * rather than returning 0 (which would map to an EOF..)
 *
 * Notes: XXX We *should* have a clue here when a non-full buffer is arrived.
 *        and tag it so that we don't re-schedule another write until
 *        we have a CRLF.
 */
int
linebuf_flush(int fd, buf_head_t *bufhead)
{
    buf_line_t *bufline;
    int retval;

    /* Check we actually have a first buffer */
    if (bufhead->list.head == NULL) {
        /* nope, so we return none .. */
        errno = EWOULDBLOCK;
        return -1;    
    }

    bufline = bufhead->list.head->data;

    /* And that its actually full .. */
    if (!bufline->terminated) {
        errno = EWOULDBLOCK;
        return -1;
    }
    
    /* Check we're flushing the first buffer */
    if (!bufline->flushing) {
        bufline->flushing = 1;
        bufhead->writeofs = 0;
    }

    /* Now, try writing data */
    retval = write(fd, bufline->buf + bufhead->writeofs, bufline->len
      - bufhead->writeofs);
    
    /* Deal with return code */
    if (retval < 0)
        return retval;
    if (retval == 0)
        return 0;

    /* we've got data, so update the write offset */
    bufhead->writeofs += retval;

    /* if we've written everything *and* the CRLF, deallocate and update
       bufhead */
    if (bufhead->writeofs == bufline->len) {
       bufhead->writeofs = 0;
       assert(bufhead->len >=0);
       linebuf_done_line(bufhead, bufline);
    }

    /* Return line length */
    return retval;
}

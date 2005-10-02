/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio.h: A header for the network subsystem.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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

#ifndef IRCD_LIB_H
# error "Do not use commio.h directly"                                   
#endif


#ifndef INCLUDED_commio_h
#define INCLUDED_commio_h


#ifdef EINPROGRESS
#define XEINPROGRESS EINPROGRESS
#else
#define XEINPROGRESS 0
#endif

#ifdef EWOULDBLOCK
#define XEWOULDBLOCK EWOULDBLOCK
#else
#define XEWOULDBLOCK 0
#endif

#ifdef EAGAIN
#define XEAGAIN EAGAIN
#else
#define XEAGAIN 0
#endif

#ifdef EINTR
#define XEINTR EINTR
#else
#define XEINTR 0
#endif

#ifdef ERESTART
#define XERESTART ERESTART
#else
#define XERESTART 0
#endif

#ifdef ENOBUFS
#define XENOBUFS ENOBUFS
#else
#define XENOBUFS 0
#endif

#define ignoreErrno(x)	((	\
x == XEINPROGRESS	||	\
x == XEWOULDBLOCK	||	\
x == XEAGAIN		||	\
x == XEINTR       	||	\
x == XERESTART    	||	\
x == XENOBUFS) ? 1 : 0)



/* Callback for completed IO events */
typedef void PF(int fd, void *);

/* Callback for completed connections */
/* int fd, int status, void * */
typedef void CNCB(int fd, int, void *);
/* callback for fd table dumps */
typedef void DUMPCB(const char *string, void *);
/*
 * priority values used in fdlist code
 */
#define FDL_SERVER   0x01
#define FDL_BUSY     0x02
#define FDL_OPER     0x04
#define FDL_DEFAULT  0x08
#define FDL_ALL      0xFF

#define FD_DESC_SZ 128		/* hostlen + comment */


/* FD type values */
enum
{
	FD_NONE,
	FD_LOG,
	FD_FILE,
	FD_FILECLOSE,
	FD_SOCKET,
	FD_PIPE,
	FD_UNKNOWN
};

enum
{
	IRCD_OK,
	IRCD_ERR_BIND,
	IRCD_ERR_DNS,
	IRCD_ERR_TIMEOUT,
	IRCD_ERR_CONNECT,
	IRCD_ERROR,
	IRCD_ERR_MAX
};

typedef struct _fde fde_t;


extern int ircd_highest_fd;
extern int number_fd;

struct Client;

struct _fde
{
	/* New-school stuff, again pretty much ripped from squid */
	/*
	 * Yes, this gives us only one pending read and one pending write per
	 * filedescriptor. Think though: when do you think we'll need more?
	 */
	int fd;			/* So we can use the fde_t as a callback ptr */
	int type;
	int ircd_index;		/* where in the poll list we live */
	char desc[FD_DESC_SZ];
	PF *read_handler;
	void *read_data;
	PF *write_handler;
	void *write_data;
	PF *timeout_handler;
	void *timeout_data;
	time_t timeout;
	PF *flush_handler;
	void *flush_data;
	time_t flush_timeout;
	struct
	{
		unsigned int open:1;
		unsigned int close_request:1;
		unsigned int write_daemon:1;
		unsigned int closing:1;
		unsigned int socket_eof:1;
		unsigned int nolinger:1;
		unsigned int nonblocking:1;
		unsigned int ipc:1;
		unsigned int called_connect:1;
	}
	flags;
	struct
	{
		/* We don't need the host here ? */
		struct irc_sockaddr_storage S;
		struct irc_sockaddr_storage hostaddr;
		CNCB *callback;
		void *data;
		/* We'd also add the retry count here when we get to that -- adrian */
	}
	connect;
	int pflags;
#ifdef __MINGW32__
	dlink_node node;
#endif

#ifdef SSL_ENABLED
	SSL *ssl;
#endif
};

#ifdef __MINGW32__
dlink_list *fd_table;
#else
extern fde_t *fd_table;
#endif

void ircd_fdlist_init(int closeall, int maxfds);

extern void ircd_open(int, unsigned int, const char *);
extern void ircd_close(int);
extern void ircd_dump(DUMPCB *, void *xdata);
#ifndef __GNUC__
extern void ircd_note(int fd, const char *format, ...);
#else
extern void ircd_note(int fd, const char *format, ...) __attribute__ ((format(printf, 2, 3)));
#endif


#if defined(HAVE_PORTS) || defined(HAVE_SIGIO)
typedef void (*ircd_event_cb_t)(void *);

typedef struct timer_data {
	timer_t		 td_timer_id;
	ircd_event_cb_t	 td_cb;
	void		*td_udata;
	int		 td_repeat;
} *ircd_event_id;

extern ircd_event_id ircd_schedule_event(time_t, int, ircd_event_cb_t, void *);
extern void ircd_unschedule_event(ircd_event_id);
#endif

#define FB_EOF  0x01
#define FB_FAIL 0x02


/* Size of a read buffer */
#define READBUF_SIZE    16384	/* used by src/packet.c and src/s_serv.c */

/* Type of IO */
#define	IRCD_SELECT_READ		0x1
#define	IRCD_SELECT_WRITE		0x2

#define IRCD_SELECT_ACCEPT		IRCD_SELECT_READ
#define IRCD_SELECT_CONNECT		IRCD_SELECT_WRITE

extern int readcalls;

extern int ircd_set_nb(int);
extern int ircd_set_buffers(int, int);

extern int ircd_get_sockerr(int);

extern void ircd_settimeout(int fd, time_t, PF *, void *);
extern void ircd_setflush(int fd, time_t, PF *, void *);
extern void ircd_checktimeouts(void *);
extern void ircd_connect_tcp(int fd, struct sockaddr *,
			     struct sockaddr *, int, CNCB *, void *, int);
extern const char *ircd_errstr(int status);
extern int ircd_socket(int family, int sock_type, int proto, const char *note);
extern int ircd_socketpair(int family, int sock_type, int proto, int *nfd, const char *note);

extern int ircd_accept(int fd, struct sockaddr *pn, socklen_t *addrlen);
extern ssize_t ircd_write(int fd, void *buf, int count);
#if defined(USE_WRITEV) 
extern ssize_t ircd_writev(int fd, struct iovec *vector, int count);
#endif
extern ssize_t ircd_read(int fd, void *buf, int count);
extern int ircd_pipe(int *fd, const char *desc);

/* These must be defined in the network IO loop code of your choice */
extern void ircd_setselect(int fd, unsigned int type,
			   PF * handler, void *client_data, time_t timeout);
extern void init_netio(void);
extern int read_message(time_t, unsigned char);
extern int ircd_select(unsigned long);
extern int disable_sock_options(int);
extern int ircd_setup_fd(int fd);

const char *inetntoa(const char *in_addr);
const char *inetntop(int af, const void *src, char *dst, unsigned int size);
int inetpton(int af, const char *src, void *dst);
const char *inetntop_sock(struct sockaddr *src, char *dst, unsigned int size);
int inetpton_sock(const char *src, struct sockaddr *dst);
extern int maxconnections;
#ifdef IPV6
extern void mangle_mapped_sockaddr(struct sockaddr *in);
#else
#define mangle_mapped_sockaddr(x) 
#endif

#ifdef __MINGW32__
#define get_errno()  errno = WSAGetLastError()
#define hash_fd(x) ((fd >> 2) % maxconnections)

static inline fde_t *
find_fd(int fd)
{
	dlink_list *hlist = &fd_table[hash_fd(fd)];
	
	dlink_node *ptr;
	DLINK_FOREACH(ptr, hlist->head)
	{
		fde_t *F = ptr->data;
		if(F->fd == fd)
			return F;
	}	
	return NULL;
}

static inline fde_t *
add_fd(int fd)
{
	int hash = hash_fd(fd);
	fde_t *F;
	dlink_list *list;
	/* look up to see if we have it already */
	if((F = find_fd(fd)) != NULL)
		return F; 
	
	F = ircd_malloc(sizeof(fde_t));
	F->fd = fd;
	list = &fd_table[hash];
	dlinkAdd(F, &F->node, list);
	return(F);
}

static inline void
remove_fd(int fd)
{
	int hash = hash_fd(fd);
	fde_t *F;
	dlink_list *list;
	list = &fd_table[hash];
	F = find_fd(fd);
	dlinkDelete(&F->node, list);
	ircd_free(F);
}


#else
#define get_errno()
#define find_fd(x) &fd_table[x]
#define add_fd(x) &fd_table[x]
#define remove_fd(x) memset(&fd_table[x], 0, sizeof(fde_t))
#endif



#endif /* INCLUDED_commio_h */

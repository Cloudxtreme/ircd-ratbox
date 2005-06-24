/*
 * fdlist.h
 *
 * $Id$
 */
#ifndef INCLUDED_fdlist_h
#define INCLUDED_fdlist_h
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>         /* time_t */
#define INCLUDED_sys_types_h
#endif
#ifndef INCLUDED_sys_socket_h
#define INCLUDED_sys_socket_h
#include <sys/socket.h>		/* Socket structs */
#endif

#include <netinet/in.h>
#include "config.h"
#include "ircd_defs.h"

#define FD_DESC_SZ 32

/*
 * priority values used in fdlist code
 */
#define FDL_SERVER   0x01
#define FDL_BUSY     0x02
#define FDL_OPER     0x04
#define FDL_DEFAULT  0x08 
#define FDL_ALL      0xFF

/* FD type values */
enum {
    FD_NONE,
    FD_LOG,
    FD_FILE,
    FD_FILECLOSE,
    FD_SOCKET,
    FD_PIPE,
    FD_UNKNOWN
};

enum {
    COMM_OK,
    COMM_ERR_BIND,
    COMM_ERR_DNS,
    COMM_ERR_TIMEOUT,
    COMM_ERR_CONNECT,
    COMM_ERROR,
    COMM_ERR_MAX
};

typedef enum fdlist_t {
    FDLIST_NONE,
    FDLIST_SERVICE,
    FDLIST_SERVER,
    FDLIST_IDLECLIENT,
    FDLIST_BUSYCLIENT,
    FDLIST_MAX
} fdlist_t;

typedef struct _fde fde_t;

/* Callback for completed IO events */
typedef void PF(int, void *);

/* Callback for completed connections */
/* int fd, int status, void * */
typedef void CNCB(int, int, void *);

extern int highest_fd;
extern int number_fd;

struct Client;

struct _fde {
    /* New-school stuff, again pretty much ripped from squid */
    /*
     * Yes, this gives us only one pending read and one pending write per
     * filedescriptor. Think though: when do you think we'll need more?
     */
    int fd;		/* So we can use the fde_t as a callback ptr */
    int type;
    fdlist_t list;	/* Which list this FD should sit on */
    int comm_index;	/* where in the poll list we live */
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
    struct {
        unsigned int open:1;
        unsigned int close_request:1;
        unsigned int write_daemon:1;   
        unsigned int closing:1;
        unsigned int socket_eof:1;
        unsigned int nolinger:1;
        unsigned int nonblocking:1;
        unsigned int ipc:1;
        unsigned int called_connect:1;
    } flags;
    struct {
        /* We don't need the host here ? */
	struct irc_sockaddr S;
	struct irc_sockaddr hostaddr;
        CNCB *callback;
        void *data;
        /* We'd also add the retry count here when we get to that -- adrian */
    } connect;
};

extern fde_t *fd_table;

void fdlist_init(void);

extern void  fd_open(int, unsigned int, const char *);
extern void  fd_close(int);
extern void  fd_dump(struct Client *sptr);
extern void  fd_note(int fd, const char *desc);


#endif /* INCLUDED_fdlist_h */


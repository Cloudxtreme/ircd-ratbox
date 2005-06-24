/************************************************************************
 *   IRC - Internet Relay Chat, src/ircd.c
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
 * $Id$
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "setup.h"
#include "config.h"

#ifdef USE_GETTEXT
#include <libintl.h>
#endif

#include "tools.h"
#include "ircd.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "fdlist.h"
#include "hash.h"
#include "irc_string.h"
#include "ircdauth.h"
#include "ircd_signal.h"
#include "list.h"
#include "s_gline.h"
#include "motd.h"
#include "ircd_handler.h"
#include "md5.h"
#include "msg.h"         /* msgtab */
#include "hostmask.h"
#include "numeric.h"
#include "parse.h"
#include "res.h"
#include "restart.h"
#include "s_auth.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_debug.h"
#include "s_log.h"
#include "s_misc.h"
#include "s_serv.h"      /* try_connections */
#include "s_stats.h"
#include "scache.h"
#include "send.h"
#include "whowas.h"
#include "modules.h"
#include "memory.h"
#include "hook.h"
#include "debug.h"
#include "ircd_getopt.h"

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif

/* /quote set variables */
struct SetOptions GlobalSetOptions;

/* config.h config file paths etc */
ConfigFileEntryType ConfigFileEntry; 
/* server info */
struct server_info ServerInfo;
/* admin info */
struct admin_info AdminInfo;

struct  Counter Count;

time_t  CurrentTime;            /* GLOBAL - current system timestamp */
int     ServerRunning;          /* GLOBAL - server execution state */
struct Client me;               /* That's me */
struct LocalUser meLocalUser;	/* That's also part of me */

struct Client* GlobalClientList = 0; /* Pointer to beginning of Client list */

/* unknown/client pointer lists */ 
dlink_list unknown_list;        /* unknown clients ON this server only */
dlink_list lclient_list;        /* local clients only ON this server */
dlink_list serv_list;           /* local servers to this server ONLY */
dlink_list oper_list;           /* our opers, duplicated in lclient_list */
dlink_list dead_list;           /* clients that have exited, to be freed */
dlink_list persist_list;        /* Persisting(i.e. no open fd) clients. */

dlink_list lazylink_channels;   /* known about lazylink channels on HUB */
dlink_list lazylink_nicks;	/* known about lazylink nicks on HUB */

static size_t      initialVMTop = 0;   /* top of virtual memory at init */
static const char* logFileName = LPATH;
static int         noDetach  = 0;

char**  myargv;
int     dorehash   = 0;
int     debuglevel = -1;        /* Server debug level */
char*   debugmode  = "";        /*  -"-    -"-   -"-  */
time_t  nextconnect = 1;        /* time for next try_connections call */

/*
 * get_vm_top - get the operating systems notion of the resident set size
 */
static size_t get_vm_top(void)
{
  /*
   * NOTE: sbrk is not part of the ANSI C library or the POSIX.1 standard
   * however it seems that everyone defines it. Calling sbrk with a 0
   * argument will return a pointer to the top of the process virtual
   * memory without changing the process size, so this call should be
   * reasonably safe (sbrk returns the new value for the top of memory).
   * This code relies on the notion that the address returned will be an 
   * offset from 0 (NULL), so the result of sbrk is cast to a size_t and 
   * returned. We really shouldn't be using it here but...
   */
#ifndef VMS
  void* vptr = sbrk(0);
  return (size_t) vptr;
#else
  return 0;
#endif
}

/*
 * get_maxrss - get the operating systems notion of the resident set size
 */
size_t get_maxrss(void)
{
  return get_vm_top() - initialVMTop;
}

/*
 * print_startup - print startup information
 */
static void
print_startup(void)
{
  printf("ircd: version %s\n", version);
  printf("ircd: pid %d\n", (int)getpid());
  printf("ircd: running in %s mode from %s\n", !noDetach ? "background"
         : "foreground", ConfigFileEntry.dpath);
}

/*
 * init_sys
 *
 * inputs	- boot_daemon flag
 * output	- none
 * side effects	- if boot_daemon flag is not set, don't daemonize
 */
static void 
init_sys(void)
{
#ifdef RLIMIT_FD_MAX
  struct rlimit limit;

  if (!getrlimit(RLIMIT_FD_MAX, &limit))
    {

      if (limit.rlim_max < MAXCONNECTIONS)
        {
          fprintf(stderr,"ircd fd table too big\n");
          fprintf(stderr,"Hard Limit: %ld IRC max: %d\n",
                        (long) limit.rlim_max, MAXCONNECTIONS);
          fprintf(stderr,"Fix MAXCONNECTIONS\n");
          exit(-1);
        }

      limit.rlim_cur = limit.rlim_max; /* make soft limit the max */
      if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
        {
          fprintf(stderr,"error setting max fd's to %ld\n",
                        (long) limit.rlim_cur);
          exit(EXIT_FAILURE);
        }
      printf("Value of NOFILE is %d\n", NOFILE);
    }
#endif        /* RLIMIT_FD_MAX */
}

int
make_daemon(void)
{
#ifndef VMS
  int pid;
  
  if((pid = fork()) < 0)
    {
      perror("fork");
      exit(EXIT_FAILURE);
    }
  else if (pid > 0)
    {
      print_startup();
      exit(EXIT_SUCCESS);
    }

  setsid();
  /*  fclose(stdin);
  fclose(stdout);
  fclose(stderr); */
#endif
  return 0;
}

static int printVersion = 0;

struct lgetopt myopts[] = {
  {"dlinefile",  &ConfigFileEntry.dlinefile, 
   STRING, "File to use for dlines.conf"},
  {"configfile", &ConfigFileEntry.configfile, 
   STRING, "File to use for ircd.conf"},
  {"klinefile",  &ConfigFileEntry.klinefile, 
   STRING, "File to use for klines.conf"},
  {"logfile",    &logFileName, 
   STRING, "File to use for ircd.log"},
  {"foreground", &noDetach, 
   YESNO, "Run in foreground (don't detach)"},
  {"version",    &printVersion, 
   YESNO, "Print version and exit"},
#ifdef DEBUGMODE
  {"debug", NULL, 
   ENDEBUG, "Enable debugging for a certain value"},
#endif
  {"help", NULL, 
   USAGE, "Print this text"},
};

void
set_time(void)
{
 static char   to_send[200];
 time_t newtime = time(NULL);
 if (newtime == -1)
 {
  log(L_ERROR, "Clock Failure (%d)", errno);
  sendto_realops_flags(FLAGS_ALL,
                       "Clock Failure (%d), TS can be corrupted", errno);
  restart("Clock Failure");
 }
 if (newtime < CurrentTime)
 {
  ircsprintf(to_send, "System clock is running backwards - (%lu < %lu)",
             newtime, CurrentTime);
  report_error(to_send, me.name, 0);
  set_back_events(CurrentTime - newtime);
 }
 CurrentTime = newtime;
}

static time_t io_loop(time_t delay)
{

  set_time();
  /* Run pending events, then get the number of seconds to the next event */
  eventRun();
  delay = eventNextTime();

  /* Do IO events */
  comm_select(delay);
  /* Do DNS events */
  /*
   * Free up dead clients
   */
  free_exited_clients();

  /*
   * Check to see whether we have to rehash the configuration ..
   */
  if (dorehash)
    {
      rehash(&me, &me, 1);
      dorehash = 0;
    }
  return delay;
}

/*
 * initalialize_global_set_options
 *
 * inputs       - none
 * output       - none
 * side effects - This sets all global set options needed 
 */
static void initialize_global_set_options(void)
{
  memset( &GlobalSetOptions, 0, sizeof(GlobalSetOptions));

  GlobalSetOptions.maxclients = MAX_CLIENTS;
  GlobalSetOptions.autoconn = 1;

  GlobalSetOptions.spam_time = MIN_JOIN_LEAVE_TIME;
  GlobalSetOptions.spam_num = MAX_JOIN_LEAVE_COUNT;

  if(ConfigFileEntry.default_floodcount)
    GlobalSetOptions.floodcount = ConfigFileEntry.default_floodcount;
  else
    GlobalSetOptions.floodcount = 10;

 /* End of global set options */

}

/*
 * initialize_message_files
 *
 * inputs       - none
 * output       - none
 * side effects - Set up all message files needed, motd etc.
 */
static void initialize_message_files(void)
  {
    InitMessageFile( HELP_MOTD, HPATH, &ConfigFileEntry.helpfile );
    InitMessageFile( USER_MOTD, MPATH, &ConfigFileEntry.motd );
    InitMessageFile( OPER_MOTD, OPATH, &ConfigFileEntry.opermotd );
    InitMessageFile( USER_LINKS, LIPATH, &ConfigFileEntry.linksfile );

    ReadMessageFile( &ConfigFileEntry.helpfile );
    ReadMessageFile( &ConfigFileEntry.motd );
    ReadMessageFile( &ConfigFileEntry.opermotd );
    ReadMessageFile( &ConfigFileEntry.linksfile );
  }

/*
 * write_pidfile
 *
 * inputs       - none
 * output       - none
 * side effects - write the pid of the ircd to PPATH
 */
static void write_pidfile(void)
{
  FBFILE* fd;
  char buff[20];
  if ((fd = fbopen(PPATH, "w")))
    {
      ircsprintf(buff,"%d\n", (int)getpid());
      if ((fbputs(buff, fd) == -1))
        log(L_ERROR,"Error writing to pid file %s (%s)", PPATH,
		    strerror(errno));
      fbclose(fd);
      return;
    }
  else
    log(L_ERROR, "Error opening pid file %s", PPATH);
}

/*
 * check_pidfile
 *
 * inputs       - none
 * output       - none
 * side effects - reads pid from pidfile and checks if ircd is in process
 *                list. if it is, gracefully exits
 * -kre
 */
static void check_pidfile(void)
{
  FBFILE* fd;
  char buff[20];
  pid_t pidfromfile;

  /* Don't do logging here, since we don't have log() initialised */
  if ((fd = fbopen(PPATH, "r")))
  {
    if (fbgets(buff, 20, fd) == NULL)
    {
      /*
      log(L_ERROR, "Error reading from pid file %s (%s)", PPATH,
          strerror(errno));
       */
    }
    else
    {
      pidfromfile = atoi(buff);
      if (!kill(pidfromfile, 0))
      {
        /* log(L_ERROR, "Server is already running"); */
        printf("ircd: daemon is already running\n");
        exit(-1);
      }
    }
    fbclose(fd);
  }
  else
  {
    /* log(L_ERROR, "Error opening pid file %s", PPATH); */
  }
}

int main(int argc, char *argv[])
{
  time_t      delay = 0;

  /*
   * save server boot time right away, so getrusage works correctly
   */
  if ((CurrentTime = time(0)) == -1)
    {
      fprintf(stderr, "ERROR: Clock Failure: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

  /* 
   * set initialVMTop before we allocate any memory
   */
  initialVMTop = get_vm_top();

  ServerRunning = 0;

  memset(&me, 0, sizeof(me));
  memset(&meLocalUser, 0, sizeof(meLocalUser));
  me.localClient = &meLocalUser;

  /* Make sure all lists are zeroed */
  memset(&unknown_list, 0, sizeof(unknown_list));
  memset(&lclient_list, 0, sizeof(lclient_list));
  memset(&serv_list, 0, sizeof(serv_list));
  memset(&oper_list, 0, sizeof(oper_list));
  memset(&lazylink_channels, 0, sizeof(lazylink_channels));
  memset(&lazylink_nicks, 0, sizeof(lazylink_nicks));

  lclient_list.head = lclient_list.tail = NULL;
  oper_list.head = oper_list.tail = NULL;
  serv_list.head = serv_list.tail = NULL;

  GlobalClientList = &me;       /* Pointer to beginning of Client list */

  memset((void *)&Count, 0, sizeof(Count));
  Count.server = 1;     /* us */

  memset((void *)&ServerInfo, 0, sizeof(ServerInfo));

  memset((void *)&AdminInfo, 0, sizeof(AdminInfo));

  ConfigFileEntry.dpath = DPATH;
  ConfigFileEntry.configfile = CPATH;   /* Server configuration file */
  ConfigFileEntry.klinefile = KPATH;    /* Server kline file */
  ConfigFileEntry.dlinefile = DLPATH;   /* dline file */
  ConfigFileEntry.glinefile = GPATH;    /* gline log file */

  myargv = argv;
  umask(077);                /* better safe than sorry --SRB */

  parseargs(&argc, &argv, myopts);

  if (printVersion) 
    {
      printf("ircd: version %s\n", version);
      exit(EXIT_SUCCESS);
    }

  if (chdir(ConfigFileEntry.dpath))
    {
      perror("chdir");
      exit(EXIT_FAILURE);
    }

  if (!noDetach)
    make_daemon();
  else
    print_startup();

  setup_signals();

  /* We need this to initialise the fd array before anything else */
  fdlist_init();

  /* Check if there is pidfile and daemon already running */
  check_pidfile();

  /* Init the event subsystem */
  eventInit();

  init_sys();
  close_all_connections();

  init_log(logFileName);
  init_netio();		/* This needs to be setup early ! -- adrian */
  init_resolver();	/* Needs to be setup before the io loop */

  initialize_message_files();

  linebuf_init();	/* set up some linebuf stuff to control paging */
  init_hash();
  id_init();
  
  clear_scache_hash_table();    /* server cache name table */
  clear_ip_hash_table();        /* client host ip hash table */
  init_host_hash();             /* Host-hashtable. */
  clear_hash_parse();

  init_client();
  initclass();
  initwhowas();
  init_stats();

  init_hooks();

  load_all_modules(1);

  initServerMask();

  init_auth();			/* Initialise the auth code */

  read_conf_files(YES);         /* cold start init conf files */

  initialize_global_set_options();

  if (ServerInfo.name == NULL)
    {
      fprintf(stderr, "Error: No server name specified\n");
      log(L_CRIT,"You need a server name to run.");
      exit(EXIT_FAILURE);
    }

  strncpy_irc(me.name, ServerInfo.name, HOSTLEN);

  if(ServerInfo.description != NULL)
    {
      strncpy_irc(me.info, ServerInfo.description, REALLEN);
    }


#ifdef USE_GETTEXT
  /*
   * For 'locale' try (in this order):
   *    Config entry "msglocale" (yyparse() will overwrite LANGUAGE)
   *    Env variable "LANGUAGE"
   *    Default of "" (so don't overwrite LANGUAGE here)
   */
 
  if (!getenv("LANGUAGE"))
    { 
      putenv("LANGUAGE=");
    }

  textdomain("ircd-hybrid");
  bindtextdomain("ircd-hybrid" , MSGPATH);
#endif
  
#ifdef USE_IAUTH
  iAuth.flags = 0;

  ConnectToIAuth();
  
  if (iAuth.socket == NOSOCK)
    {
      fprintf(stderr, "Unable to connect to IAuth server\n");
      log(L_CRIT, "Unable to connect to IAuth server\n");
      exit (EXIT_FAILURE);
    }
#endif

  me.fd = -1;
  me.from = &me;
  me.servptr = &me;
  SetMe(&me);
  make_server(&me);
  me.serv->up = me.name;
  me.lasttime = me.since = me.firsttime = CurrentTime;
  add_to_client_hash_table(me.name, &me);

  check_class();
  write_pidfile();

  log(L_NOTICE, "Server Ready");

  eventAdd("cleanup_channels", cleanup_channels, NULL,
	   CLEANUP_CHANNELS_TIME, 0);

  eventAdd("cleanup_glines", cleanup_glines, NULL,
	   CLEANUP_GLINES_TIME, 0);

  eventAdd("cleanup_tklines", cleanup_tklines, NULL,
           CLEANUP_TKLINES_TIME, 0);

#ifdef OPENSSL
  eventAdd("cryptlink_regen_key", cryptlink_regen_key, NULL,
           CRYPTLINK_REGEN_TIME, 0);
#endif

  /* We want try_connections to be called as soon as possible now! -- adrian */
  /* No, 'cause after a restart it would cause all sorts of nick collides */
  eventAdd("try_connections", try_connections, NULL, 
	   STARTUP_CONNECTIONS_TIME, 0);

  /* Setup the timeout check. I'll shift it later :)  -- adrian */
  eventAdd("comm_checktimeouts", comm_checktimeouts, NULL, 1, 0);

  ServerRunning = 1;
  while (ServerRunning)
    delay = io_loop(delay);
  return 0;
}


/************************************************************************
 *   IRC - Internet Relay Chat, src/s_log.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
#include "client.h"	/* Needed for struct Client */
#include "s_log.h"
#include "fileio.h"
#include "irc_string.h"
#include "ircd.h"
#include "s_misc.h"
#include "event.h"	/* Needed for EVH etc. */
#include "s_conf.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "memdebug.h"


#define LOG_BUFSIZE 2048 

#ifdef USE_LOGFILE
static FBFILE* logFile;
#endif
static int logLevel = INIT_LOG_LEVEL;

#ifdef USE_SYSLOG
static int sysLogLevel[] = {
  LOG_CRIT,
  LOG_ERR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_INFO,
  LOG_INFO
};
#endif

static const char *logLevelToString[] =
{ "L_CRIT",
  "L_ERROR",
  "L_WARN",
  "L_NOTICE",
  "L_TRACE",
  "L_INFO",
  "L_DEBUG"
};

/*
 * open_log - open ircd logging file
 * returns true (1) if successful, false (0) otherwise
 */
#if defined(USE_LOGFILE) 

static int open_log(const char* filename)
{
  logFile = fbopen(filename, "an");
  if (logFile == NULL) {
    syslog(LOG_ERR, "Unable to open log file: %s: %s",
           filename, strerror(errno));
    return 0;
  }
  return 1;
}
#endif

void close_log(void)
{
#if defined(USE_LOGFILE) 
  if (logFile != NULL) {
    fbclose(logFile);
    logFile = NULL;
  }
#endif
#ifdef USE_SYSLOG  
  closelog();
#endif
}

#if defined(USE_LOGFILE) 
static void write_log(const char* message)
{
  char buf[LOG_BUFSIZE];

  if( !logFile )
    return;

  snprintf(buf, LOG_BUFSIZE, "[%s] %s\n", smalldate(CurrentTime), message);
  fbputs(buf, logFile);
}
#endif
   
void log(int priority, const char* fmt, ...)
{
  char    buf[LOG_BUFSIZE];
  va_list args;
  assert(-1 < priority);
  assert(0 != fmt);

  if (priority > logLevel)
    return;

  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  va_end(args);

#ifdef USE_SYSLOG  
  if (priority <= L_DEBUG)
    syslog(sysLogLevel[priority], "%s", buf);
#endif
#if defined(USE_LOGFILE) 
  write_log(buf);
#endif
}
  
void init_log(const char* filename)
{
#if defined(USE_LOGFILE) 
  open_log(filename);
#endif
#ifdef USE_SYSLOG
  openlog("ircd", LOG_PID | LOG_NDELAY, LOG_FACILITY);
#endif
}

void set_log_level(int level)
{
  if (L_ERROR < level && level <= L_DEBUG)
    logLevel = level;
}

int get_log_level(void)
{
  return( logLevel );
}

const char *get_log_level_as_string(int level)
{
  if(level > L_DEBUG)
    level = L_DEBUG;
  else if(level < L_ERROR)
    level = L_ERROR;

  return(logLevelToString[level]);
}

static FBFILE *user_log_fb=NULL;

#ifndef SYSLOG_USERS
static EVH user_log_resync;
#endif

/*
 * log_user_exit
 *
 * inputs	- pointer to connecting client
 * output	- NONE
 * side effects - Current exiting client is logged to
 *		  either SYSLOG or to file.
 */
void log_user_exit(struct Client *sptr)
{
  time_t        on_for;

  on_for = CurrentTime - sptr->firsttime;

#ifdef SYSLOG_USERS

  if (IsPerson(sptr))
    {
      log(L_INFO, "%s (%3ld:%02ld:%02ld): %s!%s@%s %ld/%ld\n",
	  myctime(sptr->firsttime),
	  on_for / 3600, (on_for % 3600)/60,
	  on_for % 60, sptr->name,
	  sptr->username, sptr->host,
	  sptr->localClient->sendK, sptr->localClient->receiveK);
    }

#else
  {
    char        linebuf[BUFSIZ];

    /*
     * This conditional makes the logfile active only after
     * it's been created - thus logging can be turned off by
     * removing the file.
     *
     * Keep the logfile open, syncing it every 10 seconds
     * -Taner
     */
    if (IsPerson(sptr))
      {
	if (user_log_fb == NULL)
	  {
		  if( ConfigFileEntry.fname_userlog && 
			  (user_log_fb = fbopen(ConfigFileEntry.fname_userlog, "r")) != NULL )
	      {
			  fbclose(user_log_fb);
			  user_log_fb = fbopen(ConfigFileEntry.fname_userlog, "a");
	      }
	  }

	if( user_log_fb != NULL )
	  {
	    ircsprintf(linebuf,
		       "%s (%3d:%02d:%02d): %s!%s@%s %d/%d\n",
		       myctime(sptr->firsttime), on_for / 3600,
		       (on_for % 3600)/60, on_for % 60,
		       sptr->name,
		       sptr->username,
		       sptr->host,
		       sptr->localClient->sendK,
		       sptr->localClient->receiveK);

	    fbputs(linebuf, user_log_fb);

	    /* Now, schedule file resync every 60 seconds */

	    eventAdd("user_log_resync", user_log_resync, NULL,
		     60, 0 );

	  }
      }
  }
#endif
}

/*
 * user_log_resync
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	-
 */
static void
user_log_resync(void *notused)
{
  if (user_log_fb != NULL)
    {
      fbclose(user_log_fb);
      user_log_fb = NULL;
    }
}

/*
 * log_oper
 *
 * inputs	- pointer to client
 * output	- none
 * side effects - FNAME_OPERLOG is written to, if its present
 */

void log_oper( struct Client *sptr, char *name )
{
  FBFILE *oper_fb;
  char linebuf[BUFSIZE];

  if (!ConfigFileEntry.fname_operlog)
	  return;
  
  if (IsPerson(sptr))
    {
      if( (oper_fb = fbopen(ConfigFileEntry.fname_operlog, "r")) != NULL )
	{
	  fbclose(oper_fb);
	  oper_fb = fbopen(ConfigFileEntry.fname_operlog, "a");
	}

      if(oper_fb != NULL)
	{
	  ircsprintf(linebuf, "%s OPER (%s) by (%s!%s@%s)\n",
		     myctime(CurrentTime), name, 
		     sptr->name, sptr->username,
		     sptr->host);

	  fbputs(linebuf,oper_fb);
	  fbclose(oper_fb);
	}
    }
}

/*
 * $Id$
 */

/* Define if you have the poll() system call.  */
#undef USE_POLL

/* Define with our select type */
#undef SELECT_TYPE

/* Define if we are going to use /dev/poll for network IO */
#undef HAVE_DEVPOLL

#undef USE_DEVPOLL

/* Using kqueue */
#undef USE_KQUEUE

/* Using poll */
#undef USE_POLL

/* Using select */
#undef USE_SELECT

/* Using devpoll */
#undef USE_DEVPOLL

/* Define if we have socklen_t */
#undef HAVE_SOCKLEN_T

/* Define if we can include both string.h and strings.h */
#undef STRING_WITH_STRINGS

/* This is a string containing any extra underscores that must be prepended
 * to symbols loaded from modules.
 */
#undef SYMBOL_PREFIX

/* IPv6 support */
#undef IPV6

/* u_int32_t */
#undef u_int32_t

/* static modules */
#undef STATIC_MODULES
/*
 * $Id$ 
 */

#ifndef INCLUDED_s_misc_h
#define INCLUDED_s_misc_h
#ifndef INCLUDED_sys_types_h
#include <sys/types.h>
#define INCLUDED_sys_types_h
#endif

struct Client;
struct ConfItem;

extern char*   date(time_t);
extern const char* smalldate(time_t);
extern char    *small_file_date(time_t);

#define _1MEG     (1024.0)
#define _1GIG     (1024.0*1024.0)
#define _1TER     (1024.0*1024.0*1024.0)
#define _GMKs(x)  ( (x > _1TER) ? "Terabytes" : ((x > _1GIG) ? "Gigabytes" : \
                  ((x > _1MEG) ? "Megabytes" : "Kilobytes")))
#define _GMKv(x)  ( (x > _1TER) ? (float)(x/_1TER) : ((x > _1GIG) ? \
               (float)(x/_1GIG) : ((x > _1MEG) ? (float)(x/_1MEG) : (float)x)))
	       

#endif



/************************************************************************
 *   IRC - Internet Relay Chat, src/irc_string.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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
 *  $Id$
 */
#include "tools.h"
#include "irc_string.h"
#include "list.h"

#include <assert.h>
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
#include <stdlib.h>
#include <time.h>
#include "memdebug.h"

/*
 * myctime - This is like standard ctime()-function, but it zaps away
 *   the newline from the end of that string. Also, it takes
 *   the time value as parameter, instead of pointer to it.
 *   Note that it is necessary to copy the string to alternate
 *   buffer (who knows how ctime() implements it, maybe it statically
 *   has newline there and never 'refreshes' it -- zapping that
 *   might break things in other places...)
 *
 *
 * Thu Nov 24 18:22:48 1986 
 */
const char* myctime(time_t value)
{
  static char buf[32];
  char*       p;

  strcpy(buf, ctime(&value));
  if ((p = strchr(buf, '\n')) != NULL)
    *p = '\0';
  return buf;
}

/*
 * strncpy_irc - optimized strncpy
 * This may not look like it would be the fastest possible way to do it,
 * but it generally outperforms everything else on many platforms, 
 * including asm library versions and memcpy, if compiled with the 
 * optimizer on. (-O2 for gcc) --Bleep
 */
char* strncpy_irc(char* s1, const char* s2, size_t n)
{
  register char* endp = s1 + n;
  register char* s = s1;
  while (s < endp && (*s++ = *s2++))
    ;
  return s1;
}

/*
 * MyMalloc - allocate memory, call outofmemory on failure
 */
#ifdef DEBUGMEM
void* _MyMalloc(size_t x, char * file, int line)
#else
void* _MyMalloc(size_t x)
#endif
{
  void* ret = malloc(x);

  if (!ret)
    outofmemory();
  else
    bzero(ret, x); 
#ifdef DEBUGMEM
  DbgMemAlloc(file, line, x, DBGMEM_MALLOC, ret);
#endif
  return ret;
}

/*
 * MyRealloc - reallocate memory, call outofmemory on failure
 */
#ifdef DEBUGMEM
void* _MyRealloc(void* x, size_t y, char * file, int line)
#else
void* _MyRealloc(void* x, size_t y)
#endif
{
  char *ret = realloc(x, y);

  if (!ret)
    outofmemory();
#ifdef DEBUGMEM
  DbgMemRealloc(file, line, DBGMEM_MALLOC, x, y, ret);
#endif
  return ret;
}

#ifdef DEBUGMEM
void _MyFree(void *x, char * file, int line)
#else
void _MyFree(void *x)
#endif
{
  if ((x))
    free((x));
#ifdef DEBUGMEM
  DbgMemFree(file, line, DBGMEM_MALLOC, x);
#endif
}

void DupStringNoDbg(char **x, char *y) {
  (*x) = MyMalloc(strlen(y) + 1);
  strcpy((*x), y);
}

/*
 * clean_string - clean up a string possibly containing garbage
 *
 * *sigh* Before the kiddies find this new and exciting way of 
 * annoying opers, lets clean up what is sent to local opers
 * -Dianora
 */
char* 
clean_string(char* dest, const unsigned char* src, size_t len)
{
  char* d    = dest; 
  assert(0 != dest);
  assert(0 != src);

  len -= 3;  /* allow for worst case, '^A\0' */

  while (*src && (len > 0))
    {
      if(*src & 0x80)             /* if high bit is set */
        {
          *d++ = '.';
          --len;
        }
      else if(!IsPrint(*src))       /* if NOT printable */
        {
          *d++ = '^';
          --len;
          *d++ = 0x40 + *src;   /* turn it into a printable */
        }
      else
        *d++ = *src;
      ++src;
      --len;
    }
  *d = '\0';
  return dest;
}

/*
 * strip_tabs(dst, src, length)
 *
 *   Copies src to dst, while converting all \t (tabs) into spaces.
 *
 * NOTE: jdc: I have a gut feeling there's a faster way to do this.
 */
char *strip_tabs(char *dest, const unsigned char *src, size_t len)
{
  char *d = dest;
  /* Sanity check; we don't want anything nasty... */
  assert(0 != dest);
  assert(0 != src);

  while (*src && (len > 0))
  {
    if (*src == '\t')
    {
      *d++ = ' ';   /* Translate the tab into a space */
    }
    else
    {
      *d++ = *src;  /* Copy src to dst */
    }
    ++src;
    --len;
  }
  *d = '\0';   /* Null terminate, kthxbye */
  return dest;
}

#if !defined( HAVE_STRTOKEN )
/*
 * strtoken - walk through a string of tokens, using a set of separators
 *   argv 9/90
 *
 */
char* strtoken(char** save, char* str, char* fs)
{
  char* pos = *save;  /* keep last position across calls */
  char* tmp;

  if (str)
    pos = str;    /* new string scan */

  while (pos && *pos && strchr(fs, *pos) != NULL)
    ++pos;        /* skip leading separators */

  if (!pos || !*pos)
    return (pos = *save = NULL);   /* string contains only sep's */

  tmp = pos;       /* now, keep position of the token */

  while (*pos && strchr(fs, *pos) == NULL)
    ++pos;       /* skip content of the token */

  if (*pos)
    *pos++ = '\0';    /* remove first sep after the token */
  else
    pos = NULL;    /* end of string */

  *save = pos;
  return tmp;
}
#endif /* !HAVE_STRTOKEN */


/* 
 * From: Thomas Helvey <tomh@inxpress.net>
 */
static const char *IpQuadTab[] =
{
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",
   "10",  "11",  "12",  "13",  "14",  "15",  "16",  "17",  "18",  "19",
   "20",  "21",  "22",  "23",  "24",  "25",  "26",  "27",  "28",  "29",
   "30",  "31",  "32",  "33",  "34",  "35",  "36",  "37",  "38",  "39",
   "40",  "41",  "42",  "43",  "44",  "45",  "46",  "47",  "48",  "49",
   "50",  "51",  "52",  "53",  "54",  "55",  "56",  "57",  "58",  "59",
   "60",  "61",  "62",  "63",  "64",  "65",  "66",  "67",  "68",  "69",
   "70",  "71",  "72",  "73",  "74",  "75",  "76",  "77",  "78",  "79",
   "80",  "81",  "82",  "83",  "84",  "85",  "86",  "87",  "88",  "89",
   "90",  "91",  "92",  "93",  "94",  "95",  "96",  "97",  "98",  "99",
  "100", "101", "102", "103", "104", "105", "106", "107", "108", "109",
  "110", "111", "112", "113", "114", "115", "116", "117", "118", "119",
  "120", "121", "122", "123", "124", "125", "126", "127", "128", "129",
  "130", "131", "132", "133", "134", "135", "136", "137", "138", "139",
  "140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
  "150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
  "160", "161", "162", "163", "164", "165", "166", "167", "168", "169",
  "170", "171", "172", "173", "174", "175", "176", "177", "178", "179",
  "180", "181", "182", "183", "184", "185", "186", "187", "188", "189",
  "190", "191", "192", "193", "194", "195", "196", "197", "198", "199",
  "200", "201", "202", "203", "204", "205", "206", "207", "208", "209",
  "210", "211", "212", "213", "214", "215", "216", "217", "218", "219",
  "220", "221", "222", "223", "224", "225", "226", "227", "228", "229",
  "230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
  "240", "241", "242", "243", "244", "245", "246", "247", "248", "249",
  "250", "251", "252", "253", "254", "255"
};

/*
 * inetntoa - in_addr to string
 *      changed name to remove collision possibility and
 *      so behaviour is guaranteed to take a pointer arg.
 *      -avalon 23/11/92
 *  inet_ntoa --  returned the dotted notation of a given
 *      internet number
 *      argv 11/90).
 *  inet_ntoa --  its broken on some Ultrix/Dynix too. -avalon
 */

const char* inetntoa(const char* in)
{
  static char                    buf[16];
  register char*                bufptr = buf;
  register const unsigned char* a = (const unsigned char*)in;
  register const char*          n;

  n = IpQuadTab[ *a++ ];
  while (*n)
    *bufptr++ = *n++;
  *bufptr++ = '.';
  n = IpQuadTab[ *a++ ];
  while ( *n )
    *bufptr++ = *n++;
  *bufptr++ = '.';
  n = IpQuadTab[ *a++ ];
  while ( *n )
    *bufptr++ = *n++;
  *bufptr++ = '.';
  n = IpQuadTab[ *a ];
  while ( *n )
    *bufptr++ = *n++;
  *bufptr = '\0';
  return buf;
}

#ifdef IPV6
/* mk6addrstr - convert an IPv6 IP into a string */

#define IP6ADDRLEN 40

char *
mk6addrstr(struct in6_addr *addr)
{
        uint8_t *cp = (uint8_t *)&addr->s6_addr;
        char dest2[IP6ADDRLEN + 1];
        static char dest[IP6ADDRLEN + 1];
        char *c;
        int i = 0, k = 0, j;
        int lo = -1;
        int am = 0;
        int le = 0;

        /* hehe */
        sprintf(dest2,
                        "%x%x%x%x:%x%x%x%x:%x%x%x%x:%x%x%x%x:%x%x%x%x:%x%x%x%x:%x%x%x%x:%x%x%x%x",
                        cp[0]  >> 4, cp[0]  & 0xf, cp[1]  >> 4, cp[1]  & 0xf,
                        cp[2]  >> 4, cp[2]  & 0xf, cp[3]  >> 4, cp[3]  & 0xf,
                        cp[4]  >> 4, cp[4]  & 0xf, cp[5]  >> 4, cp[5]  & 0xf,
                        cp[6]  >> 4, cp[6]  & 0xf, cp[7]  >> 4, cp[7]  & 0xf,
                        cp[8]  >> 4, cp[8]  & 0xf, cp[9]  >> 4, cp[9]  & 0xf,
                        cp[10] >> 4, cp[10] & 0xf, cp[11] >> 4, cp[11] & 0xf,
                        cp[12] >> 4, cp[12] & 0xf, cp[13] >> 4, cp[13] & 0xf,
                        cp[14] >> 4, cp[14] & 0xf, cp[15] >> 4, cp[15] & 0xf);
        /* remove excessive zeros */
        for(j = 0, k = 0; dest2[j];) 
        {
                if (dest2[j] == ':') 
                {
                        dest[k++] = dest2[j++];
                        while(dest2[j] == '0')
                                j++;
                        continue;
                }
                dest[k++] = dest2[j++];
        }
        dest[k] = 0;
        return dest;
}
#endif

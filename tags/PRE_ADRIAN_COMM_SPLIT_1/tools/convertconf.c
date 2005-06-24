/************************************************************************
 *   IRC - Internet Relay Chat, tools/convertconf.c
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define BUFSIZE 512

#define IS_LEAF 0
#define IS_HUB 1

struct ConnectPair
{
  struct ConnectPair* next;     /* list node pointer */
  char*            name;     /* server name */
  char*            host;     /* host part of user@host */
  char*            c_passwd;
  char*            n_passwd;
  char*		   hub_mask;
  char*		   leaf_mask;
  int		   compressed;
  int		   lazylink;
  int              port;
  char             *class;     /* Class of connection */
};

static struct ConnectPair* base_ptr=NULL;

static void ConvertConf(FILE* file,FILE *out);
static void usage(void);
static char *getfield(char *);
static void ReplaceQuotes(char *out, char *in);
static void oldParseOneLine(FILE *out, char *in);
static void PrintOutServers(FILE *out);
static void PairUpServers(struct ConnectPair* );
static void AddHubOrLeaf(int type,char* name,char* host);
static void OperPrivsFromString(FILE* , char* );
static char* ClientFlags(FILE* ,char* ,char* );

int main(int argc,char *argv[])
{
  FILE *in;
  FILE *out;

  if(argc < 3)
    usage();

  if (( in = fopen(argv[1],"r")) == (FILE *)NULL )
    {
      fprintf(stderr,"Can't open %s for reading\n", argv[1]);
      usage();
    }

  if (( out = fopen(argv[2],"w")) == (FILE *)NULL )
    {
      fprintf(stderr,"Can't open %s for writing\n", argv[2]);
      usage();
    }
  
  ConvertConf(in,out);
  return 0;
}

static void usage()
{
  fprintf(stderr,"convertconf ircd.conf.old ircd.conf.new\n");
  exit(-1);
}

/*
** ConvertConf() 
**    Read configuration file.
**
*
* Inputs        - FILE* to config file to convert
*		- FILE* to output for new style conf
*
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

static void ConvertConf(FILE* file,FILE *out)
{
  char             line[BUFSIZE];
  char             quotedLine[BUFSIZE];
  char*            p;

  while (fgets(line, sizeof(line), file))
    {
      if ((p = strchr(line, '\n')))
        *p = '\0';

      ReplaceQuotes(quotedLine,line);

      if (!*quotedLine || quotedLine[0] == '#' || quotedLine[0] == '\n' ||
          quotedLine[0] == ' ' || quotedLine[0] == '\t')
        continue;

      if(quotedLine[0] == '.')
        {
          char *filename;
          char *back;

          if(!strncmp(quotedLine+1,"include ",8))
            {
              if( (filename = strchr(quotedLine+8,'"')) )
                filename++;
              else
                {
                  fprintf(stderr, "Bad config line: %s", quotedLine);
                  continue;
                }

              if( (back = strchr(filename,'"')) )
                *back = '\0';
              else
                {
                  fprintf(stderr, "Bad config line: %s", quotedLine);
                  continue;
                }

	    }
	}

      /* Could we test if it's conf line at all?        -Vesa */
      if (quotedLine[1] == ':')
        oldParseOneLine(out,quotedLine);

    }

  PrintOutServers(out);
  fclose(file);
}

/*
 * ReplaceQuotes
 * Inputs       - input line to quote
 * Output       - quoted line
 * Side Effects - All quoted chars in input are replaced
 *                with quoted values in output, # chars replaced with '\0'
 *                otherwise input is copied to output.
 */
static void ReplaceQuotes(char* quotedLine,char *inputLine)
{
  char *in;
  char *out;
  static char  quotes[] = {
    0,    /*  */
    0,    /* a */
    '\b', /* b */
    0,    /* c */
    0,    /* d */
    0,    /* e */
    '\f', /* f */
    0,    /* g */
    0,    /* h */
    0,    /* i */
    0,    /* j */
    0,    /* k */
    0,    /* l */
    0,    /* m */
    '\n', /* n */
    0,    /* o */
    0,    /* p */
    0,    /* q */
    '\r', /* r */
    0,    /* s */
    '\t', /* t */
    0,    /* u */
    '\v', /* v */
    0,    /* w */
    0,    /* x */
    0,    /* y */
    0,    /* z */
    0,0,0,0,0,0 
    };

  /*
   * Do quoting of characters and # detection.
   */
  for (out = quotedLine,in = inputLine; *in; out++, in++)
    {
      if (*in == '\\')
	{
          in++;
          if(*in == '\\')
            *out = '\\';
          else if(*in == '#')
            *out = '#';
	  else
	    *out = quotes[ (unsigned int) (*in & 0x1F) ];
	}
      else if (*in == '#')
        {
	  *out = '\0';
          return;
	}
      else
        *out = *in;
    }
  *out = '\0';
}

/*
 * oldParseOneLine
 * Inputs       - pointer to line to parse
 *		- pointer to output to write
 * Output       - 
 * Side Effects - Parse one old style conf line.
 */

static void oldParseOneLine(FILE *out,char* line)
{
  char conf_letter;
  char* tmp;
  char* user_field=(char *)NULL;
  char* passwd_field=(char *)NULL;
  char* host_field=(char *)NULL;
  char*	spoof_field;
  char* client_allow;
  char* port_field=(char *)NULL;
  char* class_field=(char *)NULL;
  struct ConnectPair* pair;
  int sendq = 0;
  int restricted;

  tmp = getfield(line);

  conf_letter = *tmp;

  restricted = 0;

  for (;;) /* Fake loop, that I can use break here --msa */
    {
      /* host field */
      if ((host_field = getfield(NULL)) == NULL)
	break;
      
      /* pass field */
      if ((passwd_field = getfield(NULL)) == NULL)
	break;

      /* user field */
      if ((user_field = getfield(NULL)) == NULL)
	break;

      /* port field */
      if ((port_field = getfield(NULL)) == NULL)
	break;

      /* class field */
      if ((class_field = getfield(NULL)) == NULL)
	break;
      
      break;
      /* NOTREACHED */
    }

  switch( conf_letter )
    {
    case 'A':case 'a': /* Name, e-mail address of administrator */
      fprintf(out,"\tadministrator {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", passwd_field);
      if(user_field)
	fprintf(out,"\t\temail=\"%s\";\n", user_field);
      if(passwd_field)
	fprintf(out,"\t\tdescription=\"%s\";\n", host_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'c':
      pair = (struct ConnectPair *)malloc(sizeof(struct ConnectPair));
      memset(pair,0,sizeof(struct ConnectPair));
      if(user_field)
	pair->name = strdup(user_field);
      if(host_field)
	pair->host = strdup(host_field);
      if(passwd_field)
	pair->c_passwd = strdup(passwd_field);
      if(port_field)
	pair->port = atoi(port_field);
      if(class_field)
	pair->class = strdup(class_field);
      pair->compressed = 1;
      PairUpServers(pair);
      break;

    case 'C':
      pair = (struct ConnectPair *)malloc(sizeof(struct ConnectPair));
      memset(pair,0,sizeof(struct ConnectPair));
      if(user_field)
	pair->name = strdup(user_field);
      if(host_field)
	pair->host = strdup(host_field);
      if(passwd_field)
	pair->c_passwd = strdup(passwd_field);
      if(port_field)
	pair->port = atoi(port_field);
      if(class_field)
	pair->class = strdup(class_field);
      PairUpServers(pair);
      break;

    case 'd':
      fprintf(out,"\tacl_exception {\n");
      if(user_field)
	fprintf(out,"\t\tip=\"%s\";\n", user_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'D': /* Deny lines (immediate refusal) */
      fprintf(out,"\tdeny {\n");
      if(host_field)
	fprintf(out,"\t\tip=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'H': /* Hub server line */
    case 'h':
      AddHubOrLeaf(IS_HUB,user_field,host_field);
      break;

    case 'i': 
      restricted = 1;

    case 'I': 
      fprintf(out,"\tauth {\n");

      spoof_field = (char *)NULL;
      client_allow = (char *)NULL;

      if(host_field)
	{
	  if( strcmp(host_field,"NOMATCH") && (*host_field != 'x'))
	    {
	      if( user_field && (*user_field == 'x'))
		{
		  client_allow = ClientFlags(out,NULL,host_field);
		  if(client_allow)
		    fprintf(out,"\t\tip=%s;\n", client_allow );
		}
	      else
		spoof_field = host_field;
	    }
	}

      if(passwd_field && *passwd_field)
	fprintf(out,"\t\tpasswd=\"%s\";\n", passwd_field);	
      else
	fprintf(out,"\t\tpasswd=\"*\";\n");	

      if(!client_allow && user_field)
	{
	  client_allow = ClientFlags(out,spoof_field,user_field);
	  if(client_allow)
	    {
	      fprintf(out,"\t\tuser=\"%s\";\n", client_allow );
	    }
	}

      if(restricted)
	fprintf(out,"\t\trestricted;\n");	

      if(class_field)
	fprintf(out,"\t\tclass=\"%s\";\n", class_field);	
      fprintf(out,"\t};\n\n");
      break;
      
    case 'K': /* Kill user line on irc.conf           */
    case 'k':
      fprintf(out,"\tkill {\n");
      if(host_field)
	fprintf(out,"\t\tuser=\"%s@%s\";\n", user_field,host_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'L': /* guaranteed leaf server */
    case 'l':
      AddHubOrLeaf(IS_LEAF,user_field,host_field);
      break;

/* Me. Host field is name used for this host */
      /* and port number is the number of the port */
    case 'M':
    case 'm':
      fprintf(out,"\tserverinfo {\n");
      if(host_field)
	fprintf(out,"\t\tname=%s;\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\tvhost=\"%s\";\n", passwd_field);
      if(user_field)
	fprintf(out,"\t\tdescription=\"%s\";\n", user_field);
      if(port_field)
	fprintf(out,"\t\thub=yes;\n");
      else
     	fprintf(out,"\t\thub=no;\n");
      fprintf(out,"\t};\n\n");
      break;

    case 'n': 
      pair = (struct ConnectPair *)malloc(sizeof(struct ConnectPair));
      memset(pair,0,sizeof(struct ConnectPair));
      if(user_field)
	pair->name = strdup(user_field);
      if(host_field)
	pair->host = strdup(host_field);
      if(passwd_field)
	pair->n_passwd = strdup(passwd_field);
      pair->lazylink = 1;
      if(port_field)
	pair->port = atoi(port_field);
      if(class_field)
	pair->class = strdup(class_field);
      PairUpServers(pair);
      break;

    case 'N': 
      pair = (struct ConnectPair *)malloc(sizeof(struct ConnectPair));
      memset(pair,0,sizeof(struct ConnectPair));
      if(user_field)
	pair->name = strdup(user_field);
      if(host_field)
	pair->host = strdup(host_field);
      if(passwd_field)
	pair->n_passwd = strdup(passwd_field);
      if(port_field)
	pair->port = atoi(port_field);
      if(class_field)
	pair->class = strdup(class_field);
      PairUpServers(pair);
      break;

      /* Operator. Line should contain at least */
      /* password and host where connection is  */
    case 'O':
      /* defaults */
      fprintf(out,"\toperator {\n");
      if(user_field)
	fprintf(out,"\t\tname=\"%s\";\n", user_field);
      if(host_field)
	{
	  fprintf(out,"\t\tuser=\"%s\";\n", host_field);
	}
      if(passwd_field)
	fprintf(out,"\t\tpassword=\"%s\";\n", passwd_field);
      fprintf(out,"\t\tglobal=yes;\n");
      if(port_field)
	OperPrivsFromString(out,port_field);
      if(class_field)
	fprintf(out,"\t\tclass=\"%s\";\n", class_field);	
      fprintf(out,"\t};\n\n");
      break;

      /* Local Operator, (limited privs --SRB) */
    case 'o':
      fprintf(out,"\tlocal_operator {\n");
      if(user_field)
	fprintf(out,"\t\tname=\"%s\";\n", user_field);
      if(host_field)
	fprintf(out,"\t\thost=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\tpassword=\"%s\";\n", passwd_field);
      fprintf(out,"\t\tglobal=no;\n");
      if(port_field)
	OperPrivsFromString(out,port_field);
      if(class_field)
	fprintf(out,"\t\tclass=\"%s\";\n", class_field);	
      fprintf(out,"\t};\n\n");
      break;

    case 'P': /* listen port line */
    case 'p':
      fprintf(out,"\tlisten {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", host_field);
      if(port_field)
	fprintf(out,"\t\tport=%d;\n", atoi(port_field));
      fprintf(out,"\t};\n\n");
      break;

    case 'Q': /* reserved nicks */
    case 'q': 
      fprintf(out,"\tquarantine {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'U': 
    case 'u': 
      fprintf(out,"\tshared {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'X': /* rejected gecos */
    case 'x': 
      fprintf(out,"\tgecos {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\t\treason=\"%s\";\n", passwd_field);
      fprintf(out,"\t};\n\n");
      break;

    case 'Y':
    case 'y':
      fprintf(out,"\tclass {\n");
      if(host_field)
	fprintf(out,"\t\tname=\"%s\";\n", host_field);
      if(passwd_field)
	{
	  int ping_time;
	  ping_time = atoi(passwd_field);
	  fprintf(out,"\t\tping_time=%d;\n", ping_time );
	}
      if(user_field)
	{
	  int number_per_ip;
	  number_per_ip = atoi(user_field);
	  fprintf(out,"\t\tnumber_per_ip=%d;\n", number_per_ip );
	}
      if(port_field)
	{
	  int max_number;
	  max_number = atoi(port_field);
	  fprintf(out,"\t\tmax_number=%d;\n", max_number );
	}
      if(class_field)
	sendq = atoi(class_field);
      fprintf(out,"\t\tsendq=%d;\n", sendq);
      fprintf(out,"\t};\n\n");
      break;
      
    default:
      fprintf(stderr, "Error in config file: %s", line);
      break;
    }
}

/*
 * PrintOutServers
 *
 * In		- FILE pointer
 * Out		- NONE
 * Side Effects	- Print out connect configurations
 */
static void PrintOutServers(FILE* out)
{
  struct ConnectPair* p;

  for(p = base_ptr; p; p = p->next)
    {
      if(p->name && p->c_passwd && p->n_passwd && p->host)
	{
	  fprintf(out,"\tconnect {\n");
	  fprintf(out,"\t\thost=\"%s\";\n", p->name);
	  fprintf(out,"\t\tname=\"%s\";\n", p->host);
	  fprintf(out,"\t\tsend_password=\"%s\";\n", p->c_passwd);
	  fprintf(out,"\t\taccept_password=\"%s\";\n", p->n_passwd);
	  fprintf(out,"\t\tport=%d;\n", p->port );

	  if(p->compressed)
	    fprintf(out,"\t\tcompressed=yes;\n");

	  if(p->lazylink)
	    fprintf(out,"\t\tlazylink=yes;\n");

	  if(p->hub_mask)
	    {
	      fprintf(out,"\t\thub_mask=\"%s\";\n",p->hub_mask);
	    }
	  else
	    {
	      if(p->leaf_mask)
		fprintf(out,"\t\tleaf_mask=\"%s\";\n",p->leaf_mask);
	    }

	  if(p->class)
	    fprintf(out,"\t\tclass=\"%s\";\n", p->class );

	  fprintf(out,"\t};\n\n");
	}
    }
}

/*
 * PairUpServers
 *
 * In		- pointer to ConnectPair
 * Out		- none
 * Side Effects	- Pair up C/N lines on servers into one output
 */
static void PairUpServers(struct ConnectPair* pair)
{
  struct ConnectPair *p;

  for(p = base_ptr; p; p = p->next )
    {
      if(p->name && pair->name )
	{
	  if( !strcasecmp(p->name,pair->name) )
	    {
	      if(!p->n_passwd && pair->n_passwd)
		p->n_passwd = strdup(pair->n_passwd);

	      if(!p->c_passwd && pair->c_passwd)
		p->c_passwd = strdup(pair->c_passwd);

	      p->compressed |= pair->compressed;
	      p->lazylink |= pair->lazylink;

	      if(pair->port)
		p->port = pair->port;

	      return;
	    }
	}
    }

  if(base_ptr)
    {
      pair->next = base_ptr;
      base_ptr = pair;
    }
  else
    base_ptr = pair;
}

/*
 * AddHubOrLeaf
 *
 * In		- type either IS_HUB or IS_LEAF
 *		- name of leaf or hub
 *		- mask 
 * Out		- none
 * Side Effects	- Pair up hub or leaf with connect configuration
 */
static void AddHubOrLeaf(int type,char* name,char* host)
{
  struct ConnectPair* p;
  struct ConnectPair* pair;

  for(p = base_ptr; p; p = p->next )
    {
      if(p->name && name )
	{
	  if( !strcasecmp(p->name,name) )
	    {
	      if(type == IS_HUB)
		p->hub_mask = strdup(host);

	      if(type == IS_LEAF)
		p->leaf_mask = strdup(host);
	      return;
	    }
	}
    }

  pair = (struct ConnectPair *)malloc(sizeof(struct ConnectPair));
  memset(pair,0,sizeof(struct ConnectPair));

  pair->name = strdup(name);

  if(type == IS_HUB)
    {
      pair->hub_mask = strdup(host);
    }
  else if(type == IS_LEAF)
    {
      pair->leaf_mask = strdup(host);
    }

  if(base_ptr)
    {
      pair->next = base_ptr;
      base_ptr = pair;
    }
  else
    base_ptr = pair;
}

/*
 * field breakup for ircd.conf file.
 */
static char *getfield(char *newline)
{
  static char *line = (char *)NULL;
  char  *end, *field;
        
  if (newline)
    line = newline;

  if (line == (char *)NULL)
    return((char *)NULL);

  field = line;
  if ((end = strchr(line,':')) == NULL)
    {
      line = (char *)NULL;
      if ((end = strchr(field,'\n')) == (char *)NULL)
        end = field + strlen(field);
    }
  else
    line = end + 1;
  *end = '\0';
  return(field);
}

/* OperPrivsFromString
 *
 * inputs        - privs as string
 * output        - none
 * side effects -
 */

static void OperPrivsFromString(FILE* out, char *privs)
{
  while(*privs)
    {
      if(*privs == 'O')                     /* allow global kill */
	{
	  fprintf(out,"\t\tglobal_kill=yes;\n");
	}
      else if(*privs == 'o')                /* disallow global kill */
	{
	  fprintf(out,"\t\tglobal_kill=no;\n");
	}
      else if(*privs == 'U')                /* allow unkline */
	{
	  fprintf(out,"\t\tunkline=yes;\n");
	}
      else if(*privs == 'u')                /* disallow unkline */
	{
	  fprintf(out,"\t\tunkline=no;\n");
	}
      else if(*privs == 'R')               /* allow remote squit/connect etc.*/
	{
	  fprintf(out,"\t\tremote=yes;\n");
	}
      else if(*privs == 'r')                /* disallow remote squit/connect etc.*/
	{
	  fprintf(out,"\t\tremote=no;\n");
	}
      else if(*privs == 'N')                /* allow +n see nick changes */
	{
	  fprintf(out,"\t\tnick_changes=yes;\n");
	}
      else if(*privs == 'n')                /* disallow +n see nick changes */
	{
	  fprintf(out,"\t\tnick_changes=no;\n");
	}
      else if(*privs == 'K')                /* allow kill and kline privs */
	{
	  fprintf(out,"\t\tkline_kill=yes;\n");
	}
      else if(*privs == 'k')                /* disallow kill and kline privs */
	{
	  fprintf(out,"\t\tkline_kill=no;\n");
	}
      else if(*privs == 'G')                /* allow gline */
	{
	  fprintf(out,"\t\tgline=yes;\n");
	}
      else if(*privs == 'g')                /* disallow gline */
	{
	  fprintf(out,"\t\tgline=no;\n");
	}
      else if(*privs == 'H')                /* allow rehash */
	{
	  fprintf(out,"\t\trehash=yes;\n");
	}
      else if(*privs == 'h')                /* disallow rehash */
	{
	  fprintf(out,"\t\trehash=no;\n");
	}
      else if(*privs == 'D')
	{
	  fprintf(out,"\t\tdie=yes;\n");
	}
      else if(*privs == 'd')
	{
	  fprintf(out,"\t\tdie=no;\n");
 	}
      privs++;
    }
}

/*
 *
 *
 */

static char* ClientFlags(FILE *out, char* spoof, char *tmp)
{
  for(;*tmp;tmp++)
    {
      switch(*tmp)
        {
        case '=':
	  if(spoof)
	    fprintf(out,"\t\tspoof=\"%s\";\n",spoof);	  
          break;
	case '!':
	  fprintf(out,"\t\tlimit_ip;\n");
	  break;
        case '-':
	  fprintf(out,"\t\tno_tilde;\n");	  
          break;
        case '+':
	  fprintf(out,"\t\tneed_ident;\n");	  
          break;
        case '$':
	  fprintf(out,"\t\thave_ident;\n");	  
          break;
        case '%':
	  fprintf(out,"\t\tnomatch_ip;\n");	  
          break;
        case '^':        /* is exempt from k/g lines */
	  fprintf(out,"\t\tkline_exempt=yes;\n");	  
          break;
        case '&':        /* can run a bot */
	  fprintf(out,"\t\tallow_bots=yes;\n");	  
          break;
        case '>':        /* can exceed max connects */
	  fprintf(out,"\t\texceed_limit=yes;\n");	  
          break;
        case '<':        /* can idle */
	  fprintf(out,"\t\tcan_idle=yes;\n");	  
          break;
        default:
          return tmp;
        }
    }
  return tmp;
}


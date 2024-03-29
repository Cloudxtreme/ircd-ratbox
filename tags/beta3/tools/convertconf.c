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
static void AddLoggingBlock(FILE* out);
static void AddGeneralBlock(FILE* out);
static void AddModulesBlock(FILE* out);
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

  puts("Adding the logging/general/modules blocks to your config..\n");
  AddLoggingBlock(out);
  AddGeneralBlock(out);
  AddModulesBlock(out);

  puts("The config file has been converted however you MUST rearrange the config:\n"
       "   o class blocks must be before anything that uses then (auth/connect)\n"
       "   o auth blocks must be listed in order of importance\n"
       "        - spoofs etc first\n"
       "        - *@* last\n"
       "     if this is NOT done, chances are spoofs wont work.\n"
       "   o the general/logging/modules parts will need to be edited\n"
       "   o the config file needs to be checked manually.. as this\n"
       "     doesnt have human brains ;P\n");
  return 0;
}

static void usage()
{
  fprintf(stderr,"convertconf ircd.conf.old ircd.conf.new\n");
  exit(-1);
}

static void AddLoggingBlock(FILE *out)
{
  fprintf(out, "logging {\n");
  fprintf(out, "\t/* These three paths are not *CURRENTLY* used.  They are still\n");
  fprintf(out, "\t * compiled into the ircd with config.h\n");
  fprintf(out, "\t */\n\tlogpath = \"/var/log/ircd/\";\n\toper_log = \"oper.log\";\n");
  fprintf(out, "\tgline_log = \"gline.log\";\n\n\t/* This option is used, however.\n");
  fprintf(out, "\t * The following settings are valid (This can also be changed\n");
  fprintf(out, "\t * by /quote SET LOG in the ircd)\n");
  fprintf(out, "\t * L_CRIT, L_ERROR, L_WARN, L_NOTICE, L_TRACE, L_INFO, L_DEBUG\n");
  fprintf(out, "\t */\n\tlog_level = L_INFO;\n};\n\n");
}

static void AddGeneralBlock(FILE *out)
{
  fprintf(out, "general {\n");

  fprintf(out, "\t/* Specify the default setting of FLOODCOUNT at startup.\n");
  fprintf(out, "\t * This is how many lines per second we allow before we throttle\n");
  fprintf(out, "\t * users MSGs/Notices ONLY affects the startup value, otherwise\n");
  fprintf(out, "\t * use: /quote set floodcount.\n\t*/\n");
  fprintf(out, "\tdefault_floodcount=8;\n\n");

  fprintf(out, "\t/* Send a notice to all opers on the server when someone tries\n");
  fprintf(out, "\t * to OPER and uses the wrong password.\n\t*/\n");
  fprintf(out, "\tfailed_oper_notice=yes;\n\n");

  fprintf(out, "\t/* If failed_oper_notice is set to \"yes\", also notify when someone\n");
  fprintf(out, "\t * fails to OPER because of an identity mismatch (wrong host or nick).\n\t*/\n");
  fprintf(out, "\tfailed_oper_notice = yes;\n\n");

  fprintf(out, "\t/* Define how many 'dot' characters are permitted in an ident reply\n");
  fprintf(out, "\t * before rejecting the user.\n\t*/\n");
  fprintf(out, "\tdots_in_ident=2;\n\n");

  fprintf(out, "\t/* Define how many non-wildcard (not: '.' '*' '?' '@') characters are\n");
  fprintf(out, "\t * needed in klines for them to be placed.  Does not affect\n");
  fprintf(out, "\t * klines hand-placed in kline.conf.  eg to disallow *@*.com.\n\t*/\n");
  fprintf(out, "\tmin_nonwildcard = 4;\n\n");

  fprintf(out, "\t/* Enable the nick flood control code. */\n\tanti_nick_flood = yes;\n");
  fprintf(out, "\t/* These settings will allow 5 nick changes in 20 seconds. */\n");
  fprintf(out, "\tmax_nick_time = 20 seconds;\n\tmax_nick_changes = 5;\n\n");

  fprintf(out, "\t/* Do not allow the clients exit message to be\n");
  fprintf(out, "\t * sent to a channel if the client has been on for less than\n\t * anti_spam_exit_message_time\n");
  fprintf(out, "\t * The idea is, some spambots exit with their spam, thus advertising\n");
  fprintf(out, "\t * this way. (idea due to ThaDragon, I just couldn't find =his= code).\n\t*/\n");
  fprintf(out, "\tanti_spam_exit_message_time = 5 minutes;\n\n");

  fprintf(out, "\t/* Define the time delta permissible for a remote server connection.\n");
  fprintf(out, "\t * If our timestamp and the remote server's timestamp are over\n");
  fprintf(out, "\t * ts_max_delta different, the connection will be dropped.  If it\n");
  fprintf(out, "\t * is less than ts_max_delta but more than ts_warn_delta, send\n");
  fprintf(out, "\t * a notice to opers on the server, but still allow the connection.\n");
  fprintf(out, "\tts_warn_delta = 30 seconds;\n\tts_max_delta = 5 minutes;\n\n");

  fprintf(out, "\t/* When a user QUITs, prepend their QUIT message with \"Client exit:\"\n");
  fprintf(out, "\t * in order to help prevent against faking server error messages (eg.\n");
  fprintf(out, "\t * ping timeout, connection reset by peer).\n\t*/\n");
  fprintf(out, "\tclient_exit = yes;\n\n");

  fprintf(out, "\t/* Show the reason why the user was K-lined or D-lined to the \"victim\"\n");
  fprintf(out, "\t * It's a neat feature except for one thing... If you use a tcm\n");
  fprintf(out, "\t * and it shows the nick of the oper doing the kline (as it does by\n");
  fprintf(out, "\t * default) Your opers can be hit with retaliation... Or if your\n");
  fprintf(out, "\t * opers use scripts that stick an ID into the comment field. etc.\n\t*/\n");
  fprintf(out, "\tkline_with_reason = yes;\n\n");

  fprintf(out, "\t/* Force the sign-off reason to be \"Connection closed\" when a user is\n");
  fprintf(out, "\t * K-lined.  The user will still see the real reason.  This prevents\n");
  fprintf(out, "\t * other users seeing the client disconnect from harassing the IRCops.\n\t*/\n");
  fprintf(out, "\tkline_with_connection_closed = no;\n\n");

  fprintf(out, "\t/* Set to yes if you wish your server to flag and not apply redundant\n");
  fprintf(out, "\t* K-lines.\n\t*/\n\tnon_redundant_klines = yes;\n\n");

  fprintf(out, "\t/* Enable this if you want opers to get noticed about \"things\" trying\n");
  fprintf(out, "\t * to connect as servers that don't have N: lines.  Twits with\n");
  fprintf(out, "\t * misconfigured servers can get really annoying with this enabled.\n\t*/\n");
  fprintf(out, "\twarn_no_nline = yes;\n\n");

  fprintf(out, "\t/* Set STATS o to be oper only */\n\to_lines_oper_only=yes;\n\n");

  fprintf(out, "\t/* Links Delay determines the time between updates of the user LINKS\n");
  fprintf(out, "\t * file.  The default updates it every 5 minutes.\n\t*/\n\tlinks_delay = 5 minutes;\n\n");
  
  fprintf(out, "\t/* minimum time between uses of MOTD, INFO, HELP, LINKS, TRACE */\n\tpace_wait = 10 seconds;\n\n"); 

  fprintf(out, "\t/* The minimum time  between uses of remote WHOIS before\n");
  fprintf(out, "\t * the counter is reset.\n\t*/\n\twhois_wait = 1 second;\n\n");

  fprintf(out, "\t/* Define the amount of time between KNOCKs.  Once every 5 minutes\n");
  fprintf(out, "\t * should be enough.\n\t*/\n\tknock_delay = 5 minutes;\n\n");

  fprintf(out, "\t/* Amount of time between notifying users in +g mode that someone\n");
  fprintf(out, "\t * is messaging them.\n\t*/\n\tcaller_id_wait = 1 minute;\n\n");

  fprintf(out, "\t/* There are clients ignoring the FORCE_MOTD numeric.  There is no\n");
  fprintf(out, "\t * no point forcing MOTD on connecting clients IMO.  Give them a short\n");
  fprintf(out, "\t * NOTICE telling them they should read the MOTD, and leave it at that.\n");
  fprintf(out, "\t*/\n\tshort_motd = no;\n\n");

  fprintf(out, "\t/* Set to yes to disable flood control for opers. */\n\tno_oper_flood = yes;\n\n");
  fprintf(out, "\t/* stop banned people from being able to talk in channels. */\n");
  fprintf(out, "\tquiet_on_ban = no;\n\n");

  fprintf(out, "\t/* Enable G-lines */\n\tglines = yes;\n\n");

  fprintf(out, "\t/* Set the time for how long G-lines will last.\n\tgline_time = 1 day;\n\n");

  fprintf(out, "\t/* Define the maximum amount of time a user can idle before\n");
  fprintf(out, "\t* disconnecting them.  Set to 0 to disable.\n\t*/\n\tidletime = 0;\n\n");

  fprintf(out, "\t/* Enable the server hiding feature.  This prevents users from\n");
  fprintf(out, "\t * finding out what server users are on, and hides IP's.  Note\n");
  fprintf(out, "\t * that some clients do not handle this well and may break.  Blame\n");
  fprintf(out, "\t * the packet kiddies for making this option almost a necessity.\n\t*/\n");
  fprintf(out, "\thide_server = yes;\n\n");

  fprintf(out, "\t/* How many servers to autoconnect to if theres no class\n");
  fprintf(out, "\tmaximum_links = 1;\n\n");

  fprintf(out, "\t/* Define these to the log files you want to use for user connections\n");
  fprintf(out, "\t * (userlog), successful use of /oper (operlog), and failed use of\n");
  fprintf(out, "\t * /oper (foperlog).  Logging will stop if either these files do not\n");
  fprintf(out, "\t * exist, or if they are not defined.\n\t*/\n\tfname_userlog = \"logs/userlog\";\n");
  fprintf(out, "\tfname_operlog = \"logs/operlog\";\n\tfname_foperlog = \"logs/foperlog\";\n\n");

  fprintf(out, "\t/* max_targets\n\t * only max_target targets can be PRIVMSG'ed / NOTICE'd in a single\n");
  fprintf(out, "\t * command. default is 4 if not defined here. setting to 0 will have\n");
  fprintf(out, "\t * broken results (ie PRIVMSG/NOTICE won't work).\n\t*/\n\tmax_targets = 4;\n\n");

  fprintf(out, "\t/* message_locale\n\t * default message locale to use if gettext() is enabled\n");
  fprintf(out, "\t * Use \"custom\" for the (in)famous Hybrid custom messages.\n");
  fprintf(out, "\t * Use \"standard\" for the compiled in defaults.\n\t*/\n\tmessage_locale = \"custom\";\n\n");

  fprintf(out, "\t/* List of user modes that only opers can set, see example.conf for a list. */\n");
  fprintf(out, "\toper_umodes = locops, servnotice, operwall, wallop\n\n");

  fprintf(out, "\t/* List of usermodes that get set when a user /oper's */\n");
  fprintf(out, "\toper_umodes = locops, servnotice, operwall, wallop\n\n");

  fprintf(out, "\t/* vchans_oper_only\n\t * dont allow non-opers to use CJOIN?\n\t*/\n");
  fprintf(out, "\tvchans_oper_only = yes;\n\n");

  fprintf(out, "\t/* disable_vchans\n");
  fprintf(out, "\t * disable the creation of vchans by remote servers.. we do not\n");
  fprintf(out, "\t * currently recommend this feature, as it removes a lot of\n");
  fprintf(out, "\t * functionality, however it's here for those who don't want\n");
  fprintf(out, "\t * vchan support at all.\n\t*/\n\tdisable_vchans = no;\n");

  fprintf(out, "};\n\n");
}

static void AddModulesBlock(FILE* out)
{
  fprintf(out, "modules {\n");
  fprintf(out, "\t/* set paths for module.  these paths are searched both for\n");
  fprintf(out, "\t * module=\"\" and /quote modload, when a relative pathname\n");
  fprintf(out, "\t * is specified.\n\t*/\n");

  fprintf(out, "\tpath = \"/usr/local/ircd/modules/local\";\n");
  fprintf(out, "\tpath = \"/usr/local/ircd/modules/autoload\";\n\n");

  fprintf(out, "\t/* load a module upon startup (or rehash) */\n");
  fprintf(out, "\tmodule = \"some_module.so\";\n};\n");
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
	return;
      
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
  if (!passwd_field)
    passwd_field = "";
  if (!user_field)
    user_field = "";
  if (!port_field)    
    port_field = "";
  if (!class_field)
    class_field = "";
  switch( conf_letter )
    {
    case 'A':case 'a': /* Name, e-mail address of administrator */
      fprintf(out,"administrator {\n");
      if(host_field)
	fprintf(out,"\tname=\"%s\";\n", passwd_field);
      if(user_field)
	fprintf(out,"\tdescription=\"%s\";\n", user_field);
      if(passwd_field)
	fprintf(out,"\temail=\"%s\";\n", host_field);
      fprintf(out,"};\n\n");
      break;

    case 'c':
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
      fprintf(out,"exempt {\n");
      if(user_field)
	fprintf(out,"\tip=\"%s\";\n", user_field);
      fprintf(out,"};\n\n");
      break;

    case 'D': /* Deny lines (immediate refusal) */
      fprintf(out,"deny {\n");
      if(host_field)
	fprintf(out,"\tip=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\treason=\"%s\";\n", passwd_field);
      fprintf(out,"};\n\n");
      break;

    case 'H': /* Hub server line */
    case 'h':
      AddHubOrLeaf(IS_HUB,user_field,host_field);
      break;

    /* We no longer have restricted connection in Hybrid 7 */
    case 'i': 
    case 'I': 
      fprintf(out,"auth {\n");

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
		    fprintf(out,"\tip=%s;\n", client_allow );
		}
	      else
		spoof_field = host_field;
	    }
	}

      if(passwd_field && *passwd_field)
	fprintf(out,"\tpasswd=\"%s\";\n", passwd_field);	
#if 0
      /* This part isn't needed at all */
      else
	fprintf(out,"\tpasswd=\"*\";\n");	
#endif

      if(!client_allow && user_field)
	{
	  client_allow = ClientFlags(out,spoof_field,user_field);
	  if(client_allow)
	    {
	      fprintf(out,"\tuser=\"%s\";\n", client_allow );
	    }
	}

      if(class_field)
	fprintf(out,"\tclass=\"%s\";\n", class_field);	
      fprintf(out,"};\n\n");
      break;
      
    case 'K': /* Kill user line on irc.conf           */
    case 'k':
      fprintf(out,"kill {\n");
      if(host_field)
	fprintf(out,"\tuser=\"%s@%s\";\n", user_field,host_field);
      if(passwd_field)
	fprintf(out,"\treason=\"%s\";\n", passwd_field);
      fprintf(out,"};\n\n");
      break;

    case 'L': /* guaranteed leaf server */
    case 'l':
      AddHubOrLeaf(IS_LEAF,user_field,host_field);
      break;

/* Me. Host field is name used for this host */
      /* and port number is the number of the port */
    case 'M':
    case 'm':
      fprintf(out,"serverinfo {\n");
      if(host_field)
	fprintf(out,"\tname=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\tvhost=%s;\n", passwd_field);
      if(user_field)
	fprintf(out,"\tdescription=\"%s\";\n", user_field);
      if(port_field)
	fprintf(out,"\thub=yes;\n");
      else
     	fprintf(out,"\thub=no;\n");
      /* Also print a default servername/netname */
      fprintf(out, "\tnetwork_name=\"EFNet\";\n");
      fprintf(out, "\tnetwork_desc=\"Eris Free Network\";\n");
      fprintf(out, "\tmax_clients=1024;\n");
      fprintf(out,"};\n\n");
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
      /* Local operators no longer exist        */
      /* For now, I don't force locals to have  */
      /* certain default flags.  I probably     */
      /* should if there is no port field       */
    case 'o':
    case 'O':
      /* defaults */
      fprintf(out,"operator {\n");
      if(user_field)
	fprintf(out,"\tname=\"%s\";\n", user_field);
      if(host_field)
	{
	  fprintf(out,"\tuser=\"%s\";\n", host_field);
	}
      if(passwd_field)
	fprintf(out,"\tpassword=\"%s\";\n", passwd_field);
      if(port_field)
	OperPrivsFromString(out,port_field);
      if(class_field)
	fprintf(out,"\tclass=\"%s\";\n", class_field);	
      fprintf(out,"};\n\n");
      break;

    case 'P': /* listen port line */
    case 'p':
      fprintf(out,"listen {\n");
      /* What is the purpose of this field? */
      if(host_field && *host_field)
	fprintf(out,"\tname=\"%s\";\n", host_field);
      if(port_field)
	fprintf(out,"\tport=%d;\n", atoi(port_field));
      fprintf(out,"};\n\n");
      break;

    case 'Q': /* reserved nicks */
    case 'q': 
      if(host_field && (*host_field != '#'))
        {
          fprintf(out,"quarantine {\n");
          fprintf(out,"\tname=\"%s\";\n", host_field);
        }
      else
        {
          puts("Cannot convert a channel quarantine, skipping");
          break;
        } 
     if(passwd_field)
	fprintf(out,"\treason=\"%s\";\n", passwd_field);
      fprintf(out,"};\n\n");
      break;

    case 'U': 
    case 'u': 
      fprintf(out,"shared {\n");
      if(host_field)
	fprintf(out,"\tname=\"%s\";\n", host_field);
#if 0
      if(passwd_field)
	fprintf(out,"\treason=\"%s\";\n", passwd_field);
#endif
      fprintf(out,"};\n\n");
      break;

    case 'X': /* rejected gecos */
    case 'x': 
      fprintf(out,"gecos {\n");
      if(host_field)
	fprintf(out,"\tname=\"%s\";\n", host_field);
      if(passwd_field)
	fprintf(out,"\treason=\"%s\";\n", passwd_field);
      if(port_field)
        {
          if (*user_field == '0')
            fprintf(out,"\taction=reject;\n");
          else if (*user_field == '1')
            fprintf(out,"\taction=warn;\n");
          else
            fprintf(out, "\taction=silent;\n");
        }
      fprintf(out,"};\n\n");
      break;

    case 'Y':
    case 'y':
      fprintf(out,"class {\n");
      if(host_field)
	fprintf(out,"\tname=\"%s\";\n", host_field);
      if(passwd_field)
	{
	  int ping_time;
	  ping_time = atoi(passwd_field);
	  fprintf(out,"\tping_time=%d;\n", ping_time );
	}
      if(user_field)
	{
          /* Note that connectfreq and number_per_ip set the same variable
          ** when read by the ircd.  They *ARE* interchangable, just both
          ** exist for the user's benefit
          */
	  int number_per_ip;
	  number_per_ip = atoi(user_field);
	  fprintf(out,"\tnumber_per_ip=%d;\n", number_per_ip );
	}
      if(port_field)
	{
	  int max_number;
	  max_number = atoi(port_field);
	  fprintf(out,"\tmax_number=%d;\n", max_number );
	}
      if(class_field)
	sendq = atoi(class_field);
      fprintf(out,"\tsendq=%d;\n", sendq);
      fprintf(out,"};\n\n");
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
	  fprintf(out,"connect {\n");
	  fprintf(out,"\thost=\"%s\";\n", p->host);
	  fprintf(out,"\tname=\"%s\";\n", p->name);
	  fprintf(out,"\tsend_password=\"%s\";\n", p->c_passwd);
	  fprintf(out,"\taccept_password=\"%s\";\n", p->n_passwd);
	  fprintf(out,"\tport=%d;\n", p->port );

#if 0
          /* ZIP links are gone */
	  if(p->compressed)
	    fprintf(out,"\tcompressed=yes;\n");
#endif
#if 0
	  if(p->lazylink)
	    fprintf(out,"\tlazylink=yes;\n");
#endif
	  if(p->hub_mask)
	    {
	      fprintf(out,"\thub_mask=\"%s\";\n",p->hub_mask);
	    }
	  else
	    {
	      if(p->leaf_mask)
		fprintf(out,"\tleaf_mask=\"%s\";\n",p->leaf_mask);
	    }

	  if(p->class)
	    fprintf(out,"\tclass=\"%s\";\n", p->class );

	  fprintf(out,"};\n\n");
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
      /* If the priv is lower case, it defaults to whatever=no; anyway, so we dont need this? --fl */
      if(*privs == 'O')                     /* allow global kill */
	{
	  fprintf(out,"\tglobal_kill=yes;\n");
	}
      else if(*privs == 'o')                /* disallow global kill */
	{
	  fprintf(out,"\tglobal_kill=no;\n");
	}
      else if(*privs == 'U')                /* allow unkline */
	{
	  fprintf(out,"\tunkline=yes;\n");
	}
      else if(*privs == 'u')                /* disallow unkline */
	{
	  fprintf(out,"\tunkline=no;\n");
	}
      else if(*privs == 'R')               /* allow remote squit/connect etc.*/
	{
	  fprintf(out,"\tremote=yes;\n");
	}
      else if(*privs == 'r')                /* disallow remote squit/connect etc.*/
	{
	  fprintf(out,"\tremote=no;\n");
	}
      else if(*privs == 'N')                /* allow +n see nick changes */
	{
	  fprintf(out,"\tnick_changes=yes;\n");
	}
      else if(*privs == 'K')                /* allow kill and kline privs */
	{
	  fprintf(out,"\tkline=yes;\n");
	}
      else if(*privs == 'k')                /* disallow kill and kline privs */
	{
	  fprintf(out,"\tkline=no;\n");
	}
      else if(*privs == 'G')                /* allow gline */
	{
	  fprintf(out,"\tgline=yes;\n");
	}
      else if(*privs == 'g')                /* disallow gline */
	{
	  fprintf(out,"\tgline=no;\n");
	}
      else if(*privs == 'H')                /* allow rehash */
	{
	  fprintf(out,"\trehash=yes;\n");
	}
      else if(*privs == 'h')                /* disallow rehash */
	{
	  fprintf(out,"\trehash=no;\n");
	}
      else if(*privs == 'D')
	{
	  fprintf(out,"\tdie=yes;\n");
	}
      else if(*privs == 'd')
	{
	  fprintf(out,"\tdie=no;\n");
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
            {
	      fprintf(out,"\tspoof=\"%s\";\n",spoof);	  
              fprintf(out,"\tspoof_notice=yes;\n");
            }
              break;
	case '!':
#if 0
          /* This is gone */
	  fprintf(out,"\tlimit_ip;\n");
#endif
	  break;
        case '-':
	  fprintf(out,"\tno_tilde=yes;\n");	  
          break;
        case '+':
	  fprintf(out,"\tneed_ident=yes;\n");	  
          break;
        case '$':
#if 0
          /* This is also gone */
	  fprintf(out,"\thave_ident;\n");	  
#endif
          break;
        case '%':
#if 0
          /* As is this... */
	  fprintf(out,"\tnomatch_ip;\n");	  
#endif
          break;
        case '^':        /* is exempt from k/g lines */
	  fprintf(out,"\tkline_exempt=yes;\n");	  
          break;
        case '&':        /* can run a bot */
          break;
        case '>':        /* can exceed max connects */
	  fprintf(out,"\texceed_limit=yes;\n");	  
          break;
        case '<':        /* can idle */
#if 0
          /* So's this... */
	  fprintf(out,"\tcan_idle=yes;\n");	  
#endif
          break;
         case '_':     /* gline exempt */
           fprintf(out, "\tgline_exempt=yes;\n");
           break;
        default:
          return tmp;
        }
    }
  return tmp;
}


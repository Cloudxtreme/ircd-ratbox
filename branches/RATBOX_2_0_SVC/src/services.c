/*
 *  OpenServices 1.0
 *  Base Structure and parsing tools.
 *
 *  Copyright (C) 2005 Alan "alz" Milford
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 
#include "stdinc.h"
#include "parse.h"
#include "client.h"
#include "channel.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "s_log.h"
#include "s_stats.h"
#include "send.h"
#include "msg.h"
#include "s_conf.h"
#include "memory.h"
#include "s_serv.h"
#include "hook.h"
#include "tools.h"
#include "services.h"


dlink_list services;

static struct SVCMessage *
service_cmd_parse(struct Client *client_p, const char *cmd);

static void
handle_command(struct SVCMessage *mptr, struct Client *client_p,
           struct Client *target_p, int i, const char* hpara[MAXPARA]);
           
static dlink_node *find_cmd(struct Service *service_p, struct SVCMessage *msg);

/* create_service()
 *
 * inputs   - nick
 *          - username
 *          - hostname
 *          - gecos
 *          - opered (1 = yes, 0 = no)
 * outputs  - returns pointer to created service
 *
 */
struct Service *create_service(const char *nick, const char *username, const char *host, const char *gecos, int opered)
{
    struct Service *service_p;
    struct Client *client_p = create_fake_client(nick, username, host, gecos, opered);
    
    service_p = MyMalloc(sizeof(struct Service));
    service_p->client_p = client_p;
    
    dlinkAdd(service_p, &service_p->node, &services);
    
    return service_p;
}

/* destroy_service()
 * 
 * inputs       - pointer to service struct
 * ouputs       - none
 * side_effects - kills and destroys services client
 */
void destroy_service(struct Service *service_p)
{
    dlink_node *ptr, *next_ptr;
    struct Client *client_p = service_p->client_p;
    struct Service *servp = NULL;
    
    if (service_p == NULL)
        return;
        
    DLINK_FOREACH_SAFE(ptr, next_ptr, services.head)
    {
        servp = (struct Service *)ptr->data;
        
        if (servp == service_p)
            dlinkDelete(ptr, &services);
    }
    
    if (client_p == NULL)
        return;
    destroy_fake_client(client_p);
}

/* handle_services_message()
 *
 * inputs       - data from fakeclient msg hook
 * outputs      - 0
 * side_effects - begin parsing routine for msg
 */
int handle_services_message(struct Client *source_p, struct Client *target_p, const char *text)
{
    int length;
    length = strlen(text);

    parse_services_message(source_p, target_p, text, length);
    
    return 0; 
}

/* string_to_array()
 * 
 * inputs       - string
 *              - char array
 * outputs      - string split up into parc and parv
 */               
static inline int
string_to_array(char *string, char *parv[MAXPARA])
{
    char *p, *buf = string;
    int x = 1;

    parv[x] = NULL;
    while (*buf == ' ') /* skip leading spaces */
        buf++;
    if(*buf == '\0')    /* ignore all-space args */
        return x;

    do
    {
        if(*buf == ':') /* Last parameter */
        {
            buf++;
            parv[x++] = buf;
            parv[x] = NULL;
            return x;
        }
        else
        {
            parv[x++] = buf;
            parv[x] = NULL;
            if((p = strchr(buf, ' ')) != NULL)
            {
                *p++ = '\0';
                buf = p;
            }
            else
                return x;
        }
        while (*buf == ' ')
            buf++;
        if(*buf == '\0')
            return x;
    }
    while (x < MAXPARA - 1);

    if(*p == ':')
        p++;

    parv[x++] = p;
    parv[x] = NULL;
    return x;
}

/* svc_message()
 *
 * inputs       - service pointer
 *              - client pointer
 *              - message type
 *              - message (&args)
 * outputs      - none
 * side_effects - sends either PRIVMSG or NOTICE to given
 *                client, depending on given type.
 */
 
void svc_message(struct Service *service, struct Client *target_p, int type, const char *pattern, ...)
{
    va_list args;
    char tmpBuf1[1024];

    va_start(args, pattern);
    vsprintf(tmpBuf1, pattern, args);
    va_end(args);
    if (type == SVC_NOTICE)
    {
        if (target_p->localClient) 
        {
            if (service == NULL)
                sendto_one(target_p, ":%s NOTICE %s :%s", me.name,
                    target_p->name, tmpBuf1);
            else
                sendto_one(target_p, ":%s!%s@%s NOTICE %s :%s", service->client_p->name,
                    service->client_p->username, service->client_p->host,
                    target_p->name, tmpBuf1);
        } 
        else 
        {
            if (service == NULL)
                sendto_one(target_p, ":%s NOTICE %s :%s", me.name,
                    target_p->name, tmpBuf1);
            else
                sendto_anywhere(target_p, service->client_p, "NOTICE", ":%s", tmpBuf1);
        }
    } 
    else if (type == SVC_PRIVMSG)
    {
            if (service == NULL)
                sendto_one(target_p, ":%s PRIVMSG %s :%s", me.name,
                    target_p->name, tmpBuf1);
            else
                sendto_anywhere(target_p, service->client_p, "PRIVMSG", ":%s", tmpBuf1);
    }
}
              
/* find_service()
 *
 * inputs       - client pointer
 * outputs      - service pointer
 *
 */ 
struct Service *find_service(struct Client *client_p)
{
    struct Service *service_p;
    dlink_node *ptr;
   
    //not a service?
    if (!IsFake(client_p))
        return NULL;
         
    DLINK_FOREACH(ptr, services.head)
    {
        service_p = (struct Service *)ptr->data;
        
        if (service_p->client_p == client_p)
            return service_p;
    }
    
    return NULL;
}

/* process_unknown_command()
 *
 * inputs       - source client pointer
 *              - dest client pointer
 * outputs      - none
 *
 * does nothing unless unknown command handler
 * has been overridden by the service, if so it 
 * runs the specified function
 */
void process_unknown_command(struct Client *source_p, struct Client *target_p)
{
    struct Service *service_p = find_service(target_p);
    
    if (service_p == NULL)
        return;
    
    if (service_p->unknown != 0)
        (*service_p->unknown)(source_p, service_p->client_p, 0, NULL);
}

/* try_command()
 *
 * inputs       - source client pointer
 *              - service pointer
 *              - name of command
 * outputs      - none
 * side_effects - tries to run service_p's command, does nothing on fail
 */
void try_command(struct Client *source_p, struct Service *service_p, const char *cmd)
{
    struct SVCMessage *mptr;
    dlink_node *ptr;
    
    DLINK_FOREACH(ptr, service_p->commands.head)
    {
        mptr = (struct SVCMessage *)ptr->data;
        
        if (strcasecmp(mptr->cmd, cmd) == 0)
        {
            (*mptr->handler)(source_p, service_p->client_p, 0, (const char **)cmd);
        }
    }
}
 

/* parse_message()
 *
 * given a service message, parses it and generates parv, parc and sender
 */
void
parse_services_message(struct Client *client_p, struct Client *target_p, const char *text, int length)
{
    char *ch;
    char *s;
    int i = 1;
    struct SVCMessage *mptr = NULL;
    char *pbuffer = LOCAL_COPY(text); 
    char *para[MAXPARA + 1];     
       
    //whoa! not a fake client, ignore
    if (!IsFake(target_p))
        return;
        
        
    //empty para - just incase.
    memset(&para, 0, MAXPARA+1);
    
    for (ch = pbuffer; *ch == ' '; ch++)    /* skip spaces */
        /* null statement */ ;

        if((s = strchr(ch, ' ')))
            *s++ = '\0';
            
        mptr = service_cmd_parse(target_p, ch);

        /* no command or its encap only, error */
        if(mptr == NULL)
        {
            process_unknown_command(client_p, target_p);
            return;
        }
               
    // this shouldn't happen
    if(mptr == NULL)
        return;
        
    if(s != NULL)
        i = string_to_array(s, para);
    para[0] = ch;

    handle_command(mptr, client_p, target_p, i, /* XXX discards const!!! */ (const char **)para);

}

/* svc_set_unknown()
 * 
 * inputs       - service pointer
 *              - message handler
 * outputs      - none
 * side_effects - set's given services unknown command to given
 *                message handler
 */
void svc_set_unknown(struct Service *service_p, MessageHandler unknown)
{
    if (service_p == NULL)
        return ;
        
    service_p->unknown = unknown;
}

/* handle_command()
 *
 * inputs       - message pointer
 *              - source client pointer
 *              - target client pointer
 *              - number of args (parc)
 *              - args (parv)
 * outputs      - none
 * side_effects - runs service command with given args
 */
static void
handle_command(struct SVCMessage *mptr, struct Client *source_p,
           struct Client *target_p, int parc, const char* parv[MAXPARA])
{
    if (!IsFake(target_p))
        return;

    if(IsServer(source_p))
        return;

    // no command
    if (mptr == NULL)
        return;

    /* check right amount of params is passed... --is */
    if(parc < mptr->min_para || 
       (mptr->min_para && EmptyString(parv[mptr->min_para - 1])))
    {
        return;
    }

    (*mptr->handler)(source_p, target_p, parc, parv);
    return;
}

/* svc_get_cmd()
 * 
 * inputs       - service pointer
 *              - command name
 * outputs      - service message pointer
 */
struct SVCMessage *svc_get_cmd(struct Service *service_p, char *cmd)
{
    struct SVCMessage *mptr;
    dlink_node *ptr;
    
    DLINK_FOREACH(ptr, service_p->commands.head)
    {
        mptr = (struct SVCMessage *)ptr->data;
        
        if (strcasecmp(mptr->cmd, cmd) == 0)
            return mptr;
    }
    
    return NULL;
}

/* is_svc_command
 *
 * inputs - service pointer
 *        - command pointer
 *
 * output - 1 if exists
 */
int
is_svc_command(struct Service *service_p, struct SVCMessage *msg)
{
    dlink_node *ptr;
    struct SVCMessage *mptr;
    
    DLINK_FOREACH(ptr, service_p->commands.head)
    {
        mptr = (struct SVCMessage *)ptr->data;
        if (strcasecmp(mptr->cmd, msg->cmd) == 0)
            return 1;
    }
    return 0;
}

/* svc_add_cmd
 *
 * inputs   - service to add command to
 *          - struct SVCMessage pointer
 * output   - none
 * side effects - adds this command to given service
 */
void
svc_add_cmd(struct Service *service_p, struct SVCMessage *msg)
{

    //already a command, ignore
    if (is_svc_command(service_p, msg) == 1)
        return;
        
    service_p->unknown = 0;
    
    dlinkAddAlloc(msg, &service_p->commands);
}

/* find_cmd()
 * 
 * inputs       - service pointer
 *              - service message
 * outputs      - dlink node
 */
static dlink_node *find_cmd(struct Service *service_p, struct SVCMessage *msg)
{
    dlink_node *ptr;
    struct SVCMessage *mptr;
    
    DLINK_FOREACH(ptr, service_p->commands.head)
    {
        mptr = (struct SVCMessage *)ptr->data;
        
        if (mptr == msg)
            return ptr;
    }
    
    return NULL;
}
        
/* svc_del_cmd
 *
 * inputs   - command name
 * output   - none
 * side effects - unload this one command name
 */
void
svc_del_cmd(struct Service *service_p, struct SVCMessage *msg)
{

    dlink_node *ptr;
    
    if (!is_svc_command(service_p, msg))
        return;
    
    ptr = find_cmd(service_p, msg);
        
    if (ptr != NULL)
      dlinkDelete(ptr, &service_p->commands);
}

/* service_cmd_parse
 *
 * inputs   - service client pointer
 *          - command name
 * output   - pointer to struct Message
 * side effects - 
 */
static struct SVCMessage *
service_cmd_parse(struct Client *client_p, const char *cmd)
{
    dlink_node *ptr;
    struct Service *service_p = NULL;
    struct SVCMessage *mptr;
    
    DLINK_FOREACH(ptr, services.head)
    {
        service_p = (struct Service *)ptr->data;
        
        if (service_p->client_p == client_p)
            break;
    }
    
    //failed to find service
    if (service_p == NULL)
        return NULL;
        
    DLINK_FOREACH(ptr, service_p->commands.head)
    {
        mptr = (struct SVCMessage *)ptr->data;
        
        if (strcasecmp(mptr->cmd, cmd) == 0)
            return mptr;
    }

    //no message
    return NULL;
}

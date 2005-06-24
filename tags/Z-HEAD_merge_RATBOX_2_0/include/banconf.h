/*
 * Copyright (C) 2004 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2004 ircd-ratbox development team
 *
 * $Id$
 */
#ifndef INCLUDED_banconf_h
#define INCLUDED_banconf_h

void read_kline_conf(const char *filename, int warn);
void read_dline_conf(const char *filename, int warn);
void read_xline_conf(const char *filename, int warn);
void read_resv_conf(const char *filename, int warn);

#endif

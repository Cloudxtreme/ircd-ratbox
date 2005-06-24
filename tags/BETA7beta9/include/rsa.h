/************************************************************************
 *   IRC - Internet Relay Chat, include/rsa.h
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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
 *
 *
 * $Id$
 */

extern void report_crypto_errors(void);
extern int verify_private_key(void);
extern int generate_challenge( char ** r_challenge, char ** r_response, RSA * key );
extern int crypt_data(char **out, char *in, int len, char *key);
extern int decrypt_data(char **out, char *in, int len, char *key);
extern int get_randomness( unsigned char * buf, int length );
  

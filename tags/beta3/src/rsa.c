/************************************************************************
 *   IRC - Internet Relay Chat, src/rsa.c
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

#include <assert.h>
#include <string.h>
#include "memory.h"
#include "rsa.h"
#include "client.h" /* CIPHERKEYLEN .. eww */
#ifdef OPENSSL

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/md5.h>
#include <openssl/bn.h>
#include <openssl/evp.h>

static void binary_to_hex( unsigned char * bin, char * hex, int length );
static void get_randomness( unsigned char * buf, int length );
static int absorb( char ** str, char lowest, char highest );
static RSA * str_to_RSApublic( char * key );

static void binary_to_hex( unsigned char * bin, char * hex, int length )
{
  char * trans = "0123456789ABCDEF";
  int i;

  for( i = 0; i < length; i++ )
  {
    hex[i<<1]     = trans[bin[i] >> 4];
    hex[(i<<1)+1] = trans[bin[i] & 0xf];
  }
  hex[i<<1] = '\0';
}

static void get_randomness( unsigned char * buf, int length )
{
  if ( RAND_status() )
    RAND_bytes( buf, length );
  else /* XXX - abort? */
    RAND_pseudo_bytes( buf, length );
}

static int absorb( char ** str, char lowest, char highest )
{
  char * start = *str;

  while( **str >= lowest && **str <= highest )
    (*str)++;

  return *str - start;
}

static RSA * str_to_RSApublic( char * key )
{
  char * e, * n;
  RSA * rsa;

  /* bits */
  if( !absorb( &key, '0', '9' ) )
    return NULL;

  /* space */
  if( !absorb( &key, ' ', ' ' ) )
    return NULL;

  /* e */
  e = key;
  if( !absorb( &key, '0', '9' ) )
    return NULL;
  if( *key != ' ' )
    return NULL;
  *key = '\0';

  /* n */
  n = ++key;
  if( !absorb( &key, '0', '9' ) )
    return NULL;
  if( *key != ' ' && *key != '\0' )
    return NULL;
  *key = '\0';

  /* convert to RSA key structure */
  rsa = RSA_new();
  BN_dec2bn( &(rsa->e), e );
  BN_dec2bn( &(rsa->n), n );

  return rsa;
}

int generate_challenge( char **, char **, char*);
int generate_challenge( char ** r_challenge, char ** r_response, char * key )
{
  unsigned char secret[32], *tmp;
  unsigned long length, ret;
  char *nkey;
  RSA *rsa;
  DupString(nkey, key);
  if (!(rsa = str_to_RSApublic(nkey)))
    {
     *r_challenge = NULL;
     *r_response = NULL;
     MyFree(nkey);
     return -1;
    }
  MyFree(nkey);
  get_randomness(secret, 32);
  *r_response = MyMalloc(65);
  binary_to_hex(secret, *r_response, 32);
  length = RSA_size(rsa);
  tmp = MyMalloc(length);
  ret = RSA_public_encrypt(32, secret, tmp, rsa,
                           RSA_PKCS1_PADDING);
  *r_challenge = MyMalloc( (length << 1) + 1 );
  binary_to_hex( tmp, *r_challenge, length );
  (*r_challenge)[length<<1] = 0;
  MyFree(tmp);
  return (ret<0)?-1:0;
}

/* return length of encrypted data */
int crypt_data(char **out, char *in, int len, char *key)
{
  int ret;
  int outl;

  EVP_CIPHER_CTX ctx;

  *out = MyMalloc( len + 15 );

  if (!EVP_CipherInit(&ctx, EVP_bf_cbc(), NULL, NULL, 1))
    goto fail;
  if (!EVP_CIPHER_CTX_set_key_length(&ctx, CIPHERKEYLEN))
    goto fail;
  if (!EVP_CipherInit(&ctx, NULL, key, key, -1))
    goto fail;

  outl = len + 15;

  if (!EVP_CipherUpdate(&ctx, *out, &outl, in, len))
    goto fail;
  ret = outl;

  outl = len + 15 - ret;

  if (!EVP_CipherFinal(&ctx, *out + ret, &outl))
    goto fail;
  ret += outl;

  EVP_CIPHER_CTX_cleanup(&ctx);

  return ret;

fail:
  MyFree(*out);
  return -1;
}

/* return length of encrypted data */
int decrypt_data(char **out, char *in, int len, char *key)
{
  int ret;
  int outl;

  EVP_CIPHER_CTX ctx;

  *out = MyMalloc( len + 16 );

  if (!EVP_CipherInit( &ctx, EVP_bf_cbc(), NULL, NULL, 0 ))
    goto fail;
  if (!EVP_CIPHER_CTX_set_key_length( &ctx, CIPHERKEYLEN ))
    goto fail;
  if (!EVP_CipherInit( &ctx, NULL, key, key, -1 ))
    goto fail;

  outl = len + 16;
  if (!EVP_CipherUpdate(&ctx, *out, &outl, in, len))
    goto fail;
  ret = outl;

  outl = len + 16 - ret;

  if (!EVP_CipherFinal(&ctx, *out + ret, &outl))
    goto fail;
  ret += outl;

  EVP_CIPHER_CTX_cleanup(&ctx);

  return ret;

fail:
  MyFree(*out);
  return -1;
}

#endif /* OPENSSL */

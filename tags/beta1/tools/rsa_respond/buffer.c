/*
 * $Id$
 */

#include <openssl/des.h>
#include <openssl/md5.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "buffer.h"

void bomb( char * reason )
{
  fprintf( stderr, "fatal error: %s\n", reason );
  exit( 1 );
}

Buffer * Buf_init_fd( int fd, int size )
{
  Buffer * b;

  b = Buf_new();

  if( size == 0 )
  {
    read( fd, &size, 4 );
    size = ntohl( size );
  }

  b->ofs = 0;
  b->len = size;
  b->buf = malloc( b->len );
  read( fd, b->buf, b->len );
  return b;
}

void Buf_close_fd( Buffer * b, int fd, int write_size )
{
  b->len = htonl( b->ofs );
  if( write_size )
    write( fd, &(b->len), 4 );
  write( fd, b->buf, b->ofs );
  Buf_teardown( b );
}

Buffer * Buf_new( void )
{
  Buffer * b;

  b = malloc( sizeof( Buffer ) );
  b->ofs = 0;
  b->len = -1;
  b->buf = NULL;
  return b;
}

void Buf_teardown( Buffer * b )
{
  free( b->buf );
  free( b );
}

/* --- */

unsigned char * Buf_get_bin( Buffer * b, int size )
{
  unsigned char * c;

  c = &b->buf[b->ofs];
  b->ofs += size;
  if( b->ofs > b->len )
    bomb( "buffer overrun (corrupted buffer?)" );

  return c;
}

unsigned int Buf_get_u32( Buffer * b )
{
  return ntohl( *(unsigned int *)Buf_get_bin( b, 4 ) );
}

unsigned short Buf_get_u16( Buffer * b )
{
  return ntohs( *(unsigned short *)Buf_get_bin( b, 2 ) );
}

unsigned char Buf_get_u8( Buffer * b )
{
  return *(unsigned char *)Buf_get_bin( b, 1 );
}

void Buf_get_str( Buffer * b )
{
  Buf_get_bin( b, Buf_get_u32( b ) );
}

BIGNUM * Buf_get_bn( Buffer * b )
{
  int len;
  BIGNUM * ret;

  len = Buf_get_u16( b );
  len = ( len + 7 ) / 8;

  ret = BN_new();
  BN_bin2bn( Buf_get_bin( b, len ), len, ret );
  return ret;
}

/* --- */

void Buf_put_bin( Buffer * b, unsigned char * buf, int size )
{
  if( b->len != -1 )
    bomb( "this buffer not appropriate for writing" );

  b->buf = realloc( b->buf, b->ofs + size );
  memcpy( &b->buf[b->ofs], buf, size );
  b->ofs += size;
}

void Buf_put_u32( Buffer * b, unsigned int value )
{
  unsigned int i;
  i = htonl( value );
  Buf_put_bin( b, (unsigned char *) &i, 4 );
}

void Buf_put_u16( Buffer * b, unsigned short value )
{
  unsigned short i;
  i = htons( value );
  Buf_put_bin( b, (unsigned char *) &i, 2 );
}

void Buf_put_u8( Buffer * b, unsigned char value )
{
  Buf_put_bin( b, &value, 1 );
}

void Buf_put_bn( Buffer * b, BIGNUM * value )
{
  unsigned char * tmp;

  Buf_put_u16( b, BN_num_bits( value ) );
  tmp = malloc( BN_num_bytes( value ) );
  BN_bn2bin( value, tmp );
  Buf_put_bin( b, tmp, BN_num_bytes( value ) );
  free( tmp );
}

/* --- */

void Buf_decrypt( Buffer * b, char cipher, char * passphrase )
{
  unsigned char digest[16], * buf;
  des_key_schedule key1, key2;
  des_cblock iv;
  MD5_CTX md;
  int len;

  /* no cipher? we don't need to run */
  if( !cipher )
    return;

  if( cipher != 3 ) /* 3DES */
    bomb( "keyfile encrypted with unknown cipher" );

  MD5_Init( &md );
  MD5_Update( &md, passphrase, strlen( passphrase ) );
  MD5_Final( digest, &md );
  memset( &md, 0, sizeof( md ) );

  des_set_key( (void *) digest, key1 );
  des_set_key( (void *) &digest[8], key2 );
  memset( iv, 0, sizeof( iv ) );

  len = b->len - b->ofs;
  buf = &b->buf[b->ofs];
  des_cbc_encrypt( buf, buf, len, key1, &iv, DES_DECRYPT );
  des_cbc_encrypt( buf, buf, len, key2, &iv, DES_ENCRYPT );
  des_cbc_encrypt( buf, buf, len, key1, &iv, DES_DECRYPT );
}


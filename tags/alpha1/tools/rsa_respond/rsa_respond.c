/*
 * $Id$
 */
#include "buffer.h"
#include <openssl/md5.h>
#include <openssl/rsa.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#define KEY_TAG "SSH PRIVATE KEY FILE FORMAT 1.1\n"
#define CIPHER_3DES 3

void binary_to_hex( unsigned char * bin, char * hex, int length )
{
  char * trans = "0123456789abcdef";
  int i;

  for( i = 0; i < length; i++ )
  {
    hex[i<<1]     = trans[bin[i] >> 4];
    hex[(i<<1)+1] = trans[bin[i] & 0xf];
  }
  hex[i<<1] = '\0';
}

int open_socket( void )
{
  char * name;
  int fd;
  struct sockaddr_un s;

  name = getenv( "SSH_AUTH_SOCK" );
  if( !name || ( strlen( name ) + 1 ) > sizeof( s.sun_path ) )
    return -1;

  s.sun_family = AF_UNIX;
  strcpy( s.sun_path, name );
  s.sun_len = SUN_LEN( &s ) + 1;

  fd = socket( AF_UNIX, SOCK_STREAM, 0 );
  if( connect( fd, (struct sockaddr *) &s, s.sun_len ) < 0 )
  {
    close( fd );
    return -1;
  }

  return fd;
}

char * agent_request( BIGNUM * c )
{
  BIGNUM *n = NULL, *e = NULL;
  Buffer * b;
  int howmany, bits = 0, fd;
  unsigned char session[16];
  static char hex[33];

  memset( session, 0, 16 );

  fd = open_socket();
  if( fd < 0 )
    bomb( "auth socket open failed" );

  /* Request list of identities */
  b = Buf_new();
  Buf_put_u8( b, 1 ); /* SSH_AGENTC_REQUEST_RSA_IDENTITIES */
  Buf_close_fd( b, fd, 1 );

  /* Read list */
  b = Buf_init_fd( fd, 0 );
  if( Buf_get_u8( b ) != 2 ) /* SSH_AGENT_RSA_IDENTITIES_ANSWER */
    bomb( "agent gave invalid response" );
  if( (howmany = Buf_get_u32( b )) )
  {
    bits = Buf_get_u32( b );
    e = Buf_get_bn( b );
    n = Buf_get_bn( b );
    Buf_get_str( b ); /* comment */

    while( --howmany ) /* ignore anything past the first key returned */
    {
      Buf_get_u32( b ); /* bits */
      Buf_get_bn( b ); /* e */
      Buf_get_bn( b ); /* n */
      Buf_get_str( b ); /* comment */
    }
  }
  else
    bomb( "no identities loaded in agent" );

  Buf_teardown( b );

  /* Send challenge */
  b = Buf_new();
  Buf_put_u8( b, 3 ); /* SSH_AGENTC_RSA_CHALLENGE */
  Buf_put_u32( b, bits );
  Buf_put_bn( b, e );
  Buf_put_bn( b, n );
  Buf_put_bn( b, c );
  Buf_put_bin( b, session, 16 );
  Buf_put_u32( b, 1 ); /* MD5 response type */
  Buf_close_fd( b, fd, 1 );

  /* Handle reply */
  b = Buf_init_fd( fd, 0 );
  if( Buf_get_u8( b ) != 4 ) /* SSH_AGENT_RSA_IDENTITIES_ANSWER */
    bomb( "agent gave invalid response" );
  binary_to_hex( Buf_get_bin( b, 16 ), hex, 16 );
  Buf_teardown( b );
  close( fd );

  return hex;
}

RSA * read_keyfile( char * keyfile, char * passphrase )
{
  RSA * rsa;
  Buffer * b;
  int fd, size;
  char cipher;

  if( (fd = open( keyfile, O_RDONLY )) < 0 )
    bomb( "failed to open ssh keyfile" );
  size = lseek( fd, 0, SEEK_END );
  lseek( fd, 0, SEEK_SET );
  b = Buf_init_fd( fd, size );
  close( fd );

  if( memcmp( Buf_get_bin( b, sizeof( KEY_TAG ) ), KEY_TAG, sizeof( KEY_TAG ) ) )
    bomb( "ssh keyfile has invalid tag" );

  rsa = RSA_new();
  cipher = Buf_get_u8( b );
  Buf_get_u32( b ); /* reserved data - ignore */
  Buf_get_u32( b ); /* n bits - ignore */
  rsa->n = Buf_get_bn( b );
  rsa->e = Buf_get_bn( b );
  Buf_get_str( b ); /* comment - ignore */
  Buf_decrypt( b, cipher, passphrase );
  if( Buf_get_u16( b ) != Buf_get_u16( b ) )
    bomb( "unable to decrypt ssh keyfile" );
  rsa->d = Buf_get_bn( b );
  rsa->iqmp = Buf_get_bn( b );
  rsa->q = Buf_get_bn( b );
  rsa->p = Buf_get_bn( b );

  return rsa;
}

char * respond( char method, BIGNUM * challenge, char * keyfile )
{
  unsigned char * encrypted, * decrypted;
  static char hex[33];
  char * tmp, * passphrase = "";
  int size;
  MD5_CTX md5;
  RSA * rsa;

  switch( method )
  {
    case 'a':
      return agent_request( challenge );
    case 'p':
      passphrase = getpass( "Please enter ssh keyfile passphrase: " );
      break;
    case 's':
      passphrase = malloc( 512 );
      fgets( passphrase, 512, stdin );
      if( (tmp=strstr( passphrase, "\n" )) )
        *tmp = '\0';
      break;
    case 'i':
      break;
    default:
      bomb( "unknown authentication method" );
  }

  if( !keyfile )
  {
    tmp = getenv( "HOME" );
    if( !tmp )
      bomb( "HOME environment variable unset" );
    keyfile = malloc( strlen( tmp ) + 16 );
    strcpy( keyfile, tmp );
    strcat( keyfile, "/.ssh/identity" );
  }
  else
    if( !strcmp( keyfile, "-" ) )
      agent_request( challenge );

  rsa = read_keyfile( keyfile, passphrase );
  size = BN_num_bytes( challenge );
  decrypted = malloc( size );
  encrypted = malloc( size );
  BN_bn2bin( challenge, encrypted );
  if( RSA_private_decrypt( size, encrypted, decrypted, rsa,
    RSA_PKCS1_PADDING ) != 32 )
    bomb( "unable to decrypt RSA challenge" );
  free( encrypted );

  MD5_Init( &md5 );
  MD5_Update( &md5, decrypted, 32 );
  memset( decrypted, 0, 32 );
  MD5_Update( &md5, decrypted, 16 );
  MD5_Final( decrypted, &md5 );
  binary_to_hex( decrypted, hex, 16 );
  free( decrypted );

  return hex;
}

void syntax( char * myname )
{
  fprintf( stderr, "usage: %s method challenge [keyfile]\n", myname );
  fprintf( stderr, "    method must be one of a, i, p or s\n" );
  fprintf( stderr, "       a = use ssh-agent to respond\n" );
  fprintf( stderr, "       i = use an unencrypted ssh identity file\n" );
  fprintf( stderr,
    "       p = use an encrypted ssh identity - read passphrase with getpass()\n" );
  fprintf( stderr,
    "       s = use an encrypted ssh identity - read passphrase from stdin\n" );
  fprintf( stderr, "    challenge is a hex-encoded RSA challenge\n" );
  fprintf( stderr, "    keyfile is the path to the identity file\n" );
  fprintf (stderr, "       if keyfile is omitted, ~/.ssh/identity is used\n" );
  exit( 1 );
}

int main( int argc, char ** argv )
{
  BIGNUM * challenge = NULL;

  if( argc < 3 || argc > 4 || strlen( argv[1] ) != 1 ||
    !BN_hex2bn( &challenge, argv[2] ) )
    syntax( argc?argv[0]:"rsa_respond" );

  puts( respond( argv[1][0], challenge, (argc==4)?argv[3]:NULL ) );
  return 0;
}

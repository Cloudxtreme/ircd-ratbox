/*
 * $Id$
 */

#include <openssl/bn.h>

typedef struct
{
  unsigned int ofs;
  unsigned int len;
  unsigned char * buf;
} Buffer;

void bomb( char *reason );

Buffer * Buf_init_fd( int fd, int size );
void Buf_close_fd( Buffer * b, int fd, int write_size );
Buffer * Buf_new( void );
void Buf_teardown( Buffer * b );
/* --- */
unsigned char * Buf_get_bin( Buffer * b, int size );
unsigned int Buf_get_u32( Buffer * b );
unsigned short Buf_get_u16( Buffer * b );
unsigned char Buf_get_u8( Buffer * b );
void Buf_get_str( Buffer * b );
BIGNUM * Buf_get_bn( Buffer * b );
/* --- */
void Buf_put_bin( Buffer * b, unsigned char * buf, int size );
void Buf_put_u32( Buffer * b, unsigned int value );
void Buf_put_u16( Buffer * b, unsigned short value );
void Buf_put_u8( Buffer * b, unsigned char value );
void Buf_put_bn( Buffer * b, BIGNUM * value );
/* --- */
void Buf_decrypt( Buffer * b, char cipher, char * passphrase );

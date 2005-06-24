/*
 * tools/rsa_respond/genkey.c
 * A simple RSA key generator for use with the CHALLENGE command in
 * ircd-hybrid.
 *  This code is Copyright(C)2001 by the past and present ircd-hybrid
 *  developers.
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  $Id$
 */

#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

static void
binary_to_hex( unsigned char * bin, char * hex, int length )
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

int
main(int argc, char **argv)
{
 RSA *rsa;
 MD5_CTX ctx;
 char *pass, bndt[128], bnd[256], *bnn, md5[16];
 int l;
 FILE *kfile;
 /* genkey publicfile privatefile */
 if (argc < 3)
   {
    puts("Usage: genkey publicfile privatefile");
    return -1;
   }
 rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);
 do {
   pass = getpass("Keyphrase: ");
   l = strlen(pass);
 } while (!l);
 BN_bn2bin(rsa->d, (unsigned char*)bndt);
 MD5_Init(&ctx);
 MD5_Update(&ctx, pass, l);
 MD5_Final((unsigned char*)md5, &ctx);
 for (l = 0; l < 128; l++)
   bndt[l] ^= md5[l%16];
 binary_to_hex((unsigned char*)bndt, bnd, 128);
 umask(0177);
 if (!(kfile = fopen(argv[2], "w")))
   {
    puts("Could not open the private key file.");
    exit(-1);
   }
 bnn = BN_bn2hex(rsa->n);
 fwrite(bnn, 256, 1, kfile);
 fwrite(bnd, 256, 1, kfile);
 fclose(kfile);
 if (!(kfile = fopen(argv[1], "w")))
   {
    puts("Could not open the public key file.");
    exit(-1);
   }
 fprintf(kfile, "1024 %s %s", BN_bn2dec(rsa->e), BN_bn2dec(rsa->n));
 fclose(kfile);
 return 0;
}

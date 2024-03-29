/*
 *  ircd-ratbox: A slightly useful ircd.
 *  rsa.c: Functions for use with RSA public key cryptography.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
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

#ifdef HAVE_LIBCRYPTO
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/md5.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#endif

#include "memory.h"
#include "rsa.h"
#include "s_conf.h"
#include "s_log.h"
#include "client.h"		/* CIPHERKEYLEN .. eww */

#ifdef HAVE_LIBCRYPTO

void report_crypto_errors(void);
int verify_private_key(void);
static void binary_to_hex(unsigned char *bin, char *hex, int length);



/*
 * report_crypto_errors - Dump crypto error list to log
 */
void
report_crypto_errors(void)
{
	unsigned long e = 0;
	unsigned long cnt = 0;

	ERR_load_crypto_strings();
	while ((cnt < 100) && (e = ERR_get_error()))
	{
		ilog(L_CRIT, "SSL error: %s", ERR_error_string(e, 0));
		cnt++;
	}
}

/*
 * verify_private_key - reread private key and verify against inmem key
 */
int
verify_private_key(void)
{
	BIO *file;
	RSA *key;
	RSA *mkey;

	/* If the rsa_private_key directive isn't found, error out. */
	if(ServerInfo.rsa_private_key == NULL)
	{
		ilog(L_NOTICE, "rsa_private_key in serverinfo{} is not defined.");
		return (-1);
	}

	/* If rsa_private_key_file isn't available, error out. */
	if(ServerInfo.rsa_private_key_file == NULL)
	{
		ilog(L_NOTICE, "Internal error: rsa_private_key_file isn't defined.");
		return (-1);
	}

	file = BIO_new_file(ServerInfo.rsa_private_key_file, "r");

	/*
	 * If BIO_new_file returned NULL (according to OpenSSL docs), then
	 * an error occurred.
	 */
	if(file == NULL)
	{
		ilog(L_NOTICE, "Failed to open private key file - can't validate it");
		return (-1);
	}

	/*
	 * jdc -- Let's do this a little differently.  According to the
	 *        OpenSSL documentation, you need to METHOD_free(key) before
	 *        assigning it.  Don't believe me?  Check out the following
	 *        URL:  http://www.openssl.org/docs/crypto/pem.html#BUGS
	 * P.S. -- I have no idea why the key= assignment has to be typecasted.
	 *         For some reason the system thinks PEM_read_bio_RSAPrivateKey
	 *         is returning an int, not a RSA *.
	 * androsyn -- Thats because you didn't have a prototype and including
	 *             pem.h breaks things for some reason..
	 */
	key = (RSA *) PEM_read_bio_RSAPrivateKey(file, NULL, 0, NULL);

	if(key == NULL)
	{
		ilog(L_NOTICE, "PEM_read_bio_RSAPrivateKey() failed; possibly not RSA?");
		report_crypto_errors();
		return (-1);
	}

	BIO_set_close(file, BIO_CLOSE);
	BIO_free(file);

	mkey = ServerInfo.rsa_private_key;

	/*
	 * Compare the in-memory key to the key we just loaded above.  If
	 * any of the portions don't match, then logically we have a different
	 * in-memory key vs. the one we just loaded.  This is bad, mmmkay?
	 */
	if(mkey->pad != key->pad)
		ilog(L_CRIT, "Private key corrupted: pad %i != pad %i", mkey->pad, key->pad);

	if(mkey->version != key->version)
		ilog(L_CRIT, "Private key corrupted: version %lu != version %lu",
		     (unsigned long) mkey->version, 
		     (unsigned long) key->version);

	if(BN_cmp(mkey->n, key->n))
		ilog(L_CRIT, "Private key corrupted: n differs");
	if(BN_cmp(mkey->e, key->e))
		ilog(L_CRIT, "Private key corrupted: e differs");
	if(BN_cmp(mkey->d, key->d))
		ilog(L_CRIT, "Private key corrupted: d differs");
	if(BN_cmp(mkey->p, key->p))
		ilog(L_CRIT, "Private key corrupted: p differs");
	if(BN_cmp(mkey->q, key->q))
		ilog(L_CRIT, "Private key corrupted: q differs");
	if(BN_cmp(mkey->dmp1, key->dmp1))
		ilog(L_CRIT, "Private key corrupted: dmp1 differs");
	if(BN_cmp(mkey->dmq1, key->dmq1))
		ilog(L_CRIT, "Private key corrupted: dmq1 differs");
	if(BN_cmp(mkey->iqmp, key->iqmp))
		ilog(L_CRIT, "Private key corrupted: iqmp differs");

	RSA_free(key);
	return (0);
}


static void
binary_to_hex(unsigned char *bin, char *hex, int length)
{
	const char *trans = "0123456789ABCDEF";
	int i;

	for (i = 0; i < length; i++)
	{
		hex[i << 1] = trans[bin[i] >> 4];
		hex[(i << 1) + 1] = trans[bin[i] & 0xf];
	}
	hex[i << 1] = '\0';
}

int
get_randomness(unsigned char *buf, int length)
{
	/* Seed OpenSSL PRNG with EGD enthropy pool -kre */
	if(ConfigFileEntry.use_egd && (ConfigFileEntry.egdpool_path != NULL))
	{
		if(RAND_egd(ConfigFileEntry.egdpool_path) == -1)
			return 0;
	}

	if(RAND_status())
	{
		if(RAND_bytes(buf, length) > 0)
			return 1;
	}
	else {
		if(RAND_pseudo_bytes(buf, length) >= 0)
			return 1;
	}
	return 0;
}

int
generate_challenge(char **r_challenge, char **r_response, RSA * rsa)
{
	unsigned char secret[32], *tmp;
	unsigned long length, ret;

	if(!rsa)
		return -1;

	if(get_randomness(secret, 32) == 0)
	{
		report_crypto_errors();
		return -1;
	}

	*r_response = MyMalloc(65);
	binary_to_hex(secret, *r_response, 32);

	length = RSA_size(rsa);
	tmp = MyMalloc(length);
	ret = RSA_public_encrypt(32, secret, tmp, rsa, RSA_PKCS1_PADDING);

	*r_challenge = MyMalloc((length << 1) + 1);
	binary_to_hex(tmp, *r_challenge, length);
	(*r_challenge)[length << 1] = 0;
	MyFree(tmp);

	if(ret < 0)
	{
		report_crypto_errors();
		return (-1);
	}
	return (0);
}

#endif /* HAVE_LIBCRYPTO */

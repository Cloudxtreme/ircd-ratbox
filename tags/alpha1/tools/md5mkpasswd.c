/* simple password generator by Nelson Minar (minar@reed.edu)
 * copyright 1991, all rights reserved.
 * You can use this code as long as my name stays with it.
 *
 * md5 patch by Walter Campbell <wcampbel@botbay.net>
 *
 * $Id$
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


extern char *getpass();
extern char *crypt();
/* extern long random(); */
/* extern int srandom(unsigned); */

int main(argc, argv)
int argc;
char *argv[];
{
  static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
  char salt[4];
  char fullsalt[8];
  char * plaintext;

  if (argc < 2) {
    srandom(time(0));		/* may not be the BEST salt, but its close */
    salt[0] = saltChars[random() % 64];
    salt[1] = saltChars[random() % 64];
    salt[2] = saltChars[random() % 64];
    salt[3] = 0;
  }
  else {
    salt[0] = argv[1][0];
    salt[1] = argv[1][1];
    salt[2] = argv[1][2];
    salt[3] = '\0';
    if ((strchr(saltChars, salt[0]) == NULL) || (strchr(saltChars, salt[1]) == NULL) || (strchr(saltChars, salt[2]) == NULL))
      fprintf(stderr, "illegal salt %s\n", salt), exit(1);
  }

  sprintf(fullsalt, "$1$%s$", salt);

  plaintext = getpass("plaintext: ");

  printf("%s\n", crypt(plaintext, fullsalt));
  return 0;
}


# $Id$

rsa_respond: buffer.c buffer.h rsa_respond.c
	gcc -O2 -Wall -pedantic -o rsa_respond buffer.c rsa_respond.c -lcrypto

clean:
	rm -f rsa_respond *.core *~

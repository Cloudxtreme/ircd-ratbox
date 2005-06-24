# $Id$

all: genkey respond
genkey: genkey.c
	gcc -g -O2 -Wall -pedantic -o genkey genkey.c -lcrypto -ggdb

respond: respond.c
	gcc -g -O2 -Wall -pedantic -o respond respond.c -lcrypto -ggdb

clean:
	rm -f genkey respond *.core *~

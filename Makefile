rel:
	gcc -Wall -s -O2 prod_cons.c -l pthread -o prod_cons
tgz:
	tar czvf prod-cons.tar.gz prod_cons.c Makefile

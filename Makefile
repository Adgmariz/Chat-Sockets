all:
	gcc -Wall -c common.c
	gcc -Wall user.c common.o -o user
	gcc -Wall server.c common.o -lpthread -o server

clean:
	rm common.o user server
all: server 

server: server.c
	gcc -lm server.c -o server

clean:
	rm -f server
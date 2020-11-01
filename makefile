all: server 

server: server.c
	gcc server.c -o server -lm

clean:
	rm -f server
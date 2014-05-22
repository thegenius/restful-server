all: server-1 server-2 client
server-1: server-1.c
	g++ server-1.c -o server-1
server-2: server-2.c
	g++ server-2.c -o server-2
client: client.c
	g++ client.c -o client

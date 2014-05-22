/********************************************
* this is the block version server          *
********************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


const int SERV_PORT = 8089;
const int LISTENQ   = 1000;
const int BUFFLEN   = 4096; //linux page size, this will be efficient.

int main (int argc, char *argv[]) {
		
	int sfd = socket (AF_INET, SOCK_STREAM, 0);

	//always used for check function return	
	int state;

	//bind a port	
	struct sockaddr_in addr;
	// sockaddr_in and sockaddr are same in size
	size_t addr_len = sizeof(struct sockaddr_in);
	memset (&addr, 0, addr_len);
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	addr.sin_port        = htons (SERV_PORT);
	state = bind (sfd, (struct sockaddr*)&addr, addr_len);
	if (state == -1) {
		perror ("bind error\n");
		close (sfd);
		exit (-1);
	}

	//listen
	state = listen (sfd, LISTENQ);
	if (state == -1) {
		perror ("listen error\n");
		close (sfd);
		exit (-1);
	}
	printf("listening ...");
	
	//used for accept
	struct sockaddr_in cli_addr;
	memset(&cli_addr, 0, addr_len);
	socklen_t cli_len = addr_len;
	
	//used for read, BUFFLEN = 4096 is a reasonable size
	char buff[BUFFLEN];
	
	while (true) {
		int cfd = accept (sfd, (struct sockaddr*)&cli_addr, &cli_len);
		if (cfd == -1) {
			perror("accept error\n");
		}
		state = read (cfd, buff, BUFFLEN);
		while ( state > 0) {
			printf("%s", buff);
			state = read (cfd, buff, BUFFLEN);
		}
		if (state == 0) {
			printf("recv all messages.\n");
		} else if (state == -1) {
			printf("read error\n");
		}
		send (cfd, buff, strlen(buff), 0);
		close (cfd);
	}

	close (sfd);
	return 0;
}


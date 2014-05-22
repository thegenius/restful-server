#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


int main (int argc, char *argv[]) {
	int sfd;
	sfd = socket (AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	memset (&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//if (inet_pton(AF_INET, server_addr, &addr.sin_addr) <= 0) {
	//	perror("wrong server addr\n");
	//	exit(-1);
	//}
	addr.sin_port = htons(8089);

	int state;
	state = connect (sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr));
	if (state < 0) {
		perror("connect error\n");
	}

	char send_buff[1024] = {0};
	char recv_buff[1024] = {0};
	while (gets(send_buff) != NULL) {
		puts("--------send");
		send (sfd, send_buff, strlen(send_buff), 0);
		puts("--------recv");
		recv (sfd, recv_buff, 1024, 0);
		printf("--------msg:");
		printf("%s\n", recv_buff);	
	}
	close (sfd);
	return 0;
}






#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


#include "../dependencies/http_parser.h"
#include "../dependencies/message.h"

const int SERV_PORT = 8089;
const int LISTENQ   = 1000;
const int BUFFLEN   = 4096; //linux page size, this will be efficient.
const int MAXEVENTS = 4096;



void http_parser_print(http_parser *parser) {
	printf("type: %d\n",parser->type);				
	printf("flag: %d\n",parser->flags);				
	printf("state: %d\n",parser->state);				
	printf("header_state: %d\n",parser->header_state);				
	printf("index: %d\n",parser->index);				
	printf("nread: %d\n",parser->nread);
	printf("content_length: %ld\n",parser->content_length);
	printf("http_major: %d\n",parser->http_major);				
	printf("http_minor: %d\n",parser->http_minor);				
	printf("status_code: %d\n",parser->status_code);				
	printf("method: %d\n",parser->method);				
	printf("http_errno: %d\n",parser->http_errno);				
	printf("upgrade: %d\n",parser->upgrade);				
	printf("data: %s\n",(char*)parser->data);				
}
void http_parser_url_print(http_parser_url *url) {
	printf("field_set: %hd\n", url->field_set);
	printf("port: %hd\n", url->port);
	
	//printf("%hd\n", url->field_data);
	//printf("%hd\n", url->field_set);
}

int http_parser_on_url(http_parser *parser, const char *at, size_t length) {
	printf("begin to parse url\n");
	char *buff = (char*)malloc(length);
	strncat(buff, at, length);
	printf("url is %s\n", buff);
	return 0;
	/*
	http_parser_url *url;
	url = (http_parser_url*)malloc(sizeof(http_parser_url));
	http_parser_parse_url(at, length, 1, url);
	http_parser_url_print(url);
	*/
}




static int setnonblocking (int sfd) {
	int flags;
	int state;
	/* first we checkout the flags */
  	flags = fcntl(sfd, F_GETFL, 0);
  	if (flags == -1) {
      perror ("fcntl");
      return -1;
    }

	/* add O_NONBLOCK flags  */
  	flags |= O_NONBLOCK;
  	state = fcntl(sfd, F_SETFL, flags);
  	if (state == -1){
      perror ("fcntl wrong\n");
      return -1;
    }
  	return 0;
}

int main (int argc, char *argv[]) {

	puts("--------bang up the server.");
	puts("--------we ignore SIGPIPE, so we won't crash.");
	signal(SIGPIPE, SIG_IGN);
	
	puts("--------build up a socket.");	
	int sfd = socket (AF_INET, SOCK_STREAM, 0);

	//set sfd to nonblocking
	puts("--------make socket nonblocking, so we can use epoll.");
	setnonblocking (sfd); 

	//always used for check function return	
	int state;

	//bind a port
	puts("--------bind the addr and port to the socket.");	
	struct sockaddr_in addr;
	memset (&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	addr.sin_port        = htons (SERV_PORT);
	state = bind (sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
	if (state == -1) {
		perror ("--------bind error\n");
		close (sfd);
		exit (-1);
	}

	//listen
	puts("--------we begin to listen.");
	state = listen (sfd, LISTENQ);
	if (state == -1) {
		perror ("--------listen error\n");
		close (sfd);
		exit (-1);
	}
	
	puts("--------create epoll.");
	int efd = epoll_create1 (0);
	if (efd == -1) {
		perror ("--------epoll_create1 error\n");
		close (sfd);
		exit (-1);
	}

	puts("--------add events to epoll.");
	struct epoll_event event;
	event.data.fd = sfd;
	event.events = EPOLLIN | EPOLLET;
	state = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
	if (state == -1) {
		perror ("--------epoll_ctl error\n");
		close (sfd);
		close (efd);
		exit (-1);
	}

	struct epoll_event *events;
	events = (epoll_event *) calloc (MAXEVENTS, sizeof(struct epoll_event));
	if (events == NULL) {
		perror ("--------error when calloc a memory, maybe no more memory for using\n");
		close (sfd);
		close (efd);
		exit (-1);
	}	

	//used for accept
	struct sockaddr_in cli_addr;
	memset(&cli_addr, 0, sizeof(struct sockaddr_in));
	socklen_t cli_len = sizeof(struct sockaddr_in);
	
	//used for read, BUFFLEN = 4096 is a reasonable size
	char buff[BUFFLEN]; // for recv
	char data[BUFFLEN] = "this epoll server."; // for send
	int loop_count = 0;
	while (true) {
		puts("------------------------------------------");
		printf("--------loop count : %d\n", ++loop_count);
		puts("--------waiting for somebody.");
		int nfds = epoll_wait(efd, events, MAXEVENTS, -1);
        if (nfds == -1) {
			perror("--------epoll_wait error.");
			exit(EXIT_FAILURE);
		 }
		
		puts("--------some events raised.");
		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == sfd) {
				puts("--------there is a new connection");
				int cfd = accept(sfd, (struct sockaddr *) &cli_addr, &cli_len);
				if (cfd == -1) {
					perror("--------accept error");
					exit(EXIT_FAILURE);
				}
				setnonblocking (cfd);
				struct epoll_event ev;
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = cfd;
				state = epoll_ctl (efd, EPOLL_CTL_ADD, cfd, &ev);
				if (state == -1) {
					perror("--------epoll_ctl error when add new client socket fd\n");
					exit(EXIT_FAILURE);
				}
			} else if (events[i].events & EPOLLIN) {
				puts("--------there is data on the wire");
				int read_ok  = 0;
				int read_cnt = 0;
				memset(buff, 0, sizeof(buff));
				printf("--------msg: ");
				while (true) {
					state = recv (events[i].data.fd, buff+read_cnt, BUFFLEN, 0);
					if (state == 0) { 
						// client has send FIN signal
						puts("--------FIN received.");
						read_ok = 0;   
						break;
					} else if (state == -1) {		
						if (errno == EAGAIN) { 
							// no more data
							read_ok = 1;
							break;
						} else if (errno == EINTR) { 
							// interupt
							continue;
						} else {
							puts("--------unknown err when recv...");
							read_ok = 0;
							break;
						}
					} else if (0<state && state<BUFFLEN) {
						//no more data
 						printf("%s", buff);
						read_ok = 1;
						break;
					} else { 
						// state == BUFFLEN, we should read agian
						read_cnt += BUFFLEN; 
						// for now, we assume buff no more than BUFFLEN
						puts("--------warning: overflow!!!!!!");
						printf("%s", buff);
						continue;
					}
				}

				if (read_ok) {
					puts("");
					puts("--------all data received, so we want to write");
					struct epoll_event ev;
					ev.events = EPOLLOUT | EPOLLET;
					ev.data.fd  = events[i].data.fd;
					epoll_ctl (efd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
				} else {
					puts("--------close the socket fd.");
					close(events[i].data.fd);
					events[i].data.fd = -1;
				}

			} else if (events[i].events & EPOLLOUT) {
				puts("--------now we  can write");
				//char *data = (char *)events[i].data.ptr;
				//printf("--------data is :%s\n",(char*)events[i].data.ptr);
				memset(data, 0, sizeof(data));
				memcpy(data, buff, sizeof(buff));

				//parser the url
				http_parser_settings settings;
				my_parser_settings_init(&settings);		
	
				http_parser *parser = (http_parser*)malloc(sizeof(http_parser));				
				http_parser_init(parser, HTTP_REQUEST);
				int data_len = strlen(data);				
				http_parser_execute(parser, &settings, data, data_len);
				http_parser_print(parser);
				
				http_parser_init(parser, HTTP_RESPONSE);
				http_parser_execute(parser, &settings, data, data_len);


				int write_ok  = 0;
				int write_cnt = 0;
				int send_len  = data_len<BUFFLEN?data_len:BUFFLEN;
				//printf("--------data len:%d\n", data_len);
				//printf("--------send len:%d\n", send_len);
				while (true) {
					state = send (events[i].data.fd, data+write_cnt, send_len, 0);
					if (state == -1) {
						printf("--------%s\n", strerror(errno));
						if (errno == EAGAIN) {
							// we have send it all
							write_ok = 1;
							break;
						} else if (errno == EINTR) {
							continue;
						} else {
							puts("--------unknown write error.");
							write_ok = 0;
							break;
						}
					} else if (state == 0) {
						// client has send FIN
						puts("--------FIN received.");
						write_ok = 0;
						break;
					} else if (state > 0) {
						if (write_cnt+state == data_len) {
							// we have written all
							write_ok = 1;
							break;
						} else if (write_cnt+state < data_len) {
							// we should write agian
							write_cnt += state;
							continue;
						} else {
							perror("--------write error");
							write_ok = 0;
							break;
						}
					}
				}
				
				if (write_ok) {
					puts("--------all data send.");
					struct epoll_event ev;
					ev.data.fd = events[i].data.fd;
					ev.events  = EPOLLIN | EPOLLET;
					epoll_ctl (efd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
				} else {
					puts("--------close the socket fd.");
					close(events[i].data.fd);
					events[i].data.fd = -1;
				}
			} else if (events[i].events & EPOLLERR) {
				perror("--------socket fd error");
			} else {
				perror("--------some thing is going wrong when epoll_wait\n");
			}
		}
	}
	printf("--------server shutdown");
	
	close (efd);
	close (sfd);
	return 0;
}


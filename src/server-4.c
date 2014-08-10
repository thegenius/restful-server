#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <libaio.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


#include <mysql.h>


/********************************************************************
**
********************************************************************/
const int SERV_PORT  = 8089;
const int BUFFLEN    = 4096; //linux page size, this will be efficient.
const int MAXCLIENTS = 4096;
#define SL(s) (s), sizeof(s)


enum MYSQL_SOCK_TYPE{
	MYSQL_SOCK_UNKNOWN,
	MYSQL_SOCK_QUERY,
	MYSQL_SOCK_ROW
};

struct waiter_t {
	int        mysql_fd;
	enum MYSQL_SOCK_TYPE  mysql_sock_type;
	MYSQL*     mysql_conn;
	MYSQL_RES* mysql_res;
};

struct client_t {
	int    sfd;
	char   data[BUFFLEN];
	int    data_len;
	int    waiter_index;
};

struct client_t client[MAXCLIENTS];
struct waiter_t waiter[MAXCLIENTS];

/**/
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


static int recv_sock_data(int sockfd) {
	int read_ok  = 0;
	int read_cnt = 0;
	while (true) {
		int state = recv(sockfd, client[sockfd].data+read_cnt, BUFFLEN, 0);
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
 			printf("recv data:\n%s", client[sockfd].data);
			read_cnt += state;
			read_ok = 1;
			break;
		} else { 
			// state == BUFFLEN, we should read agian
			read_cnt += BUFFLEN; 
			// for now, we assume buff no more than BUFFLEN
			puts("--------warning: overflow!!!!!!");
			printf("%s", client[sockfd].data);
			continue;
		} 	
	}

	//memset(client[sockfd].data+read_cnt, 0, BUFFLEN-read_cnt);
	client[sockfd].data_len = read_cnt;	

	if (read_ok) {
		printf("--------recv data length: %d\n", read_cnt);
		return read_cnt;
	} else {
		return -1;
	}
}

static int send_sock_data(int sockfd) {
	int write_ok  = 0;
	int write_cnt = 0;
	int send_len  = client[sockfd].data_len;
	
	while (true) {
		int state = send (sockfd, client[sockfd].data+write_cnt, send_len-write_cnt, 0);
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
			write_cnt += state;
			if (write_cnt == send_len) {
				write_ok = 1;
				break;
			} else if (write_cnt < send_len) {
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
		return write_cnt;
	} else {
		puts("--------write error.");
		return -1;
	}
}


static int
wait_for_mysql(MYSQL *mysql, int status)
{
  struct pollfd pfd;
  int timeout;
  int res;

  pfd.fd= mysql_get_socket(mysql);
  pfd.events=
    (status & MYSQL_WAIT_READ ? POLLIN : 0) |
    (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
    (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
  if (status & MYSQL_WAIT_TIMEOUT)
    timeout= 1000*mysql_get_timeout_value(mysql);
  else
    timeout= -1;
  res= poll(&pfd, 1, timeout);
  if (res == 0)
    return MYSQL_WAIT_TIMEOUT;
  else if (res < 0)
  {
    /*
      In a real event framework, we should handle EINTR and re-try the poll.
    */
    return MYSQL_WAIT_TIMEOUT;
  }
  else
  {
    int status= 0;
    if (pfd.revents & POLLIN)
      status|= MYSQL_WAIT_READ;
    if (pfd.revents & POLLOUT)
      status|= MYSQL_WAIT_WRITE;
    if (pfd.revents & POLLPRI)
      status|= MYSQL_WAIT_EXCEPT;
    return status;
  }
}

static void fatal(MYSQL *mysql, const char *msg) {
  fprintf(stderr, "%s: %s\n", msg, mysql_error(mysql));
  exit(1);
}


MYSQL* get_mysql_conn() {
	MYSQL *mysql = (MYSQL*)malloc(sizeof(MYSQL));
	mysql_init(mysql);

 	mysql_options(mysql, MYSQL_OPT_NONBLOCK, 0);
 	mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "myapp");

	mysql = mysql_real_connect(mysql, "localhost", "root", "W88Wselfy", NULL, 0, NULL, 0);
  	if (mysql == NULL) {
		perror(mysql_error(mysql));
		mysql_close(mysql);
		return NULL;
	} else {
		return mysql;
	}
}

void free_mysql_conn(MYSQL* conn) {
	mysql_close(conn);
}


void print_mysql_rows(MYSQL_ROWS *rows) {
	while (rows != NULL) {
		for(int i=0; i<rows->length; ++i) {
			printf("%s\t", rows->data[i]);
		}
		printf("\n");
		rows = rows->next;
	}
}

void print_mysql_data(MYSQL_DATA *data) {
	printf("data row count : %lld\n", data->rows);
	print_mysql_rows(data->data);	
}

void print_mysql_res(MYSQL_RES *res) {
	printf("row count : %lld\n", res->row_count);
	printf("fields : %x\n", res->fields);
	printf("data : %x\n", res->data);
	printf("rows : %x\n", res->data_cursor);
	printf("handle : %x\n", res->handle);
	printf("row: %x\n", res->row);
}

static void mysql_process(int mysqlfd) {
	
}


static void mysql_doit() {

  	int err;
    int status;
	
	MYSQL *conn = get_mysql_conn();	
	if (conn == NULL) {
		exit(-1);
	}

  	status = mysql_real_query_start(&err, conn, "SHOW STATUS", sizeof("SHOW STATUS"));
  	while (status) {
    	status= wait_for_mysql(conn, status);
    	status= mysql_real_query_cont(&err, conn, status);
  	}

  	if (err)
    	fatal(conn, "mysql_real_query() returns error");

	MYSQL_RES *res;
  	MYSQL_ROW row;

  	/* This method cannot block. */
  	res = mysql_use_result(conn);
  	if (!res)
    	fatal(conn, "mysql_use_result() returns error");
	print_mysql_res(res);
	/*/
  	for (;;) {
    	status= mysql_fetch_row_start(&row, res);
    	while (status) {
      		status= wait_for_mysql(conn, status);
      		status= mysql_fetch_row_cont(&row, res, status);
    	}
    	if (!row)
      		break;
    	printf("%s: %s\n", row[0], row[1]);
  	}
  	if (mysql_errno(conn))
    	fatal(conn, "Got error while retrieving rows");
	//*/
  	mysql_free_result(res);

	free_mysql_conn(conn);
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
	state = listen (sfd, MAXCLIENTS);
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

	puts("--------add socket fd to epoll_ctl.");
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

	puts("-------- malloc events.");
	struct epoll_event *events;
	events = (epoll_event *) calloc (MAXCLIENTS, sizeof(struct epoll_event));
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
	//char buff[BUFFLEN]; // for recv
	//char data[BUFFLEN] = "this epoll server."; // for send
	int loop_count = 0;
	while (true) {
		puts("------------------------------------------");
		printf("--------loop count : %d\n", ++loop_count);
		puts("--------waiting for somebody.");
		int nfds = epoll_wait(efd, events, MAXCLIENTS, -1);
        if (nfds == -1) {
			perror("--------epoll_wait error.");
			exit(EXIT_FAILURE);
		 }
		
		puts("--------some events raised.");
		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == sfd) {
				puts("--------there is a new connection");
				printf("-------- sfd :%d\n", sfd);
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
				printf("-------- events[i].data.fd :%d\n", events[i].data.fd);
				mysql_library_init(0, NULL, NULL);
				puts("my init finished.");
				mysql_doit();
				puts("doit finished.");
				mysql_library_end();
				int cfd = accept(sfd, (struct sockaddr *) &cli_addr, &cli_len);
				int read_cnt = recv_sock_data(events[i].data.fd);		
				if (read_cnt != -1) {	
					puts("--------all data read.");
					struct epoll_event ev;
					ev.data.fd = events[i].data.fd;
					ev.events  = EPOLLOUT | EPOLLET;
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
				int write_cnt = send_sock_data(events[i].data.fd);	
				if (write_cnt != -1) {
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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <poll.h>
#include <libaio.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <assert.h>
#include <mysql.h>

#define SL(s) (s), sizeof(s)
const int MAXCLIENTS = 20;

enum MYSQL_SOCK_TYPE{
	MYSQL_SOCK_UNKNOWN,
	MYSQL_SOCK_CONN,
	MYSQL_SOCK_QUERY_START,
	MYSQL_SOCK_QUERY_CONT,
	MYSQL_SOCK_RES_START,
	MYSQL_SOCK_RES_CONT,
	MYSQL_SOCK_ROW,
	MYSQL_SOCK_FREE_START,
	MYSQL_SOCK_FREE_CONT,
	MYSQL_SOCK_FIN
};

struct waiter_t {
	char *sql;
	char *result;

	int mysql_fd;
	enum MYSQL_SOCK_TYPE  mysql_sock_type;
	int        mysql_err;
	int        mysql_status;
	MYSQL*     mysql_conn;
	MYSQL_RES* mysql_res;
	MYSQL_ROW  mysql_row;
};

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


struct waiter_t waiter[MAXCLIENTS];
int    waiter_map[1024];
struct epoll_event waiter_event[MAXCLIENTS];

void waiter_destroy() {
	for (int i=0; i<MAXCLIENTS; ++i) {
		free_mysql_conn(waiter[i].mysql_conn);
	}
	mysql_library_end();
	puts("close all mysql connections.");
	_exit(0);
}

void waiter_init() {
	mysql_library_init(0, NULL, NULL);	
	for (int i=0; i<MAXCLIENTS; ++i) {
		MYSQL *conn = get_mysql_conn();
		waiter[i].mysql_conn = conn;
		waiter[i].mysql_fd   = mysql_get_socket(conn);
		waiter[i].mysql_sock_type = MYSQL_SOCK_CONN;
		waiter_map[waiter[i].mysql_fd] = i;
//		printf("socket fd:%d \n", waiter[i].mysql_fd);
	}
	atexit(waiter_destroy);
	signal(SIGSEGV, waiter_destroy);
	signal(SIGABRT, waiter_destroy);
	signal(SIGTERM, waiter_destroy);
	signal(SIGFPE,  waiter_destroy);
	signal(SIGILL,  waiter_destroy);
	signal(SIGINT,  waiter_destroy);
}

static void mysql_fatal(const char *msg, MYSQL* mysql) {
	fprintf(stderr, "%s : %s\n", msg, mysql_error(mysql));
	exit(-1);
}

int epoll_to_mysql(int epoll_events) {
	int mysql_status = 0;
	if (epoll_events & EPOLLIN) {
		mysql_status |= MYSQL_WAIT_READ;
	}
	if (epoll_events & EPOLLOUT) {
		mysql_status |= MYSQL_WAIT_WRITE;
	}
	if (epoll_events & EPOLLPRI) {
		mysql_status |= MYSQL_WAIT_EXCEPT;
	}
	return mysql_status;
}

int mysql_to_epoll(int mysql_status) {

	if (mysql_status == 0) {
		printf("EPOLLIN | EPOLLET:%d\n", EPOLLIN | EPOLLET);
		return EPOLLIN | EPOLLET;
	}

	int epoll_events = 0;
	epoll_events = 	(mysql_status & MYSQL_WAIT_READ   ? EPOLLIN  : 0) |
 					(mysql_status & MYSQL_WAIT_WRITE  ? EPOLLOUT : 0) |
 					(mysql_status & MYSQL_WAIT_EXCEPT ? EPOLLPRI : 0) |
					EPOLLET;
	return epoll_events;
}


void add_epoll(int epoll_fd, int mysql_fd, int mysql_status) {
	struct epoll_event event;
	event.data.fd = mysql_fd;
	event.events = mysql_to_epoll(mysql_status);
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event);
}

void delete_epoll(int epoll_fd, int mysql_fd, int mysql_status) {
	struct epoll_event event;
	event.data.fd = mysql_fd;
	event.events = mysql_to_epoll(mysql_status);
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, &event);
}

void update_epoll(int epoll_fd, int mysql_fd, int mysql_status) {
	struct epoll_event event;
	event.data.fd = mysql_fd;
	event.events = mysql_to_epoll(mysql_status);
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event.data.fd, &event);
}

void mysql_query_loop(int epoll_fd, waiter_t *waiter) {

	switch (waiter->mysql_sock_type) {
		case MYSQL_SOCK_CONN:
		case MYSQL_SOCK_QUERY_START : 
			mysql_query_start:
			waiter->mysql_status = mysql_real_query_start(&waiter->mysql_err, waiter->mysql_conn, waiter->sql, strlen(waiter->sql));
			waiter->mysql_sock_type = MYSQL_SOCK_QUERY_CONT; 
			if (waiter->mysql_status == 0) {
				if (waiter->mysql_err) {
					printf("%s\n", mysql_error(waiter->mysql_conn));
				}
				goto mysql_res_start;
			}
			break;

		case MYSQL_SOCK_QUERY_CONT : 
			mysql_query_cont:
			waiter->mysql_status = mysql_real_query_cont(&waiter->mysql_err, waiter->mysql_conn, waiter->mysql_status);
			if (waiter->mysql_status == 0) {
				if (waiter->mysql_err) {
					printf("%s\n", mysql_error(waiter->mysql_conn));
				}
				goto mysql_res_start;
			}
			break;
	
		case MYSQL_SOCK_RES_START : 
			mysql_res_start:
			waiter->mysql_status = mysql_store_result_start(&waiter->mysql_res, waiter->mysql_conn);
			waiter->mysql_sock_type = MYSQL_SOCK_RES_CONT; 
			if (waiter->mysql_status == 0) {
				if (waiter->mysql_res == NULL) {
					printf("%s\n", mysql_error(waiter->mysql_conn));
				}
				goto mysql_row;
			}
			break;

		case MYSQL_SOCK_RES_CONT : 
			mysql_res_cont:
			waiter->mysql_status = mysql_store_result_cont(&waiter->mysql_res, waiter->mysql_conn, waiter->mysql_status);
			if (waiter->mysql_status == 0) {
				if (waiter->mysql_res == NULL) {
					printf("%s\n", mysql_error(waiter->mysql_conn));
				}
				goto mysql_row;
			}
			break;
		
		case MYSQL_SOCK_ROW : 
			mysql_row:
			//MYSQL_ROW row;
			while (true) {
				waiter->mysql_status = mysql_fetch_row_start(&waiter->mysql_row, waiter->mysql_res);
				assert(waiter->mysql_status == 0);
				if (waiter->mysql_row != NULL) {
					int num_fields = mysql_num_fields(waiter->mysql_res);
					for (int i=0; i<num_fields; ++i) {
						printf("%s\t", waiter->mysql_row[i]);
					}
					printf("\n");
				} else {
					break;
				}
			}
			/*no to break here!*/

		case MYSQL_SOCK_FREE_START : 
			mysql_free_start:
			waiter->mysql_status = mysql_free_result_start(waiter->mysql_res);
			if (waiter->mysql_status == 0) {
				waiter->mysql_sock_type = MYSQL_SOCK_FIN; 
			} else { 
				goto mysql_free_cont;
			}
			break;

		case MYSQL_SOCK_FREE_CONT : 
			mysql_free_cont:
			waiter->mysql_status = mysql_free_result_cont(waiter->mysql_res, waiter->mysql_status);
			if (waiter->mysql_status == 0) {
				waiter->mysql_sock_type = MYSQL_SOCK_FIN;
			}
			break;
		case MYSQL_SOCK_FIN :
			delete_epoll(epoll_fd, waiter->mysql_fd, waiter->mysql_status);
			break;
	}
	if (waiter->mysql_sock_type != MYSQL_SOCK_FIN) {
		update_epoll(epoll_fd, waiter->mysql_fd, waiter->mysql_status);
	}
}






int main() {

	waiter_init();

	int epollfd = epoll_create1(0);

	char *sql = "SHOW STATUS";	
	for (int i=0; i<MAXCLIENTS; ++i) {
		waiter[i].sql = sql;
		mysql_query_loop(epollfd, &waiter[i]);
	}

	while (true) {
		puts("--------epoll waiting for somebody.");
		int nfds = epoll_wait(epollfd, waiter_event, MAXCLIENTS, -1);  
		for (int i=0; i<nfds; ++i) {
			int index = waiter_map[waiter_event[i].data.fd];	
			assert(waiter_event[i].data.fd == waiter[index].mysql_fd);
			mysql_query_loop(epollfd, &waiter[index]);
		}
	}	

	return 0;
}







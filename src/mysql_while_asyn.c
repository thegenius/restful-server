#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/time.h>
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
const int MAXCLIENTS = 128;

enum MYSQL_SOCK_TYPE{
	MYSQL_SOCK_UNKNOWN,
	MYSQL_SOCK_CONN,
	MYSQL_SOCK_QUERY_START,
	MYSQL_SOCK_QUERY_CONT,
	MYSQL_SOCK_RES_START,
	MYSQL_SOCK_RES_CONT,
	MYSQL_SOCK_ROW_START,
	MYSQL_SOCK_ROW_CONT
};

struct waiter_t {
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
		mysql_free_result(waiter[i].mysql_res);
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

static void fatal(MYSQL *mysql, const char *msg) {
  fprintf(stderr, "%s: %s\n", msg, mysql_error(mysql));
  exit(1);
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


void update_epoll(int epoll_fd, int mysql_fd, int mysql_status) {
	struct epoll_event event;
	event.data.fd = mysql_fd;
	event.events = mysql_to_epoll(mysql_status);
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event.data.fd, &event);
}



int mysql_do_query(MYSQL *conn, const char* sql, char** result) {
	int status;
	int err;
	status = mysql_real_query_start(&err, conn, SL("SHOW STATUS"));
	while (status) {
		status = mysql_real_query_cont(&err, conn, status);  
	}
  	//if (err) {
    //	fatal(conn, "mysql_real_query() returns error");
	//}

	MYSQL_RES *res;
	status = mysql_store_result_start(&res, conn);
	while (status) {
		status = mysql_store_result_cont(&res, conn, status);
	}
  	//if (!res) {
    //	fatal(conn, "mysql_use_result() returns error");
	//}
	
	MYSQL_ROW row;
	while (true) {
		status = mysql_fetch_row_start(&row, res);
		while (status) {
			status = mysql_fetch_row_cont(&row, res, status);
		}
		if (row != NULL) {
			int field_count = mysql_field_count(conn);
			for (int i=0; i<field_count; ++i) {
				//printf("%s\t", row[i]);
			}
			//printf("\n");
		} else {
			break;
		}
	}
	
	status = mysql_free_result_start(res);
	while (status) {
		status = mysql_free_result_cont(res, status);
	}
	return 0;
}


int mysql_do_query2(MYSQL *conn, const char* sql, char** result) {
	int err;
	err = mysql_query(conn, "SHOW STATUS");
	
	MYSQL_RES *res = mysql_store_result(conn);

	int num_fields = mysql_num_fields(res);
	MYSQL_ROW row;
	while ( (row=mysql_fetch_row(res)) ) {
		for (int i=0; i<num_fields; ++i) {
			//printf("%s ", row[i]? row[i]:"NULL");
		}
		//printf("\n");
	}
	mysql_free_result(res);
	return 0;
}

const int loop_times = 1000;
char *result;

void consume_time1() {
	for (int i=0; i<loop_times; ++i) {	
		mysql_do_query(waiter[0].mysql_conn, "SHOW STATUS", &result);
	}
}
void consume_time2() {
	for (int i=0; i<loop_times; ++i) {	
		mysql_do_query2(waiter[0].mysql_conn, "SHOW STATUS", &result);
	}
}
void gettimeofday_calc_time1() {
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    consume_time1();
    gettimeofday(&t2, NULL); 
    printf("gettimeofday() : %.2f s\n", (double) (t2.tv_sec - t1.tv_sec) +
                                        (t2.tv_usec - t1.tv_usec) / 1000000.0);
}
void gettimeofday_calc_time2() {
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    consume_time2();
    gettimeofday(&t2, NULL); 
    printf("gettimeofday() : %.2f s\n", (double) (t2.tv_sec - t1.tv_sec) +
                                        (t2.tv_usec - t1.tv_usec) / 1000000.0);
}
int main() {

	waiter_init();
	gettimeofday_calc_time1();
	gettimeofday_calc_time2();
	
	return 0;
}







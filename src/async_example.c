#include <stdlib.h>
#include <stdio.h>


#include <poll.h>
#include <mysql.h>

#define SL(s) (s), sizeof(s)

static const char *my_groups[]= { "client", NULL };

static int
wait_for_mysql(MYSQL *mysql, int status) {
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

static void
doit(const char *host, const char *user, const char *password)
{
  int err;
  MYSQL mysql, *ret;
  MYSQL_RES *res;
  MYSQL_ROW row;
  int status;

  mysql_init(&mysql);
  mysql_options(&mysql, MYSQL_OPT_NONBLOCK, 0);
  mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "myapp");

  /* Returns 0 when done, else flag for what to wait for when need to block. */
  status= mysql_real_connect_start(&ret, &mysql, host, user, password, NULL,
                                   0, NULL, 0);
  while (status)
  {
    status= mysql_real_connect_cont(&ret, &mysql, status);
  }

  if (!ret)
    fatal(&mysql, "Failed to mysql_real_connect()");

  status= mysql_real_query_start(&err, &mysql, SL("SHOW STATUS"));
  while (status)
  {
    status= mysql_real_query_cont(&err, &mysql, status);
  }
  if (err)
    fatal(&mysql, "mysql_real_query() returns error");

  /* This method cannot block. */
  //res= mysql_use_result(&mysql);
  //if (!res)
  //  fatal(&mysql, "mysql_use_result() returns error");

  status = mysql_store_result_start(&res, &mysql);
  while (status) {
  	status = mysql_store_result_cont(&res, &mysql, status);
  }
	


  for (;;)
  {
    status= mysql_fetch_row_start(&row, res);
    while (status)
    {
      status= mysql_fetch_row_cont(&row, res, status);
    }
    if (!row)
      break;
    printf("%s: %s\n", row[0], row[1]);
  }
  if (mysql_errno(&mysql))
    fatal(&mysql, "Got error while retrieving rows");
  mysql_free_result(res);

  /*
    mysql_close() sends a COM_QUIT packet, and so in principle could block
    waiting for the socket to accept the data.
    In practise, for many applications it will probably be fine to use the
    blocking mysql_close().
   */
  status= mysql_close_start(&mysql);
  while (status)
  {
    status= mysql_close_cont(&mysql, status);
  }
}

int
main(int argc, char *argv[])
{
  int err;

  if (argc != 4)
  {
    fprintf(stderr, "Usage: %s <host> <user> <password>\n", argv[0]);
    exit(1);
  }

  err= mysql_library_init(argc, argv, (char **)my_groups);
  if (err)
  {
    fprintf(stderr, "Fatal: mysql_library_init() returns error: %d\n", err);
    exit(1);
  }

  doit(argv[1], argv[2], argv[3]);

  mysql_library_end();

  return 0;
}

#include "http_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* rand */
#include <string.h>
#include <stdarg.h>

#if defined(__APPLE__)
# undef strlcat
# undef strlncpy
# undef strlcpy
#endif  /* defined(__APPLE__) */

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 2048

#define MIN(a,b) ((a) < (b) ? (a) : (b))

struct message {
  const char *name; // for debugging purposes
  const char *raw;
  enum http_parser_type type;
  enum http_method method;
  int status_code;
  char response_status[MAX_ELEMENT_SIZE];
  char request_path[MAX_ELEMENT_SIZE];
  char request_url[MAX_ELEMENT_SIZE];
  char fragment[MAX_ELEMENT_SIZE];
  char query_string[MAX_ELEMENT_SIZE];
  char body[MAX_ELEMENT_SIZE];
  size_t body_size;
  const char *host;
  const char *userinfo;
  uint16_t port;
  int num_headers;
  enum { NONE=0, FIELD, VALUE } last_header_element;
  char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
  int should_keep_alive;

  const char *upgrade; // upgraded body

  unsigned short http_major;
  unsigned short http_minor;

  int message_begin_cb_called;
  int headers_complete_cb_called;
  int message_complete_cb_called;
  int message_complete_on_eof;
  int body_is_final;
};

void message_print(struct message *msg) {
	printf("name : %s \n", msg->name);
	printf("raw  : %s \n", msg->raw);
	printf("type : %d \n", msg->type);
	printf("method : %d \n", msg->method);
	printf("status_code %d \n", msg->status_code);
	printf("response_status : %d \n", msg->response_status);
	printf("request_path : %s \n", msg->request_path);
	printf("request_url : %s \n", msg->request_url);
	printf("body : %s \n", msg->body);
	printf("body_size :%d \n", msg->body_size);
	printf("host : %s \n", msg->host);
	printf("userinfo : %s \n", msg->userinfo);
	printf("port : %s \n", msg->port);
	printf("num_header %d \n", msg->num_headers);
}









typedef message message;
static message messages[5];
static int64_t num_messages = 0;

const int NONE=0, FIELD=1, VALUE=2; 




int my_message_begin(http_parser* parser) {
	messages[num_messages].message_begin_cb_called = true;
	return 0;
}

int my_url(http_parser* parser, const char *p, size_t len) {
	strncat(messages[num_messages].request_url, p , len);
	return 0;
}

int my_status(http_parser* parser) {
	return 0;
}
int my_header_field(http_parser* parser, const char *p, size_t len) {
	message *msg = &messages[num_messages];
	if (msg->last_header_element != FIELD) {
		msg->num_headers++;
	}
	strncat(msg->headers[msg->num_headers-1][0], p, len);
	msg->last_header_element = FIELD;
	return 0;
}

int my_header_value(http_parser* parser, const char *p, size_t len) {
	message *msg = &messages[num_messages];
	strncat(msg->headers[msg->num_headers-1][1], p, len);
	msg->last_header_element = VALUE;
	return 0;
}

int my_headers_complete(http_parser* parser) {
	return 0;
}

int my_body(http_parser* parser, const char *p, size_t len) {
	strncat(messages[num_messages].body, p, len);
	return 0;
}

int my_message_complete(http_parser *parser) {
	messages[num_messages].method = parser->method;
	messages[num_messages].status_code = parser->status_code;	
	messages[num_messages].message_complete_cb_called = true;
	message_print(&messages[num_messages]);
	return 0;
}

void 
my_parser_settings_init(http_parser_settings* parser_settings) {
	
	
	memset(parser_settings, 0, sizeof(http_parser_settings));
	
	parser_settings->on_message_begin    = my_message_begin;
	parser_settings->on_url              = my_url;
	parser_settings->on_status           = my_status;
	parser_settings->on_header_field     = my_header_field;
	parser_settings->on_header_value     = my_header_value;
	parser_settings->on_headers_complete  = my_headers_complete;
	parser_settings->on_body             = my_body;
	parser_settings->on_message_complete = my_message_complete;
}













































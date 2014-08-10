#include <stdio.h>
#include <buff_char.h>

int main() {

	buff_char_t str;
	buff_char_init(&str);
	buff_char_append(&str, "hello", strlen("hello"));
	buff_char_append(&str, "hello", strlen("hello"));
	
	buff_char_append(&str, "hello", strlen("hello"));
	buff_char_append(&str, "hello", strlen("hello"));
	buff_char_append(&str, "hello", strlen("hello"));
	buff_char_append(&str, "hello", strlen("hello"));
	buff_char_append(&str, "hello", strlen("hello"));
	
	
	buff_char_destroy(&str);	
	
	
	
	printf("%s\n", buff_char_data(&str));
	return 0;
}

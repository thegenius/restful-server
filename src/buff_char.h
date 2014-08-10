#ifndef __BUFF_CHAR_H
#define __BUFF_CHAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************
* forward declaration    *
*****************************************************/

struct __buff_char_arena {	
	size_t size;
	char   data[0]; // a small trick
};
typedef struct __buff_char_arena __buff_char_arena;

struct buff_char_t {
	size_t arena_cnt;
	size_t arena_cur;
	__buff_char_arena **arena;
};
typedef struct buff_char_t buff_char_t;

void   buff_char_create(buff_char_t **buff);
void   buff_char_init(buff_char_t *buff);
size_t buff_char_size(buff_char_t *buff);
char*  buff_char_data(buff_char_t *buff);
//int    buff_char_append(buff_char_t *buff, const char* data);
int    buff_char_append(buff_char_t *buff, const char* data, const size_t len);
void   buff_char_destroy(buff_char_t *buff);

void buff_char_create(buff_char_t **buff) {
	*buff = (buff_char_t*)malloc(sizeof(buff_char_t));
}

void buff_char_init(buff_char_t *buff) {
	buff->arena_cnt = 2;
	buff->arena_cur = 0;
	buff->arena = (__buff_char_arena**)calloc(2, sizeof(__buff_char_arena*));
} 

size_t buff_char_size(buff_char_t *buff) {
	size_t size = 0;
	for (int i=0; i<buff->arena_cur; ++i) {
		size += buff->arena[i]->size;
	}
	return size;
}

char* buff_char_data(buff_char_t *buff) {
	size_t total_size = buff_char_size(buff);
	char* ret = (char*)malloc(total_size*sizeof(char)+1);
	size_t offset = 0;
	for (int i=0; i<buff->arena_cur; ++i) {
		memcpy(ret+offset, buff->arena[i]->data, buff->arena[i]->size*sizeof(char));
		offset += buff->arena[i]->size;
	}
	ret[total_size] = '\0';
	return ret;
}

//int buff_char_append(buff_char_t *buff, const char *data) {
//	buff_char_append(buff, data, strlen(data));
//}	

int buff_char_append(buff_char_t *buff, const char* data, const size_t len) {
	if (buff->arena_cur == buff->arena_cnt) {
		//be careful, the arena_cnt can be 0
		buff->arena = (__buff_char_arena**)realloc(buff->arena, 2*(buff->arena_cnt+1)*sizeof(__buff_char_arena*));
		memset(buff->arena+buff->arena_cnt, 0, (buff->arena_cnt+2)*sizeof(__buff_char_arena*));
		buff->arena_cnt = 2*(buff->arena_cnt+1);
	}		

	buff->arena[buff->arena_cur] = (__buff_char_arena*)malloc(sizeof(__buff_char_arena)+len*sizeof(char));
	buff->arena[buff->arena_cur]->size = len;
	memcpy(buff->arena[buff->arena_cur]->data, data, len*sizeof(char));
	++buff->arena_cur;
	return 0;
}

void buff_char_destroy(buff_char_t *buff) {
	if (buff != NULL) {
		for (int i=0; i<buff->arena_cur; ++i) {
			if (buff->arena[i] != NULL) {	
				free(buff->arena[i]);
			}
		}
		free(buff);
	}
}

#endif





















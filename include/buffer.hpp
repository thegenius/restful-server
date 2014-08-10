#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************
* forward declaration    *
*****************************************************/
template <typename T>
struct __buffer_arena;

template <typename T>
class Buffer {
public:
	Buffer(size_t count = 2);
	size_t size();
	T*     data();
	bool   append(T*data, size_t len);
private:
	size_t arena_cnt;
	size_t arena_cur;
	__buffer_arena<T> **arena;
};

template<>
class Buffer<char> {
public:
	Buffer(size_t count = 2);
	size_t size();
	char*     data();
	bool   append(char *data);
	bool   append(char *data, size_t len);
private:
	size_t arena_cnt;
	size_t arena_cur;
	__buffer_arena<char> **arena;
};


/*****************************************************
* for normal use *
*****************************************************/

template <typename T>
struct __buffer_arena {	
	size_t size;
	T      data[0]; // a small trick
};

template <typename T>
Buffer<T>::Buffer(size_t count) : arena_cnt(count), arena_cur(0) {
	this->arena = (__buffer_arena<T>**)calloc(count, sizeof(__buffer_arena<T>*));
}

template <typename T>
size_t Buffer<T>::size() {
	size_t size = 0;
	for (int i=0; i<arena_cur; ++i) {
		size += arena[i]->size;
	}
	return size;
}

template <typename T>
T* Buffer<T>::data() {
	size_t total_size = size();
	T* ret = (T*)malloc(total_size*sizeof(T));
	size_t offset = 0;
	for (int i=0; i<arena_cur; ++i) {
		memcpy(ret+offset, arena[i]->data, arena[i]->size*sizeof(T));
		offset += arena[i]->size;
	}
	return ret;
}

template <typename T>
bool Buffer<T>::append(T* data, size_t len) {
	if (arena_cur == arena_cnt) {
		//be careful, the arena_cnt could be 0
		this->arena = (__buffer_arena<T>**)realloc(arena, 2*(arena_cnt+1)*sizeof(__buffer_arena<T>*));
		memset(arena+arena_cnt, 0, (arena_cnt+2)*sizeof(__buffer_arena<T>*));
		arena_cnt = 2*(arena_cnt+1);
	}		

	arena[arena_cur] = (__buffer_arena<T>*)malloc(sizeof(__buffer_arena<T>)+len*sizeof(T));
	arena[arena_cur]->size = len;
	memcpy(arena[arena_cur]->data, data, len*sizeof(T));
	++arena_cur;
}


/*************************************************************
* for char                                                   *
**************************************************************/

Buffer<char>::Buffer(size_t count) : arena_cnt(count), arena_cur(0) {
	this->arena = (__buffer_arena<char>**)calloc(count, sizeof(__buffer_arena<char>*));
}

size_t Buffer<char>::size() {
	size_t size = 0;
	for (int i=0; i<arena_cur; ++i) {
		size += arena[i]->size;
	}
	return size;
}

char* Buffer<char>::data() {
	size_t total_size = size();
	char* ret = (char*)malloc(total_size*sizeof(char)+1);
	size_t offset = 0;
	for (int i=0; i<arena_cur; ++i) {
		memcpy(ret+offset, arena[i]->data, arena[i]->size*sizeof(char));
		offset += arena[i]->size;
	}
	ret[total_size] = '\0';
	return ret;
}

bool Buffer<char>::append(char *data) {
	append(data, strlen(data));
}	

bool Buffer<char>::append(char* data, size_t len) {
	if (arena_cur == arena_cnt) {
		//be careful, the arena_cnt could be 0
		this->arena = (__buffer_arena<char>**)realloc(arena, 2*(arena_cnt+1)*sizeof(__buffer_arena<char>*));
		memset(arena+arena_cnt, 0, (arena_cnt+2)*sizeof(__buffer_arena<char>*));
		arena_cnt = 2*(arena_cnt+1);
	}		

	arena[arena_cur] = (__buffer_arena<char>*)malloc(sizeof(__buffer_arena<char>)+len*sizeof(char));
	arena[arena_cur]->size = len;
	memcpy(arena[arena_cur]->data, data, len*sizeof(char));
	++arena_cur;
}
#endif





















#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <thread>
#include <vector>

struct task_t {
	task_t(){
	}
	task_t(void* (*f)(void*), void *a) {
		this->func = (void*(*)(void*))f;
		this->args = a;
	}
	void *(*func)(void*);
	void *args;	
};

typedef struct {
	void *data;
} fruit_t;

class threadpool_t {
public:
	threadpool_t(size_t thread_num=4);
	bool add_thread();
	bool put_task(void*(*func)(void*), void *);
	bool get_fruit(void *&);
	threadpool_t(const threadpool_t&) = delete;
	void operator=(const threadpool_t&) = delete;
private:
	void loopfunc_();
	boost::lockfree::queue<task_t>  task_queue_;
	boost::lockfree::queue<fruit_t> fruit_queue_;
	std::vector<std::thread>        threads_;	
	bool                            running_;
};


threadpool_t::threadpool_t(size_t thread_num)
	:task_queue_(0),
	 fruit_queue_(0),
	 running_(true) {
	puts("constructor");	
	for (int i=0; i<thread_num; ++i) {
		std::thread thrd(std::bind(&threadpool_t::loopfunc_,this));
		thrd.detach();
		threads_.push_back(thrd);
	}
	puts("thread pool built");
}
bool threadpool_t::add_thread() {
	threads_.push_back(std::thread(std::bind(&threadpool_t::loopfunc_,this)));
	return 1;
}
bool threadpool_t::put_task(void *(*func)(void*), void *args) {
	task_t *task = new task_t(func, args);
	task_queue_.push(*task);
	return true;
}

bool threadpool_t::get_fruit(void *&data) {
	fruit_t ret;
	fruit_queue_.pop(ret);
	data = ret.data;
	return true;
}

void threadpool_t::loopfunc_() {
	while (running_) {
		task_t *task = new task_t;
		task_queue_.pop(*task);
		task->func(task->args);
		fruit_t ret;
		ret.data = task->args;
		fruit_queue_.push(ret);
	}
}

void* foo(void *data) {
	char *str;
	printf("%s\n", str);
	str[1] = 's';
	return NULL;
}

int main() {
	threadpool_t pool;
	char str[32] = "hello thread pool";
	pool.put_task(foo, str);

	return 0;
}




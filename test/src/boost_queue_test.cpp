#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/atomic.hpp>
#include <iostream>
#include <thread>


boost::atomic_int producer_cnt(0);
boost::atomic_int consumer_cnt(0);
boost::lockfree::queue<int> queue(128);

const int iterations = 1000000;
const int producer_thread_cnt = 4;
const int consumer_thread_cnt = 4;

void producer(void) {
	for (int i=0; i<iterations; ++i) {
		int value = ++producer_cnt;
		while (!queue.push(value)) {
		};
	}
}

boost::atomic<bool> done (false);  
void consumer(void) {  
    int value;  
    while (!done) {  
        while (queue.pop(value)){
			++consumer_cnt; 
		} 
    }  
  
    while (queue.pop(value))  
        ++consumer_cnt;  
}



int main(int argc, char* argv[]) {  
    using namespace std;  
    cout << "boost::lockfree::queue is ";  
    if (!queue.is_lock_free())  
        cout << "not ";  
    cout << "lockfree" << endl;  
  
    boost::thread_group producer_threads, consumer_threads;  
  
    for (int i = 0; i != producer_thread_cnt; ++i) { 
        producer_threads.create_thread(producer);  
	}
  
    for (int i = 0; i != consumer_thread_cnt; ++i) { 
        consumer_threads.create_thread(consumer);  
	}
  
    producer_threads.join_all();  
    done = true;  
  
    consumer_threads.join_all();  
  
    cout << "produced " << producer_cnt << " objects." << endl;  
    cout << "consumed " << consumer_cnt << " objects." << endl;  
	return 0;
}











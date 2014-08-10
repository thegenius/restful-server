all:boost_queue_test threadpool_test
threadpool_test: threadpool.cpp
	g++ -std=c++11 -Wl,--no-as-needed  threadpool.cpp -lpthread -o threadpool_test
boost_queue_test: boost_queue_test.cpp
	g++ -std=c++0x boost_queue_test.cpp -lboost_thread -lboost_system -o boost_queue_test

all: server-1 http_parser server-2 server-3 server-4 mysql mysql2 client clean
server-1: src/server-1.c
	g++ -I./include src/server-1.c -o bin/server-1
http_parser: dependencies/http_parser.c
	g++ -c -I./dependencies dependencies/http_parser.c -o bin/http_parser.o
server-2: src/server-2.c
	g++ -c -I./include src/server-2.c -o bin/server-2.o
	g++ bin/server-2.o bin/http_parser.o -o bin/server-2 
server-3: src/server-3.c
	g++ -fpermissive -c -I./include src/server-3.c -o bin/server-3.o
	g++ bin/server-3.o bin/http_parser.o -o bin/server-3 -laio
server-4: src/server-4.c
	g++ -fpermissive -c -I./include src/server-4.c -o bin/server-4.o
	g++ -static bin/server-4.o  -o bin/server-4 -L/usr/local/mysql/lib -laio -lmysqlclient -lpthread -lz -lm -lrt -ldl
mysql: src/mysql.c
	g++ -g -static -fpermissive src/mysql.c  -o bin/mysql -L/usr/local/mysql/lib -laio -lmysqlclient -lpthread -lz -lm -lrt -ldl
mysql2: src/mysql2.c
	g++ -g -static -fpermissive src/mysql2.c  -o bin/mysql2 -L/usr/local/mysql/lib -laio -lmysqlclient -lpthread -lz -lm -lrt -ldl
client: src/client.c
	g++ -I./include src/client.c -o bin/client
clean:
	rm -v ./bin/*.o
# -lmysqlclient -lpthread -lz -lm -lrt -ldl


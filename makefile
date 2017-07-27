CC=clang++

server : server.cpp
	$(CC) -std=c++11 -o server server.cpp -luv

clean : server
	rm server
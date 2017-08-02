CC=clang++

staticserver : staticserver.cpp clientsession.h
	$(CC) -std=c++11 -o staticserver staticserver.cpp -luv

clean : staticserver
	rm staticserver

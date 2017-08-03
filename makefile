CC=g++

staticserver : staticserver.cpp clientsession.cpp staticserver.h clientsession.h filemap.h
	$(CC) -std=c++11 -o staticserver staticserver.cpp clientsession.cpp -luv

clean : staticserver
	rm staticserver

all: chatserver

chatserver.o: server.cpp pg1lib.h
	g++ -std=c++11 -c -o chatserver.o server.cpp

chatserver: chatserver.o
	g++ -std=c++11 -pthread -lcrypto -lz chatserver.o -o chatserver

clean:
	@echo cleaning
	@rm chatserver
	@rm chatserver.o

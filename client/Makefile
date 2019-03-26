all: chatclient

chatclient.o: client.cpp pg1lib.h
	g++ -std=c++11 -c -o chatclient.o client.cpp

chatclient: chatclient.o
	g++ -std=c++11 chatclient.o -o chatclient -pthread -lcrypto -lz

clean:
	@echo Cleaning...
	@rm chatclient
	@rm chatclient.o

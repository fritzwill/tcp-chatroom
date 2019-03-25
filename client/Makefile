all: chatclient

chatclient.o: client.cpp pg1lib.h
	g++ -std=c++11 -c -o chatclient.o client.cpp

chatclient: chatclient.o
	g++ -o chatclient -std=c++11 -lpthread -lcrypto -lz chatclient.o

clean:
	@echo Cleaning...
	@rm chatclient
	@rm chatclient.o

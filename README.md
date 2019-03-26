# tcp-chatroom
## Overview
This is an exmample of a client-server model where the clients connect to the server using a tcp socket connection. Both the client and the server are written in C/C++. 

The server hosts multiple concurrent clients and needs to be run once. Then each different client can use the './chatclient' binary to connect to the server. 

The clients can choose to message everyone in the chat or direct message certain users.

## How to Compile
Make sure you have the capabilities to link the 'crypto' library in C/C++ code using g++.
### Compiling the server
```
$ cd ./server
$ make
g++ -std=c++11 -c -o chatserver.o server.cpp
g++ -std=c++11 -lpthread -lcrypto -lz chatserver.o -o chatserver
```
### Compiling the client
```
$ cd ./client
$ make
g++ -std=c++11 -c -o chatclient.o client.cpp
g++ -o chatclient -std=c++11 -lpthread -lcrypto -lz chatclient.o
```

## Using the Compiled Binaries
First we have to launch the server so it can connect multiple clients.
### Launching Server
Use the following command in the '/server' folder where the chatserver binary lives. Replace 4000 below with any PORT NUMBER:
```
$ ./chatserver [PORT NUMBER]
```
Example:
```
$ ./chatserver 4000
Accepting connections on port 4000
```
### Connecting Clients
Make sure that each client is from its own independent machine. To connect to the server you use the following sytax (HOST NAME is the name of the machine he server binary is run on):
```
$ ./chatclient [HOST NAME] [PORT NUMBER] [USERNAME]
```
Example:
```
$ ./chatclient student01.cse.nd.edu 4000 will
Connecting to student01.cse.nd.edu on port 4000
Conected to student01.cse.nd.edu
pass: password

Successfully Joined Chat
Please enter a command: P (Public message), D (Direct message), Q (Quit)
> 
```
### Messaging 
#### Public
Here is an example of how a public message would look from one client:
```
Please enter a command: P (Public message), D (Direct message), Q (Quit)
> P
Enter the public message: test
```
The other client would see this in their console:
```
Please enter a command: P (Public message), D (Direct message), Q (Quit)
> 
*** Incoming public message ***: test
> 
```
#### Direct
Here is an example of how direct messaging works with clients:
```
Please enter a command: P (Public message), D (Direct message), Q (Quit)
> D
Peers online: 
 will

Peer to message: will
Message: test2
```
The other client would see this in their console:
```
Please enter a command: P (Public message), D (Direct message), Q (Quit)
> 
*** Incoming private message from wfritz ***: test2
>
```
## How 'users.csv' Works
These are just the comma-separated list of users allowed to use the system. If upon login a user is not recognized, their username and password is added to this file. The file follows the following format:
```
[user], [password for user]
```

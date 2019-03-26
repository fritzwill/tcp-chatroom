// Will Fritz
// tcp chatroom server
// 3/25/2019

// includes
#include <stdio.h>
#include <typeinfo>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sys/time.h>
#include <time.h>
#include <map>
#include <pthread.h>

// macros
#define MAX_PENDING 5
#define MAX_LINE 2048
#define MAX_CLIENTS 10

// namespace
using namespace std;

// structs
struct User{
    int mSock;
    string mPubKey;
    string mPassword;
    bool online;
};

// globals
int s, new_s, NUM_THREADS;
map<string, User> users;

// prototpes
void *clientinteraction(void*);
void handle_login();
int recvWithCheck(int, string&);
void send_string(string);
void publicMessage(int);
void directMessage(int);

// main method
int main(int argc, char * argv[]) {
	struct sockaddr_in sin, client_addr;
    char buf[MAX_LINE];
	bzero((char *)&buf, sizeof(buf));
    int addr_len, SERVER_PORT, len;
    int opt = 1;

    // hanlde command line arguments
    // first arg: server port
    if (argc == 2) {
        SERVER_PORT = atoi(argv[1]);
    } else {
		cout << "Wrong number of arguments!" << endl;
        exit(1);
    }

    /* build address data structure */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    /* setup passive open */
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("simplex-talk: socket");
        exit(1);
    }

    /* set socket option */
    if ((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)& opt,
        sizeof(int))) < 0) {
        perror ("simplex-talk:setscokt");
        exit(1);
    }

    if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("simplex-talk: bind");
        exit(1);
    }

    if ((listen(s, MAX_PENDING)) < 0) {
        perror("simplex-talk: listen");
        exit(1);
    }

    /* wait for connection, then receive and print text */
    cout << "Accepting connections on port " << SERVER_PORT << endl;
    addr_len = sizeof(client_addr);
	
	// open users file, read in users
	ifstream fin("users.csv");
    if (!fin){
        cout << "error operning users.csv file for input" << endl;
        exit(1);
    }
    string usr, pass;
    while (fin >> usr >> pass){
        User newUser;
        newUser.online = false;
        newUser.mPassword = pass;
        newUser.mPubKey = "";
        newUser.mSock = -1;
        users.insert({usr.substr(0, usr.size()-1),newUser});
    }
	fin.close();

    // take in and accept new clients each on their own thread
	while ((new_s = accept(s, (struct sockaddr *)&client_addr, (socklen_t*)&addr_len)) >= 0) {
        
		if (NUM_THREADS == 10) {
			cout << "Connection refused: max clients online" << endl;			
			continue;
		}
		
		cout << "\nConnection accepted" << endl;
         
		pthread_t thread;
		NUM_THREADS++;

        if (pthread_create( &thread, NULL,  clientinteraction, &new_s) < 0) {
            perror("Error creating thread");
            return 1;
        }
	}
}

// sends string over tcp connection
void send_string(int sock, string toSend, string message_type) {
	toSend = message_type + toSend;
    int len = toSend.length();
    if (send(sock, toSend.c_str(), len, 0) == -1) {
        perror("Client send error!\n");
        exit(1);
    }
}

// handles login workflow
void handle_login(int sock) {

	int read_size;
	string message, username, password, pubKey;
    
    recvWithCheck(sock, username);
	
    cout << "user: " << username << endl;
	auto it = users.find(username);
	
	// user exists
	if (it != users.end()) {
		it->second.mSock = sock;		
		// prompt user for password
		message = "existUser";
		send_string(sock,message, "");			

		while (1) {
			// check if correct password, continue receiving password until correct
            recvWithCheck(sock, password);	
			if (password == users.find(username)->second.mPassword) {	
				// passwords match
				send_string(sock, "OK","");
				break;
			} else {
				// passwords do not match
				message = "Incorrect password, try again: ";
				send_string(sock,message, "C");		
			}
		}
    // new user
	} else {
		// prompt user for new password
		message = "Creating New User\n Enter password: ";
		send_string(sock,message, "C");
				
		recvWithCheck(sock, password);

		// create new user
		ofstream outfile;
        outfile.open("users.csv", fstream::app);
        if (!outfile){
            cout << "error operning users.csv file for output" << endl;
            exit(1);
        }
		string data = username + ", " + password + "\n";
    
	    outfile << data;
		
		outfile.close();

		// add new user to map
		User newUser;
        newUser.online = false;
        newUser.mPassword = password;
        newUser.mPubKey = "";
        newUser.mSock = sock;
		users.insert({username, newUser});

		// send acknowledgement of success to client
		send_string(sock,"OK", "");
	}
    // get pubKey from client
    recvWithCheck(sock, pubKey);
    cout << "received pubkey from " << username << endl;
    // if username not in users we have a problem
    users[username].mPubKey = pubKey;
    users[username].online = true;

}

// thread for each client
void *clientinteraction(void *socket_desc) {

	int sock = *(int*)socket_desc;
	int read_size;
	char buf[MAX_LINE];

	handle_login(sock);
    string msgFrmClient;

    // handle client inputs
    while (1){
        recvWithCheck(sock, msgFrmClient);
        if (msgFrmClient == "pubMsg"){
            publicMessage(sock);     
        }
        else if (msgFrmClient == "dirMsg"){
            directMessage(sock);
        }
        else if (msgFrmClient.front() == 'Q'){
			string username = msgFrmClient.erase(0,1);
            users.find(username)->second.online = false;
			break;
        }
    }
	close(sock);
	NUM_THREADS--;
}

// send message to all users
void publicMessage(int sock){
    string msgFromClient;
    recvWithCheck(sock, msgFromClient);
    for (auto it = users.begin(); it != users.end(); it++){
        if (!it->second.online) {
            continue; // only send to users online
        }
        cout << "User: " << it->first << endl;
        cout << "Sock: " << it->second.mSock << endl;
        int sendSock;
        sendSock = it->second.mSock;
        if (sendSock == sock) {
            continue; // dont send to self
        }
        else {
            send_string(sendSock, msgFromClient, "D");
        }
    }
    send_string(sock, "P:OK", "C");
}

// send message to specific user
void directMessage(int sock){
    // find list of online servers
    string onlineUsers = "";
    for (auto it = users.begin(); it != users.end(); it++){
        if (it->second.online){
            onlineUsers = onlineUsers + it->first + ";"; 
        }
    }
    send_string(sock, onlineUsers,"Cusrs");
    
    map<string,User>::iterator usr;
    
    // get name of user and send message
    string usrToMsg;
    recvWithCheck(sock, usrToMsg);
    usr = users.find(usrToMsg);
    bool userDNE = false;
    if (usr == users.end()){
        send_string(sock, "DNE", "C");
        return;
    }

    string pubKey = usr->second.mPubKey;
    
    send_string(sock,pubKey,"Ckey");
    
    string encryptedMsg, frmUser;
    recvWithCheck(sock, encryptedMsg);
    recvWithCheck(sock, frmUser);
    if (usr->second.online){
        send_string(usr->second.mSock, encryptedMsg, "DE");
        send_string(usr->second.mSock, frmUser, "");
        send_string(sock, "SENT", "C");
    }
    else { // not online
        send_string(sock, "NO", "C");
    }
}
    
// recieves from client and makes sure the number of bytes read is okay
int recvWithCheck(int sockFd, string &outMsg){
    int numBytesRec;
    char buf[MAX_LINE];
    int len = sizeof(buf);
    int flags = 0;
    
    memset(buf,0,sizeof(buf));    
 
    if ((numBytesRec=recv(sockFd, buf, len, flags)) == -1){
        perror("receive error");
        close(sockFd);
        exit(1);
    }

    if (numBytesRec == 0){
        cout << "recvWithCheck: zero bytes received" << endl;
        close(sockFd);
        exit(1);
    }
    outMsg = buf;
    return numBytesRec;
}


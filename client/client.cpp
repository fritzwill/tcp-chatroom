// Will Fritz
// tcp chatroom client
// 3/25/2019

// includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <typeinfo>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "pg1lib.h"

// macros
#define MAX_LINE 2048

// namespace
using namespace std;

// globals
int sockFd;
bool PSEND;
vector<string> onlineUsers;
string USERNAME;
bool ABORT_DIR_MSG;
string PUB_KEY_FOR_MSG;

// proototypes
void *handle_messages(void*);
void handle_login(string);
int recvWithCheck(string&);
void send_string(string);
void promptUser();
void publicMessage();
void directMessage();

void send_short(short int val) {
	char toSend[MAX_LINE];
	sprintf(toSend, "%d", val);
	send_string(toSend);
}

void send_long(long int val) {
	char toSend[MAX_LINE];
	sprintf(toSend, "%d", val);
	send_string(toSend);
}

int rec_int() {
	int i;
	char buf[MAX_LINE];	

	if ((i = recv(sockFd, buf, sizeof(buf), 0)) == -1) {
		perror("did not receive int value");
		exit(1);
	}
	
	int value;
	sscanf(buf, "%d", &value); // convert value to an int

	return value;
}

int main(int argc, char * argv[]) {
    char buf[MAX_LINE];

    // commmand line arg checking
    if (argc != 4){
        fprintf(stderr, "You need 4 inputs, you provided: %d\n", argc);
        exit(1);
    }

    // load adress structs
    struct addrinfo hints, *clientInfo, *ptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int returnVal;
    if ((returnVal = getaddrinfo(argv[1],argv[2], &hints, &clientInfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", returnVal);
        exit(1);
    }

    cout << "Connecting to " << argv[1] << " on port " << argv[2] << endl;
    for (ptr = clientInfo; ptr != NULL; ptr = ptr->ai_next){
        if ((sockFd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) <0 ){
            perror("bad socket");
            continue;
        }
        if (connect(sockFd, ptr->ai_addr, ptr->ai_addrlen) < 0){
            perror("bad connect");
            close(sockFd);
            continue;
        }
    }
    cout << "Conected to " << argv[1] << endl; 
	
    // handle login
	string username(argv[3]);
    USERNAME = username;
    handle_login(username);

	pthread_t msgThread;
    if (pthread_create(&msgThread, NULL, handle_messages, NULL)){
        perror("create handle_messages thread");
        exit(1);
    }
    promptUser();
    close(sockFd);
}
void displayPrompt(){
    cout << "Please enter a command: P (Public message), D (Direct message), Q (Quit)" << endl;
    cout << "> ";
}

void promptUser(){
    bool quit = false;
    string usrInput;
    cout << "\nSuccessfully Joined Chat" << endl;
    while (!quit){
        displayPrompt();
        cin >> usrInput;
        if (usrInput == "P"){
            publicMessage();
        }
        else if (usrInput == "D"){
            directMessage();
        }
        else if (usrInput == "Q"){
            send_string(usrInput+USERNAME);
            quit = true;
        }
        else {
            cout << "Sorry, we do not understand your slection. Please choose: 'P', 'D', or 'Q'\n" << endl;
        }
    
    }
}
void directMessage(){
    send_string("dirMsg");
    string usrToMsg, msg;
    // prompt comes from other thread
    ABORT_DIR_MSG = false;
    cin >> usrToMsg;
    send_string(usrToMsg);
    if (ABORT_DIR_MSG){
        return;
    }
    cout << "Message: ";
    cin >> msg;
    char *buf = new char[MAX_LINE];
    char * pubKeyCStr = new char[PUB_KEY_FOR_MSG.length() + 1];
    char * msgToSendCStr = new char[msg.length()+1];
    strcpy(pubKeyCStr, PUB_KEY_FOR_MSG.c_str());
    strcpy(msgToSendCStr, msg.c_str());
    buf = encrypt(msgToSendCStr, pubKeyCStr);
    msg = buf;            
    delete [] pubKeyCStr;
    delete [] msgToSendCStr;
    delete [] buf;
    send_string(msg);
    send_string(USERNAME);

}

void publicMessage(){
    send_string("pubMsg");
    // no error: this is the ack from server
    // in other words we are good to transmit if no error from send_string("pubMsg")
    string msg;
    cout << "Enter the public message: ";
    cin >> msg;
    send_string(msg);
    PSEND = false;
    while(!PSEND){
        // spin until we get confirmation 
    }
}


void *handle_messages(void*){
    string message;
    int numBytesRec;
    
    while(1) {
        string message;
        int numBytesRec;
        numBytesRec = recvWithCheck(message);
        if (message.substr(0,2) == "DE"){ // encrypted private
            string frmUser;
            recvWithCheck(frmUser);
            message = message.substr(2);
            char * buf;
            char * msg;
            msg = new char[MAX_LINE];
            buf = new char[MAX_LINE];
            strcpy(msg, message.c_str());
            buf = decrypt(msg);
            message = buf; 
            cout << "\n*** Incoming private message from " << frmUser << " ***: ";
            cout << message << endl;
            cout << "> ";
            fflush(stdout);
        }
        else if (message[0] == 'D'){
            message = message.substr(1);
            cout << "\n*** Incoming public message ***: ";
            cout << message << endl;
            cout << "> ";
            fflush(stdout);
        }

        else if (message[0] == 'C'){  // command message
            if (message.substr(1) == "P:OK"){
                PSEND = true;
            }
            else if (message.substr(1,4) == "usrs"){
                string names = message.substr(5);
                stringstream ss(names);
                char c;
                string name = "";
                while (ss >> c){
                    name = name + c;
                    if (ss.peek() == ';'){
                        onlineUsers.push_back(name);
                        name = "";
                        ss.ignore();
                    }
                }
                cout << "Peers online: " << endl;
                for (auto it = onlineUsers.begin(); it != onlineUsers.end(); it++){
                    if (*it == USERNAME){
                        continue;
                    }
                    cout << " " << *it << endl;;
                }
                cout << endl;
                cout << "Peer to message: ";
                fflush(stdout);
            }
            else if (message.substr(1,3) == "key"){
                //cout << "WE GOT THE KEY" << endl;
                PUB_KEY_FOR_MSG = message.substr(4);
            }
            else if (message.substr(1,3) == "DNE") {
                cout << "User does not exist: Abort" << endl; 
                ABORT_DIR_MSG = true;
            }
            else if (message.substr(1, 2) == "NO"){
                cout << "User is not online, message not sent" << endl;
            }
            else if (message.substr(1,4) == "SENT"){
                // cout << "MESSAGE SENT" << endl;
            }
        }
        else {
            cout << "DONT RECOGNIZE THIS: " << message << endl;
        }                   
    }
}

void handle_login(string username){
    send_string(username);
    string response, password;
    recvWithCheck(response);
    //cout << "server res: " << response << endl;
    if (response == "existUser"){
        cout << "Password: ";
        cin >> password;
        send_string(password);
        recvWithCheck(response);
        while (response != "OK"){
            password = "";
            cout << "Wrong password, please try again" << endl;
            cout << "pass: ";
            cin >> password;
            send_string(password);
            recvWithCheck(response);
        }
    }
    else { // new user
        cout << "Creating new user" << endl;
        cout << "Enter password: ";
        cin >> password;
        send_string(password);
        recvWithCheck(response);
        while (response != "OK"){
            cout << "Sorry, there seemed to be an error. Please try again" << endl;
            cout << "Enter pass: ";
            cin >> password;
            send_string(password);
            recvWithCheck(response);
        }    
    }
    // gen public key and send over
    char *pubKey = getPubKey();
    string pubKeyStr(pubKey);
    send_string(pubKey);
}

int recvWithCheck(string &outMsg) {
    int numBytesRec;
    char buf[MAX_LINE];
    int len = sizeof(buf);
    int flags = 0;

    memset(buf, 0, len);
 
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

void send_string(string toSend) {
    int len = toSend.length();
    if (send(sockFd, toSend.c_str(), len, 0) == -1) {
        perror("Client send error!\n");
        exit(1);
    }
}



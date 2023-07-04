#include "common.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 2048

void usage(int argc, char **argv) {
	printf("usage: %s <server IP> <server port>\n", argv[0]);
	printf("example: %s 127.0.0.1 51511\n", argv[0]);
	exit(EXIT_FAILURE);
}

struct thread_data{
    int csock;
	int* myId;
};

void command_parse(char* message, int s, int* myId){
	//lenght(4bytes)|idMsg(4bytes)|idSender(4bytes)|idReceiver(4bytes)|message(2032bytes)
	//Copy the first 4 bytes to lenght_net
    uint32_t length_net;
    int length;
    memcpy(&length_net, message, 4);
    //Convert length_net to host format
    length = ntohl(length_net);
    
    //Copy the next 4 bytes to idMsg_net
    uint32_t idMsg_net;
    int idMsg;
    memcpy(&idMsg_net, message+4, 4);
    //Convert idMsg_net to host format
    idMsg = ntohl(idMsg_net);

    //Copy the next 4 bytes to idSender_net
    uint32_t idSender_net;
    int idSender;
    memcpy(&idSender_net, message+8, 4);
    //Convert idSender_net to host format
    idSender = ntohl(idSender_net);

    //Copy the next 4 bytes to idReceiver_net
    uint32_t idReceiver_net;
    int idReceiver;
    memcpy(&idReceiver_net, message+12, 4);
    //Convert idReceiver_net to host format
    idReceiver = ntohl(idReceiver_net);

    //Copy the next lenght bytes to message_str
    char message_str[2032];
    memset(message_str, 0, 2032);
    memcpy(message_str, message+16, length);

	if(idMsg == 4){
		//RES_LIST
		printf("%s\n", message_str);
		return;
	}
	else if(idMsg == 6){
		//MSG
		//get hour and minute
		time_t now;
		struct tm *now_tm;
		char hour[3], minute[3];
		time(&now);
		now_tm = localtime(&now);
		sprintf(hour, "%d", now_tm->tm_hour);
		sprintf(minute, "%d", now_tm->tm_min);

		if(idReceiver == -1){
			//broadcast
			if(idSender == *myId){
				printf("[%s:%s]-> all: %s\n", hour, minute, message_str);
			}
			else if(idSender == -1){
				printf("%s\n",message_str);
			}
			else{
				printf("[%s:%s] %d: %s\n", hour, minute, idSender, message_str);
			}
		}
		else{
			//unicast
			if(idSender == *myId){
				printf("P [%s:%s]-> %d: %s\n", hour, minute, idReceiver, message_str);
			}
			else{
				printf("P [%s:%s] %d: %s\n", hour, minute, idSender, message_str);
			}
		}
		return;
	}
	else if(idMsg == 7){
		if(strcmp(message_str, "02") == 0){
			printf("User not found\n");
			return;
		}
		else if(strcmp(message_str, "03") == 0){
			printf("Receiver not found\n");
			return;
		}
		else{
			printf("Error: unexpected error code received from server\n");
			exit(EXIT_SUCCESS);
		}
	}
	else if(idMsg == 8){
		if(strcmp(message_str, "01") == 0){
			printf("Removed Successfully\n");
			exit(EXIT_SUCCESS);
		}
		else{
			printf("Error: unkown error code received from server\n");
			exit(EXIT_FAILURE);
		}
	}
	else{
		printf("Error: unexpected response from server\n");
		exit(EXIT_FAILURE);
	}
}

void* listen_socket(void* data){
	struct thread_data *tdata = (struct thread_data *)data;
	while(1){
		fflush(stdout);
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		int count = 0;
		count = recv(tdata->csock, buf, BUFSZ, 0);
		if(count > 0){
			command_parse(buf, tdata->csock, tdata->myId);
		}
		else{
			return NULL;
		}
	}
}

void parse_opening_server_response(char* message, int s, int* myId){
	//lenght(4bytes)|idMsg(4bytes)|idSender(4bytes)|idReceiver(4bytes)|message(2032bytes)
    uint32_t length_net;
    int length;
    memcpy(&length_net, message, 4);
    length = ntohl(length_net);
    
    uint32_t idMsg_net;
    int idMsg;
    memcpy(&idMsg_net, message+4, 4);
    idMsg = ntohl(idMsg_net);

    char message_str[2032];
    memset(message_str, 0, 2032);
    memcpy(message_str, message+16, length);

	if(idMsg == 4){
		//printf("Connected users: %s\n", message_str);
		int count = 0;
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		count = recv(s, buf, BUFSZ, 0);
		if(count > 0){
			uint32_t length_net;
			int _length;
			memcpy(&length_net, buf, 4);
			_length = ntohl(length_net);

			uint32_t idMsg_net;
			int idMsg;
			memcpy(&idMsg_net, buf+4, 4);
			idMsg = ntohl(idMsg_net);

			char message_str[2032];
			memset(message_str, 0, 2032);
			memcpy(message_str, buf+16, _length);

			//parse message to get userId
			char keyword1[256];
			int userId = -1;
			sscanf(message_str, "%s %d", keyword1, &userId);
			//strcmp keyword1 with "User" to check if it is a valid message
			if(strcmp(keyword1, "User") != 0){
				printf("Error: unexpected response from server\n");
				exit(EXIT_FAILURE);
			}
			*myId = userId; //update myId

			if(idMsg == 6){
				printf("%s\n", message_str);
			}
			else{
				printf("Error: unexpected response from server\n");
				printf("id=%d\n", idMsg);
				exit(EXIT_FAILURE);
			}
		}
	}
	else if(idMsg == 7){
		if(strcmp(message_str, "01") == 0){
			printf("User limit exceeded\n");
			exit(EXIT_SUCCESS);
		}
		else{
			printf("Error: unkown error code received from server\n");
			exit(EXIT_FAILURE);
		}
	}
	else{
		printf("Error: unexpected response from server\n");
		exit(EXIT_FAILURE);
	}

}

void parse_command_to_send(char* command, int s, int* myId){
	/*The client accepts 4 types of commands:
	close connection
	list users
	send to idUser_j {Message}
	send all {Message}
	*/
	char keyword1[256], keyword2[256], message[2032];
	int idReceiver;
	int parsedItems = sscanf(command, "%s %s", keyword1, keyword2);
	//Concatenate the first three words with a space between them.
	char fullKeyword[512];
	sprintf(fullKeyword, "%s %s", keyword1, keyword2);
	if(parsedItems == 2 && strcmp(fullKeyword, "close connection") == 0){
		sendCommand(s, 0, 2, *myId, -1, "");
	}
	else if(parsedItems == 2 && strcmp(fullKeyword, "list users") == 0){
		sendCommand(s, 0, 4, -1, -1, "");
	}
	else if(parsedItems == 2 && strcmp(fullKeyword, "send to") == 0){
		sscanf(command, "%s %s %d %[^\n]", keyword1, keyword2, &idReceiver, message);
		sendCommand(s, strlen(message), 6, *myId, idReceiver, message);
	}
	else if(parsedItems == 2 && strcmp(fullKeyword, "send all") == 0){
		sscanf(command, "%s %s %[^\n]", keyword1, keyword2, message);
		sendCommand(s, strlen(message), 6, *myId, -1, message);
	}
	else{
		printf("Error: unkown command\n");
	}
}

int main(int argc, char **argv) {
	if (argc < 3) {
		usage(argc, argv);
	}

	struct sockaddr_storage storage;
	if (0 != addrparse(argv[1], argv[2], &storage)) {
		usage(argc, argv);
	}

	int s;
	s = socket(storage.ss_family, SOCK_STREAM, 0);
	if (s == -1) {
		logexit("socket");
	}
	struct sockaddr *addr = (struct sockaddr *)(&storage);
	if (0 != connect(s, addr, sizeof(storage))) {
		logexit("connect");
	}
	// When the client starts its execution, it sends a REQ_ADD to the server.
	sendCommand(s, 0, 1, -1, -1, "");
	// Awaits for the server response.
	// If the server response is RES_LIST, ok
	// Elseif the server response is RES_ERROR, exit program
	// Else, error: unexpected response from server

	char buf[BUFSZ];
	memset(buf, 0, BUFSZ);
	int count = 0;
	int myId = -1;
	count = recv(s, buf, BUFSZ, 0);
	if(count > 0){
		parse_opening_server_response(buf, s, &myId);
	}
	//Now the client is connected to the server.
	//We want two threads: one to send messages and one to receive messages.

	//Create thread to listen to server
	struct thread_data *tdata = malloc(sizeof(*tdata)); 
	tdata->csock = s;
	tdata->myId = &myId;

	pthread_t tid;
    int result = pthread_create(&tid, NULL, listen_socket, tdata);
	if (result != 0) {
    	perror("Could not create thread");
    	exit(EXIT_FAILURE);
	}
	pthread_detach(tid);

	//Now we want to send messages to the server from the keyboard
	while(1){
		fflush(stdout);
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);
		fgets(buf, BUFSZ, stdin);
		parse_command_to_send(buf, s, &myId);
		fflush(stdout);
	}
}
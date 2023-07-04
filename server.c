#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 2048

void usage(int argc, char **argv){
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data{
    int* clients_connected;
    int csock;
    struct sockaddr_storage storage;
    int* clients_socks;
};

void error(int csock, int idReceiver, char* code){
    sendCommand(csock, sizeof(code), 7, -1, idReceiver, code);
}

void msg(int csock, int* clients_connected, int* clients_socks, int idSender, int idReceiver, char* message_str){
    if(idReceiver == -1){
        //send message to all users
        for(int i = 1; i < 16; i++){
            if(clients_socks[i] != 0){
                sendCommand(clients_socks[i], sizeof(message_str)+16, 6, idSender, -1, message_str);
            }
        }
    }
    else{
        //send message to idReceiver and to idSender
        if(clients_socks[idReceiver] != 0){
            //send message to idSender
            sendCommand(csock, sizeof(message_str)+16, 6, idSender, idReceiver, message_str);
            //send message to idReceived
            sendCommand(clients_socks[idReceiver], sizeof(message_str)+16, 6, idSender, idReceiver, message_str);
        }
        else{
            printf("User %d not found\n", idReceiver);
            //send error message
            error(csock,idSender,"03");
        }
    }

}

void res_list(int csock, int* clients_connected, int* clients_socks){
    //send list of users using sendCommand, separated by commas
    char message[2032];
    memset(message, 0, 2032);
    int listed = 0;
    for(int i = 0; i < 15; i++){
        if(clients_socks[i] != 0){
            char idUser[3];
            sprintf(idUser, "%d", i);
            strcat(message, idUser);
            if(listed < (*clients_connected) - 1){
                strcat(message, ",");
            }
            listed++;
        }
    }
    sendCommand(csock, sizeof(message), 4, -1, -1, message);
}

void ok(int csock, int idReceiver){
    //send OK message
    char* message = "01";
    sendCommand(csock, sizeof(message), 8, -1, idReceiver, message);
}

void req_rem(int csock, int* clients_connected, int* clients_socks, int idSender){
    //remove client
    clients_socks[idSender] = 0;
    *clients_connected = *clients_connected - 1;
    printf("User %d removed\n",idSender);
    ok(csock, idSender);
    char message[512];
    sprintf(message,"User %d left the group!", idSender);
    msg(csock, clients_connected, clients_socks, -1, -1, message);
}


int req_add(int csock, int* clients_connected, int* clients_socks){
    //returns 1 if successfull, 0 otherwise
    if(*clients_connected >= 15){
        //send error message
        error(csock, -1,"01");
        return 0;
    }
    else{
        //find what position is available in the clients array, add the new client and get userId
        int userId = -1;
        for(int i = 1; i < 16; i++){
            if(clients_socks[i] == 0){
                clients_socks[i] = csock;
                userId = i;
                break;
            }
        }
        clients_connected = clients_connected + 1;
        printf("User %d added\n",userId);
        //message = "User %d joined the group!"
        char message[2032];
        memset(message, 0, 2032);
        sprintf(message, "User %d joined the group!", userId);
        res_list(csock, clients_connected, clients_socks);
        //send message to all users
        msg(csock, clients_connected, clients_socks, -1, -1, message);
        return 1;
    }
}

void command_parse(char* message, int csock, int* clients_connected, int* clients_socks){
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
    //printf("%d|%d|%d|%d|%s\n",length,idMsg,idSender,idReceiver,message_str);

    /*
    -Control commands:
    REQ_ADD:
        IdMsg = 1
        Description = Mensagem de requisição de inclusão de usuário na rede.
    RED_REM:
        IdMsg = 2
        IdSender = IdUser
        Description = Mensagem de requisição de saída de um usuário da rede,
        onde IdUser, corresponde a identificação do usuário solicitante.
    RES_LIST:
        IdMsg = 4
        Message = IdUserj,IdUser_k,…
        Description: Mensagem com lista de identificação de
            usuários, onde IdUser_j, IdUser_k,… correspondem aos identificadores dos 
            usuários transmitidos na mensagem.
    -Communication commands:
    MSG:
        IdMsg = 6
        IdSender = IdUser_i
        IdReceiver = IdUser_j
        Message = data
        Description: Mensagem transmitida em broadcast para todos osintegrantes da rede.
        O campo IdSender é obrigatório, sendo utilizado nas ocasiões de adição de usuário
        ao grupo e envio de mensagens para outros usuários. O campo IdReceiver é opcional,
        sendo utilizado somente na ocasião de uma mensagem privada ser enviada.
    ERROR:
        IdMsg = 7
        IdReceiver = IdUser_j
        Message = Code
            01: "User limit exceeded"
            02: "User not found"
            03: "Receiver not found"
        Description = Mensagem de erro transmitida do Servidor para o usuário IdUser_j.
    OK:
        IdMsg = 8
        IdReceiver = IdUser_j
        Message = Code
            01: "Removed Successfully"
        Description = Mensagem de confirmação transmitida do Servidor para o usuário IdUser_j.
    */

   switch(idMsg){
        case 1:
            //REQ_ADD
            int added = req_add(csock, clients_connected, clients_socks);
            //destroy thread if not added
            if(!added){
                pthread_exit(EXIT_SUCCESS);
            }
            break;
        case 2:
            //REQ_REM
            req_rem(csock, clients_connected, clients_socks, idSender);
            pthread_exit(EXIT_SUCCESS);
            break;
        case 4:
            //RES_LIST
            res_list(csock, clients_connected, clients_socks);
            break;
        case 6:
            //MSG
            msg(csock, clients_connected, clients_socks, idSender, idReceiver, message_str);
            //send message to idReceiver or to all users if idReceiver = -1
            break;
        default:
            printf("Invalid IdMsg\n");
            break;
    }
}

void* client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    int count = 0;
    while(1){
        memset(buf, 0 , BUFSZ);
        count = recv(cdata->csock, buf, BUFSZ, 0);
        if(count > 0){
            command_parse(buf, cdata->csock, cdata->clients_connected, cdata->clients_socks);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(s, 20)) {
        logexit("listen");
    }

    int clients_connected = 0;
    int clients_socks[16];
    //initialize clients array
    for(int i = 0; i < 16; i++){
        clients_socks[i] = 0;
    }

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        struct client_data *cdata = malloc(sizeof(*cdata));
        if (!cdata) {
            logexit("malloc");
        }
        cdata->clients_connected = &clients_connected;
        cdata->csock = csock;
        cdata->clients_socks = clients_socks;

        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));
        //memcpy(&(cdata->clients_socks), &clients_socks, sizeof(clients_socks));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);
    }

    exit(EXIT_SUCCESS);
}

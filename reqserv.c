#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <stdbool.h>

#define max(A,B) ((A)>=(B)?(A):(B))

#define BUFFER_SIZE 128
#define DEFAULT_PORT 59000

int communicateUDP(int fd, struct sockaddr_in addr, char *message, char *reply);
int checkServerReply(char *reply, int *id, char *ip, unsigned *port);

int main (int argc, char * argv[]) {
    int id_DS;
    char ip_DS[20];
    unsigned port_DS;

    int i, DS_ServerLength, centralServerLength, serviceReqX, serviceServerID, bytesReceived;
    int centralServerSocket, DS_Socket, maxfd, counter;
    fd_set rfds;
    unsigned centralServerPort = DEFAULT_PORT, serviceUdpPort, serviceTcpPort;
    char centralServerIP[20];
    char message[BUFFER_SIZE], reply[BUFFER_SIZE], buffer[BUFFER_SIZE];
    struct sockaddr_in centralServer, DS_Server;
    struct hostent *host = NULL;
    bool isDefaultServer = true;
    enum {busy, idle} state;

    if(argc < 1 || argc > 5) {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    // Argumentos de entrada
    centralServerIP[0] = '\0';
    for(i = 1; i < argc; i = i+2) {
        if(!strcmp("-i", argv[i]) && i+1 < argc) {
            sprintf(centralServerIP, "%s", argv[i+1]);
            isDefaultServer = false;
        } else if(!strcmp("-p", argv[i]) && i+1 < argc) {
            centralServerPort = atoi(argv[i+1]);
            isDefaultServer = false;
        } else {
            printf("Invalid type of arguments\n");
            exit(-1);
        }
    }

    // Inicialização das sockets UDP
    centralServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(centralServerSocket == -1) {
        exit(-1);
    }
    DS_Socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(DS_Socket == -1) {
        exit(-1);
    }

    // Cliente UDP para pedidos ao servidor central
    memset((void*)&centralServer,(int)'\0', sizeof(centralServer));
    centralServer.sin_family = AF_INET;
    if(isDefaultServer || centralServerIP[0] == '\0') { 
        if((host = gethostbyname("tejo.tecnico.ulisboa.pt")) == NULL) {
            exit(-1);
        }
		centralServer.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;
    } else {
        inet_aton(centralServerIP, &centralServer.sin_addr);
    }
    centralServer.sin_port = htons((u_short)centralServerPort);
    centralServerLength = sizeof(centralServer);

    //Informação do servidor central
    if(isDefaultServer) {
        printf("-> Default central Server\n");
    } else {
        printf("-> Custom central Server\n");
    }
    printf("Central Server IP   : %s \n", inet_ntoa(centralServer.sin_addr));
    printf("Central Server port : %d \n", ntohs(centralServer.sin_port));
    printf("\nType 'help' for valid commands\n");

    state = idle;
    while(1) {
        FD_ZERO(&rfds);
        FD_SET(fileno(stdin), &rfds); maxfd = fileno(stdin) ;
        FD_SET(DS_Socket, &rfds); maxfd = max(maxfd, DS_Socket);

        counter = select(maxfd+1,&rfds, (fd_set*)NULL,(fd_set*)NULL, (struct timeval *)NULL);
        if(counter <= 0) {
            exit(1);
        }

        if(FD_ISSET(fileno(stdin),&rfds)){
            fgets(buffer, sizeof(buffer), stdin);
            sscanf(buffer,"%[^\n]s", message);

            if(sscanf(message, "request_service %d", &serviceReqX) == 1 || sscanf(message, "rs %d", &serviceReqX) == 1) {
                switch(state){
                    case idle:
                        sprintf(message,"GET_DS_SERVER %d", serviceReqX);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                            switch (checkServerReply(reply, &id_DS, ip_DS, &port_DS)) {
                                case 0:
                                    printf("\tNo DS Server!\n");
                                    break;
                                case 1:
                                    // Cliente UDP para comunicar com o serviço
                                    memset((void*)&DS_Server,(int)'\0', sizeof(DS_Server));
                                    DS_Server.sin_family = AF_INET;
                                    inet_aton(ip_DS, &DS_Server.sin_addr);
                                    DS_Server.sin_port = htons((u_short)port_DS);
                                    DS_ServerLength = sizeof(DS_Server);
                                    communicateUDP(DS_Socket, DS_Server, "MY SERVICE ON", reply);
                                    state = busy;
                                    break;
                                default:
                                    break;
                            }
                        break;    
                    case busy:
                        printf("\tYou are connected to a service! Terminate service and try again!\n"); 
                        break;
                    default:
                        break;       
                }
            }
            else if(!strcmp("terminate_service",message)|| !strcmp("ts",message)) {
                switch(state){
                    case idle:
                        printf("\tYou are not connected to a dispatch server!\n");
                        break;
                    case busy:
                        communicateUDP(DS_Socket, DS_Server, "MY SERVICE OFF", reply);
                        state = idle;
                        break;
                    default:
                        break;
                }
            }
            else if(!strcmp("exit",message)) {
                switch(state){
                        case busy:
                            communicateUDP(DS_Socket, DS_Server, "MY SERVICE OFF", reply);
                            state = idle;
                            break;
                        default:
                            break;
                }
                break;
            }
            else if(!strcmp("help",message)) {
                printf("\nValid commands: \n");
                printf("-> request_service 'X' or rs 'X'  \n");
                printf("-> terminate_service or ts\n");
                printf("-> help \n");
                printf("-> exit \n");  
            }
            else {
                printf("Invalid Command!\n Type 'help' for valid commands\n");
            }
        }
    }
    close(centralServerSocket);
    close(DS_Socket);

    return 0; 
}

// Função de envio e receção de mensagens do tipo UDP ( para cada pedido existe sempre uma resposta)
int communicateUDP(int fd, struct sockaddr_in addr, char *message, char *reply) {
    int bytes, counter;
    fd_set rfds;
    struct timeval tv;
    unsigned length = sizeof(addr);
    printf("\tServer request: %s\n", message);
    bytes = sendto(fd, message,strlen(message), 0, (struct sockaddr*)&addr, length);
    if (bytes == -1) {
        exit(-1);
    }

    // Timeouts 
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    counter = select(fd+1,&rfds, (fd_set*)NULL,(fd_set*)NULL, &tv);
    if(counter < 0) {
        exit(1);
    } 
    else if (counter == 0) {
        printf("\ttimeout: udp server is down\n"); 
        FD_CLR(fd, &rfds);
        exit(1);
    }

    bytes = recvfrom(fd, reply, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);   
    if (bytes == -1){
        exit(-1);
    }
    reply[bytes] = '\0';
    printf("\tServer reply: %s\n", reply);    
    return 1;

}

// Função para analisar a respostas do servidor central
int checkServerReply(char *reply, int *id, char *ip, unsigned *port) {
    sscanf(reply, "OK %d;%[^;];%d", id, ip, port);
    // Resposta de erro (id < 0) ou de nao existencia do serviço (id = 0) 
    if(*id <= 0) {
        return 0;
    } 
    // Resposta com informação sobre um serviço (id > 0)
    else {
        return 1;
    } 
}
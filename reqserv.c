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

void communicateUDP(int socket, struct sockaddr_in addr, char *message, char *reply);
int checkServerReply(char *reply, int *id, char *ip, unsigned *port);

int main (int argc, char * argv[]) {
    int id_DS;
    char ip_DS[20];
    unsigned port_DS;

    int i, DS_ServerLength, centralServerLength, serviceReqX, serviceServerID, bytesReceived;
    int centralServerSocket, DS_Socket, maxfd, counter;
    fd_set rfds;
    unsigned centralServerPort = DEFAULT_PORT, serviceUdpPort, serviceTcpPort;
    char *centralServerIP = NULL, *serviceServerIP = NULL;
    char message[BUFFER_SIZE], reply[BUFFER_SIZE], buffer[BUFFER_SIZE];
    struct sockaddr_in centralServer, DS_Server;
    struct hostent *host = NULL;
    bool isDefaultServer = true;
    enum {busy, idle} state;

    
    if(argc < 1 || argc > 5) {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    // Arguments stuff
    for(i = 1; i < argc; i = i+2) {
        if(!strcmp("-i", argv[i]) && i+1 < argc) {
            centralServerIP = (char*) malloc(sizeof(argv[i+1]+1));
            strcpy(centralServerIP,argv[i+1]);
            isDefaultServer = false;
        } else if(!strcmp("-p", argv[i]) && i+1 < argc) {
            centralServerPort = atoi(argv[i+1]);
            isDefaultServer = false;
        } else {
            printf("Invalid type of arguments\n");
            exit(-1);
        }
    }



    centralServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(centralServerSocket == -1) {
        exit(-1);
    }
    DS_Socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(DS_Socket == -1) {
        exit(-1);
    }

    // UDP client for central server requests
    memset((void*)&centralServer,(int)'\0', sizeof(centralServer));
    centralServer.sin_family = AF_INET;
    if(isDefaultServer || centralServerIP == NULL) { 
        if((host = gethostbyname("tejo.tecnico.ulisboa.pt")) == NULL) {
            exit(-1);
        }
		centralServer.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;
    } else {
        inet_aton(centralServerIP, &centralServer.sin_addr);
    }
    centralServer.sin_port = htons((u_short)centralServerPort);
    centralServerLength = sizeof(centralServer);






    // Central server and service information
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


        counter=select(maxfd+1,&rfds, (fd_set*)NULL,(fd_set*)NULL,(struct timeval *)NULL);
        if(counter<=0)exit(1);//error

        if(FD_ISSET(fileno(stdin),&rfds)){
            fgets(buffer, sizeof(buffer), stdin);
            sscanf(buffer,"%[^\n]s", message);

            if(sscanf(message, "request_service %d", &serviceReqX) == 1 || sscanf(message, "rs %d", &serviceReqX) == 1) {
                sprintf(message,"GET_DS_SERVER %d", serviceReqX);
                communicateUDP(centralServerSocket, centralServer, message, reply);

                switch (checkServerReply(reply, &id_DS, ip_DS, &port_DS)) {
                    case 0:
                        printf("No DS Server!");
                        break;

                    case 1:
                        memset((void*)&DS_Server,(int)'\0', sizeof(DS_Server));
                        DS_Server.sin_family = AF_INET;
                        inet_aton(ip_DS, &DS_Server.sin_addr);
                        DS_Server.sin_port = htons((u_short)port_DS);
                        DS_ServerLength = sizeof(DS_Server);
                        communicateUDP(DS_Socket, DS_Server, "MY SERVICE ON", reply);
                        break;

                    default:
                        break;
                }
            }

            
            else if(!strcmp("terminate_service",message)) {
            
            }
            else if(!strcmp("exit",message)) {
                break;
            }
            else if(!strcmp("help",message)) {
                printf("\nValid commands: \n");
                printf("-> request_service 'X' or rs 'X'  \n");
                printf("-> terminate_service or ts\n");
                printf("-> help \n");
                printf("-> exit \n");  
            }
            else{
                printf("Invalid Command!\n Type 'help' for valid commands\n");
            }
        }


        if(FD_ISSET(DS_Socket,&rfds)){

        }






    }

    if(centralServerIP != NULL) {
        free(centralServerIP);
    }
    if(serviceServerIP != NULL) {
        free(serviceServerIP);
    }
    close(centralServerSocket);
    close(DS_Socket);

    return 0; 
}

void communicateUDP(int socket, struct sockaddr_in addr, char *message, char *reply) {
    int bytesReceived;
    unsigned length = sizeof(addr);
    printf("\tServer request: %s\n", message);
    sendto(socket, message,strlen(message), 0, (struct sockaddr*)&addr, length);
    bytesReceived = recvfrom(socket, reply, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);   
    reply[bytesReceived] = '\0';
    printf("\tServer reply: %s\n", reply);      
    return;
}

int checkServerReply(char *reply, int *id, char *ip, unsigned *port) {

    sscanf(reply, "OK %d;%[^;];%d", id, ip, port);
    if(id <= 0) {
        return 0;
    } 
    else {
        return 1;
    } 
}


//sendto = bytes_sent
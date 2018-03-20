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

#define BUFFER_SIZE 128
#define DEFAULT_PORT 59000
#define max(A,B) ((A)>=(B)?(A):(B))

void communicateUDP(int socket, struct sockaddr_in addr, char *message, char *reply);
int checkServerReply(char *reply, int *id1, int *id2, char *ip, unsigned *port);

int main (int argc, char * argv[]) {
    int i, serviceX, serviceServerID, bytesReceived;
    int centralServerSocket, UDPServerSocket, TCPServerSocket, TCPClientSocket;
    int replyID1, replyID2;
    int length;
    unsigned centralServerPort = DEFAULT_PORT, serviceUdpPort, serviceTcpPort, centralServerLength, replyPort;
    char *centralServerIP = NULL, *serviceServerIP = NULL;
    char message[BUFFER_SIZE], reply[BUFFER_SIZE], buffer[BUFFER_SIZE], replyIP[BUFFER_SIZE];
    struct sockaddr_in centralServer, UDPServer, TCPServer, TCPClient;
    struct hostent *host = NULL;
    bool isDefaultServer = true, isStartServer = false, isDSServer = false;
    fd_set rfds;
    int maxfd, counter;
    
    if(argc < 9 || argc > 13) {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    // Arguments stuff
    for(i = 1; i < argc; i = i+2) {
        if(!strcmp("-n", argv[i]) && i+1 < argc) {
            serviceServerID = atoi(argv[i+1]);
        } 
        else if(!strcmp("-j", argv[i]) && i+1 < argc) {
            serviceServerIP = (char*) malloc(sizeof(argv[i+1]+1));
            strcpy(serviceServerIP,argv[i+1]);
        } 
        else if(!strcmp("-u", argv[i]) && i+1 < argc) {
            serviceUdpPort = atoi(argv[i+1]);
        } 
        else if(!strcmp("-t", argv[i]) && i+1 < argc) {
            serviceTcpPort = atoi(argv[i+1]);
        } 
        else if(!strcmp("-i", argv[i]) && i+1 < argc) {
            centralServerIP = (char*) malloc(sizeof(argv[i+1]+1));
            strcpy(centralServerIP,argv[i+1]);
            isDefaultServer = false;
        } 
        else if(!strcmp("-p", argv[i]) && i+1 < argc) {
            centralServerPort = atoi(argv[i+1]);
            isDefaultServer = false;
        } 
        else {
            printf("Invalid type of arguments\n");
            exit(-1);
        }
    }

    centralServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(centralServerSocket == -1) {
        exit(-1);
    }
    UDPServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(UDPServerSocket == -1) {
        exit(-1);
    }
    TCPServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(TCPServerSocket == -1) {
        exit(-1);
    }
    TCPClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(TCPClientSocket == -1) {
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
    } 
    else {
        inet_aton(centralServerIP, &centralServer.sin_addr);
    }
    centralServer.sin_port = htons((u_short)centralServerPort);
    centralServerLength = sizeof(centralServer);

    // UDP Server for reqserv clients requests
    memset((void*)&UDPServer,(int)'\0', sizeof(UDPServer));
    UDPServer.sin_family = AF_INET;
    UDPServer.sin_addr.s_addr = htonl(INADDR_ANY);
    UDPServer.sin_port = htons((u_short)serviceUdpPort);

    if(bind(UDPServerSocket, (struct sockaddr *)&UDPServer, sizeof(UDPServer)) == -1) {
        exit(-1);
    }

    // TCP Server
    memset((void*)&TCPServer,(int)'\0', sizeof(TCPServer));
    TCPServer.sin_family = AF_INET;
    TCPServer.sin_addr.s_addr = htonl(INADDR_ANY);
    TCPServer.sin_port = htons((u_short)serviceTcpPort);

    if(bind(TCPServerSocket, (struct sockaddr *)&TCPServer, sizeof(TCPServer)) == -1) {
        exit(-1);
    }

    if(listen(TCPServerSocket, 5) == -1) {
        exit(-1);
                        } 

    // Central server and service information
    if(isDefaultServer) {
        printf("-> Default central Server\n");
    } 
    else {
        printf("-> Custom central Server\n");
    }
    printf("Central Server IP   : %s \n", inet_ntoa(centralServer.sin_addr));
    printf("Central Server port : %d \n", ntohs(centralServer.sin_port));
    printf("Service IP          : %s \n", serviceServerIP);
    printf("Service ID          : %d \n", serviceServerID);   
    printf("Service udp port    : %d \n", serviceUdpPort);
    printf("Service tcp port    : %d \n", serviceTcpPort);
    printf("\nType 'help' for valid commands\n");

    while(1) {
        FD_ZERO(&rfds);
		FD_SET(fileno(stdin), &rfds);
        FD_SET(UDPServerSocket, &rfds);

        maxfd = max(fileno(stdin), UDPServerSocket);

        counter = select(maxfd+1, &rfds, (fd_set*)NULL, (fd_set*)NULL, (struct timeval *)NULL);
		if(counter <= 0) {
            exit(-1);
        }
    
        if(FD_ISSET(fileno(stdin), &rfds)) {
            fgets(buffer, sizeof(buffer), stdin);
            sscanf(buffer,"%[^\n]s", message);

            if(sscanf(message, "join %d", &serviceX) == 1) {
                sprintf(message,"GET_START %d;%d", serviceX, serviceServerID);
                communicateUDP(centralServerSocket, centralServer, message, reply);
                
                switch (checkServerReply(reply, &replyID1, &replyID2, replyIP, &replyPort)) {
                    case 0:
                        printf("Server cannot handle this request\n");
                        break;

                    case 1:
                        printf("\tNew Start Server\n");             
                        sprintf(message,"SET_START %d;%d;%s;%d", serviceX, serviceServerID, serviceServerIP, serviceTcpPort);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        sprintf(message,"SET_DS %d;%d;%s;%d", serviceX, serviceServerID, serviceServerIP, serviceUdpPort);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        isDSServer = true;
                        isStartServer = true;
                        break;

                    case 2:
                        printf("\tThere is a start Server\n");
                        if(serviceServerID == replyID2) {
                            printf("\tID: %d already has one service\n", serviceServerID);
                        }

                        // TCP Client
                        memset((void*)&TCPClient,(int)'\0', sizeof(TCPClient));
                        TCPClient.sin_family = AF_INET;
                        inet_aton(replyIP, &TCPClient.sin_addr);
                        TCPClient.sin_port = htons((u_short)replyPort);

                        if(connect(TCPClientSocket, (struct sockaddr *)&TCPClient, sizeof(TCPClient)) == -1) {
                            exit(-1);
                        } 
                        sprintf(message,"NEW %d;%s;%d\n", serviceServerID, serviceServerIP, serviceTcpPort);
                        printf("\t%s", message);
                       // write();
                        
                        break;
                    
                    default:
                        break;
                }
            } 
            else if(!strcmp("show_state", message)) {

            }
            else if(!strcmp("leave", message)) {
                if(isDSServer) {
                    sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isDSServer = false;
                }                

                if(isStartServer) {
                    sprintf(message,"WITHDRAW_START %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isStartServer = false;
                }
            }
            else if(!strcmp("exit", message)) {
                if(isDSServer) {
                    sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isDSServer = false;
                }                

                if(isStartServer) {
                    sprintf(message,"WITHDRAW_START %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isStartServer = false;
                }
                break;
            } 
            else if(!strcmp("help", message)) {
            printf("Valid commands: \n");
            printf("-> join x \n");
            printf("-> show_state \n");
            printf("-> leave \n");
            printf("-> exit \n");  
            } 
            else {
                printf("Invalid command! Type 'help'\n");
            }
        }

        if(FD_ISSET(UDPServerSocket, &rfds)) {
            length = sizeof(UDPServer);
            bytesReceived = recvfrom(UDPServerSocket, reply, BUFFER_SIZE, 0,(struct sockaddr*)&UDPServer, &length);  
            reply[bytesReceived] = '\0';
            printf("\t%s\n", reply);

            if(!strcmp("MY SERVICE ON", reply)) {
                sprintf(buffer, "YOUR SERVICE ON");
                if(isDSServer) {
                    sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isDSServer = false;
                }                

                if(isStartServer) {
                    sprintf(message,"WITHDRAW_START %d;%d", serviceX, serviceServerID);
                    communicateUDP(centralServerSocket, centralServer, message, reply);
                    isStartServer = false;
                }
            } 
            else if(!strcmp("MY SERVICE OFF", reply)) {
                sprintf(buffer, "YOUR SERVICE OFF");
            }
            printf("\t%s\n", buffer);
            sendto(UDPServerSocket, buffer, strlen(buffer)+1, 0, (struct sockaddr*)&UDPServer, sizeof(UDPServer));
        }
    }

    if(centralServerIP != NULL) {
        free(centralServerIP);
    }
    if(serviceServerIP != NULL) {
        free(serviceServerIP);
    }

    close(centralServerSocket);
    close(UDPServerSocket);
    close(TCPServerSocket);
    close(TCPClientSocket);

    return 0; 
}

void communicateUDP(int socket, struct sockaddr_in addr, char *message, char *reply) {
    int bytes;
    int length = sizeof(addr);
    
    printf("\tRequest: %s\n", message);
    bytes = sendto(socket, message, strlen(message)+1, 0, (struct sockaddr*)&addr, length);
    if(bytes == -1) {
        exit(-1);
    }
    bytes = recvfrom(socket, reply, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);   
    if(bytes == -1) {
        exit(-1);
    }
    reply[bytes] = '\0';
    printf("\tReply: %s\n", reply);      
    return;
}

int checkServerReply(char *reply, int *id1, int *id2, char *ip, unsigned *port) {
    sscanf(reply, "OK %d;%d;%[^;];%d", id1, id2, ip, port);
    if(*id1 <= 0) {
        return 0;
    } 
    else if(*id1 != 0 && *id2 == 0 && (!strcmp(ip, "0.0.0.0")) && *port == 0) {
        return 1;
    } 
    else {
        return 2;
    }
}

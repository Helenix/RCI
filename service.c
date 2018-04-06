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

#define BUFFER_SIZE 512
#define DEFAULT_PORT 59000
#define max(A,B) ((A)>=(B)?(A):(B))


void communicateUDP(int fd, struct sockaddr_in addr, char *message, char *reply);
int checkServerReply(char *reply, int *id1, int *id2, char *ip, unsigned *port);
void writeTCP(int socket, char *message);
void clearSuccessors(int *id, unsigned *port, char *ip);
char checkToken(char *message);
char* checkServiceState(int state);
char* checkRingState(bool availability);
int countEndLines(char *buffer, int endLines);
char* divideBuffer(char *pointerPosition, char *divBuffer);

int main (int argc, char * argv[]) {
    int i, serviceX, serviceServerID, bytes;
    int centralServerSocket, UDPServerSocket, TCPServerSocket, TCPClientSocket, newfd, afd;
    int replyID1, replyID2;
    int successorID;
    int maxfd, counter;
    unsigned length, TCPServerLength, centralServerLength;
    unsigned centralServerPort = DEFAULT_PORT, serviceUdpPort, serviceTcpPort, replyPort, successorPort;
    char centralServerIP[20], serviceServerIP[20], successorIP[20];
    char message[BUFFER_SIZE], reply[BUFFER_SIZE], buffer[BUFFER_SIZE], replyIP[BUFFER_SIZE];
    char *ptr, buffer_write[BUFFER_SIZE], buffer_read[BUFFER_SIZE];
    struct sockaddr_in centralServer, UDPServer, TCPServer, TCPClient;
    struct hostent *host = NULL;
    bool isDefaultServer = true, isStartServer = false, isDSServer = false, ringAvailable = true, leaveFlag = false, exitFlag = false, 
        exitStep1 = false, exitStep2 = false;
    fd_set rfds;
    enum {idle, busy} stateClient, stateServer;
    enum {on, off} serviceState;
    char tokenType;

    int endLines = 0;
    char *pointerPosition;
    char divBuffer[BUFFER_SIZE];

    if(argc < 9 || argc > 13) {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    // Argumentos de entrada
    centralServerIP[0] = '\0';
    for(i = 1; i < argc; i = i+2) {
        if(!strcmp("-n", argv[i]) && i+1 < argc) {
            serviceServerID = atoi(argv[i+1]);
        } 
        else if(!strcmp("-j", argv[i]) && i+1 < argc) {
            sprintf(serviceServerIP, "%s", argv[i+1]);
        } 
        else if(!strcmp("-u", argv[i]) && i+1 < argc) {
            serviceUdpPort = atoi(argv[i+1]);
        } 
        else if(!strcmp("-t", argv[i]) && i+1 < argc) {
            serviceTcpPort = atoi(argv[i+1]);
        } 
        else if(!strcmp("-i", argv[i]) && i+1 < argc) {
            sprintf(centralServerIP, "%s", argv[i+1]);
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

    // Inicialização das sockets UDP e TCP
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

    // Cliente UDP para pedidos ao servidor central
    memset((void*)&centralServer,(int)'\0', sizeof(centralServer));
    centralServer.sin_family = AF_INET;
    if(isDefaultServer || centralServerIP[0] == '\0') { 
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

    // Servidor UDP para pedidos da aplicação reqserv
    memset((void*)&UDPServer,(int)'\0', sizeof(UDPServer));
    UDPServer.sin_family = AF_INET;
    UDPServer.sin_addr.s_addr = htonl(INADDR_ANY);
    UDPServer.sin_port = htons((u_short)serviceUdpPort);

    if(bind(UDPServerSocket, (struct sockaddr *)&UDPServer, sizeof(UDPServer)) == -1) {
        exit(-1);
    }

    // Servidor TCP para um serviço do anel
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

    // Informação do servidor central e do serviço
    if(isDefaultServer) {
        printf("-> Default central Server\n");
    } 
    else {
        printf("-> Custom central Server\n");
    }
    printf("Central Server IP   : %s \n", inet_ntoa(centralServer.sin_addr));
    printf("Central Server port : %d \n", ntohs(centralServer.sin_port));
    printf("Service IP          : %s \n", serviceServerIP);
    printf("Server ID           : %d \n", serviceServerID);   
    printf("Service udp port    : %d \n", serviceUdpPort);
    printf("Service tcp port    : %d \n", serviceTcpPort);
    printf("\nType 'help' for valid commands\n");

    stateServer = idle;
    stateClient = idle;
    serviceState = off;
    clearSuccessors(&successorID, &successorPort, successorIP);

    while(!exitStep2) {
        FD_ZERO(&rfds);
		FD_SET(fileno(stdin), &rfds);
        FD_SET(UDPServerSocket, &rfds); maxfd = max(fileno(stdin), UDPServerSocket);
        FD_SET(TCPServerSocket, &rfds); maxfd = max(maxfd, TCPServerSocket); 
        if(stateServer == busy || exitStep1) {
            FD_SET(afd, &rfds); 
            maxfd = max(maxfd,afd);
        }
        if(stateClient == busy) {
            FD_SET(TCPClientSocket, &rfds); 
            maxfd = max(maxfd, TCPClientSocket);
        }    

        counter = select(maxfd+1, &rfds, (fd_set*)NULL, (fd_set*)NULL, (struct timeval *)NULL);
		if(counter <= 0) {
            exit(-1);
        }
    
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if(FD_ISSET(fileno(stdin), &rfds)) {
            memset(message, 0, BUFFER_SIZE);
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
                        leaveFlag = false;
                       
                        break;

                    case 2:
                        printf("\tThere is a start Server\n");
                        if(serviceServerID == replyID2) {
                            printf("\tServer with ID %d already has one service\n", serviceServerID);
                            break;
                        }

                        TCPClientSocket = socket(AF_INET, SOCK_STREAM, 0);
                        if(TCPClientSocket == -1) {
                            exit(-1);
                        }

                        // Cliente TCP para um serviço do anel
                        memset((void*)&TCPClient,(int)'\0', sizeof(TCPClient));
                        TCPClient.sin_family = AF_INET;
                        inet_aton(replyIP, &TCPClient.sin_addr);
                        TCPClient.sin_port = htons((u_short)replyPort);

                        if(connect(TCPClientSocket, (struct sockaddr *)&TCPClient, sizeof(TCPClient)) == -1) {
                            printf("\tCould not connect\n");
                            break;   
                        } 
                        memset(buffer_write, 0, BUFFER_SIZE);
                        sprintf(buffer_write,"NEW %d;%s;%d\n", serviceServerID, serviceServerIP, serviceTcpPort);
                        writeTCP(TCPClientSocket, buffer_write); 

                        successorID = replyID2;
                        successorPort =  replyPort;
                        sprintf(successorIP, "%s", replyIP);
                        stateClient = busy;    
                        leaveFlag = false;
                                    
                        break;
                    
                    default:
                        break;
                }
            } 
            else if(!strcmp("show_state", message) || !strcmp("ss", message)) {
                printf("\tService state    : %s\n", checkServiceState((int)serviceState));
                printf("\tRing availability: %s\n", checkRingState(ringAvailable));
                if(successorID == 0) {
                    printf("\tSucessor ID      : No successor\n");
                }
                else {
                    printf("\tSucessor ID      : %d\n", successorID);
                }
            }
            else if(!strcmp("leave", message)) {
                if(serviceState == on) {
                    printf("\tCannot leave while provinding a service\n");
                }
                else {
                    // Caso o serviço a remover seja servidor de arranque
                    if(isStartServer) {
                        sprintf(message,"WITHDRAW_START %d;%d", serviceX, serviceServerID);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        // Mensagem para procura de um novo servidor de arranque
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "NEW_START\n");
                            writeTCP(TCPClientSocket, buffer_write);
                        }
                    }
                    
                    // Caso o serviço a remover seja servidor de despacho.
                    // Neste caso o "Token O" é enviado na receção do "Token T" ou na receção do "Token I",
                    // de modo a ser possivel atualizar a disponibilidade do anel após a sua saida
                    if(isDSServer) {
                        sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        // Mensagem para procura de um novo servidor de despacho
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "TOKEN %d;S\n", serviceServerID);
                            writeTCP(TCPClientSocket, buffer_write);
                        }
                    }

                    // Caso o serviço a remover nao seja servidor de despacho, ou caso seja o unico serviço no anel
                    if((isStartServer && !isDSServer) || (!isStartServer && !isDSServer) || successorID == 0) {
                        // Mensagem para aviso de saida do anel
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "TOKEN %d;O;%d;%s;%d\n", serviceServerID, successorID, successorIP, successorPort);
                            writeTCP(TCPClientSocket, buffer_write);
                            stateServer = idle;
	                        stateClient = idle;

                            clearSuccessors(&successorID, &successorPort, successorIP);
                        }
                        else {
                            printf("\tNot connected\n");
                        }
                    }


                    isStartServer = false;
                    isDSServer = false;
     				leaveFlag = true;
                }
            }
            else if(!strcmp("exit", message)) {
                if(serviceState == on) {
                    printf("\tCannot exit while provinding a service\n");
                }
                else {
                    // Caso o serviço de saida seja servidor de arranque
                    if(isStartServer) {
                        sprintf(message,"WITHDRAW_START %d;%d", serviceX, serviceServerID);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        // Mensagem para procura de um novo servidor de arranque
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "NEW_START\n");
                            writeTCP(TCPClientSocket, buffer_write);
                        }
                    }
                    // Caso o serviço de saida seja servidor de despacho
                    // Neste caso o "Token O" é enviado na receção do "Token T" ou na receção do "Token I",
                    // de modo a ser possivel atualizar a disponibilidade do anel após a sua saida
                    if(isDSServer) {
                        sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                        communicateUDP(centralServerSocket, centralServer, message, reply);
                        // Mensagem para procura de um novo servidor de despacho
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "TOKEN %d;S\n", serviceServerID);
                            writeTCP(TCPClientSocket, buffer_write);
                        }
                    }
                    // Caso o serviço de saida nao seja servidor de despacho, ou caso seja o unico serviço no anel
                    if((isStartServer && !isDSServer) || (!isStartServer && !isDSServer) || successorID == 0) {
                        // Mensagem para aviso de saida do anel
                        if(stateServer == busy && stateClient == busy) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "TOKEN %d;O;%d;%s;%d\n", serviceServerID, successorID, successorIP, successorPort);
                            writeTCP(TCPClientSocket, buffer_write);
                            stateServer = idle;
	                        stateClient = idle;
                            clearSuccessors(&successorID, &successorPort, successorIP);
                            exitStep1 = true;
                        }
                        else {
                            break;
                        }
                    }

                    isStartServer = false;
                    isDSServer = false;
                    leaveFlag = true;
                    exitFlag = true;  
                }
            }
            else if(!strcmp("help", message)) {
                printf("Valid commands: \n");
                printf("-> join x \n");
                printf("-> show_state or ss)\n");
                printf("-> leave \n");
                printf("-> exit \n");  
            } 
            else {
                printf("Invalid command! Type 'help'\n");
            }
        }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        else if(FD_ISSET(UDPServerSocket, &rfds)) {
            length = sizeof(UDPServer);
            bytes = recvfrom(UDPServerSocket, reply, BUFFER_SIZE, 0,(struct sockaddr*)&UDPServer, &length);  
            reply[bytes] = '\0';
            printf("\t-> Received: %s\n", reply);

            if(!strcmp("MY SERVICE ON", reply)) {
                sprintf(buffer, "YOUR SERVICE ON");
             
                sprintf(message,"WITHDRAW_DS %d;%d", serviceX, serviceServerID);
                communicateUDP(centralServerSocket, centralServer, message, reply);

                // Mensagem para procura de um novo servidor de despacho
                if(stateClient == busy && stateServer == busy) {
                    memset(buffer_write, 0, BUFFER_SIZE);
                    sprintf(buffer_write, "TOKEN %d;S\n", serviceServerID);
                    writeTCP(TCPClientSocket, buffer_write);
                } 
                else {
                    ringAvailable = false;
                }   
        
                isDSServer = false;                
                serviceState = on;
            } 
            else if(!strcmp("MY SERVICE OFF", reply)) {
                sprintf(buffer, "YOUR SERVICE OFF");

                // Caso o anel esteja indisponivel, alertar os restantes serviçoes que este passou a disponivel 
                if(!ringAvailable) {
                    memset(buffer_write, 0, BUFFER_SIZE);
                    sprintf(buffer_write, "TOKEN %d;D\n", serviceServerID);
                    writeTCP(TCPClientSocket, buffer_write);
                }    
                serviceState = off;
            }
            sendto(UDPServerSocket, buffer, strlen(buffer)+1, 0, (struct sockaddr*)&UDPServer, sizeof(UDPServer));
        }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        else if(FD_ISSET(TCPServerSocket, &rfds)) {
            TCPServerLength = sizeof(TCPServer);
            // socket temporaria para rearranjo do anel, posteriormente será usada a 'afd'
            if((newfd = accept(TCPServerSocket, (struct sockaddr*)&TCPServer, &TCPServerLength)) == -1) {
                exit(1);
            } 
            switch(stateServer)
                {
                // Primeiro caso, onde ainda nao existe anel
                case idle: 
                    afd = newfd;
                    stateServer = busy; 
                    break;
                // Segundo caso, onde o anel ja foi criado
                case busy: 
                    memset(buffer_read, 0, BUFFER_SIZE);
                    if((bytes = read(newfd, buffer_read, BUFFER_SIZE)) != 0) {
                        if(bytes == -1) {
                            exit(1);
                        }
                        printf("\t-> Received: %s", buffer_read);

                        // Caso o servidor de despacho receba NEW, circular o token N pelo anel
                        // para se realizar o rearranjo 
                        if(sscanf(buffer_read, "NEW %d;%[^;];%d\n", &replyID1, replyIP, &replyPort) == 3) {
                            memset(buffer_write, 0, BUFFER_SIZE);
                            sprintf(buffer_write, "TOKEN %d;N;%d;%s;%d\n", serviceServerID, replyID1, replyIP, replyPort);
                            writeTCP(TCPClientSocket, buffer_write);                        

                            // Caso o anel esteja indesponivel e na entrada de um novo serviço passar
                            // o anel para disponivel, e procurar um novo servidor de despacho
                            if(!ringAvailable) {
                                memset(buffer_write, 0, BUFFER_SIZE);
                                sprintf(buffer_write, "TOKEN %d;S\n", serviceServerID);
                                writeTCP(TCPClientSocket, buffer_write);

                                memset(buffer_write, 0, BUFFER_SIZE);
                                sprintf(buffer_write, "TOKEN %d;D\n", replyID1);
                                writeTCP(TCPClientSocket, buffer_write);
                            }
                        }
                    }

                    break;
                default:
                    break;
                }
        }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        else if((stateServer == busy && FD_ISSET(afd, &rfds)) || (exitStep1 == true && FD_ISSET(afd, &rfds))) {
            memset(buffer_read, 0, BUFFER_SIZE);
            if((bytes = read(afd, buffer_read, BUFFER_SIZE)) != 0) {
                if(bytes == -1) {
                    exit(1);
                }
               
                // Ponteiro para o inicio do buffer de leitura
                pointerPosition = &buffer_read[0];
                endLines = countEndLines(buffer_read, endLines);
                if(endLines == 0) {
                	printf("\tInvalid type of received message\n");
                	exit(-1);
                }

                // Ciclo executado consoante o número de mensagens protocolares contidas no buffer de leitura
                while(endLines > 0) {
	                pointerPosition = divideBuffer(pointerPosition, divBuffer);
					printf("\t-> Received: %s", divBuffer);

                    // Caso a mensagem seja do tipo Token
	                if(strstr(divBuffer, "TOKEN ") != NULL) {
	                    tokenType = checkToken(divBuffer);
	                }
	                else {
	                	tokenType = ' ';
	                }

	                // Caso em que ainda nao existe anel e procede-se à sua criação
	                if(sscanf(divBuffer, "NEW %d;%[^;];%d\n", &replyID1, replyIP, &replyPort) == 3) {
	                    TCPClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	                    if(TCPClientSocket == -1) {
	                        exit(-1);
	                    }

	                    memset((void*)&TCPClient,(int)'\0', sizeof(TCPClient));
	                    TCPClient.sin_family = AF_INET;
	                    inet_aton(replyIP, &TCPClient.sin_addr);
	                    TCPClient.sin_port = htons((u_short)replyPort);

	                    if(connect(TCPClientSocket, (struct sockaddr *)&TCPClient, sizeof(TCPClient)) == -1) {
	                        printf("\tCould not connect\n");
	                        exit(-1);    
	                    } 

	                    stateClient = busy;
	                    successorID = replyID1;
	                    successorPort =  replyPort;
	                    sprintf(successorIP, "%s", replyIP);
	                    if(!ringAvailable) {
	                        memset(buffer_write, 0, BUFFER_SIZE);
	                        sprintf(buffer_write, "TOKEN %d;S\n", serviceServerID);
	                        writeTCP(TCPClientSocket, buffer_write);
	                    }
	                }
                    // Registo de um novo servidor de arranque
	                else if(!strcmp("NEW_START\n", divBuffer)) {
	                    sprintf(message,"SET_START %d;%d;%s;%d", serviceX, serviceServerID, serviceServerIP, serviceTcpPort);
	                    communicateUDP(centralServerSocket, centralServer, message, reply);
	                    isStartServer = true;
	                }
	                else if(tokenType == 'N') {
	                    if(sscanf(divBuffer, "TOKEN %d;N;%d;%[^;];%d\n", &replyID1, &replyID2, replyIP, &replyPort) != 4) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }
                        // Quando encontrado o antecessor do servidor de arranque liga-lo ao novo serviço a ser introduzido no anel
	                    if(replyID1 == successorID) {
	                        close(TCPClientSocket);
	                        
	                        TCPClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	                        if(TCPClientSocket == -1) {
	                            exit(-1);
	                        }

	                        memset((void*)&TCPClient,(int)'\0', sizeof(TCPClient));
	                        TCPClient.sin_family = AF_INET;
	                        inet_aton(replyIP, &TCPClient.sin_addr);
	                        TCPClient.sin_port = htons((u_short)replyPort);

	                        if(connect(TCPClientSocket, (struct sockaddr *)&TCPClient, sizeof(TCPClient)) == -1) {
	                            printf("\tCould not connect\n");
	                            exit(-1);    
	                        } 
	                        successorID = replyID2;
	                        successorPort =  replyPort;
	                        sprintf(successorIP, "%s", replyIP);
	                    }
                        // Caso contrário circular o token 
	                    else {
	                        writeTCP(TCPClientSocket, divBuffer);
	                    }
	                }
	                else if(tokenType == 'S') {
	                    if(sscanf(divBuffer, "TOKEN %d;S\n", &replyID1) != 1) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }
                        // Caso seja encontrado um novo possivel servidor de despacho
	                    if(serviceServerID != replyID1) {
	                        if(serviceState == off) {
	                            sprintf(message,"SET_DS %d;%d;%s;%d",  serviceX, serviceServerID, serviceServerIP, serviceUdpPort);
	                            communicateUDP(centralServerSocket, centralServer, message, reply);

	                            memset(buffer_write, 0, BUFFER_SIZE);
	                            sprintf(buffer_write, "TOKEN %d;T\n", replyID1);
	                            writeTCP(TCPClientSocket, buffer_write);

	                            isDSServer = true;
	                        }
                            // Caso contrário circular o token
	                        else {
	                            writeTCP(TCPClientSocket, divBuffer);
	                        }
	                    }
                        // Caso o token S retorne ao serviço que o enviou, informar o anel que este se encontra disponivel
	                    else {
	                        printf("\tRing not available\n");
	                        memset(buffer_write, 0, BUFFER_SIZE);
	                        sprintf(buffer_write, "TOKEN %d;I\n", serviceServerID);
	                        writeTCP(TCPClientSocket, buffer_write);
	                        
	                        ringAvailable = false;
	                    }
	                }
                    // Confirmação do encontro de um servidor de despacho
	                else if(tokenType == 'T') {
	                    ringAvailable = true;
	                    if(sscanf(divBuffer, "TOKEN %d;T\n", &replyID1) != 1) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }

	                    if(replyID1 != serviceServerID) {
	                        writeTCP(TCPClientSocket, divBuffer);
	                    }
	                    else {
	                        printf("\tNew DS server found\n");

	                        if(leaveFlag) {
		                        memset(buffer_write, 0, BUFFER_SIZE);
		                        sprintf(buffer_write, "TOKEN %d;O;%d;%s;%d\n", serviceServerID, successorID, successorIP, successorPort);
		                        writeTCP(TCPClientSocket, buffer_write);
                				clearSuccessors(&successorID, &successorPort, successorIP);

                                stateServer = idle;
	                            stateClient = idle;

                                if(exitFlag) {
                                    exitStep1 = true;
                                }
	                        }
	                    }
	                }
                    // Aviso de anel indisponivel
	                else if(tokenType == 'I') {
	                    if(sscanf(divBuffer, "TOKEN %d;I\n", &replyID1) != 1) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }

	                    if(replyID1 != serviceServerID) {
	                        writeTCP(TCPClientSocket, divBuffer);
	                        ringAvailable = false;
	                    }
	                    else {
	                        printf("\tAll servers warned that the ring is unavailable\n");

	                        if(leaveFlag) {
		                        memset(buffer_write, 0, BUFFER_SIZE);
		                        sprintf(buffer_write, "TOKEN %d;O;%d;%s;%d\n", serviceServerID, successorID, successorIP, successorPort);
		                        writeTCP(TCPClientSocket, buffer_write);

                                stateServer = idle;
	                            stateClient = idle;
		                    
                				clearSuccessors(&successorID, &successorPort, successorIP);

                                if(exitFlag) {
                                    exitStep1 = true;
                                }
	                        }
	                    }
	                }
                    // Aviso de disponibilade do anel após situação de indisponibilidade
	                else if(tokenType == 'D') {
	                    ringAvailable = true;
	                    if(sscanf(divBuffer, "TOKEN %d;D\n", &replyID1) != 1) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }
	            
	                    if(serviceServerID > replyID1 || serviceState == on) {
	                        writeTCP(TCPClientSocket, divBuffer);
	                    }
	                    else {
	                        if(serviceServerID != replyID1) {
	                            printf("\tTOKEN %d;D Blocked\n", replyID1);
	                        }
	                        else {
	                            sprintf(message,"SET_DS %d;%d;%s;%d", serviceX, serviceServerID, serviceServerIP, serviceUdpPort);
	                            communicateUDP(centralServerSocket, centralServer, message, reply);
	                            isDSServer = true;
	                        }
	                    }
	                }
                    // Aviso de saida do anel
	                else if(tokenType == 'O') {
	                    if(sscanf(divBuffer, "TOKEN %d;O;%d;%[^;];%d\n", &replyID1, &replyID2, replyIP, &replyPort) != 4) {
	                        printf("\tInvalid type of received message\n");
	                        exit(-1);
	                    }
                        
                        // Caso o anel seja constituido apenas por dois serviços
	                    if(replyID1 == successorID && replyID2 == serviceServerID) {
	                        close(afd);
	                        close(TCPClientSocket);

	                        stateServer = idle;
	                        stateClient = idle;
	                        clearSuccessors(&successorID, &successorPort, successorIP);
	                    } 
                        // Caso tenha mais que dois serviços
                            // O serviço seguinte ao que tenciona sair 
                            // quebra a sua "ligação servidor" com ele  
                            // e circular o token
	                    else if(replyID2 == serviceServerID) {
	                        close(afd);

	                        stateServer = idle;
	                        writeTCP(TCPClientSocket, divBuffer);
	                    }
                        // Caso tenha mais que dois serviços
                            // O serviço anterior ao que tenciona sair
                            // quebra a sua "ligação cliente" com ele
                            // e liga-se ao sucessor do que tenciona sair
                            // restabelecendo assim o anel
	                    else if(replyID1 == successorID) {
	                        close(TCPClientSocket);
	                        
	                        TCPClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	                        if(TCPClientSocket == -1) {
	                            exit(-1);
	                        }

	                        memset((void*)&TCPClient,(int)'\0', sizeof(TCPClient));
	                        TCPClient.sin_family = AF_INET;
	                        inet_aton(replyIP, &TCPClient.sin_addr);
	                        TCPClient.sin_port = htons((u_short)replyPort);

	                        if(connect(TCPClientSocket, (struct sockaddr *)&TCPClient, sizeof(TCPClient)) == -1) {
	                            printf("\tCould not connect\n");
	                            exit(-1);    
	                        } 
	                        successorID = replyID2;
	                        successorPort =  replyPort;
	                        sprintf(successorIP, "%s", replyIP); 
	                    }
                        // Caso nao seja o serviço seguinte nem o anterior ao serviço que tenciona sair, circula o token
	                    else {
	                    	writeTCP(TCPClientSocket, divBuffer);
	                    }
	                } 
	                else {
	                    printf("\tInvalid type of received message\n");
	                    exit(-1);
	                }            
	            	endLines--;
	           	}
	        }
            else {     
                if(exitStep1) {
                    exitStep2 = true;
                    break;
                }
                printf("\tSocket closed at client end!\n");
                close(afd);
                afd = newfd;
            }  
        }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
        else if(stateClient == busy && FD_ISSET(TCPClientSocket, &rfds)) {
            memset(buffer_read, 0, BUFFER_SIZE);
            if((bytes = read(TCPClientSocket, buffer_read, BUFFER_SIZE)) != 0) {
                if(bytes == -1) {
                    exit(-1);
                }
               printf("\t->Received: %s", buffer_read);
            }
            else {
                close(TCPClientSocket); 
                printf("\tSocket closed at server end!\n");
            } 
        }
    }

    close(centralServerSocket);
    close(UDPServerSocket);
    close(TCPServerSocket);
    close(TCPClientSocket);
    close(newfd);
    close(afd);

    return 0; 
}

// Função de envio e receção de mensagens do tipo UDP ( para cada pedido existe sempre uma resposta)
void communicateUDP(int fd, struct sockaddr_in addr, char *message, char *reply) {
    int bytes, counter;
    fd_set rfds;
    struct timeval tv;
    unsigned length = sizeof(addr);
    
    printf("\tRequest: %s\n", message);
    bytes = sendto(fd, message, strlen(message), 0, (struct sockaddr*)&addr, length);
    if(bytes == -1) {
        exit(-1);
    }

    // Timeouts
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    counter = select(fd+1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &tv);
    if(counter < 0) {
    	exit(-1);
    	
    }
    else if(counter == 0) {
    	printf("\tTimeout: udp server is down\n"); 
    	FD_CLR(fd, &rfds);
    	exit(-1);
    }

    bytes = recvfrom(fd, reply, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &length);   
    if(bytes == -1) {
        exit(-1);
    }
    reply[bytes] = '\0';
    printf("\tReply: %s\n", reply);      
    return;
}

// Função para analisar a respostas do servidor central
int checkServerReply(char *reply, int *id1, int *id2, char *ip, unsigned *port) {
    sscanf(reply, "OK %d;%d;%[^;];%d", id1, id2, ip, port);
    // Resposta de erro (id < 0) ou de nao existencia do serviço (id = 0) 
    if(*id1 <= 0) {
        return 0;
    } 
    // Resposta do tipo OK x;0;0.0.0.0.;0
    else if(*id1 != 0 && *id2 == 0 && (!strcmp(ip, "0.0.0.0")) && *port == 0) {
        return 1;
    } 
    // reposta do tipo OK x;y;z;w
    else {
        return 2;
    }
}

// Função de envio dinamico de dados
void writeTCP(int socket, char *message) {
    int bytes, bytesLeft, bytesWritten;
    char *ptr;
    
    ptr = message;
    bytes = strlen(message);
    bytesLeft = bytes;

    while(bytesLeft > 0) {
        bytesWritten = write(socket , ptr, bytesLeft);
        if(bytesWritten <= 0) {
            exit(1);
        } 
        bytesLeft -= bytesWritten; 
        ptr += bytesWritten;
    }
}

// Função para limpar a informação do sucessor
void clearSuccessors(int *id, unsigned *port, char *ip) {
    *id = 0;
    *port = 0;
    ip[0] = '\0';
}

// Função para analisar o tipo do Token (S,T,O,...)
char checkToken(char *message) {
    char type;
    int i = 6;
    
    while(message[i] != ';') {
        i++;
    }
    type = message[i+1];
    
    return type;
}

// Função para converter formato enum em string
char* checkServiceState(int state) {
    if(state == 0) {
        return "On";
    } else {
        return "Off";
    }
}

// Função para converter formato bool em string
char* checkRingState(bool availability) {
    if(availability) {
        return "Available";
    } else {
        return "Unavailable";
    }
}

// Função para contar o número de '\n' numa mensagem recebida.
// Tendo em conta que este é o delimitador de uma mensagem TCP
// por cada '\n' corresponde uma mensagem a analisar
int countEndLines(char *buffer, int endLines) {
    int i;
   

    for(i = 0; i < strlen(buffer); i++) {
        if(buffer[i] == '\n') {
            endLines++;
        }
    }

    return endLines;
}

// Função para dividir uma mensagem com multiplos '\n' em
// sub-mensagens de modo a serem avaliadas individualmente 
char* divideBuffer(char *pointerPosition, char *divBuffer) {
    int i = 0;
    int endLines = 0;
 
    while(pointerPosition[i] != '\n') {
        divBuffer[i] = pointerPosition[i];
        i++;
    }
    divBuffer[i] = '\n';
    divBuffer[i+1] = '\0';

    pointerPosition += i+1;

    return pointerPosition;
}
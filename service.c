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

int main (int argc, char * argv[]) {
    int i, centralServerLength, serviceID;
    int centralServerSocket;
    unsigned centralServerPort = 59000, udpPort, tcpPort;
    char *centralServerIP = NULL, *serviceServerIP = NULL;
    char message[128], buffer[128];
    struct sockaddr_in centralServer;
    struct hostent *host;
    bool isDefaultServer = true;
    
    if(argc < 9 || argc > 13) {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    // Arguments stuff
    for(i = 1; i < argc; i = i+2) {
        if(!strcmp("-n", argv[i]) && i+1 < argc) {
            serviceID = atoi(argv[i+1]);
        } else if(!strcmp("-j", argv[i]) && i+1 < argc) {
            serviceServerIP = (char*) malloc(sizeof(argv[i+1]+1));
            strcpy(serviceServerIP,argv[i+1]);
        } else if(!strcmp("-u", argv[i]) && i+1 < argc) {
            udpPort = atoi(argv[i+1]);
        } else if(!strcmp("-t", argv[i]) && i+1 < argc) {
            tcpPort = atoi(argv[i+1]);
        } else if(!strcmp("-i", argv[i]) && i+1 < argc) {
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

    // UDP client for central server requests
    centralServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(centralServerSocket == -1) {
        exit(-1);
    }

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
        printf("Default central Server\n");
    }
    else {
        printf("Custom central Server\n");
    }
    printf("Central Server IP  : %s \n", inet_ntoa(centralServer.sin_addr));
    printf("Central Server port: %d \n", ntohs(centralServer.sin_port));
    printf("Service IP  : %s \n", serviceServerIP);
    printf("Service ID  : %d \n", serviceID);   
    printf("Service udp port: %d \n", udpPort);
    printf("Service tcp port: %d \n", tcpPort);
    printf("\nType 'help' for valid commands\n");

    while(1) {
        fgets(buffer, sizeof(buffer), stdin);
        sscanf(buffer,"%[^\n]s", message);

        if(!strcmp("join x",message)) {

        } else if(!strcmp("show_state",message)) {

        } else if(!strcmp("leave",message)) {
            
        } else if(!strcmp("exit",message)) {
            break;
        } else if(!strcmp("help",message)) {
          printf("Valid commands: \n");
          printf("-> join x \n");
          printf("-> show_state \n");
          printf("-> leave \n");
          printf("-> exit \n");  
        } else {
            printf("Invalid command! Type 'help'\n");
        }
    }

    if(centralServerIP != NULL) {
        free(centralServerIP);
    }
    if(serviceServerIP != NULL) {
        free(serviceServerIP);
    }

    return 0; 
}
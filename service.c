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
    unsigned centralServerPort = 59000, udpPort, tcpPort;
    char *centralServerIP = NULL, *serviceServerIP = NULL;
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
    if(isDefaultServer) {
        printf("Default central Server\n");
        if((host = gethostbyname("tejo.tecnico.ulisboa.pt")) == NULL) {
            exit(-1);
        }

        memset((void*)&centralServer,(int)'\0', sizeof(centralServer));
		centralServer.sin_family = AF_INET;
		centralServer.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;
		centralServer.sin_port = htons((u_short)centralServerPort);
        centralServerLength = sizeof(centralServerLength);
    } else {
        printf("Custom central Server\n");

        memset((void*)&centralServer,(int)'\0', sizeof(centralServer));
		centralServer.sin_family = AF_INET;
        if(centralServerIP == NULL) {
            if((host = gethostbyname("tejo.tecnico.ulisboa.pt")) == NULL) {
                exit(-1);
            }
            centralServer.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;   
        } else {
            inet_aton(centralServerIP, &centralServer.sin_addr);
        }
		centralServer.sin_port = htons((u_short)centralServerPort);
        centralServerLength = sizeof(centralServer);
    }

    printf("Host ip  : %s \n", inet_ntoa(centralServer.sin_addr));
    printf("Host port: %d \n", ntohs(centralServer.sin_port));

    if(centralServerIP != NULL) {
        free(centralServerIP);
    }
    if(serviceServerIP != NULL) {
        free(serviceServerIP);
    }

    return 0; 
}
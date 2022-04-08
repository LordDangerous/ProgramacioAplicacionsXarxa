#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

char clientFile[] = "client.cfg";


/* Packet type */
#define REG_REQ 0xa0
#define REG_ACK 0xa1
#define REG_NACK 0xa2
#define REG_REJ 0xa3
#define REG_INFO 0xa4
#define INFO_ACK 0xa5
#define INFO_NACK 0xa6
#define INFO_REJ 0xa7


/* States */
#define DISCONNECTED 0xf0
#define NOT_REGISTERED 0xf1
#define WAIT_ACK_REG 0xf2
#define WAIT_INFO 0xf3
#define WAIT_ACK_INFO 0xf4
#define REGISTERED 0xf5
#define SEND_ALIVE 0xf6



struct Client {
    char id_client[10];
    int state;
    int tcp_port;
    char elements[15*5];
    char server[9];
    int server_udp;
}; struct Client client;


struct PduUdp {
    unsigned char packet_type;
    char id_transmitter[11];
    char id_communication[11];
    char data[61];
};


/* Declaració funcions */
void removeSpaces(char* s);
void printDebug(char* message);
void printInfo(char* message);
void printError(char* message);
void readFile();
void setup();
void registerPhase(int sock, struct sockaddr_in server);




void removeSpaces(char* s) {
    char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while ((*s++ = *d++));
}


void printDebug(char* message) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - DEBUG => %s\n", hours, minutes, seconds, message);
}


void printInfo(char* message) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - INFO => %s\n", hours, minutes, seconds, message);
}


void printError(char* message) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - ERROR => %s\n", hours, minutes, seconds, message);
}


void readFile() {
    FILE* fp;
    char* p;
    char* value;
    char line[2048];
    int lineNumber = 0;
    char delimiter[2] = "=";
    fp = fopen(clientFile, "r");
    if (ferror(fp)) {
        printError("No es pot obrir l'arxiu de configuració");
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        p = strtok(line, delimiter);
        value = strtok(NULL, delimiter);
        removeSpaces(value);
        
        if (lineNumber == 0){
            if (strcmp(p, "Id ") == 0){
                strcpy(client.id_client, value);
                char* message = malloc(sizeof(char)*100);
                sprintf(message, "Assignat id_client: %s", client.id_client);
                printInfo(message);
                free(message);
            }  
            else {
                printInfo("No es pot obtenir l'identificador del client de l'arxiu de configuració");
                exit(1);
            }
            
        }
        if (lineNumber == 1){
            if (strcmp(p, "Elements ") == 0){
                strcpy(client.elements, value);
                char* message = malloc(sizeof(char)*100);
                sprintf(message, "Assignats elements: %s", client.elements);
                printInfo(message);
                free(message);
            }
            else {
                printInfo("No es poden obtenir els elements del client de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 2){
            if (strcmp(p, "Local-TCP ") == 0){
                client.tcp_port = atoi(value);
                char* tcp_port = malloc(sizeof(char)*10);
                sprintf(tcp_port, "Assignat port tcp: %d\n", client.tcp_port);
                printInfo(tcp_port);
                free(tcp_port);
            }
            else {
                printInfo("No es pot obtenir el port tcp del client de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 3){
            if (strcmp(p, "Server ") == 0){
                strcpy(client.server, value);
                char* message = malloc(sizeof(char)*100);
                sprintf(message, "Assignada IP servidor: %s", client.server);
                printInfo(message);
                free(message);
            }
            else {
                printInfo("No es pot obtenir la IP servidor de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 4){
            if (strcmp(p, "Server-UDP ") == 0){
                client.server_udp = atoi(value);
                char* server_udp = malloc(sizeof(char)*10);
                sprintf(server_udp, "Assignat port udp del servidor: %d\n", client.server_udp);
                printInfo(server_udp);
                free(server_udp);
            }
            else {
                printInfo("No es pot obtenir el port udp del servidor de l'arxiu de configuració");
                exit(1);
            }
        }
        lineNumber++;
    }
    fclose(fp);
}


/*struct PduUdp packPdu(char* packet_type, char id_transmitter, char id_communication, char data){
    struct PduUdp pdu = {packet_type, id_transmitter, id_communication, data};
    return pdu;
}*/


void registerPhase(int sock, struct sockaddr_in serveraddr) {
    printInfo("REGISTER PHASE");
    client.state = NOT_REGISTERED;

    printf("Client passa a l'estat: %x\n", client.state);
    
    char id_client[11] = {0};
    strncpy(id_client, client.id_client, 10);
    printf("Id client: %s\n", id_client);
    struct PduUdp pdu;
    pdu.packet_type = REG_REQ;
    strcpy(pdu.id_transmitter, id_client);
    strcpy(pdu.id_communication, "0000000000");
    strcpy(pdu.data, "");
    printf("PDU: %x %s %s %s\n", pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    if ((sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
        printError("Error a l'enviar pdu");
        printf("%d\n", errno);
    }
}




void setup() {
    int sock;

    struct sockaddr_in serveraddr;
    //struct hostent *he;

    /*if ((he = gethostbyname(client.server)) == NULL) {
        printError("No és possible obtenir gethostbyname");
        exit(1);
    }*/

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(client.server_udp);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //memcpy((char *) &serveraddr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

    

    registerPhase(sock, serveraddr);    
}



int main() {
    readFile();
    printInfo("READED FILE");
    setup();
}
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

#include <pthread.h>

#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

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

struct register_arg_struct {
    int sock;
    struct sockaddr_in serveraddr;
    char pdu[84];
} args;


/* Temps registre */
#define t 1
#define u 2
#define n 8
#define o 3
#define p 2
#define q 4


/* Declaració funcions NECESSARI???????*/
void removeSpaces(char* s);
void printDebug(char* message);
void printInfo(char* message);
void printError(char* message);
void readFile();
void setup();




void removeSpaces(char* s) {
    char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while ((*s++ = *d++));
}

int max (int x, int y) {
    if (x > y)
        return x;
    else
        return y;
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
    char* key;
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
        key = strtok(line, delimiter);
        value = strtok(NULL, delimiter);
        removeSpaces(value);
        
        if (lineNumber == 0){
            if (strcmp(key, "Id ") == 0){
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
            if (strcmp(key, "Elements ") == 0){
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
            if (strcmp(key, "Local-TCP ") == 0){
                client.tcp_port = atoi(value);
                char* tcp_port = malloc(sizeof(char)*100);
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
            if (strcmp(key, "Server ") == 0){
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
            if (strcmp(key, "Server-UDP ") == 0){
                client.server_udp = atoi(value);
                char* server_udp = malloc(sizeof(char)*100);
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


struct PduUdp packPduUdp(struct PduUdp pdu, int packet_type, char* id_transmitter, char* id_communication, char* data){
    pdu.packet_type = packet_type;
    char id_client[11] = {0};
    strncpy(id_client, id_transmitter, 10);

    strcpy(pdu.id_transmitter, id_client);
    strcpy(pdu.id_communication, id_communication);
    strcpy(pdu.data, data);

    char* pdu_message = malloc(sizeof(char)*100);
    sprintf(pdu_message, "PDU -> tipus paquet: %x id transmissor: %s id comunicació: %s dades: %s\n", pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printDebug(pdu_message);
    free(pdu_message);
    
    return pdu;
}


void registerPhase(int sock, struct sockaddr_in serveraddr) {
    printInfo("REGISTER PHASE");
    int packets_sent = 0;
    client.state = NOT_REGISTERED;

    char* message = malloc(sizeof(char)*100);
    sprintf(message, "Client passa a l'estat: %x", client.state);
    printInfo(message);
    

    struct PduUdp pdu;
    pdu = packPduUdp(pdu, REG_REQ, client.id_client, "0000000000", "");
    int bytes_sent;
    if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
        printError("Error a l'enviar pdu.");
        printf("%d\n", errno);
    }
    packets_sent += 1;
    printf("Num total paquets enviats: %d\n", packets_sent);

    sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printDebug(message);
    printInfo("Paquet 1 REG_REQ enviat");

    client.state = WAIT_ACK_REG;
    sprintf(message, "Client passa a l'estat: %x", client.state);
    printInfo(message);

    clock_t start_t, end_t;
    int t_register = t;
    sprintf(message, "%d %d", t_register, packets_sent);
    printInfo(message);
    start_t = clock();
    while (1) {
        end_t = clock();
        if ((end_t - start_t) == t_register && packets_sent <= p) {
            printf("Temps: %ld\n", (end_t - start_t));
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printError("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            printf("Num total paquets enviats: %d\n", packets_sent);
            sprintf(message, "Enviatp -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printDebug(message);
            start_t = clock();
        }
        else if ((end_t - start_t) == t_register && packets_sent > p && packets_sent < n) {
            printf("Temps: %ld\n", (end_t - start_t));
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printError("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            printf("Num total paquets enviats: %d\n", packets_sent);
            if (packets_sent > p) {
                if (t_register < (q * t)) {
                    t_register = t_register + t;
                    printf("T_Register incrementat a: %d\n", t_register);
                }
            }
            sprintf(message, "Enviatpp -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printDebug(message);
            start_t = clock();
        }
        else if (packets_sent == n) {
            break;
        }
    }
    printf("Waiting 2 s\n");
    sleep(u);
}


void* handleRegister(void* argp) {

    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;


    int register_number = 0;
    while (register_number < o) {
        registerPhase(sock, serveraddr);
        register_number += 1;
        printf("Fase de registre número %d finalitzada\n", register_number);
    }
    
    printInfo("No s'ha pogut contactar amb el servidor");
    pthread_exit(NULL);
    return NULL;
}

struct PduUdp unpackPduUdp(char *data) {
    int i;
    char* message = malloc(sizeof(char) * 100);
    
    printDebug("---------------- UNPACK PDU --------------");
    printf("SIZE: %d\n", strlen(data));
    
    struct PduUdp pdu;
    pdu.packet_type = data[0];
    sprintf(message, "Tipus paquet: %x", pdu.packet_type);
    printDebug(message);
    memcpy(pdu.id_transmitter, data + 1, sizeof(char) * 11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printDebug(message);
    memcpy(pdu.id_communication, data + 12, sizeof(char) * 11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printDebug(message);
    memcpy(pdu.data, data + 23, sizeof(char) * 61);
    sprintf(message, "Dades: %s", pdu.data);
    printDebug(message);
    return pdu;
}


void* handleUdpPacket(void* argp) {
    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    char data[84];
    memcpy(data, args->pdu, 84);
    printf("SIZE HANDLE: %d\n", sizeof(data));
    unpackPduUdp(data);

    printf("Rebut paquet UDP, creat procés per atendre'l\n");
    pthread_exit(NULL);
    return NULL;
}



void setup() {
    int sock_udp;

    struct sockaddr_in serveraddr;
    //struct hostent *he;

    /*if ((he = gethostbyname(client.server)) == NULL) {
        printError("No és possible obtenir gethostbyname");
        exit(1);
    }*/

    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(client.server_udp);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //memcpy((char *) &serveraddr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

    pthread_t thread;
    args.sock = sock_udp;
    args.serveraddr = serveraddr;
    
    if (pthread_create(&thread, NULL, &handleRegister, (void *)&args) != 0) {
        printf("ERROR creating thread");
    }


    /* SELECT */
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    fd_set rset;
    
    // PER TCP: int maxsock = max(sock_udp, sock_tcp) + 1;
    int maxsock = sock_udp + 1;
    int input;

    ssize_t bytes_received;
    socklen_t len;
    char buffer[84];

    while (1) {
        FD_ZERO(&rset);
        FD_SET(sock_udp, &rset);
        //PER TCP: FD_SET(sock_tcp, &rset);

        input = select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock_udp, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock_udp, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printf("Bytes rebuts: %ld\n", bytes_received);
            puts(buffer);
            strcpy(args.pdu, buffer);
            if (pthread_create(&thread, NULL, &handleUdpPacket, (void *)&args) != 0) {
                printf("ERROR creating thread");
            }
        }
    }
}



int main() {
    readFile();
    printInfo("READED FILE");
    setup();
}
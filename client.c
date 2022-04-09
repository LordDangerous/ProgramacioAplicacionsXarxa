#include <stdio.h>
#include <stdlib.h>
//#include <stdbool.h>
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
} client;


struct Server {
    char id_server[10];
    char id_communication[10];
    char udp_port[5];
    char tcp_port[4];
} server;


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
void* handleUdpPacket(void* argp);
struct PduUdp unpackPduUdp(char* data);
void* registerPhase(int sock, struct sockaddr_in serveraddr);




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

void printClientState() {
    char* state;
    if (client.state == 0xf0) {
        state = "DISCONNECTED";
    }
    else if (client.state == 0xf1) {
        state = "NOT_REGISTERED";
    }
    else if (client.state == 0xf2) {
        state = "WAIT_ACK_REG";
    }
    else if (client.state == 0xf3) {
        state = "WAIT_INFO";
    }
    else if (client.state == 0xf4) {
        state = "WAIT_ACK_INFO";
    }
    else if (client.state == 0xf5) {
        state = "REGISTERED";
    }
    else if (client.state == 0xf6) {
        state = "SEND_ALIVE";
    }

    char* message = malloc(sizeof(char)*100);
    sprintf(message, "Client passa a l'estat: %s", state);
    printInfo(message);
    free(message);
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


struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data){
    struct PduUdp pdu;
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

void* handleUdpPacket(void* argp) {
    printf("Rebut paquet UDP, creat procés per atendre'l\n");
    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;
    char data_received[84];
    memcpy(data_received, args->pdu, 84);
    printf("SIZE HANDLE: %ld\n", sizeof(data_received));

    struct PduUdp pdu;
    pdu = unpackPduUdp(data_received);

    if (client.state == WAIT_ACK_REG && pdu.packet_type == REG_ACK) {
        printInfo("Rebut paquet REG_ACK");
        char* message = malloc(sizeof(char)*100);
        strcpy(server.id_server, pdu.id_transmitter);
        strcpy(server.id_communication, pdu.id_communication);
        strcpy(server.udp_port, pdu.data);

        int sock_udp_server = socket(AF_INET, SOCK_DGRAM, 0);
    
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(server.udp_port);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

        int bytes_sent;
        char data[5 + 15*5 + 1];
        sprintf(data, "%d,%s", client.tcp_port, client.elements);
        struct PduUdp pduREG_INFO = packPduUdp(REG_INFO, client.id_client, server.id_communication, data);
        if ((bytes_sent = sendto(sock_udp_server, &pduREG_INFO, sizeof(pduREG_INFO), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
            printError("Error a l'enviar pdu.");
            printf("%d\n", errno);
        }

        client.state = WAIT_ACK_INFO;
        printClientState();

        /* SELECT */
        struct timeval timeout;
        timeout.tv_sec = 2*t;
        timeout.tv_usec = 0;
        
        fd_set rset;
        
        int maxsock = sock + 1;
        int input;

        ssize_t bytes_received;
        socklen_t len;
        char buffer[84];
        FD_ZERO(&rset);
        FD_SET(sock, &rset);

        input = select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printf("Bytes rebuts: %ld\n", bytes_received);
            printf("%s\n", buffer);
            struct PduUdp pdu_received = unpackPduUdp(buffer);

            if (client.state == WAIT_ACK_INFO ) {
                if (server.id_server == pdu_received.id_transmitter) {
                    if (server.id_communication == pdu_received.id_communication) {
                        if (pdu_received.packet_type == INFO_ACK) {
                            client.state = REGISTERED;
                            printClientState();
                            //END REGISTER!!!!!!!!
                        }
                        else if (pdu_received.packet_type == INFO_NACK) {
                            sprintf(message, "Descartat paquet de informació adicional de subscripció, motiu: %s", pdu_received.data);
                            printInfo(message);
                            client.state = NOT_REGISTERED;
                            printClientState();
                            //CONTINUAR REGISTRE
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                        printDebug(message);
                    }
                    
                } 
                else {
                    sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", serveraddr.sin_addr.s_addr, pdu_received.id_transmitter);
                    printDebug(message);
                }
            }
        }

        client.state = NOT_REGISTERED;
        printClientState();
        //NOU PROCÉS REGISTRE
    } 
    else if (pdu.packet_type == REG_NACK) {
        return NULL;
        //CONTINUAR REGISTRE
    }
    else if (pdu.packet_type == REG_REJ) {
        client.state = NOT_REGISTERED;
        printClientState();
        //NOU PROCÉS REGISTRE
    }
    else {
        client.state = NOT_REGISTERED;
        //NOU PROCÉS REGISTRE
    }
    pthread_exit(NULL);
    return NULL;
}


void* registerPhase(int sock, struct sockaddr_in serveraddr) {
    printInfo("REGISTER PHASE");

    int packets_sent = 0;
    client.state = NOT_REGISTERED;
    printClientState();
    char* message = malloc(sizeof(char)*100);
    struct PduUdp pdu;
    pdu = packPduUdp(REG_REQ, client.id_client, "0000000000", "");
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
    printClientState();

    

    /* SELECT */
    struct timeval timeout;
    timeout.tv_sec = t;
    timeout.tv_usec = 0;
    
    fd_set rset;
    
    // PER TCP: int maxsock = max(sock_udp, sock_tcp) + 1;
    int maxsock = sock + 1;
    int input;

    ssize_t bytes_received;
    socklen_t len;
    char buffer[84];
    FD_ZERO(&rset);

    while (1) {
        
        FD_SET(sock, &rset);
        //PER TCP: FD_SET(sock_tcp, &rset);

        input = select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printf("Bytes rebuts: %ld\n", bytes_received);
            printf("%s\n", buffer);
            strcpy(args.pdu, buffer);
            handleUdpPacket((void *)&args);
        }
        else if (packets_sent <= p) {
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printError("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            printf("Num total paquets enviats: %d\n", packets_sent);
            sprintf(message, "Enviatp -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printDebug(message);
        }
        else if (packets_sent > p && packets_sent < n) {
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printError("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            printf("Num total paquets enviats: %d\n", packets_sent);
            if (packets_sent > p) {
                if (timeout.tv_sec < (q * t)) {
                    timeout.tv_sec = timeout.tv_sec + t;
                    printf("Timeout incrementat a: %ld\n", timeout.tv_sec);
                }
            }
            sprintf(message, "Enviatpp -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printDebug(message);
        }
        else if (packets_sent == n) {
            break;
        }
    }
    printf("Waiting 2 s\n");
    sleep(u);
    return NULL;
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
    printf("SIZE: %ld\n", strlen(data));
    
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
    
    
    pthread_join(thread, NULL);
    
}



int main() {
    readFile();
    printInfo("READED FILE");
    setup();
}
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
int register_number = 0;
bool end_register_phase = false;

/* Packet type */
#define REG_REQ 0xa0
#define REG_ACK 0xa1
#define REG_NACK 0xa2
#define REG_REJ 0xa3
#define REG_INFO 0xa4
#define INFO_ACK 0xa5
#define INFO_NACK 0xa6
#define INFO_REJ 0xa7

#define ALIVE 0xb0
#define ALIVE_NACK 0xb1
#define ALIVE_REJ 0xb2


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
    int udp_port;
    int tcp_port;
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
    char* pdu;
} args;


/* Temps registre */
#define t 1
#define u 2
#define n 8
#define o 3
#define p 2
#define q 4
#define v 2


/* Declaració funcions */
void removeSpaces(char* s);
void printDebug(char* message);
void printInfo(char* message);
void printError(char* message);
void readFile();
void setup();
void handleUdpPacket(int sock, struct sockaddr_in serveraddr, char* data_received);
struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data);
struct PduUdp unpackPduUdp(char* data);
void* registerPhase(void* argp);
void* handleAlive(void* argp);




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
    char* message = malloc(sizeof(char)*1000);
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
    char* message = malloc(sizeof(char)*1000);
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
                sprintf(message, "Assignat id_client: %s", client.id_client);
                printInfo(message);
            }  
            else {
                printInfo("No es pot obtenir l'identificador del client de l'arxiu de configuració");
                exit(1);
            }
            
        }
        if (lineNumber == 1){
            if (strcmp(key, "Elements ") == 0){
                strcpy(client.elements, value);
                sprintf(message, "Assignats elements: %s", client.elements);
                printInfo(message);
            }
            else {
                printInfo("No es poden obtenir els elements del client de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 2){
            if (strcmp(key, "Local-TCP ") == 0){
                client.tcp_port = atoi(value);
                sprintf(message, "Assignat port tcp: %d\n", client.tcp_port);
                printInfo(message);
            }
            else {
                printInfo("No es pot obtenir el port tcp del client de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 3){
            if (strcmp(key, "Server ") == 0){
                strcpy(client.server, value);
                sprintf(message, "Assignada IP servidor: %s", client.server);
                printInfo(message);
            }
            else {
                printInfo("No es pot obtenir la IP servidor de l'arxiu de configuració");
                exit(1);
            }
        }
        if (lineNumber == 4){
            if (strcmp(key, "Server-UDP ") == 0){
                client.server_udp = atoi(value);
                sprintf(message, "Assignat port udp del servidor: %d\n", client.server_udp);
                printInfo(message);
            }
            else {
                printInfo("No es pot obtenir el port udp del servidor de l'arxiu de configuració");
                exit(1);
            }
        }
        lineNumber++;
    }
    fclose(fp);
    free(message);
}


struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data){
    char* message = malloc(sizeof(char)*1000);   

    printDebug("---------------- PACK PDU --------------");
    struct PduUdp pdu;

    pdu.packet_type = packet_type;
    sprintf(message, "Tipus paquet: %x", pdu.packet_type);
    printDebug(message);

    char id_client[11] = {0};
    strncpy(id_client, id_transmitter, 10);
    memcpy(pdu.id_transmitter, id_client, sizeof(char)*11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printDebug(message);

    memcpy(pdu.id_communication, id_communication, sizeof(char)*10);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printDebug(message);

    memcpy(pdu.data, data, sizeof(char)*61);
    sprintf(message, "Dades: %s", pdu.data);
    printDebug(message);

    printDebug("---------------- END PACK PDU --------------");

    free(message);
    return pdu;
}

void handleUdpPacket(int sock, struct sockaddr_in serveraddr, char* data_received) {
    printf("Rebut paquet UDP, creat procés per atendre'l\n");
    char* message = malloc(sizeof(char)*1000);

    struct PduUdp pdu;
    pdu = unpackPduUdp(data_received);

    if (client.state == WAIT_ACK_REG && pdu.packet_type == REG_ACK) {
        printInfo("Rebut paquet REG_ACK");
        strncpy(server.id_server, pdu.id_transmitter, 10);
        strncpy(server.id_communication, pdu.id_communication, 10);
        printf("IDCOM: %s\n", server.id_communication);
        server.udp_port = atoi(pdu.data);

        int sock_udp_server = socket(AF_INET, SOCK_DGRAM, 0);
    
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(server.udp_port);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

        int bytes_sent;
        char data[5 + 15*5 + 1];
        sprintf(data, "%d,%s", client.tcp_port, client.elements);
        printf("Data: %s", data);
        struct PduUdp pduREG_INFO = packPduUdp(REG_INFO, client.id_client, server.id_communication, data);
        if ((bytes_sent = sendto(sock_udp_server, &pduREG_INFO, sizeof(pduREG_INFO), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
            printError("Error a l'enviar pdu.");
            printf("%d\n", errno);
        }
        sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pduREG_INFO.packet_type, pduREG_INFO.id_transmitter, pduREG_INFO.id_communication, pduREG_INFO.data);
        printDebug(message);

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
                            //END REGISTER
                            pthread_exit(NULL);
                            return;
                        }
                        else if (pdu_received.packet_type == INFO_NACK) {
                            sprintf(message, "Descartat paquet de informació adicional de subscripció, motiu: %s", pdu_received.data);
                            printInfo(message);
                            client.state = NOT_REGISTERED;
                            printClientState();
                            //CONTINUAR REGISTRE
                            return;
                        }
                        else {
                            printInfo("Paquet incorrecte");
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
        else {
            client.state = NOT_REGISTERED;
            printClientState();
            //NOU PROCÉS REGISTRE
            end_register_phase = true;
        }
    } 
    else if (pdu.packet_type == REG_NACK) {
        //CONTINUAR REGISTRE
        return;
    }
    else if (pdu.packet_type == REG_REJ) {
        client.state = NOT_REGISTERED;
        printClientState();
        //NOU PROCÉS REGISTRE
        end_register_phase = true;
    }
    else {
        client.state = NOT_REGISTERED;
        //NOU PROCÉS REGISTRE
        end_register_phase = true;
    }
    free(message);
    pthread_exit(NULL);
    return;
}


void* registerPhase(void* argp) {
    printInfo("REGISTER PHASE");

    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    int packets_sent = 0;
    client.state = NOT_REGISTERED;
    printClientState();
    char* message = malloc(sizeof(char)*1000);
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
    int timeout_s = t;
    
    fd_set rset;
    
    // PER TCP: int maxsock = max(sock_udp, sock_tcp) + 1;
    int maxsock = sock + 1;
    int input;

    ssize_t bytes_received;
    socklen_t len;
    char buffer[84] = {0};
    FD_ZERO(&rset);

    while (1) {
        
        FD_SET(sock, &rset);
        //PER TCP: FD_SET(sock_tcp, &rset);

        input = select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printf("Bytes rebuts: %ld\n", bytes_received);
            handleUdpPacket(sock, serveraddr, buffer);
            if (end_register_phase) {
                end_register_phase = false;
                pthread_exit(NULL);
                return NULL;
            }
        }
        if (packets_sent < n) {
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printError("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            printf("Num total paquets enviats: %d\n", packets_sent);
        
            if (timeout_s < (q * t)) {
                timeout_s = timeout_s + t;
                timeout.tv_sec = timeout_s;
                printf("Timeout incrementat a: %ld\n", timeout.tv_sec);
            }
            else if (packets_sent < n && timeout_s == (q * t)) {
                timeout.tv_sec = timeout_s;
                printf("Timeout a: %ld\n", timeout.tv_sec);
            }
            
            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printDebug(message);
        }
        else if (packets_sent == n) {
            break;
        }
    }
    printDebug("Esperant 2 s\n");
    sleep(u);
    free(message);
    pthread_exit(NULL);
    return NULL;
}


struct PduUdp unpackPduUdp(char* data) {
    char* message = malloc(sizeof(char) * 1000);
    char idcom[11] = {0};
    
    printDebug("---------------- UNPACK PDU --------------");
    struct PduUdp pdu;
    memset(&pdu, 0, sizeof(pdu));
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
    printDebug("---------------- END UNPACK PDU --------------");

    free(message);
    return pdu;
}


void* handleAlive(void* argp) {
    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    int bytes_sent;
    char* message = malloc(sizeof(char)*1000);
    struct PduUdp pdu = packPduUdp(ALIVE, client.id_client, server.id_communication, "");


    /* SELECT */
    struct timeval timeout;
    timeout.tv_sec = v;
    timeout.tv_usec = 0;
    int timeout_s = v;
    
    fd_set rset;
    
    int maxsock = sock + 1;
    int input;

    ssize_t bytes_received;
    socklen_t len;
    char buffer[84];
    FD_ZERO(&rset);
    FD_SET(sock, &rset);

    while (1) {

        input = select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printf("Bytes rebuts: %ld\n", bytes_received);
            printf("%s\n", buffer);
            struct PduUdp pdu_received = unpackPduUdp(buffer);
            timeout.tv_sec = timeout_s;
            if (server.id_server == pdu_received.id_transmitter) {
                if (server.id_communication == pdu_received.id_communication) {
                    if (client.id_client == pdu_received.data) {
                        
                    }
                    else {
                        sprintf(message, "Error en el valor del camp dades (rebut: %s, esperat: %s)", pdu_received.data, client.id_client);
                        printDebug(message);
                        client.state = NOT_REGISTERED;
                        printClientState();
                    }

                }
                else {
                    sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                    printDebug(message);
                    client.state = NOT_REGISTERED;
                    printClientState();
                }    
            } 
            else {
                sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", serveraddr.sin_addr.s_addr, pdu_received.id_transmitter);
                printDebug(message);
                client.state = NOT_REGISTERED;
                printClientState();
            }
        }
    }
    









    if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
        printError("Error a l'enviar pdu.");
        printf("%d\n", errno);
    }
    sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printDebug(message);


    free(message);
}



void setup() {
    char* message = malloc(sizeof(char)*1000);
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

    pthread_t thread_REGISTER, thread_ALIVE;
    args.sock = sock_udp;
    args.serveraddr = serveraddr;
    
    while (register_number < 3) {
        sprintf(message, "Procés de subscripció: %d", register_number);
        printInfo(message);
        if (pthread_create(&thread_REGISTER, NULL, &registerPhase, (void *)&args) != 0) {
            printf("ERROR creating thread");
        }
        
        pthread_join(thread_REGISTER, NULL);

        if (client.state == REGISTERED) {
            if (pthread_create(&thread_ALIVE, NULL, &handleAlive, (void *)&args) != 0) {
                printf("ERROR creating thread");
            }
        }
        
        pthread_join(thread_ALIVE, NULL);

        printInfo("No s'ha pogut contactar amb el servidor");
        register_number += 1;
    }
    

    free(message);
}



int main() {
    readFile();
    printInfo("READED FILE");
    setup();
}
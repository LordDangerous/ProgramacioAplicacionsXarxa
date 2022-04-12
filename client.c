#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
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

/* Definim una variable global que conté inicialment el nom de l'arxiu per defecte "client.cfg", el nombre de procediments de registre, 
un booleà que ens ajudarà a saber si hem d'acabar el registre o fer un nou procés de registre i un altre booleà per saber si s'ha
especificat l'opció "-d" al executar el client */
char clientFile[] = "client.cfg";
int register_number = 1;
bool end_register_phase = false;
bool debug_mode = false;

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

#define SEND_DATA 0xc0
#define DATA_ACK 0xc1
#define DATA_NACK 0xc2
#define DATA_REJ 0xc3
#define SET_DATA 0xc4
#define GET_DATA 0xc5

/* States */
#define DISCONNECTED 0xf0
#define NOT_REGISTERED 0xf1
#define WAIT_ACK_REG 0xf2
#define WAIT_INFO 0xf3
#define WAIT_ACK_INFO 0xf4
#define REGISTERED 0xf5
#define SEND_ALIVE 0xf6

//STDIN per les comandes de terminal
#define STDIN 0

//Nivells per al print
#define MSG "MSG"
#define INFO "INFO"
#define ERROR "ERROR"
#define DEBUG "DEBUG"

struct Element {
    char element[8];
    char value[15];
}; 


struct Client {
    char id_client[10];
    int state;
    int tcp_port;
    struct Element elements[5];
    int num_elements;
    char server[9];
    int server_udp;
} client;


struct Server {
    char id_server[11];
    char id_communication[11];
    int udp_port;
    int tcp_port;
} server;


struct PduUdp {
    unsigned char packet_type;
    char id_transmitter[11];
    char id_communication[11];
    char data[61];
};


struct PduTcp {
    unsigned char packet_type;
    char id_transmitter[11];
    char id_communication[11];
    char element[8];
    char value[16];
    char info[80];
};


struct register_arg_struct {
    int sock;
    struct sockaddr_in serveraddr;
} args;


struct alive_arg_struct {
    int sock;
    struct sockaddr_in serveraddr;
    struct PduUdp pdu;
} argAlive;


/* Constants de temps */
#define t 1
#define u 2
#define n 8
#define o 3
#define p 2
#define q 4
#define v 2
#define r 2
#define m 3


/* Declaració funcions */
void removeSpaces(char* s);
void printTerminal(char* message, char* level);
void readFile();
void setup();
void handleUdpPacket(char* data_received, ssize_t bytes_received);
struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data);
struct PduUdp unpackPduUdp(char* data, ssize_t bytes_received);
struct PduTcp packPduTcp(int packet_type, char* id_transmitter, char* id_communication, char* element, char* value, char* info);
struct PduTcp unpackPduTcp(char* data, ssize_t bytes_received);
void* registerPhase(void* argp);
void* handleAlive(void* argp);
void* sendAlives(void* argAlive);
void handleCommands(char* buffer);
void removeNewLine(char* s);
void handleSetGet(int conn_tcp, struct sockaddr_in tcpaddr, char* buffer, ssize_t bytes_received);
char* getHour();
void parseArgs(int argc, char* argv[]);
char* packetTypeConverter(int packet_type);


//Funció auxiliar per borrar espais (si n'hi ha) a un array de chars
void removeSpaces(char* s) {
    char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while ((*s++ = *d++));
}

//Funció auxiliar per calcular el màxim entre dos enters
int max (int x, int y) {
    if (x > y)
        return x;
    else
        return y;
}


//Funció auxiliar que permet printejar l'estat del client amb forma de char 
void printClientState() {
    //Utilizaré sempre un malloc per guardar memòria per fer tots els prints (printTerminal)
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
    
    //Guardo la cadena de caràcters concatenada a la variable message mitjançant sprintf per poder passar-li a la funció de print específica
    sprintf(message, "Client passa a l'estat: %s", state);
    printTerminal(message, MSG);
    //Alliberem sempre al final de la funció l'espai de memòria que havíem reservat, ja que no l'utilitzarem més
    free(message);
}


//Funció per printejar els missatges (msg, info, error i debug) per la terminal
void printTerminal(char* message, char* level) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    
    if (strcmp(level, MSG) == 0) {
        printf("%02d:%02d:%02d - MSG => %s\n", hours, minutes, seconds, message);
    }
    else if (strcmp(level, INFO) == 0) {
        printf("%02d:%02d:%02d - INFO => %s\n", hours, minutes, seconds, message);
    }
    else if (strcmp(level, ERROR) == 0) {
        printf("%02d:%02d:%02d - ERROR => %s\n", hours, minutes, seconds, message);
    }
    else if (strcmp(level, DEBUG) == 0) {
        //Printejar els missatges de debug (si l'opció "-d" ha estat introduïda) per la terminal
        if (debug_mode) {
            //Utilitzo %02d per completar el número amb zeros a l'esquerra si és un nombre d'una xifra (per exemple 14:1:20 -> 14:01:20)
            printf("%02d:%02d:%02d - DEBUG => %s\n", hours, minutes, seconds, message);
        }
    }
}


//Funció per llegir l'arxiu de configuració per defecte o especificat mitjançant el paràmetre a l'inici del programa
void readFile() {
    char* message = malloc(sizeof(char)*1000);
    FILE* fp;
    char* key;
    char* value;
    char line[2048];
    int lineNumber = 0;
    char delimiter[2] = "=";
    fp = fopen(clientFile, "r");
    if (fp == NULL) {
        sprintf(message, "No es pot obrir l'arxiu de configuració: %s", clientFile);
        printTerminal(message, ERROR);
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        key = strtok(line, delimiter);
        value = strtok(NULL, "\n");
        removeSpaces(value);
        
        if (lineNumber == 0){
            if (strcmp(key, "Id ") == 0){
                strcpy(client.id_client, value);
                sprintf(message, "Assignat id_client: %s", client.id_client);
                printTerminal(message, INFO);
            }  
            else {
                printTerminal("No es pot obtenir l'identificador del client de l'arxiu de configuració", ERROR);
                exit(1);
            }
            
        }
        if (lineNumber == 1){
            if (strcmp(key, "Elements ") == 0) {
                char* element;
                int i = 0;
                element = strtok(value, ";");
                printTerminal("Assignats elements: ", INFO);
                while (element != NULL) {
                    strncpy(client.elements[i].element, element, 7);
                    client.elements[i].element[7] = '\0';
                    strcpy(client.elements[i].value, "NONE");
                    sprintf(message, "%s\t%s", client.elements[i].element, client.elements[i].value);
                    printTerminal(message, INFO);
                    element = strtok(NULL, ";");
                    i++;
                }
                client.num_elements = i;
            }
            else {
                printTerminal("No es poden obtenir els elements del client de l'arxiu de configuració", ERROR);
                exit(1);
            }
        }
        if (lineNumber == 2){
            if (strcmp(key, "Local-TCP ") == 0) {
                client.tcp_port = atoi(value);
                sprintf(message, "Assignat port tcp: %d", client.tcp_port);
                printTerminal(message, INFO);
            }
            else {
                printTerminal("No es pot obtenir el port tcp del client de l'arxiu de configuració", ERROR);
                exit(1);
            }
        }
        if (lineNumber == 3){
            if (strcmp(key, "Server ") == 0) {
                strcpy(client.server, value);
                sprintf(message, "Assignada IP servidor: %s", client.server);
                printTerminal(message, INFO);
            }
            else {
                printTerminal("No es pot obtenir la IP servidor de l'arxiu de configuració", ERROR);
                exit(1);
            }
        }
        if (lineNumber == 4){
            if (strcmp(key, "Server-UDP ") == 0) {
                client.server_udp = atoi(value);
                sprintf(message, "Assignat port udp del servidor: %d", client.server_udp);
                printTerminal(message, INFO);
            }
            else {
                printTerminal("No es pot obtenir el port udp del servidor de l'arxiu de configuració", ERROR);
                exit(1);
            }
        }
        lineNumber++;
    }
    fclose(fp);
    free(message);
}


//Funció auxiliar per convertir el tipus de paquet a cadena de caràcters per mostrar-ho per la terminal
char* packetTypeConverter(int packet_type) {
    char* packet_type_s;
    if (packet_type == 0xa0) {
        packet_type_s = "REG_REQ";
    }
    else if (packet_type == 0xa1) {
        packet_type_s = "REG_ACK";
    }
    else if (packet_type == 0xa2) {
        packet_type_s = "REG_NACK";
    }
    else if (packet_type == 0xa3) {
        packet_type_s = "REG_REJ";
    }
    else if (packet_type == 0xa4) {
        packet_type_s = "REG_INFO";
    }
    else if (packet_type == 0xa5) {
        packet_type_s = "INFO_ACK";
    }
    else if (packet_type == 0xa6) {
        packet_type_s = "INFO_NACK";
    }
    else if (packet_type == 0xa7) {
        packet_type_s = "INFO_REJ";
    }
    else if (packet_type == 0xb0) {
        packet_type_s = "ALIVE";
    }
    else if (packet_type == 0xb1) {
        packet_type_s = "ALIVE_NACK";
    }
    else if (packet_type == 0xb2) {
        packet_type_s = "ALIVE_REJ";
    }
    else if (packet_type == 0xc0) {
        packet_type_s = "SEND_DATA";
    }
    else if (packet_type == 0xc1) {
        packet_type_s = "DATA_ACK";
    }
    else if (packet_type == 0xc2) {
        packet_type_s = "DATA_NACK";
    }
    else if (packet_type == 0xc3) {
        packet_type_s = "DATA_REJ";
    }
    else if (packet_type == 0xc4) {
        packet_type_s = "SET_DATA";
    }
    else if (packet_type == 0xc5) {
        packet_type_s = "GET_DATA";
    }
    return packet_type_s;
} 


struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data){
    char* message = malloc(sizeof(char)*1000);   

    printTerminal("---------------- PACK PDU UDP --------------", DEBUG);
    struct PduUdp pdu;

    pdu.packet_type = packet_type;
    char* packet_type_s =  packetTypeConverter(packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG);

    char id_client[11] = {0};
    strncpy(id_client, id_transmitter, 10);
    memcpy(pdu.id_transmitter, id_client, sizeof(char)*11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG);

    memcpy(pdu.id_communication, id_communication, sizeof(char)*11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG);

    memcpy(pdu.data, data, sizeof(char)*61);
    sprintf(message, "Dades: %s", pdu.data);
    printTerminal(message, DEBUG);

    printTerminal("---------------- END PACK PDU UDP --------------", DEBUG);

    free(message);
    return pdu;
}


struct PduTcp packPduTcp(int packet_type, char* id_transmitter, char* id_communication, char* element, char* value, char* info){
    char* message = malloc(sizeof(char)*1000);   

    printTerminal("---------------- PACK PDU TCP --------------", DEBUG);
    struct PduTcp pdu;

    pdu.packet_type = packet_type;
    char* packet_type_s =  packetTypeConverter(packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG);

    char id_client[11] = {0};
    strncpy(id_client, id_transmitter, 10);
    memcpy(pdu.id_transmitter, id_client, sizeof(char)*11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG);

    memcpy(pdu.id_communication, id_communication, sizeof(char)*11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG);

    memcpy(pdu.element, element, sizeof(char)*8);
    sprintf(message, "Element: %s", pdu.element);
    printTerminal(message, DEBUG);

    memcpy(pdu.value, value, sizeof(char)*16);
    sprintf(message, "Value: %s", pdu.value);
    printTerminal(message, DEBUG);

    memcpy(pdu.info, info, sizeof(char)*80);
    sprintf(message, "Info: %s", pdu.info);
    printTerminal(message, DEBUG);

    printTerminal("---------------- END PACK PDU TCP --------------", DEBUG);

    free(message);
    return pdu;
}


struct PduTcp unpackPduTcp(char* data, ssize_t bytes_received) {
    char* message = malloc(sizeof(char) * 1000);
    sprintf(message, "Bytes rebuts: %ld", bytes_received);
    printTerminal(message, DEBUG);
    
    printTerminal("---------------- UNPACK PDU TCP --------------", DEBUG);
    struct PduTcp pdu;
    memset(&pdu, 0, sizeof(pdu));
    pdu.packet_type = data[0];
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG);
    memcpy(pdu.id_transmitter, data + 1, sizeof(char) * 11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG);
    memcpy(pdu.id_communication, data + 12, sizeof(char) * 11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG);
    memcpy(pdu.element, data + 23, sizeof(char) * 8);
    sprintf(message, "Element: %s", pdu.element);
    printTerminal(message, DEBUG);
    memcpy(pdu.value, data + 31, sizeof(char) * 16);
    sprintf(message, "Value: %s", pdu.value);
    printTerminal(message, DEBUG);
    memcpy(pdu.info, data + 47, sizeof(char) * 80);
    sprintf(message, "Info: %s", pdu.info);
    printTerminal(message, DEBUG);
    printTerminal("---------------- END UNPACK PDU TCP --------------", DEBUG);

    free(message);
    return pdu;
}

// void print_hex(char* input) {
//     int loop = 0;
//     while(input[loop] != '\0') {
//         printf("%02x ", input[loop]);
//         loop++;
//     }
//     printf("\n");
// }


void handleUdpPacket(char* data_received, ssize_t bytes_received) {
    printTerminal("Rebut paquet UDP, creat procés per atendre'l", DEBUG);
    char* message = malloc(sizeof(char)*1000);

    struct PduUdp pdu;
    pdu = unpackPduUdp(data_received, bytes_received);

    if (client.state == WAIT_ACK_REG && pdu.packet_type == REG_ACK) {
        printf("Rebut paquet REG_ACK");
        strncpy(server.id_server, pdu.id_transmitter, 11);
        strncpy(server.id_communication, pdu.id_communication, 11);
        server.udp_port = atoi(pdu.data);

        int sock_udp_server;
        sock_udp_server = socket(AF_INET, SOCK_DGRAM, 0);
    
        struct sockaddr_in serveraddrnew;
        serveraddrnew.sin_family = AF_INET;
        serveraddrnew.sin_port = htons(server.udp_port);
        serveraddrnew.sin_addr.s_addr = htonl(INADDR_ANY);

        int bytes_sent;
        char data[5 + 1 + 7*5+4 + 1];
        char* semicolon = ";";
        sprintf(data, "%d,", client.tcp_port);
        for (int i = 0; i < client.num_elements; i++) {
            strcat(data, client.elements[i].element);
            if (i < client.num_elements - 1) {
                strcat(data, semicolon);
            }
        }

        struct PduUdp pduREG_INFO = packPduUdp(REG_INFO, client.id_client, server.id_communication, data);
        if ((bytes_sent = sendto(sock_udp_server, &pduREG_INFO, sizeof(pduREG_INFO), 0, (struct sockaddr*)&serveraddrnew, sizeof(serveraddrnew))) == -1) {
            printTerminal("Error a l'enviar pdu.", ERROR);
            //printf("%d\n", errno);
        }
        sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pduREG_INFO.packet_type, pduREG_INFO.id_transmitter, pduREG_INFO.id_communication, pduREG_INFO.data);
        printf(message);

        client.state = WAIT_ACK_INFO;
        printClientState();
        
        /* SELECT */
        struct timeval timeout;
        timeout.tv_sec = 2*t;
        timeout.tv_usec = 0;
        
        fd_set rset;
        
        int maxsock = sock_udp_server + 1;
        int input;

        ssize_t bytes_received;
        socklen_t len;
        char buffer[84];
        FD_ZERO(&rset);
        FD_SET(sock_udp_server, &rset);
        
        while(timeout.tv_sec) {

            input = select(maxsock, &rset, NULL, NULL, NULL);

            if (input) {
                if (FD_ISSET(sock_udp_server, &rset)) {
                    len = sizeof(serveraddrnew);
                    bytes_received = recvfrom(sock_udp_server, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddrnew, &len);
                    struct PduUdp pdu_received = unpackPduUdp(buffer, bytes_received);

                    if (client.state == WAIT_ACK_INFO) {
                        if (pdu_received.packet_type == INFO_ACK || pdu_received.packet_type == INFO_NACK) {
                            if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                                if (strcmp(server.id_communication, pdu_received.id_communication) == 0) {
                                    if (pdu_received.packet_type == INFO_ACK) {
                                        client.state = REGISTERED;
                                        printClientState();
                                        server.tcp_port = atoi(pdu_received.data);
                                        //END REGISTER
                                        pthread_exit(NULL);
                                        return;
                                    }
                                    else if (pdu_received.packet_type == INFO_NACK) {
                                        sprintf(message, "Descartat paquet de informació adicional de subscripció, motiu: %s", pdu_received.data);
                                        printTerminal(message, INFO);
                                        client.state = NOT_REGISTERED;
                                        printClientState();
                                        //CONTINUAR REGISTRE
                                        return;
                                    }
                                    else {
                                        printTerminal("Paquet incorrecte", INFO);
                                    }
                                }
                                else {
                                    sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                                    printTerminal(message, DEBUG);
                                }
                                
                            } 
                            else {
                                sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %s, id: %s)", server.id_server, pdu_received.id_transmitter);
                                printTerminal(message, DEBUG);
                            }
                        }
                        else if (pdu_received.packet_type == REG_NACK) {
                            //CONTINUAR REGISTRE
                            break;
                        }
                        else if (pdu_received.packet_type == REG_REJ) {
                            client.state = NOT_REGISTERED;
                            printClientState();
                            //NOU PROCÉS REGISTRE
                            end_register_phase = true;
                            break;
                        }
                        else {
                            printTerminal("Paquet desconegut", INFO);
                            client.state = NOT_REGISTERED;
                            printClientState();
                            //NOU PROCÉS REGISTRE
                            end_register_phase = true;
                            break;
                        }
                    }
                }  
            }            
        }
        if (client.state != NOT_REGISTERED) {
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
        printClientState();
        //NOU PROCÉS REGISTRE
        end_register_phase = true;
    }
    free(message);
    pthread_exit(NULL);
    return;
}


void* registerPhase(void* argp) {
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
        printf("Error a l'enviar pdu.");
        printf("%d\n", errno);
    }
    packets_sent += 1;
    printf("Num total paquets enviats: %d\n", packets_sent);

    sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printf(message);
    printTerminal("Paquet 1 REG_REQ enviat", DEBUG);

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
            handleUdpPacket(buffer, bytes_received);
            if (end_register_phase) {
                end_register_phase = false;
                pthread_exit(NULL);
                return NULL;
            }
        }
        if (packets_sent < n) {
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printf("Error a l'enviar pdu.");
            }
            packets_sent += 1;
            sprintf(message, "Num total paquets enviats: %d\n", packets_sent);
            printTerminal(message, DEBUG);
        
            if (timeout_s < (q * t)) {
                timeout_s = timeout_s + t;
                timeout.tv_sec = timeout_s;
                sprintf(message, "Timeout incrementat a: %ld\n", timeout.tv_sec);
                printTerminal(message, DEBUG);
            }
            else if (packets_sent < n && timeout_s == (q * t)) {
                timeout.tv_sec = timeout_s;
                sprintf(message, "Timeout a: %ld\n", timeout.tv_sec);
                printTerminal(message, DEBUG);
            }
            
            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printf(message);
        }
        else if (packets_sent == n) {
            break;
        }
    }
    printTerminal("Esperant 2 s", DEBUG);
    sleep(u);
    free(message);
    pthread_exit(NULL);
    return NULL;
}


struct PduUdp unpackPduUdp(char* data, ssize_t bytes_received) {
    char* message = malloc(sizeof(char) * 1000);
    sprintf(message, "Bytes rebuts: %ld", bytes_received);
    printTerminal(message, DEBUG);
    printTerminal("---------------- UNPACK PDU UDP --------------", DEBUG);
    struct PduUdp pdu;
    memset(&pdu, 0, sizeof(pdu));
    pdu.packet_type = data[0];
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG);
    memcpy(pdu.id_transmitter, data + 1, sizeof(char) * 11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG);
    memcpy(pdu.id_communication, data + 12, sizeof(char) * 11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG);

    memcpy(pdu.data, data + 23, sizeof(char) * 61);
    sprintf(message, "Dades: %s", pdu.data);
    printTerminal(message, DEBUG);
    printTerminal("---------------- END UNPACK PDU UDP --------------", DEBUG);

    free(message);
    return pdu;
}


void* sendAlives(void* argAlive) {
    char* message = malloc(sizeof(char)*1000);
    struct alive_arg_struct *args = argAlive;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;
    struct PduUdp pdu = args->pdu;

    int bytes_sent;
    time_t start_t, end_t, total_t;
    start_t = time(NULL);

    while (client.state == REGISTERED || client.state == SEND_ALIVE) {
        end_t = time(NULL);
        total_t = end_t - start_t;
        if (total_t == v) {
            if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
                printf("Error a l'enviar pdu des de sendAlives.");
                printf("%d\n", errno);
            }
            sprintf(message, "Enviat ALIVE -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
            printf(message);
            start_t = time(NULL);
        }
    }
    
    free(message);
    pthread_exit(NULL);
    return NULL;
}


void* handleAlive(void* argp) {
    struct register_arg_struct *args = argp;
    int sock_udp = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    int bytes_sent;
    char* message = malloc(sizeof(char)*1000);
    struct PduUdp pdu = packPduUdp(ALIVE, client.id_client, server.id_communication, "");

    argAlive.sock = sock_udp;
    argAlive.serveraddr = serveraddr;
    argAlive.pdu = pdu;

    if ((bytes_sent = sendto(sock_udp, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
        printf("Error a l'enviar pdu.");
        printf("%d\n", errno);
    }
    sprintf(message, "Enviat 1r ALIVE -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printf(message);
    

    
    /* SELECT */
    struct timeval timeout;
    timeout.tv_sec = r;
    timeout.tv_usec = 0;
    
    fd_set rset;
    
    int maxsock = sock_udp + 1;

    ssize_t bytes_received;
    socklen_t len;
    time_t actual_time, time_alive;
    char buffer[84];
    char buffer_tcp[127];
    FD_ZERO(&rset);
    FD_SET(sock_udp, &rset);

    select(maxsock, &rset, NULL, NULL, &timeout);

    if (FD_ISSET(sock_udp, &rset)) {
        len = sizeof(serveraddr);
        bytes_received = recvfrom(sock_udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
        struct PduUdp pdu_received = unpackPduUdp(buffer, bytes_received);
        if (pdu.packet_type == ALIVE) {
            if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                if (strcmp(server.id_communication, pdu_received.id_communication) == 0){
                    if (strncmp(client.id_client, pdu_received.data, sizeof(client.id_client)) == 0){
                        if (client.state == REGISTERED) {
                            client.state = SEND_ALIVE;
                            time_alive = time(NULL);
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp dades (rebut: %s, esperat: %s)", pdu_received.data, client.id_client);
                        printTerminal(message, DEBUG);
                        client.state = NOT_REGISTERED;
                        printClientState();
                        pthread_exit(NULL);
                        return NULL;
                    }
                }
                else {
                    sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                    pthread_exit(NULL);
                    return NULL;
                }    
            } 
            else {
                sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", serveraddr.sin_port, pdu_received.id_transmitter);
                printTerminal(message, DEBUG);
                client.state = NOT_REGISTERED;
                printClientState();
                pthread_exit(NULL);
                return NULL;
            }
        }
        else if (pdu.packet_type == ALIVE_REJ) {
            printTerminal("Rebut paquet ALIVE_REJ", DEBUG);
            client.state = NOT_REGISTERED;
            printClientState();
        }
    }
    else {
        client.state = NOT_REGISTERED;
        printClientState();
        pthread_exit(NULL);
        return NULL;
    }

    
    pthread_t thread_SENDALIVE;
    if (pthread_create(&thread_SENDALIVE, NULL, &sendAlives, (void *)&argAlive) != 0) {
        printTerminal("Error al crear el thread", ERROR);
    }

    
    int sock_tcp, conn_tcp;
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tcpaddr;

    tcpaddr.sin_family = AF_INET;
    tcpaddr.sin_port = htons(client.tcp_port);
    tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sock_tcp, (struct sockaddr*)&tcpaddr, sizeof(tcpaddr));
    listen(sock_tcp, 10);

    FD_ZERO(&rset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    maxsock = max(sock_udp, sock_tcp) + 1;

    while(client.state == SEND_ALIVE) {
        FD_SET(sock_udp, &rset);
        FD_SET(sock_tcp, &rset);
        FD_SET(STDIN, &rset);

        select(maxsock, &rset, NULL, NULL, &timeout);

        //UDP
        if (FD_ISSET(sock_udp, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock_udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            struct PduUdp pdu_received = unpackPduUdp(buffer, bytes_received);
            if (pdu_received.packet_type == ALIVE) {
                if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                    if (strcmp(server.id_communication, pdu_received.id_communication) == 0){
                        if (strncmp(client.id_client, pdu_received.data, sizeof(client.id_client)) == 0){
                            printTerminal("REBUT PAQUET ALIVE CORRECTE", DEBUG);
                            //Reiniciar temps ALIVE (per als 3 consecutius)
                            time_alive = time(NULL);
                        }
                        else {
                            sprintf(message, "Error en el valor del camp dades (rebut: %s, esperat: %s)", pdu_received.data, client.id_client);
                            printTerminal(message, DEBUG);
                            client.state = NOT_REGISTERED;
                            printClientState();
                            pthread_exit(NULL);
                            return NULL;
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                        printTerminal(message, DEBUG);
                        client.state = NOT_REGISTERED;
                        printClientState();
                        pthread_exit(NULL);
                        return NULL;
                    }    
                } 
                else {
                    sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", serveraddr.sin_port, pdu_received.id_transmitter);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                    pthread_exit(NULL);
                    return NULL;
                }
            }
            else if (pdu_received.packet_type == ALIVE_REJ) {
                printTerminal("Rebut paquet ALIVE_REJ", DEBUG);
                client.state = NOT_REGISTERED;
                printClientState();
                break;
            }
        }

        //TCP
        if (FD_ISSET(sock_tcp, &rset)) {
            len = sizeof(tcpaddr);
            conn_tcp = accept(sock_tcp, (struct sockaddr*)&tcpaddr, &len);
            bytes_received = read(conn_tcp, buffer_tcp, sizeof(buffer_tcp));
            handleSetGet(conn_tcp, tcpaddr, buffer_tcp, bytes_received);
        }

        //LLEGIR TERMINAL
        if (FD_ISSET(STDIN, &rset)) {
            fgets(buffer, sizeof(buffer), stdin);
            handleCommands(buffer);
        }


        //Comprovació 3 alives consecutius
        actual_time = time(NULL);  

        if ((actual_time - time_alive) > 6) {
            client.state = DISCONNECTED;
            printClientState();
            printTerminal("S'ha perdut la connexió amb el servidor al no rebre 3 ALIVE consecutius", MSG);
        }
    }

    close(sock_tcp);

    free(message);
    pthread_exit(NULL);
    return NULL;
}


void handleSetGet(int conn_tcp, struct sockaddr_in tcpaddr, char* buffer, ssize_t bytes_received) {
    char* message = malloc(sizeof(char)*1000);
    struct PduTcp pdu_received = unpackPduTcp(buffer, bytes_received);
    int bytes_sent;
    struct PduTcp pdu;
    if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
        if (strcmp(server.id_communication, pdu_received.id_communication) == 0) {
            if (strcmp(pdu_received.info, client.id_client) == 0) {
                bool found = false;
                for (int i = 0; i < client.num_elements; i++) {
                    if(strcmp(pdu_received.element, client.elements[i].element) == 0) {
                        found = true;
                        char* info = getHour();
                        if (pdu_received.packet_type == SET_DATA && (strcmp(&pdu_received.element[6], "I") == 0)) {                             
                            strcpy(client.elements[i].value, pdu_received.value);
                            pdu = packPduTcp(DATA_ACK, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, info);
                            bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
                            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
                            printf(message);
                        }
                        else if (pdu_received.packet_type == GET_DATA) {
                            pdu = packPduTcp(DATA_ACK, client.id_client, server.id_communication, pdu_received.element, client.elements[i].value, info);
                            bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
                            
                            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
                            printf(message);
                        }
                    }
                }
                if (!found) {
                    sprintf(message, "Element: [%s] no pertany al dispositiu", pdu_received.element);
                    printTerminal(message, DEBUG);
                    pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Element incorrecte");
                    bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
                    sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
                    printf(message);
                }
            }
            else {
                sprintf(message, "Error en el valor del camp info (rebut: %s, esperat: %s)", pdu_received.info, client.id_client);
                printTerminal(message, DEBUG);
                pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Camp info incorrecte");
                bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
                sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
                printf(message);
                client.state = NOT_REGISTERED;
                printClientState();
            }
        }
        else {
            sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
            printTerminal(message, DEBUG);
            pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Id. comunicació incorrecte");
            bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
            printf(message);
            client.state = NOT_REGISTERED;
            printClientState();
        }    
    } 
    else {
        sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", tcpaddr.sin_port, pdu_received.id_transmitter);
        printTerminal(message, DEBUG);
        pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Id. transmitter incorrecte");
        bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
        
        sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
        printf(message);
        client.state = NOT_REGISTERED;
        printClientState();
    }
}


char* getHour() {
    int year, month, day, hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    static char info[80];

    year = 1900 + local->tm_year;
    month = local->tm_mon;
    day = local->tm_mday;
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;

    sprintf(info, "%d-%02d-%d;%02d:%02d:%02d", year, month, day, hours, minutes, seconds);
    return info;
}


void removeNewLine(char* s) {
    int length = strlen(s);
    if ((length > 0) && (s[length-1] == '\n'))
        s[length - 1] = '\0';
}



void handleCommands(char* buffer) {
    char* message = malloc(sizeof(char)*1000);
    removeNewLine(buffer);
    char delimiter[] = " ";
    char* command = strtok(buffer, delimiter);

    if (command != NULL) {
        if (strcasecmp(command, "stat") == 0) {
            printf("********************* DADES DISPOSITIU ***********************\n");
            printf("  Identificador: %s\n", client.id_client);
            printf("  Estat: %x\n", client.state);
            printf("    Param  \tvalor\n");
            printf("    -------\t---------------\n");
            for (int i = 0; i < client.num_elements; i++) {
                printf("    %s\t%s\n", client.elements[i].element, client.elements[i].value);
            }
            printf("**************************************************************\n");
        }
        else if (strcasecmp(command, "set") == 0) {
            char* id_element = strtok(NULL, delimiter);
            char* new_value = strtok(NULL, delimiter);
            if (id_element != NULL && new_value != NULL) {
                sprintf(message, "ID element: %s", id_element);
                printTerminal(message, DEBUG);
                sprintf(message, "Nou valor: %s", new_value);
                printTerminal(message, DEBUG);
                bool found = false;
                for (int i = 0; i < client.num_elements; i++) {
                    if(strcmp(id_element, client.elements[i].element) == 0) {
                        strcpy(client.elements[i].value, new_value);
                        found = true;
                    }
                }
                if (!found) {
                    sprintf(message, "Element: [%s] no pertany al dispositiu", id_element);
                    printTerminal(message, DEBUG);
                } 
            }
            else {
                printTerminal("Error de sintàxi. (set <element> <valor>)", MSG);
            }  
        }
        else if (strcasecmp(command, "send") == 0) {
            char* id_element = strtok(NULL, delimiter);
            char value[16];
            if (id_element != NULL) {
                bool found = false;
                for (int i = 0; i < client.num_elements; i++) {
                    if(strcmp(id_element, client.elements[i].element) == 0) {
                        strcpy(value, client.elements[i].value);
                        found = true;
                    }
                }
                if (!found) {
                    sprintf(message, "Element: [%s] no pertany al dispositiu", id_element);
                    printTerminal(message, DEBUG);
                    return;
                } 
            }
            else {
                printTerminal("Error de sintàxi. (send <element>)", MSG);
                return;
            }


            int socktcp;

            if ((socktcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printTerminal("Error creació socket", ERROR);
            }

            struct sockaddr_in tcp_addr;

            tcp_addr.sin_family = AF_INET;
            tcp_addr.sin_port = htons(server.tcp_port);
            tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (connect(socktcp, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
                printTerminal("TCP Connect ha fallat", ERROR);
                return;
            }
            sprintf(message, "Iniciada comunicació TCP amb el servidor (port: %d)", server.tcp_port);
            printTerminal(message, MSG);

            

            char* info;
            info = getHour();
            int bytes_sent;

            struct PduTcp pdu;
            pdu = packPduTcp(SEND_DATA, client.id_client, server.id_communication, id_element, value, info);
            bytes_sent = write(socktcp, &pdu, sizeof(pdu));
            
            sprintf(message, "Enviat -> bytes: %d  paquet: %x  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, pdu.packet_type, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
            printf(message);

            /* SELECT */
            struct timeval timeout;
            timeout.tv_sec = m;
            timeout.tv_usec = 0;
            
            fd_set rset;
            int maxsock, bytes_received;
            char buffer_tcp[127];

            maxsock = socktcp + 1;

            FD_ZERO(&rset);
            FD_SET(socktcp, &rset);

            select(maxsock, &rset, NULL, NULL, &timeout);

            if (FD_ISSET(socktcp, &rset)) {
                bytes_received = read(socktcp, buffer_tcp, sizeof(buffer_tcp));
                struct PduTcp pdu_received;
                pdu_received = unpackPduTcp(buffer_tcp, bytes_received);
                if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                    if (strcmp(server.id_communication, pdu_received.id_communication) == 0){
                        if (pdu_received.packet_type == DATA_ACK) {
                            if (strcmp(id_element, pdu_received.element) == 0) {
                                if (strcmp(pdu_received.info, client.id_client) == 0) {
                                    sprintf(message, "Acceptat l'enviament de dades (element: %s, valor: %s). Info: %s", id_element, value, pdu_received.info);
                                    printTerminal(message, MSG);
                                }
                                else {
                                    sprintf(message, "Error en el valor del camp info (rebut: %s, esperat: %s)", pdu_received.info, client.id_client);
                                    printTerminal(message, DEBUG);
                                    client.state = NOT_REGISTERED;
                                    printClientState();
                                }
                            }
                            else {
                                sprintf(message, "Error en el valor del camp element (rebut: %s, esperat: %s)", pdu_received.element, id_element);
                                printTerminal(message, DEBUG);
                                client.state = NOT_REGISTERED;
                                printClientState();
                            }
                        }
                        else if (pdu_received.packet_type == DATA_NACK) {
                            printTerminal("Rebut DATA NACK", DEBUG);
                        }
                        else if (pdu_received.packet_type == DATA_REJ) {
                            printTerminal("Rebut DATA_REJ", DEBUG);
                            client.state = NOT_REGISTERED;
                            printClientState();
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                        printTerminal(message, DEBUG);
                        client.state = NOT_REGISTERED;
                        printClientState();
                    }    
                } 
                else {
                    sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", tcp_addr.sin_port, pdu_received.id_transmitter);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                }
            }

            close(socktcp);
            sprintf(message, "Finalitzada comunicació TCP amb el servidor (port: %d)", server.tcp_port);
            printTerminal(message, MSG);


        }
        else if (strcasecmp(command, "quit") == 0) {
            //close(sock_udp);
            //close(sock_tcp);
            //NEED TO FIX QUIT
            exit(1);
        }
        else {
            sprintf(message, "Commanda incorrecta (%s)", command);
            printTerminal(message, MSG);
        }
    }
    free(message);
}


void parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = true;
        }
        else if (strcmp(argv[i], "-u") == 0) {
            if (argv[i+1]) {
                strcpy(clientFile, argv[i+1]);
            }
            else {
                printTerminal("Arxiu de configuració no especificat", ERROR);
                exit(1);
            }
        }
    }
}



void setup() {
    char* message = malloc(sizeof(char)*1000);
    int sock_udp;

    struct sockaddr_in serveraddr;
    //struct hostent *he;

    /*if ((he = gethostbyname(client.server)) == NULL) {
        printTerminal("No és possible obtenir gethostbyname", ERROR);
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
    
    while (register_number <= 3) {
        sprintf(message, "Procés de subscripció: %d", register_number);
        printTerminal(message, MSG);
        if (pthread_create(&thread_REGISTER, NULL, &registerPhase, (void *)&args) != 0) {
            printTerminal("ERROR en la creació del procés", ERROR);
        }
        
        pthread_join(thread_REGISTER, NULL);

        if (client.state == REGISTERED) {
            if (pthread_create(&thread_ALIVE, NULL, &handleAlive, (void *)&args) != 0) {
                printTerminal("ERROR en la creació del procés", ERROR);
            }
            pthread_join(thread_ALIVE, NULL);
        }
        
        
        register_number += 1;
    }
    sprintf(message, "Superat el nombre de processos de subscripció (%d)", register_number - 1);
    printTerminal(message, MSG);

    free(message);
    exit(1);
}



int main(int argc, char* argv[]) {
    parseArgs(argc, argv);
    readFile();
    printTerminal("Llegit arxiu de configuració correctament", DEBUG);
    setup();
}
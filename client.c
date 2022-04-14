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
un booleà que ens ajudarà a saber si hem completad el registre, un altre per fer un nou procés de registre, un altre booleà per saber si s'ha
especificat l'opció "-d" al executar el client i un booleà quit per saber si s'ha introduït la comanda per terminal i fer el pertinent*/
char clientFile[] = "client.cfg";
int register_number = 1;
bool completed_register_phase = false;
bool end_register_phase = false;
bool debug_mode = false;
bool debug_mode_2 = false;
bool quit = false;

//Tipus de paquet
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

//Estats
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
#define DEBUG2 "DEBUG2"


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


//Struct per guardar els arguments del thread
struct handle_udp_struct {
    char* data_received;
    ssize_t bytes_received;
} argHandleUdp;


/* Constants de temps */
#define t 1
#define u 2
#define n 8
#define o 3
#define p 2
#define q 4
#define v 2
#define r 2
#define s 3
#define m 3


/* Declaració funcions */
void removeSpaces(char* str);
void printTerminal(char* message, char* level);
void readFile();
void setup();
void* handleUdpPacket(void* argHandleUdp);
struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data);
struct PduUdp unpackPduUdp(char* data, ssize_t bytes_received);
struct PduTcp packPduTcp(int packet_type, char* id_transmitter, char* id_communication, char* element, char* value, char* info);
struct PduTcp unpackPduTcp(char* data, ssize_t bytes_received);
void* registerPhase(void* argp);
void* handleAlive(void* argp);
void* sendAlives(void* argAlive);
void* handleCommands(void* unused);
void removeNewLine(char* str);
char* getHour();
void parseArgs(int argc, char* argv[]);
char* packetTypeConverter(int packet_type);
void sendUdp(int sock, struct sockaddr_in serveraddr, struct PduUdp pdu);
void sendTcp(int conn_tcp, struct PduTcp pdu);


//Funció auxiliar per borrar espais (si n'hi ha) a un array de chars
void removeSpaces(char* str) {
    char* d = str;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while ((*str++ = *d++));
}


//Funció auxiliar per calcular el màxim entre dos enters
int max (int x, int y) {
    if (x > y)
        return x;
    else
        return y;
}


//Funció auxiliar que permet printejar l'estat del client amb forma de cadena de caràcters
void printClientState() {
    //Utilizaré sempre un malloc per guardar memòria per fer tots els prints de cada nivell després (printTerminal)
    char* message = malloc(sizeof(char)*1000);
    char* state;
    if (client.state == DISCONNECTED) {
        state = "DISCONNECTED";
    }
    else if (client.state == NOT_REGISTERED) {
        state = "NOT_REGISTERED";
    }
    else if (client.state == WAIT_ACK_REG) {
        state = "WAIT_ACK_REG";
    }
    else if (client.state == WAIT_INFO) {
        state = "WAIT_INFO";
    }
    else if (client.state == WAIT_ACK_INFO) {
        state = "WAIT_ACK_INFO";
    }
    else if (client.state == REGISTERED) {
        state = "REGISTERED";
    }
    else if (client.state == SEND_ALIVE) {
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
    else if (strcmp(level, DEBUG2) == 0) {
        if (debug_mode_2) {
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
    //Obrir l'arxiu en mode lectura
    fp = fopen(clientFile, "r");
    //Comprovar si s'ha produït algun error a l'obrir l'arxiu, si és així, informar per terminal i acabar el programa
    if (fp == NULL) {
        sprintf(message, "No es pot obrir l'arxiu de configuració: %s", clientFile);
        printTerminal(message, ERROR);
        exit(1);
    }
    //Utilitzar fgets per llegir cada linía de l'arxiu per separat
    while (fgets(line, sizeof(line), fp) != NULL) {
        //Ús de la funció strtok per fer un split de la línia fins al caràcter "=" (variable delimiter)
        key = strtok(line, delimiter);
        //Un altre split fins al caràcter "\n" que ens servirà per obtenir el que hi ha a la dreta del "="
        value = strtok(NULL, "\n");
        removeSpaces(key);
        removeSpaces(value);
        
        if (lineNumber == 0){
            if (strcmp(key, "Id") == 0){
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
            if (strcmp(key, "Elements") == 0) {
                char* element;
                int i = 0;
                //Split per obtenir cada element per separat mitjançant el delimitador ";" i un bucle
                element = strtok(value, ";");
                printTerminal("Assignats elements: ", INFO);
                while (element != NULL) {
                    strncpy(client.elements[i].element, element, 7);
                    //Assignem al final de la cadena de caràcters el caràcter '\0' per marcar el final
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
            if (strcmp(key, "Local-TCP") == 0) {
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
            if (strcmp(key, "Server") == 0) {
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
            if (strcmp(key, "Server-UDP") == 0) {
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
    //Tancar descriptor de fitxer
    fclose(fp);

    free(message);
}


//Funció auxiliar per convertir el tipus de paquet a cadena de caràcters per mostrar-ho per la terminal
char* packetTypeConverter(int packet_type) {
    char* packet_type_s;
    if (packet_type == REG_REQ) {
        packet_type_s = "REG_REQ";
    }
    else if (packet_type == REG_ACK) {
        packet_type_s = "REG_ACK";
    }
    else if (packet_type == REG_NACK) {
        packet_type_s = "REG_NACK";
    }
    else if (packet_type == REG_REJ) {
        packet_type_s = "REG_REJ";
    }
    else if (packet_type == REG_INFO) {
        packet_type_s = "REG_INFO";
    }
    else if (packet_type == INFO_ACK) {
        packet_type_s = "INFO_ACK";
    }
    else if (packet_type == INFO_NACK) {
        packet_type_s = "INFO_NACK";
    }
    else if (packet_type == INFO_REJ) {
        packet_type_s = "INFO_REJ";
    }
    else if (packet_type == ALIVE) {
        packet_type_s = "ALIVE";
    }
    else if (packet_type == ALIVE_NACK) {
        packet_type_s = "ALIVE_NACK";
    }
    else if (packet_type == ALIVE_REJ) {
        packet_type_s = "ALIVE_REJ";
    }
    else if (packet_type == SEND_DATA) {
        packet_type_s = "SEND_DATA";
    }
    else if (packet_type == DATA_ACK) {
        packet_type_s = "DATA_ACK";
    }
    else if (packet_type == DATA_NACK) {
        packet_type_s = "DATA_NACK";
    }
    else if (packet_type == DATA_REJ) {
        packet_type_s = "DATA_REJ";
    }
    else if (packet_type == SET_DATA) {
        packet_type_s = "SET_DATA";
    }
    else if (packet_type == GET_DATA) {
        packet_type_s = "GET_DATA";
    }
    return packet_type_s;
} 


//Funció per fer un empaquetament de tota la informació a enviar del tipus UDP dins d'una struct PduUdp
struct PduUdp packPduUdp(int packet_type, char* id_transmitter, char* id_communication, char* data){
    char* message = malloc(sizeof(char)*1000);   

    printTerminal("---------------- PACK PDU UDP --------------", DEBUG2);
    struct PduUdp pdu;

    pdu.packet_type = packet_type;
    char* packet_type_s =  packetTypeConverter(packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG2);
    //Utilitzar memcpy per assegurar que només es copien els bytes desitjats
    memcpy(pdu.id_transmitter, id_transmitter, sizeof(char)*11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG2);

    memcpy(pdu.id_communication, id_communication, sizeof(char)*11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG2);

    memcpy(pdu.data, data, sizeof(char)*61);
    sprintf(message, "Dades: %s", pdu.data);
    printTerminal(message, DEBUG2);

    printTerminal("---------------- END PACK PDU UDP --------------", DEBUG2);

    free(message);
    return pdu;
}


//Funció per fer un empaquetament de tota la informació a enviar de tipus TCP dins d'una struct PduTcp
struct PduTcp packPduTcp(int packet_type, char* id_transmitter, char* id_communication, char* element, char* value, char* info){
    char* message = malloc(sizeof(char)*1000);   

    printTerminal("---------------- PACK PDU TCP --------------", DEBUG2);
    struct PduTcp pdu;

    pdu.packet_type = packet_type;
    char* packet_type_s =  packetTypeConverter(packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG2);
    
    memcpy(pdu.id_transmitter, id_transmitter, sizeof(char)*11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG2);

    memcpy(pdu.id_communication, id_communication, sizeof(char)*11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG2);

    memcpy(pdu.element, element, sizeof(char)*8);
    sprintf(message, "Element: %s", pdu.element);
    printTerminal(message, DEBUG2);

    memcpy(pdu.value, value, sizeof(char)*16);
    sprintf(message, "Value: %s", pdu.value);
    printTerminal(message, DEBUG2);

    memcpy(pdu.info, info, sizeof(char)*80);
    sprintf(message, "Info: %s", pdu.info);
    printTerminal(message, DEBUG2);

    printTerminal("---------------- END PACK PDU TCP --------------", DEBUG2);

    free(message);
    return pdu;
}


/* Funció encarregada de passar la informació rebuda de tipus TCP a una struct PduTcp on la informació estarà separada 
pels diferents camps per després poder evaluar la correctesa d'aquesta i fer el pertinent */
struct PduTcp unpackPduTcp(char* data, ssize_t bytes_received) {
    char* message = malloc(sizeof(char) * 1000);
    
    printTerminal("---------------- UNPACK PDU TCP --------------", DEBUG2);
    struct PduTcp pdu;
    memset(&pdu, 0, sizeof(pdu));
    pdu.packet_type = data[0];
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG2);
    memcpy(pdu.id_transmitter, data + 1, sizeof(char) * 11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG2);
    memcpy(pdu.id_communication, data + 12, sizeof(char) * 11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG2);
    memcpy(pdu.element, data + 23, sizeof(char) * 8);
    sprintf(message, "Element: %s", pdu.element);
    printTerminal(message, DEBUG2);
    memcpy(pdu.value, data + 31, sizeof(char) * 16);
    sprintf(message, "Value: %s", pdu.value);
    printTerminal(message, DEBUG2);
    memcpy(pdu.info, data + 47, sizeof(char) * 80);
    sprintf(message, "Info: %s", pdu.info);
    printTerminal(message, DEBUG2);
    printTerminal("---------------- END UNPACK PDU TCP --------------\n", DEBUG2);

    sprintf(message, "Rebut -> bytes: %ld  paquet: %s  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_received, packet_type_s, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
    printTerminal(message, DEBUG);

    free(message);
    return pdu;
}


//Funció encarregada d'enviar per un socket udp la pdu UDP passada com a argument (ús de la funció sendto)
void sendUdp(int sock, struct sockaddr_in serveraddr, struct PduUdp pdu) {
    char* message = malloc(sizeof(char)*1000);
    int bytes_sent;
    if ((bytes_sent = sendto(sock, &pdu, sizeof(pdu), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) == -1) {
        printTerminal("Error a l'enviar pdu.", ERROR);
    }
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    sprintf(message, "Enviat -> bytes: %d  paquet: %s  id transmissor: %s  id comunicació: %s  dades: %s", bytes_sent, packet_type_s, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printTerminal(message, DEBUG);
}


//Funció encarregada d'enviar per una connexió tcp la pdu TCP passada com a argument mitjannçant la funció write
void sendTcp(int conn_tcp, struct PduTcp pdu) {
    char* message = malloc(sizeof(char)*1000);
    int bytes_sent;
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    bytes_sent = write(conn_tcp, &pdu, sizeof(pdu));
    sprintf(message, "Enviat -> bytes: %d  paquet: %s  id transmissor: %s  id comunicació: %s  element: %s  valor: %s  info: %s", bytes_sent, packet_type_s, pdu.id_transmitter, pdu.id_communication, pdu.element, pdu.value, pdu.info);
    printTerminal(message, DEBUG);
}


/*Funció cridada mitjançant un pthread (retorna un void* i els arguments han de ser de tipus void*) encarregada 
d'analitzar el paquet de tipus UDP rebut durant la fase de registre del client*/
void* handleUdpPacket(void* argHandleUdp) {
    char* message = malloc(sizeof(char)*1000);

    //Obtenir els arguments guardats a la struct global argHandleUdp
    struct handle_udp_struct *args = argHandleUdp;
    char* data_received = args->data_received;
    ssize_t bytes_received = args->bytes_received;

    struct PduUdp pdu;
    pdu = unpackPduUdp(data_received, bytes_received);

    //Només podem obtenir el paquet de tipus REG_ACK si el client es troba en l'estat WAIT_ACK_REG
    if (client.state == WAIT_ACK_REG && pdu.packet_type == REG_ACK) {
        printTerminal("Rebut paquet REG_ACK", DEBUG2);
        //Guardar la informació rebuda al paquet REG_ACK que necessitarem posteriorment (id, id. com. i port del servidor)
        strncpy(server.id_server, pdu.id_transmitter, 11);
        strncpy(server.id_communication, pdu.id_communication, 11);
        server.udp_port = atoi(pdu.data);

        //Creació del socket UDP amb el port UDP rebut al camp data del paquet REG_ACK
        int sock_udp_server;
        sock_udp_server = socket(AF_INET, SOCK_DGRAM, 0);
    
        struct sockaddr_in serveraddrnew;
        serveraddrnew.sin_family = AF_INET;
        serveraddrnew.sin_port = htons(server.udp_port);
        serveraddrnew.sin_addr.s_addr = htonl(INADDR_ANY);

        /*Creació d'una cadena de caràcters per concatenar el port TCP del client i els diferents elements guardats a la 
        struct Client que, a la seva vegada, conté un struct Element amb el id del element i el seu valor */
        char data[5 + 1 + 7*5+4 + 1];
        char* semicolon = ";";
        sprintf(data, "%d,", client.tcp_port);
        for (int i = 0; i < client.num_elements; i++) {
            strcat(data, client.elements[i].element);
            if (i < client.num_elements - 1) {
                strcat(data, semicolon);
            }
        }

        //Contestació amb l'enviament d'un paquet REG_INFO
        struct PduUdp pduREG_INFO = packPduUdp(REG_INFO, client.id_client, server.id_communication, data);
        sendUdp(sock_udp_server, serveraddrnew, pduREG_INFO);

        client.state = WAIT_ACK_INFO;
        printClientState();
        
        /* SELECT */
        //Ús de la struct timeval per definir el temps que ha d'esperar la funció select per detectar si arriba un paquet
        struct timeval timeout;
        timeout.tv_sec = 2*t;
        timeout.tv_usec = 0;
        
        //Creació d'un set que contindrà els sockets (descriptors de fitxer)
        fd_set rset;
        
        //Definir un enter amb el número de descriptor de fitxer més gran + 1
        int maxsock = sock_udp_server + 1;
        ssize_t bytes_received;
        socklen_t len;
        char buffer[84];

        //Borrar tots els descriptors de fitxer del set
        FD_ZERO(&rset);
        //Afegir el descriptor de fitxer udp al set
        FD_SET(sock_udp_server, &rset);
        
        /* Esperar fins que el timeout no es NULL, ja que quan el select s'hagi esperat el timeout especificat 
        el posarà a NULL, i sabrem que no s'ha rebut un paquet si seguim encara dins del bucle */
        while(timeout.tv_sec) {

            /* Funció select encarregada d'esperar a que un descriptor de fitxer estigui preparat, 
            en el nostre cas només en lectura durant el timeout especificat anteriorment */
            select(maxsock, &rset, NULL, NULL, &timeout);

            //Comprovem si el descriptor de fitxer encara està present al set, i per conseqüent sabem que és de tipus UDP
            if (FD_ISSET(sock_udp_server, &rset)) {
                len = sizeof(serveraddrnew);
                //Obtenir la informació del socket i emmagatzemar-la al buffer
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
                                    //Acabar registre ja que s'ha completat correctament
                                    completed_register_phase = true;
                                    //Sortir i finalitzar el thread
                                    return NULL;
                                }
                                else if (pdu_received.packet_type == INFO_NACK) {
                                    sprintf(message, "Descartat paquet de informació adicional de subscripció, motiu: %s", pdu_received.data);
                                    printTerminal(message, INFO);
                                    client.state = NOT_REGISTERED;
                                    printClientState();
                                    //Continuar el registre
                                    return NULL;
                                }
                                else {
                                    printTerminal("Paquet incorrecte", INFO);
                                    client.state = NOT_REGISTERED;
                                    printClientState();
                                    //Iniciar un nou procés de registre
                                    end_register_phase = true;
                                    break;
                                }
                            }
                            else {
                                sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                                printTerminal(message, DEBUG);
                                client.state = NOT_REGISTERED;
                                printClientState();
                                //Iniciar un nou procés de registre
                                end_register_phase = true;
                                break;
                            }
                            
                        } 
                        else {
                            sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %s, id: %s)", server.id_server, pdu_received.id_transmitter);
                            printTerminal(message, DEBUG);
                            client.state = NOT_REGISTERED;
                            printClientState();
                            //Iniciar un nou procés de registre
                            end_register_phase = true;
                            break;
                        }
                    }
                    else if (pdu_received.packet_type == REG_NACK) {
                        sprintf(message, "Descartat paquet de subscripció enviat, motiu: %s", pdu_received.data);
                        printTerminal(message, INFO);
                        client.state = NOT_REGISTERED;
                        printClientState();
                        //Continuar el registre
                        break;
                    }
                    else if (pdu_received.packet_type == REG_REJ) {
                        client.state = NOT_REGISTERED;
                        printClientState();
                        //Iniciar un nou procés de registre
                        end_register_phase = true;
                        break;
                    }
                    else {
                        printTerminal("Paquet desconegut", INFO);
                        client.state = NOT_REGISTERED;
                        printClientState();
                        //Iniciar un nou procés de registre
                        end_register_phase = true;
                        break;
                    }
                }
            }            
        }
        if (client.state != NOT_REGISTERED) {
            printTerminal("Temporització per manca de resposta al paquet enviat: REG_INFO", DEBUG);
            client.state = NOT_REGISTERED;
            printClientState();
            //Iniciar un nou procés de registre
            end_register_phase = true;
        }
        
    } 
    else if (pdu.packet_type == REG_NACK) {
        sprintf(message, "Descartat paquet de subscripció enviat, motiu: %s", pdu.data);
        printTerminal(message, INFO);
        client.state = NOT_REGISTERED;
        printClientState();
        //Continuar el registre
    }
    else if (pdu.packet_type == REG_REJ) {
        client.state = NOT_REGISTERED;
        printClientState();
        //Iniciar un nou procés de registre
        end_register_phase = true;
    }
    else {
        client.state = NOT_REGISTERED;
        printClientState();
        //Iniciar un nou procés de registre
        end_register_phase = true;
    }
    free(message);
    return NULL;
}


//Funció encarregada de la fase de registre
void* registerPhase(void* argp) {
    char* message = malloc(sizeof(char)*1000);

    struct register_arg_struct *args = argp;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    client.state = NOT_REGISTERED;
    printClientState();
    
    struct PduUdp pdu;
    pdu = packPduUdp(REG_REQ, client.id_client, "0000000000", "");

    int packets_sent = 0;
    sendUdp(sock, serveraddr, pdu);
    packets_sent += 1;
    sprintf(message, "Num total paquets enviats: %d", packets_sent);
    printTerminal(message, DEBUG2);
    printTerminal("Paquet 1 REG_REQ enviat", DEBUG2);

    client.state = WAIT_ACK_REG;
    printClientState();

    
    /* SELECT */
    struct timeval timeout;
    timeout.tv_sec = t;
    timeout.tv_usec = 0;
    int timeout_s = t;
    
    fd_set rset;
    
    int maxsock = sock + 1;

    ssize_t bytes_received;
    socklen_t len;
    char buffer[84] = {0};
    FD_ZERO(&rset);

    while (1) {
        
        FD_SET(sock, &rset);

        select(maxsock, &rset, NULL, NULL, &timeout);

        if (FD_ISSET(sock, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            printTerminal("Rebut paquet UDP, creat procés per atendre'l", DEBUG);
            argHandleUdp.data_received = buffer;
            argHandleUdp.bytes_received = bytes_received;
            //Creació d'un thread encarregat de processar el paquet rebut pel descriptor de fitxer
            pthread_t thread_HANDLEUDP;
            if (pthread_create(&thread_HANDLEUDP, NULL, &handleUdpPacket, (void *)&argHandleUdp) != 0) {
                printTerminal("Error al crear el thread", ERROR);
            }
            pthread_join(thread_HANDLEUDP, NULL);
            if (end_register_phase) {
                //Acabar el registre per iniciar un nou procés de registre
                end_register_phase = false;
                printTerminal("Esperant 2 s", DEBUG2);
                sleep(u);
                return NULL;
            }
            else if (completed_register_phase) {
                //Acabar el registre perquè el dispositiu passa a REGISTERED (s'ha completat la fase de registre correctament)
                completed_register_phase = false;
                return NULL;
            }
            else {
                //Iniciar enviament REG_REQ sense iniciar nou procés de registre
                packets_sent = 0;
            }
        }
        if (packets_sent < n) {
            sendUdp(sock, serveraddr, pdu);
            //Iniciar l'enviament de REG_REQ sense nou procés de registre
            if (packets_sent == 0) {
                client.state = WAIT_ACK_REG;
                printClientState();
                timeout.tv_sec = t;
            }
            //Incrementar timeout fins a q * t
            if (packets_sent != 0 && timeout_s < (q * t)) {
                timeout_s = timeout_s + t;
                timeout.tv_sec = timeout_s;
            }
            //Mantenir timeout a q * t fins que s'arribi al limit de paquets a enviar (n)
            else if (packets_sent < n && timeout_s == (q * t)) {
                timeout.tv_sec = timeout_s;
            }

            packets_sent += 1;
            sprintf(message, "Num total paquets enviats: %d", packets_sent);
            printTerminal(message, DEBUG2);

            sprintf(message, "Timeout: %ld", timeout.tv_sec);
            printTerminal(message, DEBUG2);
        }
        else if (packets_sent == n) {
            break;
        }
    }
    printTerminal("Esperant 2 s", DEBUG2);
    sleep(u);
    free(message);
    return NULL;
}


/* Funció encarregada de passar la informació rebuda de tipus UDP a una struct PduUdp on la informació estarà separada 
pels diferents camps per després poder evaluar la correctesa d'aquesta i fer el pertinent */
struct PduUdp unpackPduUdp(char* data, ssize_t bytes_received) {
    char* message = malloc(sizeof(char) * 1000);

    printTerminal("---------------- UNPACK PDU UDP --------------", DEBUG2);
    struct PduUdp pdu;
    memset(&pdu, 0, sizeof(pdu));
    pdu.packet_type = data[0];
    char* packet_type_s =  packetTypeConverter(pdu.packet_type);
    sprintf(message, "Tipus paquet: %s", packet_type_s);
    printTerminal(message, DEBUG2);
    memcpy(pdu.id_transmitter, data + 1, sizeof(char) * 11);
    sprintf(message, "Id transmissor: %s", pdu.id_transmitter);
    printTerminal(message, DEBUG2);
    memcpy(pdu.id_communication, data + 12, sizeof(char) * 11);
    sprintf(message, "Id comunicació: %s", pdu.id_communication);
    printTerminal(message, DEBUG2);
    memcpy(pdu.data, data + 23, sizeof(char) * 61);
    sprintf(message, "Dades: %s", pdu.data);
    printTerminal(message, DEBUG2);
    printTerminal("---------------- END UNPACK PDU UDP --------------", DEBUG2);

    sprintf(message, "Rebut -> bytes: %ld  paquet: %s  id transmissor: %s  id comunicació: %s  dades: %s", bytes_received, packet_type_s, pdu.id_transmitter, pdu.id_communication, pdu.data);
    printTerminal(message, DEBUG);

    free(message);
    return pdu;
}


//Funció cridada per un pthread encarregada d'enviar paquets ALIVE cada v segons mentre el client es trobi en l'estat REGISTERED o SEND_ALIVE
void* sendAlives(void* argAlive) {
    char* message = malloc(sizeof(char)*1000);
    printTerminal("Creat procés per enviament periòdic de ALIVE", DEBUG);
    struct alive_arg_struct *args = argAlive;
    int sock = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;
    struct PduUdp pdu = args->pdu;

    //Ús de la funció time() i el tipus time_t per comptar el temps entre els diferents ALIVE a enviar
    time_t start_t, end_t, total_t;
    //Enviar primer ALIVE
    sendUdp(sock, serveraddr, pdu);
    start_t = time(NULL);

    while (client.state == REGISTERED || client.state == SEND_ALIVE) {
        end_t = time(NULL);
        total_t = end_t - start_t;
        if (total_t == v) {
            sendUdp(sock, serveraddr, pdu);
            start_t = time(NULL);
        }
    }
    
    free(message);
    return NULL;
}


/* Funció encarregada de mantenir la comunicació periòdica amb el servidor (iniciar el procés d'enviament d'ALIVES, 
la comprovació del 1r ALIVE i la recepció dels ALIVE consecutius i, si tot és correcte
obrir el port TCP i permetre la introducció de comandes) */
void* handleAlive(void* argp) {
    struct register_arg_struct *args = argp;
    int sock_udp = args->sock;
    struct sockaddr_in serveraddr = args->serveraddr;

    char* message = malloc(sizeof(char)*1000);
    struct PduUdp pdu = packPduUdp(ALIVE, client.id_client, server.id_communication, "");

    argAlive.sock = sock_udp;
    argAlive.serveraddr = serveraddr;
    argAlive.pdu = pdu;

    //Creació del procés encarregat d'enviar ALIVES cada v segons
    pthread_t thread_SENDALIVE;
    if (pthread_create(&thread_SENDALIVE, NULL, &sendAlives, (void *)&argAlive) != 0) {
        printTerminal("Error al crear el thread", ERROR);
    }
    
    
    /* SELECT */
    struct timeval timeout;
    //Establir timeout per la recepció del 1r ALIVE
    timeout.tv_sec = r*v;
    timeout.tv_usec = 0;
    
    fd_set rset;
    
    int maxsock = sock_udp + 1;

    ssize_t bytes_received;
    socklen_t len;
    time_t actual_time, time_alive;
    char buffer[84];
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
                            //Comptador processos de registre reiniciat ja que el 1r ALIVE s'ha enviat i rebut correctament
                            register_number = 0;
                            //Reiniciar temporitzador per la comprovació dels ALIVE consecutius
                            time_alive = time(NULL);
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp dades (rebut: %s, esperat: %s)", pdu_received.data, client.id_client);
                        printTerminal(message, DEBUG);
                        client.state = NOT_REGISTERED;
                        printClientState();
                        return NULL;
                    }
                }
                else {
                    sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                    return NULL;
                }    
            } 
            else {
                sprintf(message, "Error en les dades d'identificació del servidor (rebut ip: %d, id: %s)", serveraddr.sin_port, pdu_received.id_transmitter);
                printTerminal(message, DEBUG);
                client.state = NOT_REGISTERED;
                printClientState();
                return NULL;
            }
        }
        else if (pdu.packet_type == ALIVE_REJ) {
            printTerminal("Rebut paquet ALIVE_REJ", DEBUG);
            client.state = NOT_REGISTERED;
            printClientState();
            return NULL;
        }
    }
    else {
        sprintf(message, "Finalitzat el temporitzador per la confirmació del primer ALIVE (%d seg.)", r*v);
        printTerminal(message, MSG);
        close(sock_udp);
        printTerminal("Tancat socket UDP per la comunicació amb el servidor", DEBUG);
        sprintf(message, "Finalitzat procés %ld", pthread_self());
        printTerminal(message, DEBUG);
        client.state = NOT_REGISTERED;
        printClientState();
        return NULL;
    }

    
    int sock_tcp, conn_tcp;
    char buffer_tcp[127];
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tcpaddr;

    //Setup de la comunicació TCP amb el port TCP del client definit a l'arxiu de configuració
    tcpaddr.sin_family = AF_INET;
    tcpaddr.sin_port = htons(client.tcp_port);
    tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sock_tcp, (struct sockaddr*)&tcpaddr, sizeof(tcpaddr));
    listen(sock_tcp, 10);

    sprintf(message, "Obert port TCP %d per la comunicació amb el servidor", client.tcp_port);
    printTerminal(message, MSG);


    FD_ZERO(&rset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    maxsock = max(sock_udp, sock_tcp) + 1;

    while(client.state == SEND_ALIVE) {
        FD_SET(sock_udp, &rset);
        FD_SET(sock_tcp, &rset);
        FD_SET(STDIN, &rset);

        select(maxsock, &rset, NULL, NULL, &timeout);

        //Rebut quelcom pel socket udp
        if (FD_ISSET(sock_udp, &rset)) {
            len = sizeof(serveraddr);
            bytes_received = recvfrom(sock_udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &len);
            struct PduUdp pdu_received = unpackPduUdp(buffer, bytes_received);
            if (pdu_received.packet_type == ALIVE) {
                if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                    if (strcmp(server.id_communication, pdu_received.id_communication) == 0){
                        if (strncmp(client.id_client, pdu_received.data, sizeof(client.id_client)) == 0){
                            //Reiniciar temps ALIVE per la comprovació dels 3 consecutius
                            time_alive = time(NULL);
                        }
                        else {
                            sprintf(message, "Error en el valor del camp dades (rebut: %s, esperat: %s)", pdu_received.data, client.id_client);
                            printTerminal(message, DEBUG);
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
                    sprintf(message, "Error en les dades d'identificació del servidor (esperat: %s, id: %s)", server.id_server, pdu_received.id_transmitter);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                }
            }
            else if (pdu_received.packet_type == ALIVE_REJ) {
                printTerminal("Rebut paquet ALIVE_REJ", DEBUG);
                client.state = NOT_REGISTERED;
                printClientState();
            }
        }

        //Rebut quelcom pel socket tcp
        if (FD_ISSET(sock_tcp, &rset)) {
            len = sizeof(tcpaddr);
            conn_tcp = accept(sock_tcp, (struct sockaddr*)&tcpaddr, &len);
            bytes_received = read(conn_tcp, buffer_tcp, sizeof(buffer_tcp));
            char* message = malloc(sizeof(char)*1000);
            struct PduTcp pdu_received = unpackPduTcp(buffer_tcp, bytes_received);
            struct PduTcp pdu;
            if (strcmp(server.id_server, pdu_received.id_transmitter) == 0) {
                if (strcmp(server.id_communication, pdu_received.id_communication) == 0) {
                    if (strcmp(pdu_received.info, client.id_client) == 0) {
                        bool found = false;
                        //Iterar per tots els elements que té el client per veure si el que ha enviat el servidor pertany al dispositiu
                        for (int i = 0; i < client.num_elements; i++) {
                            if(strcmp(pdu_received.element, client.elements[i].element) == 0) {
                                found = true;
                                char* info = getHour();
                                if (pdu_received.packet_type == SET_DATA && (strcmp(&pdu_received.element[6], "I") == 0)) {                             
                                    strcpy(client.elements[i].value, pdu_received.value);
                                    pdu = packPduTcp(DATA_ACK, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, info);
                                    sendTcp(conn_tcp, pdu);
                                }
                                else if (pdu_received.packet_type == SET_DATA && (strcmp(&pdu_received.element[6], "O") == 0)) {
                                    sprintf(message, "Error paquet rebut. Element: %s és sensor i no permet establir el seu valor", pdu_received.element);
                                    printTerminal(message, DEBUG);
                                    pdu = packPduTcp(DATA_NACK, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Element es sensor, no permet [set]");
                                    sendTcp(conn_tcp, pdu);
                                }
                                if (pdu_received.packet_type == GET_DATA) {
                                    pdu = packPduTcp(DATA_ACK, client.id_client, server.id_communication, pdu_received.element, client.elements[i].value, info);
                                    sendTcp(conn_tcp, pdu);
                                }
                            }
                        }
                        if (!found) {
                            sprintf(message, "Element: [%s] no pertany al dispositiu", pdu_received.element);
                            printTerminal(message, DEBUG);
                            pdu = packPduTcp(DATA_NACK, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Element no pertany al dispositiu");
                            sendTcp(conn_tcp, pdu);
                        }
                    }
                    else {
                        sprintf(message, "Error en el valor del camp info (rebut: %s, esperat: %s)", pdu_received.info, client.id_client);
                        printTerminal(message, DEBUG);
                        pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Camp info incorrecte");
                        sendTcp(conn_tcp, pdu);
                        client.state = NOT_REGISTERED;
                        printClientState();
                    }
                }
                else {
                    sprintf(message, "Error en el valor del camp id. com. (rebut: %s, esperat: %s)", pdu_received.id_communication, server.id_communication);
                    printTerminal(message, DEBUG);
                    pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Id. comunicació incorrecte");
                    sendTcp(conn_tcp, pdu);
                    client.state = NOT_REGISTERED;
                    printClientState();
                }    
            } 
            else {
                sprintf(message, "Error en les dades d'identificació del servidor (rebut: %s, esperat: %s)", pdu_received.id_transmitter, server.id_server);
                printTerminal(message, DEBUG);
                pdu = packPduTcp(DATA_REJ, client.id_client, server.id_communication, pdu_received.element, pdu_received.value, "Id. transmitter incorrecte");
                sendTcp(conn_tcp, pdu);
                client.state = NOT_REGISTERED;
                printClientState();
            }
            //Tancar la connexió TCP al acabar l'enviament de dades
            close(conn_tcp);
        }

        //Rebut quelcom per terminal
        if (FD_ISSET(STDIN, &rset)) {
            //Creació d'un procés encarregat d'efectuar la comprovació de comandes i de fer el pertinent
            pthread_t thread_HANDLECOMMANDS;
            if (pthread_create(&thread_HANDLECOMMANDS, NULL, &handleCommands, NULL) != 0) {
                printTerminal("ERROR en la creació del procés handleCommands", ERROR);
            }
        }


        //Comprovació 3 alives consecutius
        actual_time = time(NULL);  

        if ((actual_time - time_alive) > v * 3) {
            client.state = DISCONNECTED;
            printClientState();
            sprintf(message, "S'ha perdut la connexió amb el servidor al no rebre %d ALIVE consecutius", s);
            printTerminal(message, MSG);
        }
        //Si s'ha introduït la comanda quit el valor de la variable global passa a true i surt del bucle
        if (quit) {
            break;
        }
    }

    close(sock_tcp);
    printTerminal("Tancat socket TCP per la comunicació amb el servidor", DEBUG);
    close(sock_udp);
    printTerminal("Tancat socket UDP per la comunicació amb el servidor", DEBUG);
    sprintf(message, "Finalitzat procés %ld", pthread_self());
    printTerminal(message, DEBUG);

    //Addicionalment, si la comanda quit ha estat introduïda, aturar el programa després de tancar els sockets i finalitzar el procés
    if (quit) {
        exit(1);
    }

    free(message);
    return NULL;
}


/* Funció auxiliar per obtenir l'hora actual que s'utilitza al camp info del paquet SEND_DATA i DATA_ACK
(utilitzant %02d per completar amb zeros a l'esquerra si és necessari) */
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


//Funció auxiliar per borrar el caràcter de nova línia al final d'una cadena de caràcters
void removeNewLine(char* str) {
    int length = strlen(str);
    if ((length > 0) && (str[length-1] == '\n'))
        str[length - 1] = '\0';
}


//Funció encarregada de comprovar la comanda introduïda i respondre de la forma necessaria
void* handleCommands(void* unused) {
    char buffer[80];
    fgets(buffer, sizeof(buffer), stdin);
    char* message = malloc(sizeof(char)*1000);
    //Esborrar el caràcter de nova línia del final de la cadena de caràcters per evitar errors
    removeNewLine(buffer);
    char delimiter[] = " ";
    //Utilitzar un altre cop la funció strtok per fer un "split" del buffer però aquest cop amb el delimitar " " (un espai)
    char* command = strtok(buffer, delimiter);

    if (command != NULL) {
        //Evaluar si la comanda introduïda és correcta sense tenir en compte l'ús de majúscules
        if (strcasecmp(command, "stat") == 0) {
            //Convertir l'estat actual del client a cadena de caràcters
            char* state;
            if (client.state == DISCONNECTED) {
                state = "DISCONNECTED";
            }
            else if (client.state == NOT_REGISTERED) {
                state = "NOT_REGISTERED";
            }
            else if (client.state == WAIT_ACK_REG) {
                state = "WAIT_ACK_REG";
            }
            else if (client.state == WAIT_INFO) {
                state = "WAIT_INFO";
            }
            else if (client.state == WAIT_ACK_INFO) {
                state = "WAIT_ACK_INFO";
            }
            else if (client.state == REGISTERED) {
                state = "REGISTERED";
            }
            else if (client.state == SEND_ALIVE) {
                state = "SEND_ALIVE";
            }
            printf("********************* DADES DISPOSITIU ***********************\n");
            printf("  Identificador: %s\n", client.id_client);
            printf("  Estat: %s\n", state);
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
                //Iterar per tots els elements que té el client per veure si s'ha introduït un element correcte per terminal
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
                    return NULL;
                } 
            }
            else {
                printTerminal("Error de sintàxi. (send <element>)", MSG);
                return NULL;
            }

            //Creació d'una nova comunicació TCP amb el port especificat al camp data del paquet INFO_ACK (guardat a la variable global server.tcp_port)
            int socktcp;

            if ((socktcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printTerminal("Error creació socket", ERROR);
            }

            struct sockaddr_in tcp_addr;

            tcp_addr.sin_family = AF_INET;
            tcp_addr.sin_port = htons(server.tcp_port);
            tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            //Connexió amb el servidor pel nou socket TCP
            if (connect(socktcp, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
                printTerminal("TCP Connect ha fallat", ERROR);
                return NULL;
            }
            sprintf(message, "Iniciada comunicació TCP amb el servidor (port: %d)", server.tcp_port);
            printTerminal(message, MSG);

            

            char* info;
            info = getHour();

            struct PduTcp pdu;
            pdu = packPduTcp(SEND_DATA, client.id_client, server.id_communication, id_element, value, info);
            sendTcp(socktcp, pdu);

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

            //Esperar la resposta del servidor durant m segons mitjançant la struct timeval definint timeout.tv_sec = m
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
                                printTerminal("Caldria reenviar les dades al servidor", DEBUG);
                            }
                        }
                        else if (pdu_received.packet_type == DATA_NACK) {
                            printTerminal("Rebut DATA NACK", DEBUG2);
                            printTerminal("Caldria reenviar les dades al servidor", DEBUG);
                        }
                        else if (pdu_received.packet_type == DATA_REJ) {
                            printTerminal("Rebut DATA_REJ", DEBUG2);
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
                    sprintf(message, "Error en les dades d'identificació del servidor (esperat: %s, id: %s)", server.id_server, pdu_received.id_transmitter);
                    printTerminal(message, DEBUG);
                    client.state = NOT_REGISTERED;
                    printClientState();
                }
            }
            else {
                printTerminal("No s'ha rebut resposta del servidor per la comunicació TCP", DEBUG);
                printTerminal("Caldria reenviar les dades al servidor", DEBUG);
            }

            close(socktcp);
            sprintf(message, "Finalitzada comunicació TCP amb el servidor (port: %d)", server.tcp_port);
            printTerminal(message, MSG);


        }
        else if (strcasecmp(command, "quit") == 0) {
            //Establir la variable global quit a true per informar als altres processos que s'ha introduït la comanda per terminal
            quit = true;
        }
        else {
            sprintf(message, "Commanda incorrecta (%s)", command);
            printTerminal(message, MSG);
        }
    }
    free(message);
    return NULL;
}


//Funció encarregada d'analitzar els arguments de la línia de comandes introduïts al posar en marxa el programa
void parseArgs(int argc, char* argv[]) {
    printTerminal("Ús ./client -d (debug) -d 2 (debug nivell 2) -u <arxiu configuració>", INFO);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            /* Canviar la variable debug_mode a cert ja que s'ha introduït l'opció "-d" i així es 
            mostraran per terminal els missatges del nivell DEBUG */
            debug_mode = true;
            if (argv[i+1]) {
                if (strcmp(argv[i+1], "2") == 0) {
                    /* Addicionalment, si s'ha introduït "-d 2" canviar aquesta variable debug_mode_2 a cert 
                    per mostrar els missatges de nivell DEBUG2 per terminal */
                    debug_mode_2 = true;
                }
            }
        }
        else if (strcmp(argv[i], "-u") == 0) {
            if (argv[i+1]) {
                //Si s'ha introduït l'opció "-u" juntament amb un arxiu guardem a la variable global clientFile el nom d'aquest
                strcpy(clientFile, argv[i+1]);
            }
            else {
                printTerminal("Arxiu de configuració no especificat", ERROR);
                exit(1);
            }
        }
    }
}


/* Funció encarregada d'inicialitzar els socket UDP i els diferents processos de REGISTRE i ALIVE i, sobretot, 
comptar el nombre d'intents de registre que s'han efectuat */
void setup() {
    char* message = malloc(sizeof(char)*1000);
    int sock_udp;

    struct sockaddr_in serveraddr;

    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(client.server_udp);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    pthread_t thread_REGISTER, thread_ALIVE;
    args.sock = sock_udp;
    args.serveraddr = serveraddr;
    
    while (register_number <= o) {
        /* Tornar a inicialitzar el socket UDP, ja que s'havia tancat, i guardar a la struct register_arg_struct tant el socket com 
        la struct encarregada del port i l'adreça */
        sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
        args.sock = sock_udp;
        args.serveraddr = serveraddr;

        sprintf(message, "Procés de subscripció: %d", register_number);
        printTerminal(message, MSG);
        if (pthread_create(&thread_REGISTER, NULL, &registerPhase, (void *)&args) != 0) {
            printTerminal("ERROR en la creació del procés", ERROR);
        }
        //Esperem a que el procés de registre acabi, ja sigui satisfactoriament o no, per continuar
        pthread_join(thread_REGISTER, NULL);

        //Si la fase de registre ha estat correcta (estat del client REGISTERED) continuem amb la fase de mantenir la comunicació periòdica
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
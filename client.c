#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

char clientFile[] = "client.cfg";


struct Client {
    char id_client[10];
    char state[15];
    int tcp_port;
    char elements[15*5];
    char server[9];
    int server_udp;
};


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
    printf("%d:%d:%d - DEBUG => %s", hours, minutes, seconds, message);
}


void printInfo(char* message) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - INFO => %s", hours, minutes, seconds, message);
}


void printError(char* message) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - ERROR => %s", hours, minutes, seconds, message);
}


void readFile() {
    struct Client client;
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
                sprintf(message, "Assignada IP servidor: %s", client.id_client);
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










int main() {
    readFile();
}
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
    } while (*s++ = *d++);
}


void printDebug(char* s) {
    int hours, minutes, seconds;
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%d:%d:%d - DEBUG => %s", hours, minutes, seconds, s);
}



void readFile() {
    struct Client client;
    FILE* fp;
    char* p;
    char line[2048];
    int lineNumber = 0;
    char delimiter[2] = "=";
    fp = fopen(clientFile, "r");
    if (ferror(fp)) {
        printf("File couldn't be opened");
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        printDebug(line);
        p = strtok(line, delimiter);

        
        if (lineNumber == 0){
            if (strcmp(p, "Id ") == 0){
                p = strtok(NULL, delimiter);
                removeSpaces(p);
                strcpy(client.id_client, p);
                printf(client.id_client);
            }  
            
        }
        if (lineNumber == 1){
            if (strcmp(p, "Elements ") == 0){
                p = strtok(NULL, delimiter);
                removeSpaces(p);
                strcpy(client.elements, p);
                printf(client.elements);
            }
        }
        if (lineNumber == 2){
            if (strcmp(p, "Local-TCP ") == 0){
                p = strtok(NULL, delimiter);
                removeSpaces(p);
                client.tcp_port = atoi(p);
                printf("%d\n", client.tcp_port);
            }
        }
        if (lineNumber == 3){
            if (strcmp(p, "Server ") == 0){
                p = strtok(NULL, delimiter);
                removeSpaces(p);
                strcpy(client.server, p);
                printf(client.server);
            }
        }
        if (lineNumber == 4){
            if (strcmp(p, "Server-UDP ") == 0){
                p = strtok(NULL, delimiter);
                removeSpaces(p);
                client.server_udp = atoi(p);
                printf("%d\n", client.server_udp);
            }
        }
        lineNumber++;
    }
    


    fclose(fp);
}










int main() {
    readFile();
}
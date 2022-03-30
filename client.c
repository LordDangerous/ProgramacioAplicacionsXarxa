#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

char clientFile[] = "client.cfg";


struct Client {
    char id_client[10];
    char state[15];
    int tcp_port;
    char elements[15*5];
    char server[9];
    int server_udp;
};


void remove_spaces(char* s) {
    char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while (*s++ = *d++);
}




void readFile() {
    struct Client client;
    FILE* fp;
    char* p;
    char line[2048];
    int i = 0;
    char delimiter[2] = "=";
    fp = fopen(clientFile, "r");
    if (ferror(fp)) {
        printf("File couldn't be opened");
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf(line);
        p = strtok(line, delimiter);

        while(p != NULL){
            if (i == 0){
                if (strcmp(p, "Id ") == 0){
                    p = strtok(NULL, delimiter);
                    remove_spaces(p);
                    strcpy(client.id_client, p);
                    printf(client.id_client);
                    p = NULL;
                }  
                
            }
            if (i == 1){
                if (strcmp(p, "Elements ") == 0){
                    p = strtok(NULL, delimiter);
                    remove_spaces(p);
                    strcpy(client.elements, p);
                    printf(client.elements);
                    p = NULL;
                }
            }
            if (i == 2){
                if (strcmp(p, "Local-TCP ") == 0){
                    p = strtok(NULL, delimiter);
                    remove_spaces(p);
                    client.tcp_port = atoi(p);
                    printf("%d\n", client.tcp_port);
                    p = NULL;
                }
            }
            if (i == 3){
                if (strcmp(p, "Server ") == 0){
                    p = strtok(NULL, delimiter);
                    remove_spaces(p);
                    strcpy(client.server, p);
                    printf(client.server);
                    p = NULL;
                }
            }
            if (i == 4){
                if (strcmp(p, "Server-UDP ") == 0){
                    p = strtok(NULL, delimiter);
                    remove_spaces(p);
                    client.server_udp = atoi(p);
                    printf("%d\n", client.server_udp);
                    p = NULL;
                }
            }
            i++;
        }
    }
    


    fclose(fp);
}










int main() {
    readFile();
}
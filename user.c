#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "verify.h"


char *ASIP = NULL;
char *ASport = NULL;
char *FSIP = NULL;
char *FSport = NULL;
int isLogged = 0;
char *uid = NULL;
char *pass = NULL;
int rid = -1;
int tid = -1;

void parseArgs(int argc, char **argv);
void makeConnection();
int parseInput(char *buffer, char *command, char *second, char *third);
void formatMessage(char *message, int code, char *second, char *third);
void sendMessage(int code, int fd_as, int fd_fs, char *message);
void readMessage(int fd, char *answer);
int verifyAnswer(char *answer);


int main(int argc, char **argv) {
    parseArgs(argc, argv);
    uid = malloc(5 * sizeof(char));
    pass = malloc(9 * sizeof(char));
    makeConnection();
    free(uid);
    free(pass);

    return 0;
}

void parseArgs(int argc, char **argv) {
    //verificar???
    ASIP = argv[1];
    ASport = argv[2];
    FSIP = argv[3];
    FSport = argv[4];
}

void makeConnection() {
    int fd_as, fd_fs, code, out_fds;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res_as, *res_fs;
    struct sockaddr_in addr;
    char buffer[128]; // verificar tamanho
    char command[9], second[6], third[25], answer[128], message[128]; // verificar tamanhos, preciso mallocs?
    fd_set inputs, testfds;

    fd_as = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_as == -1) exit(1);

    fd_fs = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_fs == -1) exit(1);

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd_as, &inputs);
    FD_SET(fd_fs, &inputs);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    code = getaddrinfo(ASIP, ASport, &hints, &res_as);
    if (code != 0) exit(1);

    code = getaddrinfo(FSIP, FSport, &hints, &res_fs);
    if (code != 0) exit(1);

    n = connect(fd_as, res_as->ai_addr, res_as->ai_addrlen);
    if (n == -1) exit(1);

    n = connect(fd_fs, res_fs->ai_addr, res_fs->ai_addrlen);
    if (n == -1) exit(1);

    while (1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        switch (out_fds) {
            case 0:
                break;
            case -1:
                perror("select");
                exit(1);
        default:
            memset(answer, 0, sizeof(answer));
            memset(message, 0, sizeof(message));
            memset(buffer, 0, sizeof(buffer));
            memset(command, 0, sizeof(command));
            memset(second, 0, sizeof(second));
            memset(third, 0, sizeof(third));

            if (FD_ISSET(0, &testfds)) {
                fgets(buffer, 128, stdin);
                code = parseInput(buffer, command, second, third);

                if (code == ERROR || code == EXIT) break;

                formatMessage(message, code, second, third);
                sendMessage(code, fd_as, fd_fs, message);
            }
            else if(FD_ISSET(fd_as, &testfds)) {
                readMessage(fd_as, answer);
                verifyAnswer(answer);
            }
            break;
        }
        if (code == EXIT) break;
    }

    freeaddrinfo(res_as);
    freeaddrinfo(res_fs);
    close(fd_as);
    close(fd_fs);
}

int parseInput(char *buffer, char *command, char *second, char *third) {
    int code;

    sscanf(buffer, "%s %s %s", command, second, third);
    
    code = verifyCommand(command);
    if (code != LOGIN && !isLogged) {
        printf("You aren't logged in\n");
        return ERROR;
    }

    switch (code) {
        case LOGIN:
            if (isLogged) {
                printf("You are already logged in\n");
                code = ERROR;
            }
            else if (verifyUid(second) == 0 && verifyPass(third) == 0) {
                strcpy(uid, second);
                strcpy(pass, third);
            }
            else code = ERROR;
            break;

        case REQ:
            if (verifyFop(second, third) != 0) code = ERROR;
            break;

        case VAL:
            if (verifyVc(second) != 0) code = ERROR;
            break;

        case EXIT:
            break;
    
        default:
            code = ERROR;
            break;
    }

    return code;
}


void formatMessage(char *message, int code, char *second, char *third) {
    switch (code) {
        case LOGIN:
            sprintf(message, "LOG %s %s\n", second, third);
            break;

        case REQ:
            rid = rand() % 10000;
            if (third == NULL) sprintf(message, "REQ %s %d %s\n", uid, rid, second);
            else sprintf(message, "REQ %s %d %s %s\n", uid, rid, second, third);
            break;

        case VAL:
            sprintf(message, "AUT %s %d %s\n", uid, rid, second);
    }
}


void sendMessage(int code, int fd_as, int fd_fs, char *message) {
    int fd = code < 5 ? fd_as : fd_fs;
    int nleft = strlen(message);
    int nwritten = 0;
    char *ptr = message;

    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        if (nwritten <= 0) exit(1); //?????
        nleft -= nwritten;
        ptr += nwritten;
    }
}


void readMessage(int fd, char *answer) {
    char *ptr = answer;
    int nread;
     // um bocado martelado ver melhor
    while (*ptr != '\n') { // se o servidor não cumprir o protocolo isto não vai funcionar
        nread = read(fd, ptr, 127); //change size
        if (nread == -1) exit(1);
        else if(nread == 0) break; // o que fazer aqui
        ptr += nread;
        if (*(ptr-1) == '\n') break;
    }
    *(ptr++) = '\0';
}


int verifyAnswer(char *answer) {
    if (strcmp(answer, "RLO OK\n") == 0) {
        isLogged = 1;
        printf("Login successful: %s", answer);
        return 1;
    }
    else if (strcmp(answer, "RLO NOK\n") == 0) printf("Log in failed: %s", answer);
    else if (strcmp(answer, "RRQ OK\n") == 0) printf("Request accepted: %s", answer);
    else if (strcmp(answer, "RLO ERR\n") == 0) printf("ERROR 1\n");
    else if (strcmp(answer, "RRQ ERR\n") == 0) printf("ERROR 2\n");
    else if (strcmp(answer, "RRQ ELOG\n") == 0) printf("Not logged in: %s", answer);
    else if (strcmp(answer, "RRQ EPD\n") == 0) printf("Could not reach PD: %s", answer);
    else if (strcmp(answer, "RRQ EUSER\n") == 0) printf("Wrong UID: %s", answer);
    else if (strcmp(answer, "RRQ EFOP\n") == 0) printf("Invalid file operation: %s", answer);
    else if (strcmp(answer, "RAU 0\n") == 0) printf("Two-factor authentication failed: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR: %s\n", answer);
    else if (answer != NULL && strlen(answer) > 3) { // mudar
        sscanf(answer, "RAU %d", &tid); // verificar se TID é correto?
        printf("Two-factor authentication successful: %s", answer);
    }

    return 0;
}
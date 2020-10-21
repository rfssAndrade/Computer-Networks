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

#define ERROR -1
#define PASS_LENGTH 8

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
int verifyCommand(char *command);
int verifyUid(char *uid);
int verifyPass(char *pass);
int verifyFop(char *fop, char *fname);
int verifyVc(char *vc);
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
    struct timeval timeout;

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
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);

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

                if (code == ERROR) break;
                if (code == 3) break;

                formatMessage(message, code, second, third);
                sendMessage(code, fd_as, fd_fs, message);
            }
            else if(FD_ISSET(fd_as, &testfds)) {
                readMessage(fd_as, answer);
                verifyAnswer(answer);
            }
            break;
        }
        if (code == 3) break;
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

    if (code == 3) return code;

    else if (code == 0) {
        if (isLogged) {
            printf("You are already logged in\n");
            return ERROR;
        }
        else if (verifyUid(second) == 0 && verifyPass(third) == 0) {
            strcpy(uid, second);
            strcpy(pass, third);
        }
        else return ERROR;
    }
    else if (!isLogged) {
        printf("You aren't logged in\n");
        return ERROR;
    }
    else if (code == 1) {
        if (verifyFop(second, third) != 0) return ERROR;
    }

    else if (code == 2) {
        if (verifyVc(second) != 0) return ERROR;
    }
    return code;
}

int verifyCommand(char *command) {
    if (strcmp(command, "login") == 0) return 0;
    if (strcmp(command, "req") == 0) return 1;
    if (strcmp(command, "val") == 0) return 2;
    if (strcmp(command, "exit") == 0) return 3;

    printf("Invalid command\n");
    return -1;
}


int verifyUid(char *uid) {
    if ((strlen(uid) == 5 && atoi(uid) != 0) || strcmp(uid, "00000") == 0) return 0;

    printf("Invalid UID\n");
    return ERROR;
}


int verifyPass(char *pass) {
    char *temp = pass;

    if (strlen(pass) != PASS_LENGTH) {
        printf("Invalid password\n");
        return ERROR;
    }

    while (*temp++) {
        if (!(*temp >= 'a' && *temp <= 'z') && !(*temp >= 'A' && *temp <= 'Z') && !(*temp >= '0' && *temp <= '9') && *temp != '\0') {
            printf("Invalid password\n");
            return ERROR;
        }
    }
    return 0;
}

int verifyFop(char *fop, char *fname) {
    if (strcmp(fop, "R") == 0 || strcmp(fop, "U") == 0 || strcmp(fop, "D") == 0 || strcmp(fop, "L") == 0 || strcmp(fop, "X") == 0) return 0;
    //verify fname
    printf("Invalid operation\n");
    return ERROR;
}


int verifyVc(char *vc) {
    printf("--%s--", vc);
    if ((strlen(vc) == 4 && atoi(vc) != 0) || strcmp(vc, "0000") == 0) return 0; // modify to check if VC is right

    printf("Invalid VC\n");
    return ERROR;
}


void formatMessage(char *message, int code, char *second, char *third) {
    if (code == 0) sprintf(message, "LOG %s %s\n", second, third);
    else if (code == 1) {
        rid = rand() % 10000;
        if (1) {
            sprintf(message, "REQ %s %d %s\n", uid, rid, second);
        }
        else sprintf(message, "REQ %s %d %s %s\n", uid, rid, second, third);
    }
    else if (code == 2) sprintf(message, "AUT %s %d %s\n", uid, rid, second);
}


void sendMessage(int code, int fd_as, int fd_fs, char *message) {
    int fd = code < 3 ? fd_as : fd_fs;
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
    else if (strcmp(answer, "RLO ERR\n") == 0) printf("ERROR\n");
    else if (strcmp(answer, "RRQ ERR\n") == 0) printf("ERROR\n");
    else if (strcmp(answer, "RRQ ELOG\n") == 0) printf("Not logged in: %s", answer);
    else if (strcmp(answer, "RRQ EPD\n") == 0) printf("Could not reach PD: %s", answer);
    else if (strcmp(answer, "RRQ EUSER\n") == 0) printf("Wrong UID: %s", answer);
    else if (strcmp(answer, "RRQ EFOP\n") == 0) printf("Invalid file operation: %s", answer);
    else if (strcmp(answer, "RAU 0\n") == 0) printf("Two-factor authentication failed: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR\n");
    else if (answer != NULL) {
        sscanf(answer, "RAU %d", tid); // verificar se TID é correto?
        printf("Two-factor authentication successful: %s", answer);
    }

    return 0;
}
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include "verify.h"


char *ASIP = NULL;
char *ASport = NULL;
char *FSIP = NULL;
char *FSport = NULL;
int isLogged = 0;
char *uid = NULL;
char *pass = NULL;
char *fname = NULL;
int rid = -1;
int tid = -1;


void parseArgs(int argc, char **argv);
void makeConnection();
int parseInput(char *buffer, char *command, char *second, char *third);
void formatMessage(char *message, int code, char *second, char *third);
void sendMessage(int code, int fd_as, int fd_fs, char *message);
int readMessageAS(int fd, char *answer);
int readMessageFS(int fd);
int verifyAnswerAS(char *answer);
int parseAnswerFS(char *operation, int code, int fd);
int verifyAnswerFS(char *answer);
int parseAnswerAS(char *answer, char *command, char *second);


int main(int argc, char **argv) {
    FSIP = malloc(16 * sizeof(char));
    FSport = malloc(6 * sizeof(char));
    ASIP = malloc(16 * sizeof(char));
    ASport = malloc(6 * sizeof(char));

    parseArgs(argc, argv);

    uid = malloc(5 * sizeof(char));
    pass = malloc(9 * sizeof(char));
    fname = malloc(25 * sizeof(char));

    makeConnection();

    free(ASIP);
    free(ASport);
    free(FSport);
    free(FSIP);
    free(fname);
    free(uid);
    free(pass);

    return 0;
}


void parseArgs(int argc, char **argv) {
    int i = 1, code;

    while(i < argc) {
        code = verifyArg(argv[i], argv[i+1]);
        switch (code) {
            case M:
                strcpy(FSIP, argv[i+1]);
                break;
            case Q:
                strcpy(FSport, argv[i+1]);
                break;
            case N:
                strcpy(ASIP, argv[i+1]);
                break;
            case P:
                strcpy(ASport, argv[i+1]);
                break;
            default:
                printf("Bad argument\n");
                exit(1);
                break;
        }
        i += 2;
    }

    if (strlen(FSIP) == 0) strcpy(FSIP, "127.0.0.1");
    if (strlen(FSport) == 0) strcpy(FSport, "59034");
    if (strlen(ASIP) == 0) strcpy(ASIP, "127.0.0.1");
    if (strlen(ASport) == 0) strcpy(ASport, "58034");
}


void makeConnection() {
    int fd_as, fd_fs = -1, code, out_fds;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res_as, *res_fs;
    struct sockaddr_in addr;
    char buffer[128]; // verificar tamanho
    char command[128], second[128], third[128], answer[128], message[128]; // verificar tamanhos, preciso mallocs?
    fd_set inputs, testfds;

    fd_as = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_as == -1) exit(1);

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd_as, &inputs);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    code = getaddrinfo(ASIP, ASport, &hints, &res_as);
    if (code != 0) exit(1);

    n = getaddrinfo(FSIP, FSport, &hints, &res_fs);
    if (n != 0) exit(1);

    n = connect(fd_as, res_as->ai_addr, res_as->ai_addrlen);
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
                if (code > 4) {

                    fd_fs = socket(AF_INET, SOCK_STREAM, 0);
                    if (fd_fs == -1) exit(1);

                    n = connect(fd_fs, res_fs->ai_addr, res_fs->ai_addrlen);
                    if (n == -1) exit(1);

                    FD_SET(fd_fs, &inputs);
                }
                sendMessage(code, fd_as, fd_fs, message);
            }
            else if (FD_ISSET(fd_as, &testfds)) {
                code = readMessageAS(fd_as, answer);
                if (verifyAnswerAS(answer) != 0) parseAnswerAS(answer, command, second);
            }
            else if (fd_fs != -1 && FD_ISSET(fd_fs, &testfds)) {
                code = readMessageFS(fd_fs);
                //verifyAnswerFS(answer);
                FD_CLR(fd_fs, &inputs);
                close(fd_fs); //verify?
                fd_fs = -1;
                memset(fname, 0, sizeof(fname));
            }
            break;
        }
        if (code == EXIT) break;
        else if (code == SOCKET_ERROR) break;
    }

    freeaddrinfo(res_as);
    freeaddrinfo(res_fs);
    close(fd_as);
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

        case RETRIEVE:
        case UPLOAD:
        case DELETE:
            if (verifyFname(second) != 0) code = ERROR;
            else strcpy(fname, second);
            break;

        case LIST:
        case REMOVE:
        case EXIT:
            break;
    
        default:
            printf("Invalid command\n");
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
            if (strlen(third) == 0) sprintf(message, "REQ %s %04d %s\n", uid, rid, second);
            else sprintf(message, "REQ %s %04d %s %s\n", uid, rid, second, third);
            break;

        case VAL:
            sprintf(message, "AUT %s %d %s\n", uid, rid, second);
            break;

        case LIST:
            sprintf(message, "LST %s %04d\n", uid, tid);
            break;

        case RETRIEVE:
            sprintf(message, "RTV %s %04d %s\n", uid, tid, fname);
            break;
        
        case UPLOAD:
            puts("TO DO");
            break;

        case DELETE:
            sprintf(message, "DEL %s %04d %s\n", uid, tid, third);
            break;

        case REMOVE:
            sprintf(message, "REM %s %04d\n", uid, tid);
            break;
    }
}


void sendMessage(int code, int fd_as, int fd_fs, char *message) {
    int fd = code < 5 ? fd_as : fd_fs;
    int nleft = strlen(message);
    int nwritten = 0;
    char *ptr = message;

    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        if (nwritten <= 0) puts("ERROR ON SEND"); //?????
        nleft -= nwritten;
        ptr += nwritten;
    }
}


int readMessageFS(int fd) {
    char operation[4];
    char *ptr = operation;
    int nread, code;

    while (nread != 3) {
        nread = read(fd, ptr, 3);
        if (nread == -1) puts("ERROR ON READ");
        else if (nread == 0) {
            printf("Server closed socket\n");
            return SOCKET_ERROR;
        }
        ptr += nread;
    }
    *ptr = '\0';

    code = verifyOperation(operation);
    if (code == RLS || code == RRT) parseAnswerFS(operation, code, fd);
}


int readMessageAS(int fd, char *answer) {
    char *ptr = answer;
    int nread;
    // um bocado martelado ver melhor
    while (*ptr != '\n') { // se o servidor não cumprir o protocolo isto não vai funcionar
        nread = read(fd, ptr, 127); //change size
        if (nread == -1) puts("ERROR ON READ");
        else if(nread == 0) {
            printf("Server closed socket\n");
            return SOCKET_ERROR;
        }
        ptr += nread;
        if (*(ptr-1) == '\n') break;
    }
    *(ptr++) = '\0';
    return 0;
}


int verifyAnswerAS(char *answer) {
    if (strcmp(answer, "RLO OK\n") == 0) {
        isLogged = 1;
        printf("Login successful: %s", answer);
    }
    else if (strcmp(answer, "RLO NOK\n") == 0) printf("Wrong password: %s", answer);
    else if (strcmp(answer, "RLO ERR\n") == 0) printf("User ID doesn't exist: %s", answer);
    else if (strcmp(answer, "RRQ OK\n") == 0) printf("Request accepted: %s", answer);
    else if (strcmp(answer, "RRQ ELOG\n") == 0) printf("User not logged in: %s", answer);
    else if (strcmp(answer, "RRQ EPD\n") == 0) printf("Could not reach PD: %s", answer);
    else if (strcmp(answer, "RRQ EUSER\n") == 0) printf("Wrong UID: %s", answer);
    else if (strcmp(answer, "RRQ EFOP\n") == 0) printf("Invalid file operation: %s", answer);
    else if (strcmp(answer, "RRQ ERR\n") == 0) printf("Invalid request format: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR: %s\n", answer);
    else if (strcmp(answer, "RAU 0\n") == 0) printf("Two-factor authentication failed: %s", answer);
    else return 1;

    return 0;
}


int parseAnswerAS(char *answer, char *command, char *second) {
    sscanf(answer, "%s %s", command, second);

    if (verifyOperation(command) == RAU && verifyTid(second) == 0) {
        tid = atoi(second);
        printf("Two-factor authentication successful: %s", answer);
        return 0;
    }

    printf("Invalid message: %s", answer);
    return ERROR;
}


int verifyAnswerFS(char *answer) {
    if (strcmp(answer, "RUP OK\n") == 0) printf("Upload request approved: %s", answer);
    else if (strcmp(answer, "RUP NOK\n") == 0) printf("UID doesn't exist: %s", answer);
    else if (strcmp(answer, "RUP DUP\n") == 0) printf("File already exists: %s", answer);
    else if (strcmp(answer, "RUP FULL\n") == 0) printf("Files limit reached: %s", answer);
    else if (strcmp(answer, "RUP INV\n") == 0) printf("Invalid TID: %s", answer);
    else if (strcmp(answer, "RUP ERROR\n") == 0) printf("Invalid upload request format: %s", answer);
    else if (strcmp(answer, "RDL OK\n") == 0) printf("Delete request approved: %s", answer);
    else if (strcmp(answer, "RDL EOF\n") == 0) printf("File not available: %s", answer);
    else if (strcmp(answer, "RDL NOK\n") == 0) printf("UID doesn't exist: %s", answer);
    else if (strcmp(answer, "RDL INV\n") == 0) printf("Invalid TID: %s", answer);
    else if (strcmp(answer, "RDL ERR\n") == 0) printf("Invalid delete request format: %s", answer);
    else if (strcmp(answer, "RRM OK\n") == 0) printf("Remove request approved: %s", answer);
    else if (strcmp(answer, "RRM NOK\n") == 0) printf("UID doesn't exist: %s", answer);
    else if (strcmp(answer, "RRM INV\n") == 0) printf("Invalid TID: %s", answer);
    else if (strcmp(answer, "RRM ERR\n") == 0) printf("Invalid remove request format: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR: %s\n", answer);
    else if (strcmp(answer, "RRT NOK\n") == 0) printf("No content available for UID: %s", answer);
    else if (strcmp(answer, "RRT EOF\n") == 0) printf("File not available: %s", answer);
    else if (strcmp(answer, "RRT INV\n") == 0) printf("Wrong TID: %s", answer);
    else if (strcmp(answer, "RRT ERR\n") == 0) printf("Invalid request format: %s", answer);
    else if (strcmp(answer, "RLS EOF\n") == 0) printf("No files available: %s", answer);
    else if (strcmp(answer, "RLS INV\n") == 0) printf("Wrong TID: %s", answer);
    else if (strcmp(answer, "RLS ERR\n") == 0) printf("Invalid request format: %s", answer);
    else printf("ERROR: %s\n", answer);

    return 0;
}


int parseAnswerFS(char *operation, int code, int fd) {
    char status[4], buffer[128];
    char *ptr = status;
    int nread = 0, nFiles, spacesRead = 0, i = 1;

    switch (code) {
        case RLS:
            while (nread != 3) {
                nread = read(fd, ptr, 3);
                if (nread == -1) puts("ERROR ON READ");
                else if (nread == 0) {
                    printf("Server closed socket\n");
                    return SOCKET_ERROR;
                }
                ptr += nread;
            }
            nFiles = atoi(status);
            if (nFiles > 0) {
                ptr = buffer;
                while (i <= nFiles) {
                    nread = read(fd, ptr, 1);
                    if (nread == -1) puts("ERROR ON READ");
                    else if (nread == 0) {
                        printf("Server closed socket\n");
                        return SOCKET_ERROR;
                    }
                    if (*ptr != ' ' || *ptr!= '\n') ptr++;
                    else if (strlen(buffer) > 1) spacesRead++;

                    if (spacesRead == 2) {
                        *(ptr-1) = '\0';
                        printf("%d - %s\n", i, buffer);
                        i++;
                        spacesRead = 0;
                        memset(buffer, 0, sizeof(buffer));
                        ptr = buffer;
                    }
                }
            }
            else {
                nread = 0;
                while (nread != 2) {
                    nread = read(fd, ptr, 2);
                    if (nread == -1) puts("ERROR ON READ");
                    else if (nread == 0) {
                        printf("Server closed socket\n");
                        return SOCKET_ERROR;
                    }
                    ptr += nread;
                }
                if (sprintf(buffer, "%s%s", operation, status) != 7) code = ERROR;
                else verifyAnswerFS(buffer);
                printf("BUFFER: %s", buffer);
            }

            break;

        case RRT:
            //if (strcmp(answer, "RRT OK\n") == 0) printf("Retrieve request approved: %s", answer); // do things with size
            //printf("%s", answer);
            break;
    
        default:
            //printf("Invalid message: %s", answer);
            code = ERROR;
            break;
    }

    return code;
}
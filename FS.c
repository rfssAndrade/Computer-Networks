#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <arpa/inet.h>
#include "verify.h"
#include "userinfo.h"
#include "message.h"


char *FSport = NULL;
char *ASIP = NULL;
char *ASport = NULL;
int verbose = 0;


void parseArgs(int argc, char **argv);
void makeConnection();
int parseMessageUser(char *buffer, char *message);
void parseMessageAS(char *buffer, char *message, userinfo *fds, int size);
int fopCode(char *fop);
int findUser(userinfo *fds, char *uid, int size);


int main(int argc, char **argv) {
    FSport = malloc(6 * sizeof(char));
    ASIP = malloc(16 * sizeof(char));
    ASport = malloc(6 * sizeof(char));

    parseArgs(argc, argv);

    makeConnection();

    free(ASIP);
    free(ASport);
    free(FSport);

    return 0;
}


void parseArgs(int argc, char **argv) {
    int i = 1, code;

    while(i < argc) {
        code = i+1 == argc ? verifyArg(argv[i], NULL) : verifyArg(argv[i], argv[i+1]);
        switch (code) {
            case V:
                verbose = 1;
                i++;
                break;
            case Q:
                strcpy(FSport, argv[i+1]);
                i += 2;
                break;
            case N:
                strcpy(ASIP, argv[i+1]);
                i += 2;
                break;
            case P:
                strcpy(ASport, argv[i+1]);
                i += 2;
                break;
            default:
                printf("Bad argument\n");
                exit(1);
                break;
        }
    }

    if (strlen(FSport) == 0) strcpy(FSport, "59034");
    if (strlen(ASIP) == 0) strcpy(ASIP, "127.0.0.1");
    if (strlen(ASport) == 0) strcpy(ASport, "58034");
}


void makeConnection() {
    int fd_udp, fd_tcp, out_fds, new_fd;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
    struct sockaddr_in addr;
    fd_set inputs, testfds;
    struct sigaction action;
    int size = 1; // mudar
    userinfo *fds = calloc(size, sizeof(struct userinfo));
    int nextFreeEntry = 0;
    DIR *d;
    char buffer[128], message[128];
    int len, code;

    if ((d = opendir("USERSF")) == NULL) {
        n = mkdir("USERSF", 0777);
        if (n == -1) exit(1);
    }
    else closedir(d);

    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == -1) exit(1);

    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1) exit(1);

    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1) exit(1);

    FD_ZERO(&inputs);
    FD_SET(fd_udp, &inputs);
    FD_SET(fd_tcp, &inputs);

    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;
    hints_udp.ai_socktype = SOCK_DGRAM;

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    n = getaddrinfo(ASIP, ASport, &hints_udp, &res_udp);
    if (n != 0) exit(1);

    n = getaddrinfo(NULL, FSport, &hints_tcp, &res_tcp);
    if (n != 0) exit(1);

    if (bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) exit(1);
    if (listen(fd_tcp, 20) == -1) exit(1); // what size?

    while(1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        switch (out_fds) {
            case 0:
                break;
            case -1:
                perror("select");
                exit(1);
                break;

            default:
                if (FD_ISSET(fd_udp, &testfds)) {
                    addrlen = sizeof(addr);
                    n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
                    if (n == ERROR) puts("ERROR");
                    parseMessageAS(buffer, message, fds, size);
                }
                
                else if (FD_ISSET(fd_tcp, &testfds)) {
                    addrlen = sizeof(addr);
                    new_fd = accept(fd_tcp, (struct sockaddr *)&addr, &addrlen);
                    if (new_fd == ERROR) exit(1); //mudar
                    FD_SET(new_fd, &inputs);
                    fds[nextFreeEntry] = createUserinfo(new_fd, addr);
                    nextFreeEntry = findNextFreeEntry(fds, size);
                    if (nextFreeEntry == size) {
                        fds = realloc(fds, (size * 2) * sizeof(userinfo));
                        for (int i = size; i < size * 2; i++) fds[i] = NULL;
                        size *= 2;
                    }
                }

                else {
                    for (int i = 0; i < size; i++) {
                        if (fds[i]->fd != 0 && FD_ISSET(fds[i]->fd, &testfds)) {
                            n = readTcp(fds[i]->fd, 15, buffer);
                            printf("%s\n", buffer);
                            if (n == -1) break;
                            if (n == SOCKET_ERROR) {
                                FD_CLR(fds[i]->fd, &inputs);
                                close(fds[i]->fd);
                                free(fds[i]->uid);
                                free(fds[i]);
                                fds[i] = NULL;
                                break;
                            }

                            len = parseMessageUser(buffer, message);
                            if (len > 8) {
                                code = sendto(fd_udp, message, strlen(message), 0, res_udp->ai_addr, res_udp->ai_addrlen); //mudar
                                if (code == ERROR) puts("ERROR");
                            }
                            else writeTcp(fds[i]->fd, len, message);
                            printf("%s", message);

                            FD_CLR(fds[i]->fd, &inputs);
                            close(fds[i]->fd);
                            free(fds[i]->uid);
                            free(fds[i]);
                            fds[i] = NULL;

                            break;
                        }
                    }
                }
                break;
        }
    }
    free(fds);
    freeaddrinfo(res_tcp);
    freeaddrinfo(res_udp);
    closeFds(size, fds, fd_udp, fd_tcp);
}


int parseMessageUser(char *buffer, char *message) {
    int code, len;
    char  operation[4], uid[8], tid[8];

    sscanf(buffer, "%s %s %s", operation, uid, tid);
    code = verifyOperation(operation);

    switch (code) {
        case LIST:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "RLS ERR\n");
            else len = sprintf(message, "VLD %s %s\n", uid, tid);
            break;
        case RETRIEVE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "RRT ERR\n");
            else len = sprintf(message, "VLD %s %s\n", uid, tid);
            break;
        case UPLOAD:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "RUP ERR\n");
            else len = sprintf(message, "VLD %s %s\n", uid, tid);
            break;
        case DELETE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "DEL ERR\n");
            else len = sprintf(message, "VLD %s %s\n", uid, tid);
            break;
        case REMOVE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "REM ERR\n");
            else len = sprintf(message, "VLD %s %s\n", uid, tid);
            break;
        default:
            len = sprintf(message, "ERR\n");
            break;
    }
    return len;
}


void parseMessageAS(char *buffer, char *message, userinfo *fds, int size) {
    int code, fd;
    char operation[4], uid[6], third[9], fourth[16], fifth[26];

    sscanf(buffer, "%s %s %s %s %s", operation, uid, third, fourth, fifth);

    code = verifyOperation(operation);
    if (code != CNF || verifyUid(uid) != 0 || verifyTid(third) != 0 || verifyFop(fourth, fifth) != 0) return;

    fd = findUser(fds, uid, size);

    code = fopCode(fourth);

    switch (code) {
        case LIST:
            //list(fd);
            break;
        case RETRIEVE:
            break;
        case UPLOAD:
            break;
        case DELETE:
            break;
        case REMOVE:
            break;
        case INV:
            break;
    }
    printf("%s", buffer);
}


int fopCode(char *fop) {
    if (strcmp(fop, "L") == 0) return LIST;
    if (strcmp(fop, "R") == 0) return RETRIEVE;
    if (strcmp(fop, "U") == 0) return UPLOAD;
    if (strcmp(fop, "D") == 0) return DELETE;
    if (strcmp(fop, "X") == 0) return REMOVE;
    if (strcmp(fop, "E") == 0) return INV;

    return ERROR;
}


int findUser(userinfo *fds, char *uid, int size) {
    for (int i = 0; i < size; i++) {
        if (strcmp(fds[i]->uid, uid) == 0) return fds[i]->fd;
    }
    return ERROR;
}
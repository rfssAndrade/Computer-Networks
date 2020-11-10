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


typedef struct sockinfo {
    int fd;
    struct sockaddr_in addr;
} *Sockinfo;


char * ASport = NULL;
int verbose = 0;


void parseArgs(int argc, char **argv);
void makeConnection();
void closeFds(int size, Sockinfo *fds, int fd_udp, int fd_tcp);
int readMessageUdp(int fd, char *buffer, struct sockaddr_in *addr);
int parseMessage(char *buffer, char *message, char *operation, char *uid, char *third, char *fourth, char *fifth, Sockinfo sockinfo);
int formatMessage(int codeOperation, int codeStatus, char *message);
void sendMessageUdp(int fd, char *message, int len, struct sockaddr_in addr);
int searchDir(DIR *d, struct dirent *dir, char *uid);
int registerUser(char *uid, char *pass, char *PDIP, char *PDport);
int unregisterUser(char *uid, char *pass);
void sendMessageTcp(Sockinfo sockinfo, char *message, int len);
int readMessageTcp(Sockinfo sockinfo, char *buffer);
Sockinfo createSockinfo(int fd, struct sockaddr_in addr);
int findNextFreeEntry(Sockinfo *fds, int size);
int loginUser(char *uid, char *pass, Sockinfo sockinfo);


int main(int argc, char **argv) {
    ASport = malloc(6 * sizeof(char));

    parseArgs(argc, argv);
    makeConnection();

    free(ASport);

    return 0;
}


void parseArgs(int argc, char ** argv) {
    int i = 1, code;

    while (i < argc) {
        code = i+1 == argc ? verifyArg(argv[i], NULL) : verifyArg(argv[i], argv[i+1]);
        switch (code) {
            case P:
                strcpy(ASport, argv[i+1]);
                i += 2;
                break;
            case V:
                verbose = 1;
                i++;
                break;
            default:
                printf("Bad argument\n");
                exit(1);
                break;
        }
    }

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
    pid_t pid;
    int size = 1; // mudar
    Sockinfo *fds = calloc(size, sizeof(struct sockinfo));
    int nextFreeEntry = 0;
    char buffer[128], message[128];
    char operation[4], uid[6], third[9], fourth[16], fifth[26];
    int code, len;
    DIR *d;

    if ((d = opendir("USERS")) == NULL) {
        n = mkdir("USERS", 0777);
        if (n == -1) exit(1);
    }
    else closedir(d);

    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == -1) exit(1); //????

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
    hints_udp.ai_flags = AI_PASSIVE;

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    n = getaddrinfo(NULL, ASport, &hints_udp, &res_udp);
    if (n != 0) exit(1);

    n = getaddrinfo(NULL, ASport, &hints_tcp, &res_tcp);
    if (n != 0) exit(1);

    if (bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen) == -1) exit(1);

    if (bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) exit(1);
    if (listen(fd_tcp, 20) == -1) exit(1); // what size?

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
                memset(buffer, 0, sizeof(buffer));
                memset(message, 0, sizeof(message));
                memset(operation, 0, sizeof(operation));
                memset(uid, 0, sizeof(uid));
                memset(third, 0, sizeof(third));
                memset(fourth, 0, sizeof(fourth));
                memset(fifth, 0, sizeof(fifth));
                
                if (FD_ISSET(fd_udp, &testfds)) {
                    n = readMessageUdp(fd_udp, buffer, &addr);
                    if (n == -1) break;
                    len = parseMessage(buffer, message, operation, uid, third, fourth, fifth, NULL); // tirar addr
                    sendMessageUdp(fd_udp, message, len, addr);
                }
                else if (FD_ISSET(fd_tcp, &testfds)) {
                    addrlen = sizeof(addr);
                    new_fd = accept(fd_tcp, (struct sockaddr *)&addr, &addrlen);
                    if (new_fd == ERROR) exit(1); //mudar
                    FD_SET(new_fd, &inputs);

                    fds[nextFreeEntry] = createSockinfo(new_fd, addr);
                    nextFreeEntry = findNextFreeEntry(fds, size);
                    if (nextFreeEntry == size) {
                        fds = realloc(fds, (size * 2) * sizeof(Sockinfo));
                        for (int i = size; i < size * 2; i++) fds[i] = NULL;
                        size *= 2;
                    }
                }
                else {
                    for (int i = 0; i < size; i++) {
                        if (fds[i]->fd != 0 && FD_ISSET(fds[i]->fd, &testfds)) {
                            n = readMessageTcp(fds[i], buffer);
                            if (n == -1) break;
                            if (n == SOCKET_ERROR) {
                                close(fds[i]->fd);
                                //erase some files
                                free(fds[i]);
                                fds[i] = NULL;
                                break;
                            }
                            if (n == -1 || n == SOCKET_ERROR) break;
                            len = parseMessage(buffer, message, operation, uid, third, fourth, fifth, fds[i]);
                            sendMessageTcp(fds[i], message, len);
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


void closeFds(int size, Sockinfo *fds, int fd_udp, int fd_tcp) {
    for (int i = 0; i < size; i++) {
        if (fds[i] != NULL) {
            close(fds[i]->fd);
            free(fds[i]);
        }
    }
    close(fd_udp);
    close(fd_tcp);
    free(fds);
}


int readMessageUdp(int fd, char *buffer, struct sockaddr_in *addr) {
    int code;
    socklen_t addrlen = sizeof(addr);
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    code = recvfrom(fd, buffer, 127, 0, (struct sockaddr *)addr, &addrlen);
    if (code == ERROR) printf("Error on receive\n");
    else if (verbose) {
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        port = ntohs(addr->sin_port);
        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer); // ver \n
    }
    return code;
}


int parseMessage(char *buffer, char *message, char *operation, char *uid, char *third, char *fourth, char *fifth, Sockinfo sockinfo) {
    int codeOperation, codeStatus, len;

    sscanf(buffer, "%s %s %s %s %s", operation, uid, third, fourth, fifth);

    codeOperation = verifyOperation(operation);

    switch (codeOperation) {
        case REG:
            codeStatus = registerUser(uid, third, fourth, fifth);
            len = formatMessage(codeOperation, codeStatus, message);
            break;
        
        case EXIT:
            codeStatus = unregisterUser(uid, third);
            len = formatMessage(codeOperation, codeStatus, message);
            break;
        
        case RVC:
            break;

        case LOGIN:
            codeStatus = loginUser(uid, third, sockinfo);
            len = formatMessage(codeOperation, codeStatus, message);
            break;

        case REQ:
            break;

        case VAL:
            break;

        case VLD:
            break;

        default:
            len = sprintf(message, "ERR\n");
            break;
    }
    return len;
}


int registerUser(char *uid, char *pass, char *PDIP, char *PDport) {
    DIR *dUsers;
    struct dirent *dir;
    int code, nread, nwritten, len;
    char path[32], buffer[128];
    FILE *fptr;

    if (verifyUid(uid) != 0 || verifyPass(pass) != 0 || verifyIp(PDIP) != 0 || verifyPort(PDport) != 0) return NOK;

    dUsers = opendir("USERS");
    if (dUsers == NULL) return NOK;
    if (!searchDir(dUsers, dir, uid)) {
        sprintf(path, "./USERS/%s", uid);
        code = mkdir(path, 0777);
        if (code == -1) {
            closedir(dUsers);
            return NOK;
        }
    }
    closedir(dUsers);

    sprintf(path, "./USERS/%s/pass.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) {
        if ((fptr = fopen(path, "w")) == NULL) return NOK;
        len = strlen(pass);
        nwritten = fwrite(pass, sizeof(char), len, fptr);
        if (nwritten != len) {
            fclose(fptr);
            return NOK;
        }
    }
    else {
        nread = fread(buffer, sizeof(char), 16, fptr);
        if (strcmp(buffer, pass) != 0) {
            fclose(fptr);
            return NOK;
        }
    }
    fclose(fptr);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return NOK;

    sprintf(buffer, "%s %s", PDIP, PDport);
    len = strlen(buffer);
    nwritten = fwrite(buffer, sizeof(char), len, fptr);
    if (nwritten != len) {
            fclose(fptr);
            return NOK;
    }
    fclose(fptr);
    return OK;
}


int searchDir(DIR *d, struct dirent *dir, char *uid) {
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, uid) == 0) return 1;
    }
    return 0;
}


int formatMessage(int codeOperation, int codeStatus, char *message) {
    int len;
    switch (codeOperation) {
        case REG:
            if (codeStatus == OK) len = sprintf(message, "RRG OK\n");
            else len = sprintf(message, "RRG NOK\n");
            break;
        case EXIT:
            if (codeStatus == OK) len = sprintf(message, "RUN OK\n");
            else len = sprintf(message, "RUN NOK\n");
            break;
        case LOGIN:
            if (codeStatus == OK) len = sprintf(message, "RLO OK\n");
            else if (codeStatus == NOK) len = sprintf(message, "RLO NOK\n");
            else len = sprintf(message, "RLO ERR\n");
    }
    return len;
}


void sendMessageUdp(int fd, char *message, int len, struct sockaddr_in addr) {
    int code;
    socklen_t addrlen = sizeof(addr);
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    code = sendto(fd, message, len, 0, (struct sockaddr *)&addr, addrlen);
    if (code == ERROR) puts("Error on send\n");
    else if (verbose) {
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        port = ntohs(addr.sin_port);
        printf("SENT TO %s %u: %s\n", ip, port, message);
    }
}


int unregisterUser(char *uid, char *pass) {
    char path[32], buffer[16];
    FILE *fptr;
    int nread;

    sprintf(path, "./USERS/%s/pass.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) return NOK;

    nread = fread(buffer, sizeof(char), 16, fptr);
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    if (remove(path) != 0) return NOK;

    return OK;
}


int readMessageTcp(Sockinfo sockinfo, char *buffer) {
    char *ptr = buffer;
    int nread;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    while (1) {
        nread = read(sockinfo->fd, ptr, 127);
        if (nread == -1) {
            puts("ERROR ON READ");
            return ERROR;
        }
        else if (nread == 0) {
            //printf("Server closed socket\n");
            return SOCKET_ERROR;
        }
        ptr += nread;
        if (*(ptr-1) == '\n') break;
    }
    *ptr = '\0';

    if (verbose) {
        inet_ntop(AF_INET, &sockinfo->addr.sin_addr, ip, sizeof(ip));
        port = ntohs(sockinfo->addr.sin_port);
        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
    }

    return 0;
}


void sendMessageTcp(Sockinfo sockinfo, char *message, int len) {
    int nwritten;
    char *ptr = message;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    while (len > 0) {
        nwritten = write(sockinfo->fd, ptr, len);
        if (nwritten <= 0) {
            puts("ERROR ON SEND");
            break;
        }
        len -= nwritten;
        ptr += nwritten;
    }
    if (verbose) {
        inet_ntop(AF_INET, &sockinfo->addr.sin_addr, ip, sizeof(ip));
        port = ntohs(sockinfo->addr.sin_port);
        printf("SENT TO %s %u: %s\n", ip, port, message);
    }
}


int loginUser(char *uid, char *pass, Sockinfo sockinfo) {
    char path[32], buffer[16];
    FILE *fptr;
    int nread, nwritten, len;
    DIR *dUsers;
    struct dirent *dir;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    dUsers = opendir("USERS");
    if (dUsers == NULL) return ERR;
    if (!searchDir(dUsers, dir, uid)) {
        closedir(dUsers);
        return ERR;
    }
    closedir(dUsers);

    sprintf(path, "USERS/%s/pass.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) return NOK;

    nread = fread(buffer, sizeof(char), 16, fptr); // ver nread
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "USERS/%s/login.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return ERR;
    
    inet_ntop(AF_INET, &sockinfo->addr.sin_addr, ip, sizeof(ip));
    port = ntohs(sockinfo->addr.sin_port);

    len = sprintf(buffer, "%s %u", ip, port);
    nwritten = fwrite(buffer, sizeof(char), len, fptr);
    if (nwritten != len) {
        fclose(fptr);
        return ERR;
    }
    fclose(fptr);

    return OK;
}


Sockinfo createSockinfo(int fd, struct sockaddr_in addr) {
    Sockinfo new = malloc(sizeof(struct sockinfo));

    new->fd = fd;
    memcpy(&new->addr, &addr, sizeof(addr));

    return new;
}


int findNextFreeEntry(Sockinfo *fds, int size) {
    for (int i = 0; i < size; i++) {
        if (fds[i] == NULL) return i;
    }
    return size;
}
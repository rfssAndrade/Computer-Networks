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


char * ASport = NULL;
int verbose = 0;


void parseArgs(int argc, char **argv);
void makeConnection();
void closeFds(int size, userinfo *fds, int fd_udp, int fd_tcp);
int readMessageUdp(int fd, char *buffer, struct sockaddr_in *addr);
int parseMessage(char *buffer, userinfo userinfo, int fd_udp, struct sockaddr_in addr);
int formatMessage(int codeOperation, int codeStatus, char *message);
void sendMessageUdp(int fd, char *message, int len, struct sockaddr_in addr);
int searchDir(DIR *d, struct dirent *dir, char *uid);
int registerUser(char *uid, char *pass, char *PDIP, char *PDport);
int unregisterUser(char *uid, char *pass);
void sendMessageTcp(userinfo userinfo, char *message, int len);
int readMessageTcp(userinfo userinfo, char *buffer);
int loginUser(char *uid, char *pass, userinfo userinfo);
int approveRequest(char *uid, char *third, char *fourth, char *fifth, int *vc, struct sockaddr_in *addr);
void logoutUser(char *uid);


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
    int size = 1; // mudar
    userinfo *fds = calloc(size, sizeof(struct userinfo));
    int nextFreeEntry = 0;
    int code, len;
    char buffer[128];
    DIR *d;

    if ((d = opendir("USERS")) == NULL) {
        n = mkdir("USERS", 0777);
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
                if (FD_ISSET(fd_udp, &testfds)) {
                    n = readMessageUdp(fd_udp, buffer, &addr);
                    if (n == -1) break;
                    len = parseMessage(buffer, NULL, fd_udp, addr);
                }
                else if (FD_ISSET(fd_tcp, &testfds)) {
                    addrlen = sizeof(addr);
                    new_fd = accept(fd_tcp, (struct sockaddr *)&addr, &addrlen);
                    if (new_fd == ERROR) exit(1); //mudar
                    FD_SET(new_fd, &inputs);

                    fds[nextFreeEntry] = createSockinfo(new_fd, addr);
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
                            n = readMessageTcp(fds[i], buffer);
                            if (n == -1) break;
                            if (n == SOCKET_ERROR) {
                                FD_CLR(fds[i]->fd, &inputs);
                                close(fds[i]->fd);
                                if (fds[i]->uid != NULL) logoutUser(fds[i]->uid);
                                free(fds[i]->uid);
                                free(fds[i]);
                                fds[i] = NULL;
                                break;
                            }
                            if (n == -1 || n == SOCKET_ERROR) break;
                            len = parseMessage(buffer, fds[i], -1, addr);
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


int parseMessage(char *buffer, userinfo userinfo, int fd_udp, struct sockaddr_in addr) {
    int codeOperation, codeStatus, len, *vc = -1;
    char message[128], operation[4], uid[6], third[9], fourth[16], fifth[26];

    sscanf(buffer, "%s %s %s %s %s", operation, uid, third, fourth, fifth);

    codeOperation = verifyOperation(operation);

    switch (codeOperation) {
        case REG:
            codeStatus = registerUser(uid, third, fourth, fifth);
            len = formatMessage(codeOperation, codeStatus, message);
            sendMessageUdp(fd_udp, message, len, addr);
            break;
        
        case EXIT:
            codeStatus = unregisterUser(uid, third);
            len = formatMessage(codeOperation, codeStatus, message);
            sendMessageUdp(fd_udp, message, len, addr);
            break;
        
        case RVC:
            break;

        case LOGIN:
            codeStatus = loginUser(uid, third, userinfo);
            len = formatMessage(codeOperation, codeStatus, message);
            sendMessageTcp(userinfo, message, len);
            break;

        case REQ:
            codeStatus = approveRequest(uid, third, fourth, fifth, &vc, &addr);
            if (vc == -1) len = formatMessage(codeOperation, codeStatus, message);
            else len = sprintf(message, "VLC %s %04d %s");//mudar
            sendMessageUdp(fd_udp, message, len, addr);
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
        buffer[nread] = '\0';
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
        case RVC:
            
            break;
        case LOGIN:
            if (codeStatus == OK) len = sprintf(message, "RLO OK\n");
            else if (codeStatus == NOK) len = sprintf(message, "RLO NOK\n");
            else len = sprintf(message, "RLO ERR\n");
            break;
        case REQ:
            if (codeStatus == OK) len = sprintf(message, "RRQ OK\n");
            else if (codeStatus == ELOG) len = sprintf(message, "RRQ ELOG\n");
            else if (codeStatus == EPD) len = sprintf(message, "RRQ EPD\n");
            else if (codeStatus == EUSER) len = sprintf(message, "RRQ EUSER\n");
            else if (codeStatus == EFOP) len = sprintf(message, "RRQ EFOP\n");
            else if (codeStatus == ERR) len = sprintf(message, "RRQ ERR\n");
            break;
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
    buffer[nread] = '\0'; // need?
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    if (remove(path) != 0) return NOK;

    return OK;
}


int readMessageTcp(userinfo userinfo, char *buffer) {
    char *ptr = buffer;
    int nread;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    while (1) {
        nread = read(userinfo->fd, ptr, 127);
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
        inet_ntop(AF_INET, &userinfo->addr.sin_addr, ip, sizeof(ip));
        port = ntohs(userinfo->addr.sin_port);
        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
    }

    return 0;
}


void sendMessageTcp(userinfo userinfo, char *message, int len) {
    int nwritten;
    char *ptr = message;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    while (len > 0) {
        nwritten = write(userinfo->fd, ptr, len);
        if (nwritten <= 0) {
            puts("ERROR ON SEND");
            break;
        }
        len -= nwritten;
        ptr += nwritten;
    }
    if (verbose) {
        inet_ntop(AF_INET, &userinfo->addr.sin_addr, ip, sizeof(ip));
        port = ntohs(userinfo->addr.sin_port);
        printf("SENT TO %s %u: %s\n", ip, port, message);
    }
}


int loginUser(char *uid, char *pass, userinfo userinfo) {
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
    buffer[nread] = '\0';
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "USERS/%s/login.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return ERR;
    
    //inet_ntop(AF_INET, &userinfo->addr.sin_addr, ip, sizeof(ip));
    //port = ntohs(userinfo->addr.sin_port);

    len = sprintf(buffer, "%d", userinfo->fd);
    nwritten = fwrite(buffer, sizeof(char), len, fptr);
    if (nwritten != len) {
        fclose(fptr);
        return ERR;
    }
    fclose(fptr);

    strcpy(userinfo->uid, uid);

    return OK;
}


void logoutUser(char *uid) {
    char path[32];
    FILE *fptr;
    int nread;

    sprintf(path, "./USERS/%s/login.txt", uid);
    if (remove(path) != 0) puts("ERROR on logout");
}


int approveRequest(char *uid, char *third, char *fourth, char *fifth, int *vc, struct sockaddr_in *addr) {
    return 1;
}
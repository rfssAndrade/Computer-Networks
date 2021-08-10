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
#include <sys/time.h>
#include "verify.h"
#include "userinfo.h"
#include "message.h"
#include "AS.h"


char *ASport = NULL;
int verbose = 0;
userinfo *fds = NULL;
int size = 20;
fd_set inputs;


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
    fd_set testfds;
    struct sigaction action;
    int nextFreeEntry = 0;
    int len;
    char buffer[128];
    DIR *d;
    struct timeval tv_tcp_r, tv_udp_r, t2;
    int elapsedTime = 0;
    char message[128];

    fds = calloc(size, sizeof(struct userinfo));

    tv_tcp_r.tv_sec = 1;
    tv_tcp_r.tv_usec = 0;

    tv_udp_r.tv_sec = 0;
    tv_udp_r.tv_usec = 100;

    if ((d = opendir("USERS")) == NULL) {
        n = mkdir("USERS", 0777);
        if (n == ERROR) exit(1);
    }
    else closedir(d);

    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == ERROR) exit(1);

    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == ERROR) exit(1);

    n = setsockopt(fd_udp, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_udp_r, sizeof tv_udp_r);
    if (n == ERROR) exit(1);

    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == ERROR) exit(1);

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

    if (bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen) == ERROR) exit(1);

    if (bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == ERROR) exit(1);
    if (listen(fd_tcp, 20) == ERROR) exit(1);

    while (1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        switch (out_fds) {
            case 0:
                break;
            
            case ERROR:
                perror("select");
                exit(1);
            
            default:    
                memset(buffer, 0, 128);
                memset(message, 0, 128);

                if (FD_ISSET(fd_udp, &testfds)) {
                    n = readMessageUdp(fd_udp, buffer, &addr);
                    if (n == ERROR) break;
                    parseMessage(buffer, NULL, fd_udp, addr);
                }
                else if (FD_ISSET(fd_tcp, &testfds)) {
                    addrlen = sizeof(addr);
                    new_fd = accept(fd_tcp, (struct sockaddr *)&addr, &addrlen);
                    if (new_fd == ERROR) break;

                    n = setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_tcp_r, sizeof tv_tcp_r);
                    if (n == ERROR) break;

                    FD_SET(new_fd, &inputs);

                    fds[nextFreeEntry] = createUserinfo(new_fd, addr);
                    nextFreeEntry = findNextFreeEntry(fds, size);
                    if (nextFreeEntry == size) {
                        fds = realloc(fds, (size * 2) * sizeof(userinfo));
                        for (int i = nextFreeEntry; i < size * 2; i++) fds[i] = NULL;
                        size *= 2;
                    }
                }
                else {
                    for (int i = 0; i < size; i++) {
                        if (fds[i] != NULL && fds[i]->fd != 0 && FD_ISSET(fds[i]->fd, &testfds)) {
                            n = readMessageTcp(fds[i], buffer);
                            if (n == ERROR) break;
                            if (n == SOCKET_ERROR) {
                                FD_CLR(fds[i]->fd, &inputs);
                                close(fds[i]->fd);
                                if (fds[i]->uid != NULL) logoutUser(fds[i]->uid);
                                free(fds[i]->uid);
                                free(fds[i]->lastUploadedFile);
                                free(fds[i]);
                                fds[i] = NULL;
                                break;
                            }
                            parseMessage(buffer, fds[i], fd_udp, addr);
                            break;
                        }
                    }
                }
                break;
        }
        for (int i = 0; i < size; i++) {
            if (fds[i] != NULL && fds[i]->fd != 0 && fds[i]->waitingForPd == 1) {
                gettimeofday(&t2, NULL);
                elapsedTime = (t2.tv_sec - fds[i]->t.tv_sec) * 1000.0;
                elapsedTime += (t2.tv_usec - fds[i]->t.tv_usec) / 1000.0;
                if (elapsedTime > 5000) {
                    fds[i]->waitingForPd = 0;
                    len = sprintf(message, "RRQ EPD\n");
                    writeTcp(fds[i]->fd, len, message);
                }
            }
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
    if (verbose) {
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        port = ntohs(addr->sin_port);
        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer); // ver \n
    }
    return code;
}


void parseMessage(char *buffer, userinfo user, int fd_udp, struct sockaddr_in addr) {
    int codeOperation, codeStatus = 0, len, vc = -1, nread;
    char message[128], operation[4], uid[6], third[9], fourth[16], fifth[26];
    struct addrinfo *res;

    nread = sscanf(buffer, "%s %s %s %s %s", operation, uid, third, fourth, fifth);

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
            codeStatus = rvc(third);
            user = findUser(fds, uid, size);
            if (user == NULL || (user->uid != NULL && strcmp(user->uid, uid)) != 0) codeStatus = NOK;
            len = formatMessage(codeOperation, codeStatus, message);
            user->waitingForPd = 0;
            writeTcp(user->fd, len, message);;
            break;

        case LOGIN:
            codeStatus = loginUser(uid, third, user);
            len = formatMessage(codeOperation, codeStatus, message);
            writeTcp(user->fd, len, message);;
            break;

        case REQ:
            if (user->uid != NULL && strcmp(user->uid, uid) != 0) codeStatus = EUSER;
            else codeStatus = approveRequest(uid, third, fourth, fifth, &vc, &res);
            if (codeStatus != OK) {
                len = formatMessage(codeOperation, codeStatus, message);
                writeTcp(user->fd, len, message);;
            }
            else {
                if (nread == 5) len = sprintf(message, "VLC %s %04d %s %s\n", uid, vc, fourth, fifth);
                else len = sprintf(message, "VLC %s %04d %s\n", uid, vc, fourth);
                user->waitingForPd = 1;
                gettimeofday(&user->t, NULL);
                sendMessageUdpClient(fd_udp, message, len, res);
            }
            break;

        case VAL:
            if (user->uid != NULL && strcmp(user->uid, uid) != 0) codeStatus = 0;
            else codeStatus = validateUser(uid, third, fourth);
            len = formatMessage(codeOperation, codeStatus, message);
            writeTcp(user->fd, len, message);;
            break;

        case VLD:
            len = validateOperation(uid, third, message);
            sendMessageUdp(fd_udp, message, len, addr);
            break;

        default:
            len = sprintf(message, "ERR\n");
            if (user == NULL) sendMessageUdp(fd_udp, message, len, addr);
            else writeTcp(user->fd, len, message);;
            break;
    }
}


int registerUser(char *uid, char *pass, char *PDIP, char *PDport) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    int code, nwritten, len;
    char path[64], buffer[128];
    FILE *fptr;

    if (verifyUid(uid) != 0 || verifyPass(pass) != 0 || verifyIp(PDIP) != 0 || verifyPort(PDport) != 0) return NOK;

    dUsers = opendir("USERS");
    if (dUsers == NULL) return NOK;
    if (!searchDir(dUsers, dir, uid)) {
        sprintf(path, "./USERS/%s", uid);
        code = mkdir(path, 0777);
        if (code == ERROR) {
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
        memset(buffer, 0, 128);
        fread(buffer, sizeof(char), 16, fptr);
        if (strcmp(buffer, pass) != 0) {
            fclose(fptr);
            return NOK;
        }
    }
    fclose(fptr);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return NOK;

    len = sprintf(buffer, "%s %s", PDIP, PDport);
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
    int len = 0;
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
            break;
        case RVC:
            if (codeStatus == OK) len = sprintf(message, "RRQ OK\n");
            else if (codeStatus == NOK) len = sprintf(message, "RRQ EUSER\n");
            break;
        case REQ:
            if (codeStatus == ELOG) len = sprintf(message, "RRQ ELOG\n");
            else if (codeStatus == EPD) len = sprintf(message, "RRQ EPD\n");
            else if (codeStatus == EUSER) len = sprintf(message, "RRQ EUSER\n");
            else if (codeStatus == EFOP) len = sprintf(message, "RRQ EFOP\n");
            else if (codeStatus == ERR) len = sprintf(message, "RRQ ERR\n");
            break;
        case VAL:
            if (codeStatus == 0) len = sprintf(message, "RAU 0\n");
            else len = sprintf(message, "RAU %04d\n", codeStatus);
            break;
    }
    return len;
}


void sendMessageUdp(int fd, char *message, int len, struct sockaddr_in addr) {
    int code;
    socklen_t addrlen = sizeof(addr);

    code = sendto(fd, message, len, 0, (struct sockaddr *)&addr, addrlen);
    if (code == ERROR) printf("Error on send\n");
}


void sendMessageUdpClient(int fd, char *message, int len, struct addrinfo *res) {
    int code;

    code = sendto(fd, message, len, 0, res->ai_addr, res->ai_addrlen);
    if (code == ERROR) printf("Error on send\n");
}


int unregisterUser(char *uid, char *pass) {
    char path[64], buffer[16];
    FILE *fptr;

    sprintf(path, "./USERS/%s/pass.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) return NOK;

    memset(buffer, 0, 16);
    fread(buffer, sizeof(char), 16, fptr);
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    remove(path);

    return OK;
}


int readMessageTcp(userinfo user, char *buffer) {
    char *ptr = buffer;
    int nread;
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    nread = readTcp(user->fd, 127, ptr);
    if (nread <= 0) return nread;
    ptr += nread;
    *ptr = '\0';

    if (verbose) {
        inet_ntop(AF_INET, &user->addr.sin_addr, ip, sizeof(ip));
        port = ntohs(user->addr.sin_port);
        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
    }

    return 0;
}


int loginUser(char *uid, char *pass, userinfo user) {
    char path[64], buffer[16];
    FILE *fptr;
    int nwritten, len;
    DIR *dUsers;
    struct dirent *dir = NULL;

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

    memset(buffer, 0, 16);
    fread(buffer, sizeof(char), 16, fptr);
    if (strcmp(buffer, pass) != 0) {
        fclose(fptr);
        return NOK;
    }
    fclose(fptr);

    sprintf(path, "USERS/%s/login.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return ERR;

    len = sprintf(buffer, "%d", user->fd);
    nwritten = fwrite(buffer, sizeof(char), len, fptr);
    if (nwritten != len) {
        fclose(fptr);
        return ERR;
    }
    fclose(fptr);

    strcpy(user->uid, uid);

    return OK;
}


void logoutUser(char *uid) {
    char path[64];

    sprintf(path, "./USERS/%s/login.txt", uid);
    remove(path);
}


int approveRequest(char *uid, char *rid, char *fop, char *fname, int *vc, struct addrinfo **res) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    FILE *fptr;
    char path[64], buffer[128], PDIP[32], PDport[8];
    int len, nwritten;
    struct addrinfo hints;
    ssize_t n;

    if (verifyTid(rid) != 0) return ERR;
    if (verifyUid(uid) != 0) return EUSER;
    if (verifyFop(fop, fname) != 0) return EFOP;

    dUsers = opendir("USERS");
    if (dUsers == NULL) return EUSER;
    if (!searchDir(dUsers, dir, uid)) {
            closedir(dUsers);
            return ELOG;
    }
    closedir(dUsers);

    sprintf(path, "./USERS/%s", uid);
    dUsers = opendir(path);
    if (dUsers == NULL) return ELOG;
    if (!searchDir(dUsers, dir, "login.txt")) {
            closedir(dUsers);
            return ELOG;
    }
    closedir(dUsers);

    sprintf(path, "./USERS/%s/reg.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) return EPD;

    memset(buffer, 0, 128);
    fread(buffer, sizeof(char), 127, fptr);
    sscanf(buffer, "%s %s", PDIP, PDport);
    fclose(fptr);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    n = getaddrinfo(PDIP, PDport, &hints, res);
    if (n == ERROR) return EPD;

    sprintf(path, "./USERS/%s/req.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return ERR;

    memset(buffer, 0, 128);
    *vc = rand() % 10000;
    len = sprintf(buffer, "%s %04d %s %s", rid, *vc, fop, fname);
    nwritten = fwrite(buffer, sizeof(char), len, fptr);
    if (nwritten != len) {
            fclose(fptr);
            return NOK;
    }
    fclose(fptr);

    return OK;
}


int rvc(char *status) {
    if (strcmp(status, "OK") == 0) return OK;
    return NOK;
}


int validateUser(char *uid, char *rid, char *vc) {
    FILE *fptr;
    char path[64], buffer[128], temp[128], fop[4], fname[32], localRid[8], localVc[8];
    int tid, len;

    if (verifyUid(uid) != 0) return 0;

    sprintf(path, "USERS/%s/req.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) return 0;

    memset(buffer, 0, 128);
    fread(buffer, sizeof(char), 127, fptr);
    sscanf(buffer, "%s %s %s %s", localRid, localVc, fop, fname);

    if (strcmp(localRid, rid) != 0 || strcmp(localVc, vc) != 0) {
        fclose(fptr);
        return 0;
    }
    fclose(fptr);
    remove(path);

    sprintf(path, "USERS/%s/tid.txt", uid);
    fptr = fopen(path, "w");
    if (fptr == NULL) return 0;

    tid = rand() % 10000;
    len = sprintf(temp, "%04d %s %s", tid, fop, fname);
    fwrite(temp, sizeof(char), len, fptr);

    fclose(fptr);
    return tid;
}


int validateOperation(char *uid, char *tid, char *message) {
    FILE *fptr;
    char path[64], buffer[128], localTid[8], fop[4], fname[32];
    int len, nread, fopC;
    userinfo user;

    sprintf(path, "USERS/%s/tid.txt", uid);
    fptr = fopen(path, "r");
    if (fptr == NULL) {
        len = sprintf(message, "CNF %s %s E\n", uid, tid);
        return len;
    }

    memset(buffer, 0, 128);
    fread(buffer, sizeof(char), 127, fptr);
    nread = sscanf(buffer, "%s %s %s", localTid, fop, fname);
    fclose(fptr);

    if (strcmp(localTid, tid) != 0) {
        len = sprintf(message, "CNF %s %s E\n", uid, tid);
        return len;
    }
    
    fopC = fopCode(fop);
    if (fopC != LIST || fopC != REMOVE) len = sprintf(message, "CNF %s %s %s %s\n", uid, tid, fop, fname); // ver isto
    else len = sprintf(message, "CNF %s %s %s\n", uid, tid, fop);

    if (fopCode(fop) == REMOVE) {
        user = findUser(fds, uid, size);
        FD_CLR(user->fd, &inputs);
        close(user->fd);
        removeUser(uid);
        free(user->uid);
        free(user->lastUploadedFile);
        free(user);
        user = NULL;
    }

    return len;
}


void removeUser(char *uid) {
    DIR *dUser;
    struct dirent *dir;
    char path[64];

    sprintf(path, "USERS/%s", uid);
    dUser = opendir(path);
    while((dir = readdir(dUser)) != NULL) {
        sprintf(path, "USERS/%s/%s", uid, dir->d_name);
        remove(path);
    }
    closedir(dUser);

    sprintf(path, "USERS/%s", uid);
    rmdir(path);
}
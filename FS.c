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
#include "FS.h"


char *FSport = NULL;
char *ASIP = NULL;
char *ASport = NULL;
int verbose = 0;


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


// Parse arguments from program execution
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
                exit(1);
                break;
        }
    }

    if (strlen(FSport) == 0) strcpy(FSport, "59034");
    if (strlen(ASIP) == 0) strcpy(ASIP, "127.0.0.1");
    if (strlen(ASport) == 0) strcpy(ASport, "58034");
}


// Handles all connections
void makeConnection() {
    int fd_udp, fd_tcp, out_fds, new_fd;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints_udp, hints_tcp, *res_udp, *res_tcp;
    struct sockaddr_in addr;
    fd_set inputs, testfds;
    struct sigaction action;
    int size = 20;
    userinfo *fds = calloc(size, sizeof(struct userinfo));
    int nextFreeEntry = 0;
    DIR *d;
    char buffer[128], message[128];
    int len, code, nread;
    userinfo user;
    struct timeval tv_tcp_r, tv_udp_r;
    char dummy[1024];
    char ip[INET_ADDRSTRLEN];
    unsigned int port;

    tv_tcp_r.tv_sec = 1;
    tv_tcp_r.tv_usec = 0;

    tv_udp_r.tv_sec = 0;
    tv_udp_r.tv_usec = 100;

    // create USERSF directory to store user's files
    if ((d = opendir("USERSF")) == NULL) {
        n = mkdir("USERSF", 0777);
        if (n == ERROR) exit(1);
    }
    else closedir(d);

    // Handle signal
    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == ERROR) exit(1);

    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == ERROR) exit(1);

    // Set socket timeout
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

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    n = getaddrinfo(ASIP, ASport, &hints_udp, &res_udp);
    if (n != 0) exit(1);

    n = getaddrinfo(NULL, FSport, &hints_tcp, &res_tcp);
    if (n != 0) exit(1);

    if (bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == ERROR) exit(1);
    if (listen(fd_tcp, 20) == ERROR) exit(1);

    while(1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        switch (out_fds) {
            case 0:
                break;
            case ERROR:
                perror("select");
                exit(1);
                break;

            default:
                memset(buffer, 0, 128);
                memset(message, 0, 128);

                if (FD_ISSET(fd_udp, &testfds)) { // AS
                    addrlen = sizeof(addr);
                    n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
                    if (n == ERROR) break;

                    if (verbose) {
                        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
                        port = ntohs(addr.sin_port);
                        printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
                    }
                    
                    user = parseMessageAS(buffer, message, fds, size);
                    if (user != NULL) {
                        FD_CLR(user->fd, &inputs);
                        close(user->fd);
                        free(user->lastUploadedFile);
                        free(user->uid);
                        free(user);
                        user = NULL;
                    }
                }
                
                else if (FD_ISSET(fd_tcp, &testfds)) { // accept user applications and store their info
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
                        if (fds[i] != NULL && fds[i]->fd != 0 && FD_ISSET(fds[i]->fd, &testfds)) { // user applications
                            n = readTcp(fds[i]->fd, 3, buffer);

                            code = verifyOperation(buffer);
                            if (code == UPLOAD) {
                                n = readTcp(fds[i]->fd, 12, buffer + 3);
                                if (verbose) {
                                    inet_ntop(AF_INET, &fds[i]->addr.sin_addr, ip, sizeof(ip));
                                    port = ntohs(fds[i]->addr.sin_port);
                                    printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
                                }
                                len = upload(buffer, message, fds[i]);
                                if (len <= 9) {
                                    nread = 1;
                                    while (nread > 0) {
                                        nread = readTcp(fds[i]->fd, 1023, dummy);
                                    }
                                }
                            }
                            else {
                                n = readTcp(fds[i]->fd, 124, buffer + 3);
                                len = parseMessageUser(buffer, message, fds[i]);
                                if (verbose) {
                                    inet_ntop(AF_INET, &fds[i]->addr.sin_addr, ip, sizeof(ip));
                                    port = ntohs(fds[i]->addr.sin_port);
                                    printf("RECEIVED FROM %s %u: %s\n", ip, port, buffer);
                                }
                            }


                            if (len > 9) {
                                code = sendto(fd_udp, message, strlen(message), 0, res_udp->ai_addr, res_udp->ai_addrlen);
                                if (code == ERROR) break;
                            }
                            else {
                                writeTcp(fds[i]->fd, len, message);
                                FD_CLR(fds[i]->fd, &inputs);
                                close(fds[i]->fd);
                                free(fds[i]->lastUploadedFile);
                                free(fds[i]->uid);
                                free(fds[i]);
                                fds[i] = NULL;
                            }
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


// Parse message from user
int parseMessageUser(char *buffer, char *message, userinfo user) {
    int code, len, error;
    char  operation[4], uid[8], tid[8], fname[32], path[64];
    DIR *dUsers;
    struct dirent *dir = NULL;

    sscanf(buffer, "%s %s %s %s", operation, uid, tid, fname);
    code = verifyOperation(operation);

    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "%s NOK\n", operation);
        return len;
    }

    // create UID directory to store user's files
    sprintf(path, "USERSF/%s", uid);
    if (!searchDir(dUsers, dir, uid)) {
        error = mkdir(path, 0777);
        if (error == ERROR) {
            closedir(dUsers);
            len = sprintf(message, "%s NOK\n", operation);
            return len;
        }
    }
    closedir(dUsers);

    switch (code) {
        case LIST:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "RLS ERR\n");
            else {
                len = sprintf(message, "VLD %s %s\n", uid, tid);
                strcpy(user->uid, uid);
            }
            break;
        case RETRIEVE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0 || verifyFname(fname) != 0) len = sprintf(message, "RRT ERR\n");
            else {
                len = sprintf(message, "VLD %s %s\n", uid, tid);
                strcpy(user->uid, uid);
            }
            break;
        case DELETE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0 || verifyFname(fname) != 0) len = sprintf(message, "DEL ERR\n");
            else {
                len = sprintf(message, "VLD %s %s\n", uid, tid);
                strcpy(user->uid, uid);
            }
            break;
        case REMOVE:
            if (verifyUid(uid) != 0 || verifyTid(tid) != 0) len = sprintf(message, "REM ERR\n");
            else {
                len = sprintf(message, "VLD %s %s\n", uid, tid);
                strcpy(user->uid, uid);
            }
            break;
        default:
            len = sprintf(message, "ERR\n");
            break;
    }
    user->lastOp = code;
    return len;
}


// Parse messages from AS
userinfo parseMessageAS(char *buffer, char *message, userinfo *fds, int size) {
    int code, len;
    userinfo user;
    char operation[4], uid[6], third[9], fourth[16], fifth[26];

    sscanf(buffer, "%s %s %s %s %s", operation, uid, third, fourth, fifth);

    code = verifyOperation(operation);
    if (code != CNF || verifyUid(uid) != 0 || verifyTid(third) != 0 || verifyFop(fourth, fifth) != 0) return NULL;

    user = findUser(fds, uid, size);

    if (user == NULL) return NULL;
    else code = fopCode(fourth);

    switch (code) {
        case LIST:
            list(user, uid);
            break;
        case RETRIEVE:
            retrieve(user, uid, fifth);
            break;
        case DELETE:
            delete(user, uid, fifth);
            break;
        case REMOVE:
            removeUser(user, uid);
            break;
        case UPLOAD:
            len = sprintf(message, "RUP OK\n");
            writeTcp(user->fd, len, message);
            break;
        case INV:
            if (user->lastOp == UPLOAD) removeFile(user, uid);
            len = sprintf(message, "%s INV\n", operation);
            writeTcp(user->fd, len, message);
            break;
    }
    return user;
}


// Do list operation
void list(userinfo user, char *uid) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    char path[64], files[1024], message[1024], temp[64];
    int nfiles = 0, len;
    off_t fsize;

   memset(files, 0, 1024);

    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "RLS ERR\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!searchDir(dUsers, dir, uid)) { // check if files are available
        closedir(dUsers);
        len = sprintf(message, "RLS EOF\n");
        writeTcp(user->fd, len, message);
        return;
    }
    closedir(dUsers);

    sprintf(path, "USERSF/%s", uid);
    dUsers = opendir(path);
    while ((dir = readdir(dUsers)) != NULL) {
        if (strlen(dir->d_name) > 3) { // avoid dirs . and ..
            nfiles++;
            sprintf(path, "USERSF/%s/%s", uid, dir->d_name);
            fsize = fileSize(path);
            sprintf(temp, " %s %lu", dir->d_name, fsize);
            strcat(files, temp); // construct files list
        }
    }
    sprintf(path, "USERSF/%s", uid);
    closedir(dUsers);
    if (nfiles == 0) len = sprintf(message, "RLS EOF\n");
    else len = sprintf(message, "RLS %d%s\n", nfiles, files);

    writeTcp(user->fd, len, message);
}


// Search directory for directory or file
int searchDir(DIR *d, struct dirent *dir, char *uid) {
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, uid) == 0) return 1;
    }
    return 0;
}


// Determine file size
off_t fileSize(char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    return ERROR;
}


// Do remove request
void removeUser(userinfo user, char *uid) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    char path[64], message[16];
    int len;

    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "RRM NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!searchDir(dUsers, dir, uid)) {
        closedir(dUsers);
        len = sprintf(message, "RRM NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    closedir(dUsers);

    sprintf(path, "USERSF/%s", uid);
    dUsers = opendir(path);
    while((dir = readdir(dUsers)) != NULL) {
        sprintf(path, "USERSF/%s/%s", uid, dir->d_name);
        remove(path);
    }
    closedir(dUsers);
    sprintf(path, "USERSF/%s", uid);
    rmdir(path);

    len = sprintf(message, "RRM OK\n");
    writeTcp(user->fd, len, message);
}


// Do delete request
void delete(userinfo user, char *uid, char *fname) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    char path[64], message[16];
    int len = 0, deleted = 0;

    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "RDL NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!searchDir(dUsers, dir, uid)) {
        closedir(dUsers);
        len = sprintf(message, "RDL NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    closedir(dUsers);

    sprintf(path, "USERSF/%s", uid);
    dUsers = opendir(path);
    if (dUsers == NULL) {
        len = sprintf(message, "RDL NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!searchDir(dUsers, dir, fname)) {
        closedir(dUsers);
        len = sprintf(message, "RDL EOF\n");
        writeTcp(user->fd, len, message);
        return;
    }
    closedir(dUsers);

    sprintf(path, "USERSF/%s/%s", uid, fname);
    len = remove(path);
    if(len == 0) deleted = 1;

    if (deleted) len = sprintf(message, "RDL OK\n");
    else len = sprintf(message, "RDL EOF\n");

    writeTcp(user->fd, len , message);
}


// Do retrieve request
void retrieve(userinfo user, char *uid, char *fname) {
    DIR *dUsers;
    struct dirent *dir = NULL;
    char path[64], message[64], buffer[1024];
    int len, found = 0, empty = 1, nread = 0, nwritten = 0;
    off_t fsize;
    FILE *fptr;

    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "RRT NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!searchDir(dUsers, dir, uid)) {
        closedir(dUsers);
        len = sprintf(message, "RRT NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    closedir(dUsers);

    sprintf(path, "USERSF/%s", uid);
    dUsers = opendir(path);
    while((dir = readdir(dUsers)) != NULL) {
        empty = 0;
        if (strcmp(fname, dir->d_name) == 0) {
            found = 1;
            break;
        }
    }
    closedir(dUsers);

    if (empty) {
        len = sprintf(message, "RRT NOK\n");
        writeTcp(user->fd, len, message);
        return;
    }
    if (!found) {
        len = sprintf(message, "RRT EOF\n");
        writeTcp(user->fd, len, message);
        return;
    }

    sprintf(path, "USERSF/%s/%s", uid, fname);
    fptr = fopen(path, "r");
    if (fptr == NULL) {
        len = sprintf(message, "RRT EOF\n");
        writeTcp(user->fd, len, message);
        return;
    }

    fsize = fileSize(path);

    len = sprintf(message, "RRT OK %ld ", fsize);
    writeTcp(user->fd, len, message);

    while (fsize > 0) {
        nread = fread(buffer, sizeof(char), 1023, fptr);

        nwritten = writeTcp(user->fd, nread, buffer);

        fsize -= nwritten;
    }
    writeTcp(user->fd, 1, "\n");
    fclose(fptr);
}


// Do upload request
int upload(char *buffer, char *message, userinfo user) {
    char  operation[4], uid[8], tid[8], fname[32], size[16];
    char *ptr = fname, path[64];
    off_t fsize;
    int nread = 0, len = 0, code, nfiles = 0, found = 0;
    FILE *fptr;
    DIR *dUsers;
    struct dirent *dir = NULL;

    sscanf(buffer, "%s %s %s", operation, uid, tid);

    while (1) {
        nread  = readTcp(user->fd, 1, ptr);
        if (nread <= 0) return nread;
        ptr += nread;

        if (*(ptr-1) == ' ') break;
    }
    *(ptr-1) = '\0';

    ptr = size;
    while (1) {
        nread  = readTcp(user->fd, 1, ptr);
        if (nread <= 0) return nread;
        ptr += nread;

        if (*(ptr-1) == ' ') break;
    }
    *(ptr-1) = '\0';
    fsize = atoi(size);
    if (fsize == 0 || fsize > 999999999 || verifyUid(uid) != 0 || verifyTid(tid) != 0 || verifyFname(fname) != 0) {
        len = sprintf(message, "RUP ERR\n");
        return len;
    }
    
    dUsers = opendir("USERSF");
    if (dUsers == NULL) {
        len = sprintf(message, "RUP NOK\n");
        return len;
    }
    sprintf(path, "USERSF/%s", uid);
    if (!searchDir(dUsers, dir, uid)) {
        code = mkdir(path, 0777);
        if (code == ERROR) {
            closedir(dUsers);
            len = sprintf(message, "RUP NOK\n");
            return len;
        }
    }
    closedir(dUsers);

    dUsers = opendir(path);
    while((dir = readdir(dUsers)) != NULL) {

        if (strlen(dir->d_name) > 3) nfiles++;
        if (strcmp(fname, dir->d_name) == 0) {
            found = 1;
            break;
        }
    }
    closedir(dUsers);
    if (nfiles == 15) {
        len = sprintf(message, "RUP FULL\n");
        return len;
    }
    if (found) {
        len = sprintf(message, "RUP DUP\n");
        return len;
    }

    ptr = buffer;
    sprintf(path, "USERSF/%s/%s", uid, fname);
    fptr = fopen(path, "w");
    if (fptr == NULL) {
        len = sprintf(message, "RUP NOK\n");
        return len;
    }
    while (fsize > 0) {
        if (fsize < 127) nread  = readTcp(user->fd, fsize, ptr);
        else nread  = readTcp(user->fd, 127, ptr);
        if (nread < 0) {
            fclose(fptr);
            return nread;
        }
        fsize -= nread;
        if (fsize == -1) {
            ptr += nread - 1;
            *ptr = EOF;
        }
        fwrite(buffer, sizeof(char), nread, fptr);
        ptr = buffer;
    }
    ptr = buffer;
    readTcp(user->fd, 1, ptr);
    len = sprintf(message, "VLD %s %s\n", uid, tid);
    fclose(fptr);
    strcpy(user->uid, uid);
    memset(user->lastUploadedFile, 0, 32);
    strcpy(user->lastUploadedFile, fname);
    user->lastOp = UPLOAD;

    return len;
}


// Remove file for user with uid
void removeFile(userinfo user, char *uid) {
    char path[64];

    sprintf(path, "USERSF/%s/%s", uid, user->lastUploadedFile);
    remove(path);
}
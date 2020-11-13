#ifndef USERINFO_H
#define USERINFO_H


#include <netdb.h>


typedef struct userinfo {
    int fd;
    struct sockaddr_in addr;
    char *uid;
    int lastOp;
    char *lastUploadedFile;
    struct timeval t;
    int waitingForPd;
} *userinfo;


userinfo createUserinfo(int fd, struct sockaddr_in addr);
int findNextFreeEntry(userinfo *fds, int size);
void closeFds(int size, userinfo *fds, int fd_udp, int fd_tcp);
userinfo findUser(userinfo *fds, char *uid, int size);


#endif
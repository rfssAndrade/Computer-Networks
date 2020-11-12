#ifndef SOCKINFO_H
#define SOCKINFO_H


#include <netdb.h>


typedef struct sockinfo {
    int fd;
    struct sockaddr_in addr;
    char *uid;
} *Sockinfo;


Sockinfo createSockinfo(int fd, struct sockaddr_in addr);
int findNextFreeEntry(Sockinfo *fds, int size);
void closeFds(int size, Sockinfo *fds, int fd_udp, int fd_tcp);


#endif
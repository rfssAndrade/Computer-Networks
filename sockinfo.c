#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sockinfo.h"


Sockinfo createSockinfo(int fd, struct sockaddr_in addr) {
    Sockinfo new = malloc(sizeof(struct sockinfo));

    new->fd = fd;
    memcpy(&new->addr, &addr, sizeof(addr));
    new->uid = calloc(6, sizeof(char));

    return new;
}


int findNextFreeEntry(Sockinfo *fds, int size) {
    for (int i = 0; i < size; i++) {
        if (fds[i] == NULL) return i;
    }
    return size;
}


void closeFds(int size, Sockinfo *fds, int fd_udp, int fd_tcp) {
    for (int i = 0; i < size; i++) {
        if (fds[i] != NULL) {
            close(fds[i]->fd);
            free(fds[i]->uid);
            free(fds[i]);
        }
    }
    close(fd_udp);
    close(fd_tcp);
    free(fds);
}
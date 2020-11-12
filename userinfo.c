#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "userinfo.h"


userinfo createSockinfo(int fd, struct sockaddr_in addr) {
    userinfo new = malloc(sizeof(struct userinfo));

    new->fd = fd;
    memcpy(&new->addr, &addr, sizeof(addr));
    new->uid = calloc(6, sizeof(char));

    return new;
}


int findNextFreeEntry(userinfo *fds, int size) {
    for (int i = 0; i < size; i++) {
        if (fds[i] == NULL) return i;
    }
    return size;
}


void closeFds(int size, userinfo *fds, int fd_udp, int fd_tcp) {
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
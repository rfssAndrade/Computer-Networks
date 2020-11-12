#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "message.h"
#include "verify.h"


extern int errno;


int readTcp(int fd, int nBytes, char *ptr) {
    int nread = 0, tread = 0;
    printf("%d--",nBytes);
    while (tread != nBytes) {
        nread = read(fd, ptr, nBytes - tread);
        if (nread < 0) {
            if (tread == 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                puts("Read timeout");
                return ERROR;
            }
            else return tread;
        }
        else if (nread == 0) {
            printf("Server closed socket\n");
            if (tread == 0) return SOCKET_ERROR;
            else return tread;
        }

        ptr += nread;
        tread += nread;
    }
    return tread;
}


int writeTcp(int fd, int nBytes, char *ptr) {
    int nwritten = 0, twritten = 0;

    while (twritten != nBytes) {
        nwritten = write(fd, ptr, nBytes - twritten);

        if (nwritten < 0) {
            if (twritten == 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                puts("Write timeout");
                return ERROR;
            }
            else if (twritten == 0 && errno == EPIPE) {
                printf("Server closed socket\n");
                return SOCKET_ERROR;
            }
            else return twritten;
        }

        ptr += nwritten;
        twritten += nwritten;
    }
    return twritten;
}
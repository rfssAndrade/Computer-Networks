#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "message.h"
#include "verify.h"


extern int errno;


int readTcp(int fd, int nBytes, char *ptr) {
    int nread = 0, tread = 0;

    while (tread != nBytes) {
        nread = read(fd, ptr, nBytes - tread);

        if (nread < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {puts("ERROR ON READ");
            return ERROR;}
        }
        else if (tread == 0 && nread == 0) {
            printf("Server closed socket\n");
            return SOCKET_ERROR;
        }
        else if (nread == 0) return tread;

        ptr += nread;
        tread += nread;
        //if (*(ptr-1) == '\n') break;
    }
    return tread;
}
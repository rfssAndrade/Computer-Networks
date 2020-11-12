#ifndef MESSAGE_H
#define MESSAGE_H


#define SOCKET_ERROR -2


int readTcp(int fd, int nBytes, char *ptr);
int writeTcp(int fd, int nBytes, char *ptr);


#endif
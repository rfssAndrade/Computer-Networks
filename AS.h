#ifndef AS_H
#define AS_H


#include "userinfo.h"
#include <dirent.h>


void parseArgs(int argc, char **argv);
void makeConnection();
int readMessageUdp(int fd, char *buffer, struct sockaddr_in *addr);
void parseMessage(char *buffer, userinfo user, int fd_udp, struct sockaddr_in addr);
int formatMessage(int codeOperation, int codeStatus, char *message);
void sendMessageUdp(int fd, char *message, int len, struct sockaddr_in addr);
void sendMessageUdpClient(int fd, char *message, int len, struct addrinfo *res);
int searchDir(DIR *d, struct dirent *dir, char *uid);
int registerUser(char *uid, char *pass, char *PDIP, char *PDport);
int unregisterUser(char *uid, char *pass);
int readMessageTcp(userinfo user, char *buffer);
int loginUser(char *uid, char *pass, userinfo userinfo);
void logoutUser(char *uid);
int approveRequest(char *uid, char *rid, char *fop, char *fname, int *vc, struct addrinfo **res);
int rvc(char *status);
int validateUser(char *uid, char *rid, char *vc);
int validateOperation(char *uid, char *tid, char *message);
void removeUser(char *uid);


#endif
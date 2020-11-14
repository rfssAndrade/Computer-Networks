#ifndef PD_H
#define PD_H


void parseArgs(int argc, char **argv);
int parseInput(char *buffer, char *command, char *uid, char *pass);
void makeConnection();
int verifyAnswer(char *answer);
void formatMessage(char *message, int code, char *second, char *third);
void sendMessageClient(int fd, char *message, struct addrinfo *res);
void readMessage(int fd, char *answer, struct sockaddr_in *addr);
void sendMessageServer(int fd, char *message, struct sockaddr_in addr);
void parseMessage(char *answer, char *message, char *command, char *second);


#endif
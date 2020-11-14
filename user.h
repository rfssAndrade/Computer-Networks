#ifndef USER_H
#define USER_H


void parseArgs(int argc, char **argv);
void makeConnection();
int parseInput(char *buffer, char *command, char *second, char *third);
void formatMessage(char *message, int code, char *second, char *third);
void sendMessage(int code, int fd_as, int fd_fs, char *message);
int readMessageAS(int fd, char *answer);
int readMessageFS(int fd);
int verifyAnswerAS(char *answer);
int parseAnswerFS(char *operation, int code, int fd);
int verifyAnswerFS(char *answer);
int parseAnswerAS(char *answer, char *command, char *second);
int uploadFile(int fd, char *message);
off_t fileSize(char *filename);


#endif
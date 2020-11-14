#ifndef FS_H
#define FS_H


#include "userinfo.h"
#include "dirent.h"


void parseArgs(int argc, char **argv);
void makeConnection();
int parseMessageUser(char *buffer, char *message, userinfo user);
userinfo parseMessageAS(char *buffer, char *message, userinfo *fds, int size);
void list(userinfo user, char *uid);
int searchDir(DIR *d, struct dirent *dir, char *uid);
off_t fileSize(char *filename);
void removeUser(userinfo user, char *uid);
void delete(userinfo user, char *uid, char *fname);
void retrieve(userinfo user, char *uid, char *fname);
int upload(char *buffer, char *message, userinfo user);
void removeFile(userinfo user, char *uid);


#endif
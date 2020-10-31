#ifndef VERIFY_H
#define VERIFY_H


#define ERROR -1
#define PASS_SIZE 8
#define UID_SIZE 5
#define VC_SIZE 4
#define FILE_SIZE 24
#define EXTENSION_SIZE 3
#define PORT_SIZE 5


int verifyUid(char *uid);
int verifyPass(char *pass);
int verifyFop(char *fop, char *fname);
int verifyFname(char *fname);
int verifyVc(char *vc);
int verifyPort(char *port);
int validateNumber(char *str);
int verifyIp(char *ip);


#endif
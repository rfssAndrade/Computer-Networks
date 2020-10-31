#ifndef VERIFY_H
#define VERIFY_H

// SIZES
#define PASS_SIZE 8
#define UID_SIZE 5
#define VC_SIZE 4
#define FILE_SIZE 24
#define EXTENSION_SIZE 3
#define PORT_SIZE 5

//CODES
#define ERROR -1
#define REG 1
#define LOGIN 2
#define REQ 3
#define VAL 4
#define LIST 5
#define RETRIEVE 6
#define UPLOAD 7
#define DELETE 8
#define REMOVE 9
#define EXIT 10


int verifyCommand(char *command);
int verifyUid(char *uid);
int verifyPass(char *pass);
int verifyFop(char *fop, char *fname);
int verifyFname(char *fname);
int verifyVc(char *vc);
int verifyPort(char *port);
int validateNumber(char *str);
int verifyIp(char *ip);


#endif
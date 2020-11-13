#ifndef VERIFY_H
#define VERIFY_H


// SIZES
#define PASS_SIZE 8
#define UID_SIZE 5
#define VC_SIZE 4
#define FILE_NAME_SIZE 24
#define EXTENSION_SIZE 3
#define PORT_SIZE 5
#define FILE_SIZE_DIGITS 10


//CODES USER INPUT
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


// CODES PROTOCOL
#define RAU 11
#define RVC 12
#define RLS 13
#define RRT 14
#define VLC 15
#define VLD 16
#define CNF 17


// CODES COMMAND LINE
#define D 1
#define N 2
#define P 3
#define M 4
#define Q 5
#define V 6


// CODES ERROR
#define ERROR -1
#define SOCKET_ERROR -2


// CODES STATUS
#define OK 18
#define NOK 19
#define ERR 20
#define ELOG 21
#define EPD 22
#define EUSER 23
#define EFOP 24
#define EOFILE 25
#define INV 26
#define DUP 27
#define FULL 28



int verifyArg(char *flag, char *arg);
int verifyCommand(char *command);
int verifyOperation(char *operation);
int verifyUid(char *uid);
int verifyPass(char *pass);
int verifyFop(char *fop, char *fname);
int verifyFname(char *fname);
int verifyVc(char *vc);
int verifyPort(char *port);
int validateNumber(char *str);
int verifyIp(char *ip);
int verifyTid(char *tid);
int verifyFsize(char *fsize);
int fopCode(char *fop);


#endif
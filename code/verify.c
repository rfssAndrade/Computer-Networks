#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "verify.h"


int verifyArg(char *flag, char *arg) {
    if (strcmp(flag, "-d") == 0 && verifyPort(arg) == 0) return D;
    if (strcmp(flag, "-n") == 0 && verifyIp(arg) == 0) return N;
    if (strcmp(flag, "-p") == 0 && verifyPort(arg) == 0) return P;
    if (strcmp(flag, "-m") == 0 && verifyIp(arg) == 0) return M;
    if (strcmp(flag, "-q") == 0 && verifyPort(arg) == 0) return Q;
    if (strcmp(flag, "-v") == 0) return V;

    return ERROR;
}


int verifyCommand(char *command) {
    if (strcmp(command, "reg") == 0) return REG;
    if (strcmp(command, "login") == 0) return LOGIN;
    if (strcmp(command, "req") == 0) return REQ;
    if (strcmp(command, "val") == 0) return VAL;
    if (strcmp(command, "list") == 0 || strcmp(command, "l") == 0) return LIST;
    if (strcmp(command, "retrieve") == 0 || strcmp(command, "r") == 0) return RETRIEVE;
    if (strcmp(command, "upload") == 0 || strcmp(command, "u") == 0) return UPLOAD;
    if (strcmp(command, "delete") == 0 || strcmp(command, "d") == 0) return DELETE;
    if (strcmp(command, "remove") == 0 || strcmp(command, "x") == 0) return REMOVE;
    if (strcmp(command, "exit") == 0) return EXIT;
    
    return ERROR;
}


int verifyOperation(char *operation) {
    if (strcmp(operation, "REG") == 0) return REG;
    if (strcmp(operation, "UNR") == 0) return EXIT;
    if (strcmp(operation, "LOG") == 0) return LOGIN;
    if (strcmp(operation, "REQ") == 0) return REQ;
    if (strcmp(operation, "AUT") == 0) return VAL;
    if (strcmp(operation, "LST") == 0) return LIST;
    if (strcmp(operation, "RTV") == 0) return RETRIEVE;
    if (strcmp(operation, "UPL") == 0) return UPLOAD;
    if (strcmp(operation, "DEL") == 0) return DELETE;
    if (strcmp(operation, "REM") == 0) return REMOVE;
    if (strcmp(operation, "RAU") == 0) return RAU;
    if (strcmp(operation, "RVC") == 0) return RVC;
    if (strcmp(operation, "RLS") == 0) return RLS;
    if (strcmp(operation, "RRT") == 0) return RRT;
    if (strcmp(operation, "VLC") == 0) return VLC;
    if (strcmp(operation, "VLD") == 0) return VLD;
    if (strcmp(operation, "CNF") == 0) return CNF;

    return ERROR;
}


int verifyUid(char *uid) {
    if ((strlen(uid) == UID_SIZE && atoi(uid) != 0) || strcmp(uid, "00000") == 0) return 0;

    printf("Invalid UID\n");
    return ERROR;
}


int verifyPass(char *pass) {
    char *temp = pass;

    if (strlen(pass) != PASS_SIZE) {
        printf("Invalid password\n");
        return ERROR;
    }

    while (*temp++) {
        if (!(*temp >= 'a' && *temp <= 'z') && !(*temp >= 'A' && *temp <= 'Z') && !(*temp >= '0' && *temp <= '9') && *temp != '\0') {
            printf("Invalid password\n");
            return ERROR;
        }
    }
    return 0;
}


int verifyFop(char *fop, char *fname) {
    if (strcmp(fop, "L") == 0 || strcmp(fop, "X") == 0) return 0;
    else if ((strcmp(fop, "R") == 0 || strcmp(fop, "U") == 0 || strcmp(fop, "D") == 0) && verifyFname(fname) == 0) return 0;

    printf("Invalid operation\n");
    return ERROR;
}


int verifyFname(char *fname) {
    char *temp = fname;
    int extension = 0;

    if (fname == NULL || strlen(fname) > FILE_NAME_SIZE) {
        printf("Invalid filename\n");
        return ERROR;
    }

    while (*temp) {
        if (!(*temp >= 'a' && *temp <= 'z') && !(*temp >= 'A' && *temp <= 'Z') && !(*temp >= '0' && *temp <= '9') 
            && *temp != '\0' && *temp != '.' && *temp != '-' && *temp != '_') {
            printf("Invalid filename\n");
            return ERROR;
        }
        temp++;
    }

    temp--;
    while (*temp) {
        if (*temp == '.' || extension > EXTENSION_SIZE) break; //pode ter mais que um .?
        extension++;
        temp--;
    }

    if (extension != EXTENSION_SIZE) {
        printf("Invalid filename\n");
        return ERROR;
    }

    return 0;
}


int verifyVc(char *vc) {
    if ((strlen(vc) == VC_SIZE && atoi(vc) != 0) || strcmp(vc, "0000") == 0) return 0;

    printf("Invalid VC\n");
    return ERROR;
}


int verifyPort(char *port) {
    int temp = atoi(port);

    if (strlen(port) > PORT_SIZE || temp > 65353 || temp < 0 || (temp == 0 && strcmp(port, "0") != 0)) {
        printf("Invalid port\n");
        return ERROR;
    }

    return 0;
}


int verifyNumber(char *str) {
   while (*str) {
      if(!isdigit(*str)) return ERROR;
      str++;
   }
   return 0;
}


int verifyIp(char *ip) {
    int num, dots = 0;
    char *temp;
    char *ipCopy = malloc(sizeof(ip));
    strcpy(ipCopy, ip);

    if (ipCopy == NULL) return ERROR;

    temp = strtok(ipCopy, ".");
    if (temp == NULL) return ERROR;

    while (temp) {
        if (verifyNumber(temp) != 0) return ERROR;
        num = atoi(temp);
        if (num >= 0 && num <= 255) {
            temp = strtok(NULL, ".");
            if (temp != NULL) dots++;
        } 
        else return ERROR;
    }
    if (dots != 3) return ERROR;

    return 0;
}


int verifyTid(char *tid) {
    if (strlen(tid) == 4 && atoi(tid) != 0) return 0;

    printf("Invalid TID\n");
    return ERROR;
}


int verifyFsize(char *fsize) {
    int size = atoi(fsize);

    if (size > 999999999 || size == 0) {
        printf("Invalid fSize: %d\n", size);
        return 0;
    }

    return size;
}


int fopCode(char *fop) {
    if (strcmp(fop, "L") == 0) return LIST;
    if (strcmp(fop, "R") == 0) return RETRIEVE;
    if (strcmp(fop, "U") == 0) return UPLOAD;
    if (strcmp(fop, "D") == 0) return DELETE;
    if (strcmp(fop, "X") == 0) return REMOVE;
    if (strcmp(fop, "E") == 0) return INV;

    return ERROR;
}
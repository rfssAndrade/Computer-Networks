#include <stdlib.h>
#include <string.h>
#include "verify.h"


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
    if (strcmp(fop, "L") == 0 || strcmp(fop, "X") == 0) return 0; //fname needs to be null?
    else if ((strcmp(fop, "R") == 0 || strcmp(fop, "U") == 0 || strcmp(fop, "D") == 0) && verifyFname(fname) == 0) return 0;

    printf("Invalid operation\n");
    return ERROR;
}


int verifyFname(char *fname) {
    char *temp = fname;
    int extension = 0;

    if (fname == NULL || strlen(fname) > FILE_SIZE || strlen(fname) < 1) {
        printf("Invalid filename\n");
        return ERROR;
    }

    while (*temp) {
        if (!(*temp >= 'a' && *temp <= 'z') && !(*temp >= 'A' && *temp <= 'Z') && !(*temp >= '0' && *temp <= '9') 
            && *temp != '\0' && *temp != '.' && *temp != '-' && *temp != '_') {
            printf("Invalid filename\n");
            return ERROR;
        }
        *temp++;
    }

    *temp--;
    while (*temp) {
        if (*temp == '.' || extension > EXTENSION_SIZE) break; //pode ter mais que um .?
        extension++;
        *temp--;
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
    strcpy(ipCopy, ip); // verify??

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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>


#define MAX_INPUT 20


char *PDIP = NULL;
char *PDport = NULL;
char *ASIP = NULL;
char *ASport = NULL;
int isRegistered = 0;


//TODO: select
void parseArgs(int argc, char **argv);
int parseInput(char *buffer, char *command, char *uid, char *pass);
void makeConnection();
int verifyCommand(char *command);
int verifyUid(char *uid);
int verifyPass(char *pass);
void verifyAnswer(char *answer);


int main(int argc, char **argv) {
    parseArgs(argc, argv);
    makeConnection();

    return(0);
}


void parseArgs(int argc, char **argv) {
    //verificar???
    PDIP = argv[1];
    PDport = argv[2];
    ASIP = argv[3];
    ASport = argv[4];
}


void makeConnection() {
    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[MAX_INPUT];
    char command[5], uid[6], pass[9];
    char answer[128];
    char message[42];
    int code;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);  // correto?

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; 

    errcode = getaddrinfo(ASIP, ASport, &hints, &res);
    if (errcode != 0) exit(1); // correto?
    
    while (1) {
        memset(answer, 0, sizeof(answer));
        memset(message, 0, sizeof(message));
        memset(buffer, 0, sizeof(buffer));

        fgets(buffer, MAX_INPUT, stdin);
        errcode = parseInput(buffer, command, uid, pass);

        if (errcode == -1) continue;
        if (errcode == 1 && !isRegistered) break; // specific case where the user never registers

        if (errcode == 0) sprintf(message, "REG %s %s %s %s\n", uid, pass, PDIP, PDport);
        else sprintf(message, "UNR %s %s\n", uid, pass);

        n = sendto(fd, message, strlen(message), 0,res->ai_addr, res->ai_addrlen);
        if (n == -1) puts("ERROR");//?????

        addrlen = sizeof(addr);
        n = recvfrom(fd, answer, 128, 0, (struct sockaddr *)&addr, &addrlen);
        if (n == -1) puts("ERROR");//??????

        verifyAnswer(answer);
        if (errcode == 1) break;
    }

    freeaddrinfo(res);
    close(fd);
}


int parseInput(char *buffer, char *command, char *uid, char *pass) {
    int code;

    if (!isRegistered) {
        sscanf(buffer, "%s %s %s", command, uid, pass); // verificar?
        code = verifyCommand(command);
        if (code == 0 && (verifyUid(uid) != 0 || verifyPass(pass)) != 0) return -1;
    }
    else {
        sscanf(buffer, "%s", command);
        code = verifyCommand(command);
        if (code == 0) {
            printf("You can only register one user\n");
            code = -1;
        }
    }
    return code;  
}


int verifyCommand(char *command) {
    if (strcmp(command, "exit") == 0) return 1;

    if (strcmp(command, "reg") == 0) return 0;

    printf("Invalid command \n");
    return -1;
}


int verifyUid(char *uid) {
    if ((strlen(uid) == 5 && atoi(uid) != 0) || strcmp(uid, "00000") == 0) return 0;

    printf("Invalid UID\n");
    return -1;
}


int verifyPass(char *pass) {
    char *temp = pass;

    if (strlen(pass) != 8) {
        printf("Invalid password");
        return -1;
    }

    while (*temp++) {
        if (!(*temp >= 'a' && *temp <= 'z') && !(*temp >= 'A' && *temp <= 'Z') && !(*temp >= '0' && *temp <= '9') && *temp != '\0') {
            printf("Invalid password\n");
            return -1;
        }
    }
    return 0;
}


void verifyAnswer(char *answer) {
    if (strcmp(answer, "RRG OK\n") == 0) {
        isRegistered = 1;
        printf("Registration successful: %s", answer);
    }
    else if (strcmp(answer, "RRG NOK\n") == 0) printf("Registration failed: %s", answer);
    else if (strcmp(answer, "RUN OK\n") == 0) printf("Unresgitration successful: %s", answer);
    else if(strcmp(answer, "RUN NOK\n") == 0) printf("Unresgistrtion failed: %s", answer);
    else printf("ERROR\n");
}
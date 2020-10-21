#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


#define MAX_INPUT 20
#define PASS_LENGTH 8
#define ERROR -1
#define EXIT 1

// falta fazer se não houver args na linha de comandos
// falta verificar args linha de comandos
// falta se RG NOK tem que dar para registar outro


char *PDIP = NULL;
char *PDport = NULL;
char *ASIP = NULL;
char *ASport = NULL;
int isRegistered = 0;
char *uid = NULL;
char *pass = NULL;


void parseArgs(int argc, char **argv);
int parseInput(char *buffer, char *command, char *uid, char *pass);
void makeConnection();
int verifyCommand(char *command);
int verifyUid(char *uid);
int verifyPass(char *pass);
int verifyAnswer(char *answer);


int main(int argc, char **argv) {
    parseArgs(argc, argv);
    uid = malloc(6 * sizeof(char));
    pass = malloc(9 * sizeof(char));
    makeConnection();
    free(uid);
    free(pass);

    return 0;
}


void parseArgs(int argc, char **argv) {
    //verificar???
    PDIP = argv[1];
    PDport = argv[2];
    ASIP = argv[3];
    ASport = argv[4];
}


void makeConnection() {
    int fd, errcode = 0, out_fds;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[MAX_INPUT], command[5], second[6], third[9], answer[128], message[42]; //change answer
    fd_set inputs, testfds;
    struct timeval timeout;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);  // correto?

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd, &inputs);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addr.sin_port = htons(atoi(PDport));

    errcode = getaddrinfo(ASIP, ASport, &hints, &res);
    if (errcode != 0) exit(1); // correto?

    if(bind(fd,res->ai_addr,res->ai_addrlen)==-1)/*error*/exit(1);
    
    while (1) {
        testfds = inputs;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);
        switch (out_fds) {
            case 0:
            puts("no fd available");
                break;
            case -1:
                perror("select"); // certo?
                exit(1);
            default:
                memset(answer, 0, sizeof(answer));
                memset(message, 0, sizeof(message));
                memset(buffer, 0, sizeof(buffer));
                memset(command, 0, sizeof(command));
                memset(second, 0, sizeof(second));
                memset(third, 0, sizeof(third));

                if (FD_ISSET(0, &testfds)) {
                    puts("input");
                    fgets(buffer, MAX_INPUT, stdin); // se a pass for maior (password1) ele não vai ler o 1 e vai ter sucesso ao mandar para o server. Correto?
                    errcode = parseInput(buffer, command, second, third);

                    if (errcode == ERROR) break; // verify se é break ou continue
                    if (errcode == 1 && !isRegistered) break; // specific case where the user never registers

                    if (errcode == 0) sprintf(message, "REG %s %s %s %s\n", second, third, PDIP, PDport);
                    else sprintf(message, "UNR %s %s\n", uid, pass);

                    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen); // pode ser blocking?
                    if (n == ERROR) puts("ERROR");//?????
                }
                else if (FD_ISSET(fd, &testfds)) {
                    puts("read socket");
                    addrlen = sizeof(addr);
                    n = recvfrom(fd, answer, 128, 0, (struct sockaddr *)&addr, &addrlen);
                    if (n == ERROR) puts("ERROR");//??????
                    
                    n = verifyAnswer(answer);
                    if (n == 2) {
                        sprintf(message, "RVC OK\n");
                        n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
                        if (n == ERROR) puts("ERROR");
                    }
                    // falta para RVC NOK
                }
                break;
        }
        if (errcode == EXIT) break;
    }

    freeaddrinfo(res);
    close(fd);
}


int parseInput(char *buffer, char *command, char *second, char *third) {
    int code;

    if (!isRegistered) {
        sscanf(buffer, "%s %s %s", command, second, third); // verificar?
        code = verifyCommand(command);
        if (code == 0 && verifyUid(second) == 0 && verifyPass(third) == 0) {
            strcpy(uid, second);
            strcpy(pass, third);
        }
        else return ERROR;
    }
    else {
        sscanf(buffer, "%s", command);
        code = verifyCommand(command);
        if (code == 0) {
            printf("You can only register one user\n");
            code = ERROR;
        }
    }
    return code;  
}


int verifyCommand(char *command) {
    if (strcmp(command, "exit") == 0) return EXIT;

    if (strcmp(command, "reg") == 0) return 0;

    printf("Invalid command\n");
    return ERROR;
}


int verifyUid(char *uid) {
    if ((strlen(uid) == 5 && atoi(uid) != 0) || strcmp(uid, "00000") == 0) return 0;

    printf("Invalid UID\n");
    return ERROR;
}


int verifyPass(char *pass) {
    char *temp = pass;

    if (strlen(pass) != PASS_LENGTH) {
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


int verifyAnswer(char *answer) {
    char temp_uid[6], vc[5], fop, fname[25]; // verificar fname?

    if (strcmp(answer, "RRG OK\n") == 0) {
        isRegistered = 1;
        printf("Registration successful: %s", answer);
        return 1;
    }
    else if (strcmp(answer, "RRG NOK\n") == 0) printf("Registration failed: %s", answer);
    else if (strcmp(answer, "RUN OK\n") == 0) printf("Unresgitration successful: %s", answer);
    else if (strcmp(answer, "RUN NOK\n") == 0) printf("Unresgistrtion failed: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR\n");
    else {
        printf("%s", answer);
        sscanf(answer, "VLC %s %s %c %s", temp_uid, vc, fop, fname); // verificar args?
        printf("VC = %s, %c: %s\n", vc, fop, fname);
        return 2;
    }

    return 0;
}
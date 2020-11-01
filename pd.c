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
#include "verify.h"


#define MAX_INPUT 20


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
    int fd_client, fd_server, errcode = 0, out_fds;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints_as, hints_pd, *res_as, *res_pd;
    struct sockaddr_in addr;
    char buffer[MAX_INPUT], command[5], second[6], third[9], answer[128], message[42]; //change answer
    fd_set inputs, testfds;

    fd_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_client == -1) exit(1);  // correto?

    fd_server = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_server == -1) exit(1);

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd_client, &inputs);
    FD_SET(fd_server, &inputs);

    memset(&hints_as, 0, sizeof hints_as);
    hints_as.ai_family = AF_INET;
    hints_as.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(ASIP, ASport, &hints_as, &res_as);
    if (errcode != 0) exit(1); // correto?

    memset(&hints_pd, 0, sizeof hints_pd);
    hints_pd.ai_family = AF_INET;
    hints_pd.ai_socktype = SOCK_DGRAM;
    hints_pd.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(PDIP, PDport, &hints_pd, &res_pd);
    if (errcode != 0) exit(1);

    while (bind(fd_server,res_pd->ai_addr,res_pd->ai_addrlen) ==  -1) puts("Can't bind");
        
    while (1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);
        switch (out_fds) {
            case 0:
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
                    fgets(buffer, MAX_INPUT, stdin); // se a pass for maior (password1) ele não vai ler o 1 e vai ter sucesso ao mandar para o server. Correto?
                    errcode = parseInput(buffer, command, second, third);

                    if (errcode == ERROR || (errcode == EXIT && !isRegistered)) break;

                    if (errcode == REG) sprintf(message, "REG %s %s %s %s\n", second, third, PDIP, PDport);
                    else sprintf(message, "UNR %s %s\n", uid, pass);

                    n = sendto(fd_client, message, strlen(message), 0, res_as->ai_addr, res_as->ai_addrlen); // pode ser blocking?
                    if (n == ERROR) puts("ERROR");//?????
                }
                else if (FD_ISSET(fd_client, &testfds)) {
                    addrlen = sizeof(addr);
                    n = recvfrom(fd_client, answer, 128, 0, (struct sockaddr *)&addr, &addrlen);
                    if (n == ERROR) puts("ERROR");//??????
                    
                    verifyAnswer(answer);
                }

                else if (FD_ISSET(fd_server, &testfds)) {
                    addrlen = sizeof(addr);
                    n = recvfrom(fd_server, answer, 128, 0, (struct sockaddr *)&addr, &addrlen);
                    if (n == ERROR) puts("ERROR");//??????

                    n = verifyAnswer(answer);
                    sprintf(message, "RVC OK\n");
                    n = sendto(fd_server, message, strlen(message), 0, (struct sockaddr *)&addr, addrlen); //mudar
                    if (n == ERROR) puts("ERROR");
                    // falta para RVC NOK
                }
                break;
        }
        if (errcode == EXIT) break;
    }

    freeaddrinfo(res_as);
    freeaddrinfo(res_pd);
    close(fd_client);
    close(fd_server);
}


int parseInput(char *buffer, char *command, char *second, char *third) {
    int code;

    sscanf(buffer, "%s %s %s", command, second, third); // verificar?

    code = verifyCommand(command);
    switch (code) {
        case REG:
            if (isRegistered) {
                printf("You can only register one user\n");
                code = ERROR;
                break;
            }
            else if (verifyUid(second) == 0 && verifyPass(third) == 0) {
                strcpy(uid, second);
                strcpy(pass, third);
            }
            else code = ERROR;
            break;

        case EXIT:
            break;

        default:
            printf("Invalid command\n");
            code = ERROR;
            break;
    }

    return code;
}


int verifyAnswer(char *answer) {
    char temp_uid[6], vc[5], fop[2], fname[25]; // verificar fname?

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
        sscanf(answer, "VLC %s %s %s %s", temp_uid, vc, fop, fname); // verificar args?
        printf("VC = %s, %s: %s\n", vc, fop, fname);
        return 2;
    }

    return 0;
}
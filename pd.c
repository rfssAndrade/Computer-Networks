#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "verify.h"
#include "pd.h"


char *PDIP = NULL;
char *PDport = NULL;
char *ASIP = NULL;
char *ASport = NULL;
int isRegistered = 0;
char *uid = NULL;
char *pass = NULL;


int main(int argc, char **argv) {
    PDport = malloc(6 * sizeof(char));
    ASIP = malloc(16 * sizeof(char));
    ASport = malloc(6 * sizeof(char));

    parseArgs(argc, argv);

    uid = malloc(6 * sizeof(char));
    pass = malloc(9 * sizeof(char));

    makeConnection();

    free(ASIP);
    free(ASport);
    free(PDport);
    free(uid);
    free(pass);

    return 0;
}


// Parse arguments from program execution
void parseArgs(int argc, char **argv) {
    int i = 2, code;
    PDIP = argv[1];
    if (verifyIp(PDIP) != 0) {
        printf("Invalid PDIP address: %s\n", PDIP);
        exit(1);
    }

    while(i < argc) {
        code = verifyArg(argv[i], argv[i+1]);
        switch (code) {
            case D:
                strcpy(PDport, argv[i+1]);
                break;
            case N:
                strcpy(ASIP, argv[i+1]);
                break;
            case P:
                strcpy(ASport, argv[i+1]);
                break;
            default:
                printf("Bad argument\n");
                exit(1);
                break;
        }
        i += 2;
    }

    if (strlen(PDport) == 0) strcpy(PDport, "57034");
    if (strlen(ASIP) == 0) strcpy(ASIP, "127.0.0.1");
    if (strlen(ASport) == 0) strcpy(ASport, "58034");
}


// Handles all connections
void makeConnection() {
    int fd_client, fd_server, code = 0, out_fds;
    ssize_t n;
    struct addrinfo hints_as, hints_pd, *res_as, *res_pd;
    struct sockaddr_in addr_client, addr_server;
    char buffer[128], command[16], second[16], third[16], answer[128], message[128];
    fd_set inputs, testfds;
    struct sigaction action;

    // Handle signal
    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == ERROR) {
        printf("Failed to handle signal\n");
        exit(1);
    }

    fd_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_client == ERROR) {
        printf("Failed to open socket\n");
        exit(1);
    }

    fd_server = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_server == ERROR) {
        printf("Failed to open socket\n");
        exit(1);
    }

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd_client, &inputs);
    FD_SET(fd_server, &inputs);

    memset(&hints_as, 0, sizeof hints_as);
    hints_as.ai_family = AF_INET;
    hints_as.ai_socktype = SOCK_DGRAM;

    n = getaddrinfo(ASIP, ASport, &hints_as, &res_as);
    if (n != 0) {
        printf("Failed to get AS address\n");
        exit(1);
    }

    memset(&hints_pd, 0, sizeof hints_pd);
    hints_pd.ai_family = AF_INET;
    hints_pd.ai_socktype = SOCK_DGRAM;
    hints_pd.ai_flags = AI_PASSIVE;

    n = getaddrinfo(PDIP, PDport, &hints_pd, &res_pd);
    if (n != 0) {
        printf("Faild to get address\n");
        exit(1);
    }

    while (bind(fd_server,res_pd->ai_addr,res_pd->ai_addrlen) ==  ERROR) printf("Can't bind");
        
    while (1) {
        testfds = inputs;

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);
        switch (out_fds) {
            case 0:
                break;
            case ERROR:
                perror("select");
                exit(1);
            default:
                memset(answer, 0, 128);
                memset(message, 0, 128);
                memset(buffer, 0, 128);
                memset(command, 0, 16);
                memset(second, 0, 16);
                memset(third, 0, 16);

                if (FD_ISSET(0, &testfds)) { // stdin
                    fgets(buffer, 128, stdin);
                    code = parseInput(buffer, command, second, third);

                    if (code == ERROR || (code == EXIT && !isRegistered)) break;

                    formatMessage(message, code, second, third);
                    sendMessageClient(fd_client, message, res_as);
                }
                else if (FD_ISSET(fd_client, &testfds)) { // PD client of AS
                    readMessage(fd_client, answer, &addr_client);
                    verifyAnswer(answer);
                }
                else if (FD_ISSET(fd_server, &testfds)) { // AS client of PD
                    readMessage(fd_server, answer, &addr_server);
                    parseMessage(answer, message, command, second);
                    sendMessageServer(fd_server, message, addr_server);
                }
                break;
        }
        if (code == EXIT) break;
    }

    freeaddrinfo(res_as);
    freeaddrinfo(res_pd);
    close(fd_client);
    close(fd_server);
}


// Parse input and verify received parameters
int parseInput(char *buffer, char *command, char *second, char *third) {
    int code;

    sscanf(buffer, "%s %s %s", command, second, third);

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


// Format message to be sent
void formatMessage(char *message, int code, char *second, char *third) {
    switch (code) {
        case REG:
            sprintf(message, "REG %s %s %s %s\n", second, third, PDIP, PDport);
            break;
        case EXIT:
            sprintf(message, "UNR %s %s\n", uid, pass);
            break;
    }
}


// Send message PD client of AS
void sendMessageClient(int fd, char *message, struct addrinfo *res) {
    int code;

    code = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (code == ERROR) printf("ERROR");
}


// Read message
void readMessage(int fd, char *answer, struct sockaddr_in *addr) {
    int code;
    socklen_t addrlen = sizeof(addr);

    code = recvfrom(fd, answer, 128, 0, (struct sockaddr *)addr, &addrlen);
    if (code == ERROR) printf("ERROR");
}


// Send message AS client of PD
void sendMessageServer(int fd, char *message, struct sockaddr_in addr) {
    int code;
    socklen_t addrlen = sizeof(addr);

    code = sendto(fd, message, strlen(message), 0, (struct sockaddr *)&addr, addrlen); //mudar
    if (code == ERROR) printf("ERROR");
}


// Process message received from AS
int verifyAnswer(char *answer) {
    if (strcmp(answer, "RRG OK\n") == 0) {
        isRegistered = 1;
        printf("Registration successful: %s", answer);
        return 1;
    }
    else if (strcmp(answer, "RRG NOK\n") == 0) {
        memset(uid, 0, 6);
        memset(pass, 0, 9);
        printf("Registration failed: %s", answer);
    }
    else if (strcmp(answer, "RUN OK\n") == 0) printf("Unresgitration successful: %s", answer);
    else if (strcmp(answer, "RUN NOK\n") == 0) printf("Unresgistrtion failed: %s", answer);
    else printf("ERROR: %s", answer);

    return 0;
}


// Parse message received from AS
void parseMessage(char *answer, char *message, char *command, char *second) {
    sscanf(answer, "%s %s", command, second);
        if (verifyOperation(command) == VLC && strcmp(uid, second) == 0) {
            sprintf(message, "RVC %s OK\n", uid);
            printf("%s", answer);
        }
        else {
            sprintf(message, "RVC %s NOK\n", uid);
            printf("Bad message: %s\n", answer);
        }
}
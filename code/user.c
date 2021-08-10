#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include "verify.h"
#include "message.h"
#include "user.h"


char *ASIP = NULL;
char *ASport = NULL;
char *FSIP = NULL;
char *FSport = NULL;
int isLogged = 0;
char *uid = NULL;
char *pass = NULL;
char *fname = NULL;
int rid = -1;
int tid = -1;


int main(int argc, char **argv) {
    FSIP = malloc(16 * sizeof(char));
    FSport = malloc(6 * sizeof(char));
    ASIP = malloc(16 * sizeof(char));
    ASport = malloc(6 * sizeof(char));

    parseArgs(argc, argv);

    uid = malloc(5 * sizeof(char));
    pass = malloc(9 * sizeof(char));
    fname = malloc(25 * sizeof(char));

    makeConnection();

    free(ASIP);
    free(ASport);
    free(FSport);
    free(FSIP);
    free(fname);
    free(uid);
    free(pass);

    return 0;
}


// Parse arguments from program execution
void parseArgs(int argc, char **argv) {
    int i = 1, code;

    while(i < argc) {
        code = verifyArg(argv[i], argv[i+1]);
        switch (code) {
            case M:
                strcpy(FSIP, argv[i+1]);
                break;
            case Q:
                strcpy(FSport, argv[i+1]);
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

    if (strlen(FSIP) == 0) strcpy(FSIP, "127.0.0.1");
    if (strlen(FSport) == 0) strcpy(FSport, "59034");
    if (strlen(ASIP) == 0) strcpy(ASIP, "127.0.0.1");
    if (strlen(ASport) == 0) strcpy(ASport, "58034");
}


// Handles all connections
void makeConnection() {
    int fd_as, fd_fs = -1, code, out_fds;
    ssize_t n;
    struct addrinfo hints, *res_as, *res_fs;
    char buffer[128];
    char command[16], second[32], third[32], answer[128], message[128];
    fd_set inputs, testfds;
    struct timeval tv_as_r, tv_fs_r;
    struct sigaction action;

    // Handle signal
    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == ERROR) {
        printf("Failed to handle signal\n");
        exit(1);
    }

    tv_as_r.tv_sec = 0;
    tv_as_r.tv_usec = 100;

    tv_fs_r.tv_sec = 1;
    tv_fs_r.tv_usec = 0;

    fd_as = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_as == ERROR) {
        printf("Failed to open socket\n");
        exit(1);
    }

    // Set socket timeout
    n = setsockopt(fd_as, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_as_r, sizeof tv_as_r);
    if (n == ERROR) {
        printf("Failed to set socket timeout\n");
        exit(1);
    }

    FD_ZERO(&inputs);
    FD_SET(0, &inputs);
    FD_SET(fd_as, &inputs);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    n = getaddrinfo(ASIP, ASport, &hints, &res_as);
    if (n != 0) {
        printf("Failed to get AS address\n");
        exit(1);
    }

    n = getaddrinfo(FSIP, FSport, &hints, &res_fs);
    if (n != 0) {
        printf("Failed to get FS address\n");
        exit(1);
    }

    n = connect(fd_as, res_as->ai_addr, res_as->ai_addrlen);
    if (n == ERROR) {
        printf("Failed to connect to server\n");
        exit(1);
    }

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
            memset(second, 0, 32);
            memset(third, 0, 32);

            if (FD_ISSET(0, &testfds)) { // stdin
                fgets(buffer, 128, stdin);

                code = parseInput(buffer, command, second, third);

                if (code == ERROR || code == EXIT) break;

                if (code > 4) {

                    fd_fs = socket(AF_INET, SOCK_STREAM, 0);
                    if (fd_fs == ERROR) {
                        printf("Failed to open socket\n");
                        break;
                    }

                    n = setsockopt(fd_fs, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_fs_r, sizeof tv_fs_r);
                    if (n == ERROR) {
                        printf("Failed to set socket timeout\n");
                        close(fd_fs);
                        break;
                    }

                    n = connect(fd_fs, res_fs->ai_addr, res_fs->ai_addrlen);
                    if (n == ERROR) {
                        printf("Failed to connect to server\n");
                        close(fd_fs);
                        break;
                    }

                    FD_SET(fd_fs, &inputs);
                }
                if (code == UPLOAD) uploadFile(fd_fs, message);
                else {
                    formatMessage(message, code, second, third);
                    sendMessage(code, fd_as, fd_fs, message); 
                }
            }
            else if (FD_ISSET(fd_as, &testfds)) { // AS
                code = readMessageAS(fd_as, answer);
                if (code == SOCKET_ERROR) break;
                if (verifyAnswerAS(answer) != 0) parseAnswerAS(answer, command, second);
            }
            else if (fd_fs != -1 && FD_ISSET(fd_fs, &testfds)) { // FS
                code = readMessageFS(fd_fs);
                FD_CLR(fd_fs, &inputs);
                close(fd_fs);
                fd_fs = -1;
                memset(fname, 0, 25);
            }
            break;
        }
        if (code == EXIT || code == SOCKET_ERROR) break;
    }

    freeaddrinfo(res_as);
    freeaddrinfo(res_fs);
    close(fd_as);
}


// Parse input and verify received parameters

int parseInput(char *buffer, char *command, char *second, char *third) {
    int code;

    sscanf(buffer, "%s %s %s", command, second, third);

    code = verifyCommand(command);
    if (code != LOGIN && !isLogged && code != EXIT) {
        printf("You aren't logged in\n");
        return ERROR;
    }
    switch (code) {
        case LOGIN:
            if (isLogged) {
                printf("You are already logged in\n");
                code = ERROR;
            }
            else if (verifyUid(second) == 0 && verifyPass(third) == 0) {
                strcpy(uid, second);
                strcpy(pass, third);
            }
            else code = ERROR;
            break;

        case REQ:
            if (verifyFop(second, third) != 0) code = ERROR;
            break;

        case VAL:
            if (verifyVc(second) != 0) code = ERROR;
            break;

        case RETRIEVE:
        case UPLOAD:
        case DELETE:
            if (verifyFname(second) != 0) code = ERROR;
            else strcpy(fname, second);
            break;

        case LIST:
        case REMOVE:
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
        case LOGIN:
            sprintf(message, "LOG %s %s\n", second, third);
            break;

        case REQ:
            rid = rand() % 10000;
            if (strlen(third) == 0) sprintf(message, "REQ %s %04d %s\n", uid, rid, second);
            else sprintf(message, "REQ %s %04d %s %s\n", uid, rid, second, third);
            break;

        case VAL:
            sprintf(message, "AUT %s %04d %s\n", uid, rid, second);
            break;

        case LIST:
            sprintf(message, "LST %s %04d\n", uid, tid);
            break;

        case RETRIEVE:
            sprintf(message, "RTV %s %04d %s\n", uid, tid, fname);
            break;

        case DELETE:
            sprintf(message, "DEL %s %04d %s\n", uid, tid, fname);
            break;

        case REMOVE:
            sprintf(message, "REM %s %04d\n", uid, tid);
            break;
    }
}


// Send message
void sendMessage(int code, int fd_as, int fd_fs, char *message) {
    int fd = code < 5 ? fd_as : fd_fs;
    int nleft = strlen(message);
    int nwritten = 0;
    char *ptr = message;

    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        if (nwritten <= 0) {
            printf("ERROR ON SEND");
            break;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
}


// Read message from FS
int readMessageFS(int fd) {
    char operation[9];
    char *ptr = operation;
    int nread, code;

    // Read operation
    nread  = readTcp(fd, 3, ptr);
    if (nread <= 0) return nread;
    ptr += nread;
    *ptr = '\0';

    code = verifyOperation(operation);
    if (code == RLS || code == RRT) parseAnswerFS(operation, code, fd);
    else {
        nread  = readTcp(fd, 5, ptr); // read remaining message
        if (nread == ERROR) return nread;
        ptr += nread;
        *ptr = '\0';
        code = verifyAnswerFS(operation);
    }

    return code;
}


// Read message from AS
int readMessageAS(int fd, char *answer) {
    char *ptr = answer;
    int nread;

    nread = readTcp(fd, 127, ptr);
    if (nread <= 0) return nread;
    ptr += nread;
    *ptr = '\0';

    return 0;
}


// Process message received from AS
int verifyAnswerAS(char *answer) {
    if (strcmp(answer, "RLO OK\n") == 0) {
        isLogged = 1;
        printf("Login successful: %s", answer);
    }
    else if (strcmp(answer, "RLO NOK\n") == 0) printf("Wrong password: %s", answer);
    else if (strcmp(answer, "RLO ERR\n") == 0) printf("User ID doesn't exist: %s", answer);
    else if (strcmp(answer, "RRQ OK\n") == 0) printf("Request accepted: %s", answer);
    else if (strcmp(answer, "RRQ ELOG\n") == 0) printf("User not logged in: %s", answer);
    else if (strcmp(answer, "RRQ EPD\n") == 0) printf("Could not reach PD: %s", answer);
    else if (strcmp(answer, "RRQ EUSER\n") == 0) printf("Wrong UID: %s", answer);
    else if (strcmp(answer, "RRQ EFOP\n") == 0) printf("Invalid file operation: %s", answer);
    else if (strcmp(answer, "RRQ ERR\n") == 0) printf("Invalid request format: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR: %s\n", answer);
    else if (strcmp(answer, "RAU 0\n") == 0) printf("Two-factor authentication failed: %s", answer);
    else return 1;

    return 0;
}


// Parse message received from AS
int parseAnswerAS(char *answer, char *command, char *second) {
    sscanf(answer, "%s %s", command, second);

    if (verifyOperation(command) == RAU && verifyTid(second) == 0) {
        tid = atoi(second);
        printf("Two-factor authentication successful: %s", answer);
        return 0;
    }

    printf("Invalid message: %s", answer);
    return ERROR;
}


// Process message received from FS
int verifyAnswerFS(char *answer) {
    if (strcmp(answer, "RUP OK\n") == 0) printf("Upload request approved: %s", answer);
    else if (strcmp(answer, "RUP NOK\n") == 0) printf("UID doesn't exist: %s", answer);
    else if (strcmp(answer, "RUP DUP\n") == 0) printf("File already exists: %s", answer);
    else if (strcmp(answer, "RUP FULL\n") == 0) printf("Files limit reached: %s", answer);
    else if (strcmp(answer, "RUP INV\n") == 0) printf("Invalid TID: %s", answer);
    else if (strcmp(answer, "RUP ERROR\n") == 0) printf("Invalid upload request format: %s", answer);
    else if (strcmp(answer, "RDL OK\n") == 0) printf("Delete request approved: %s", answer);
    else if (strcmp(answer, "RDL EOF\n") == 0) printf("File not available: %s", answer);
    else if (strcmp(answer, "RDL NOK\n") == 0) printf("UID doesn't exist: %s", answer);
    else if (strcmp(answer, "RDL INV\n") == 0) printf("Invalid TID: %s", answer);
    else if (strcmp(answer, "RDL ERR\n") == 0) printf("Invalid delete request format: %s", answer);
    else if (strcmp(answer, "RRT EOF\n") == 0) printf("File not available: %s", answer);
    else if (strcmp(answer, "RRT INV\n") == 0) printf("Wrong TID: %s", answer);
    else if (strcmp(answer, "RRT ERR\n") == 0) printf("Invalid request format: %s", answer);
    else if (strcmp(answer, "RLS EOF\n") == 0) printf("No files available: %s", answer);
    else if (strcmp(answer, "RLS INV\n") == 0) printf("Wrong TID: %s", answer);
    else if (strcmp(answer, "RLS ERR\n") == 0) printf("Invalid request format: %s", answer);
    else if (strcmp(answer, "ERR\n") == 0) printf("ERROR: %s\n", answer);
    else if (strcmp(answer, "RRM OK\n") == 0) {
        printf("Remove request approved: %s", answer);
        return EXIT;
    }
    else if (strcmp(answer, "RRM NOK\n") == 0) {
        printf("UID doesn't exist: %s", answer);
        return EXIT;
    }
    else if (strcmp(answer, "RRM INV\n") == 0) {
        printf("Invalid TID: %s", answer);
        return EXIT;
    }
    else if (strcmp(answer, "RRM ERR\n") == 0) {
        printf("Invalid remove request format: %s", answer);
        return EXIT;
    }
    else if (strcmp(answer, "RRT NOK\n") == 0) printf("No content available for UID: %s", answer);
    else printf("ERROR: %s\n", answer);

    return 0;
}


// Parse message received from FS
int parseAnswerFS(char *operation, int code, int fd) {
    char status[4], buffer[128];
    char *ptr = status;
    int nread = 0, nFiles, spacesRead = 0, i = 1, fSize = 0;
    FILE *fptr;

    nread = readTcp(fd, 3, ptr); // read status
    if (nread <= 0) return nread;
    ptr += nread;
    *ptr = '\0';

    switch (code) {
        case RLS:
            nFiles = atoi(status);
            if (nFiles > 0) {
                if (nFiles > 9) {
                    nread = readTcp(fd, 1, ptr); // read space
                    if (nread <= 0) return nread;
                }
                ptr = buffer;
                while (i <= nFiles) {
                    nread = readTcp(fd, 1, ptr); // read fname and fsize until space or \n is reached
                    if (nread <= 0) return nread;
                    if (*ptr == ' ' || *ptr == '\n') spacesRead++;
                    ptr++;

                    if (spacesRead == 2) {
                        *(ptr-1) = '\0';
                        printf("%d - %s\n", i, buffer);
                        i++;
                        spacesRead = 0;
                        memset(buffer, 0, 128);
                        ptr = buffer;
                    }
                }
            }
            else {
                nread  = readTcp(fd, 2, ptr); // read remaining message
                if (nread <= 0) return nread;
                ptr += nread;
                *ptr = '\0';

                if (sprintf(buffer, "%s%s", operation, status) != 8) code = ERROR;
                else verifyAnswerFS(buffer);
            }
            break;

        case RRT:
            if (strcmp(status, " OK") == 0) {
                nread  = readTcp(fd, 1, ptr); // read space
                if (nread <= 0) return nread;
                
                ptr = buffer;
                memset(buffer, 0, 128);
                while (1) {
                    nread  = readTcp(fd, 1, ptr); // read fsize
                    if (nread <= 0) return nread;
                    ptr += nread;

                    if (*(ptr-1) == ' ') break;
                }
                *(ptr-1) = '\0';
                fSize = atoi(buffer);
                if (fSize == 0 || fSize > 999999999) printf("Invalid fSize: %d\n", fSize);
                else {
                    ptr = buffer;
                    fptr = fopen(fname, "w");
                    if (fptr == NULL) {
                        printf("Error creating file %s\n", fname);
                        return ERROR;
                    }
                    while (fSize > 0) {
                        if (fSize < 127) nread  = readTcp(fd, fSize, ptr); // read file content
                        else nread  = readTcp(fd, 127, ptr);
                        if (nread < 0) {
                            fclose(fptr);
                            return nread;
                        }
                        fSize -= nread;
                        if (fSize == -1) {
                            ptr += nread - 1;
                            *ptr = EOF;
                        }
                        fwrite(buffer, sizeof(char), nread, fptr);
                        ptr = buffer;
                    }
                    printf("File in ./%s\n", fname);
                    fclose(fptr);
                }
            }
            else {
                nread  = readTcp(fd, 2, ptr); // read remaining message
                if (nread <= 0) return nread;
                ptr += nread;
                *ptr = '\0';

                if (sprintf(buffer, "%s%s", operation, status) != 8) code = ERROR;
                else verifyAnswerFS(buffer);
            }
            break;
    
        default:
            code = ERROR;
            break;
    }

    return code;
}


// Upload file to FS
int uploadFile(int fd, char *message) {
    FILE *fptr;
    off_t fsize;
    int len, nwritten, nread;
    char buffer[1024], *ptr = message;

    fptr = fopen(fname, "r");
    if (fptr == NULL) {
        printf("Error opening file %s\n", fname);
        return ERROR;
    }

    fsize = fileSize(fname);
    if (fsize == ERROR) {
        printf("Cannot determine size of %s\n", fname);
        fclose(fptr);
        return ERROR;
    }

    len = sprintf(message, "UPL %s %04d %s %ld ", uid, tid, fname, fsize);
    nwritten = writeTcp(fd, len, ptr);
    if (nwritten < 0) {
        fclose(fptr);
        return nwritten;
    }
    ptr = buffer;
    while (fsize > 0) {
        nread = fread(buffer, sizeof(char), 1023, fptr);
        if (nread <= 0) {
            fclose(fptr);
            printf("Error on read %s\n", fname);
            return ERROR;
        }

        nwritten = writeTcp(fd, nread, ptr);
        if (nwritten < 0) {
            fclose(fptr);
            return nwritten;
        }
        fsize -= nwritten;
        ptr = buffer;
    }
    if (fsize <= 0) write(fd, "\n", 1);

    printf("%s sent with success\n", fname);
    fclose(fptr);
    return 0;
}


// Determine file size
off_t fileSize(char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    return ERROR;
}
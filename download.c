#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#define PORT "3456"

// send a command (append newline)
void send_cmd(FILE *fp, const char *cmd) {
    fprintf(fp, "%s\n", cmd);
    fflush(fp);
}

// read a line or exit
void read_line(FILE *fp, char *buf, size_t sz) {
    if (!fgets(buf, sz, fp)) {
        perror("recv");
        exit(1);
    }
}

// opens a TCP connection to host:PORT, or exits on failure
int connect_to_server(const char *host) {
    struct addrinfo hints = {0}, *res, *rp;
    int sockfd;

    hints.ai_family   = AF_UNSPEC;    // IPv4 or IPv6, ensures it works for both
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if (getaddrinfo(host, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    for (rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sockfd);
    }
    freeaddrinfo(res);
    if (!rp) {
        fprintf(stderr, "Error: cannot connect to %s:%s\n", host, PORT);
        exit(1);
    }
    return sockfd;
}

int main(void) {
    int choice;
    printf("Which server?\n1) Newark\n2) London\n> ");
    if (scanf("%d", &choice)!=1) {
        fprintf(stderr, "scan failed\n");
        return 1;
    }

    if (choice == 1) {
        printf("You chose Newark\n");
    } else if (choice == 2) {
        printf("You chose London\n");
    }

    const char *hosts[2] = {
        "newark.cs.sierracollege.edu",
        "london.cs.sierracollege.edu"
    };

    int sock = connect_to_server(hosts[choice-1]);
    printf("Connected to %s\n", hosts[choice-1]);
    FILE *sockfp = fdopen(sock, "r+");
    if (!sockfp) {
        perror("fdopen");
        return 1;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), sockfp)) {
        perror("fgets");
        return 1;
    }
    printf("Server says: %s", line);

    while (1) {
        printf("\nMenu:\n");
        printf(" 1) List files\n");
        printf(" 2) Download file\n");
        printf(" 3) Quit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) break;

        if (choice == 1) {
            char buf[1024];
            send_cmd(sockfp, "LIST");
            read_line(sockfp, buf, sizeof(buf));  // should be "+OK"
            if (strncmp(buf, "+OK", 3) != 0) {
                fprintf(stderr, "LIST failed: %s", buf);
            } else {
                // read until "."
                while (1) {
                    read_line(sockfp, buf, sizeof(buf));
                    if (strcmp(buf, ".\n") == 0 || strcmp(buf, ".") == 0)
                        break;
                    printf("  %s", buf);
                }
            }
        }
        else if (choice == 2) {
            // placeholder for GET
            printf(">> GET goes here\n");
        }
        else if (choice == 3) {
            // send QUIT and break
            fprintf(sockfp, "QUIT\n");
            fflush(sockfp);
            break;
        }
        else {
            printf("Invalid choice.\n");
        }
    }


    fclose(sockfp);
    close(sock);
}
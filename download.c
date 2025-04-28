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

// opens a TCP connection to host:PORT, connects TCP, or exit on failure
int connect_to_server(const char *host) {
    struct addrinfo hints = {0}, *res, *rp;
    int sockfd;

    hints.ai_family   = AF_UNSPEC;    // allows both IPv4 or IPv6
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
    
    const char *hosts[2] = {
        "newark.cs.sierracollege.edu",
        "london.cs.sierracollege.edu"
    };

    // asking the user to pick a server
    printf("Which server?\n1) Newark\n2) London\n> ");

    // ensuring user chose a valid choice
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 2) {
        fprintf(stderr, "Invalid Choice\n");
        return 1;
    }

    // connects & wrap socket in FILE*
    int sock = connect_to_server(hosts[choice-1]);
    printf("Connected to %s\n", hosts[choice-1]);
    FILE *sockfp = fdopen(sock, "r+");
    if (!sockfp) {
        perror("fdopen");
        return 1;
    }

    // reading and displaying initial greeting
    char line[1024];
    read_line(sockfp, line, sizeof(line));
    printf("Server says: %s", line);

    // implemented user-facing menu
    while (1) {
        printf("\nMenu:\n");
        printf(" 1) List files\n");
        printf(" 2) Download file\n");
        printf(" 3) Quit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) break;

        // LIST
        if (choice == 1) {
            char buf[1024];
            send_cmd(sockfp, "LIST");
            read_line(sockfp, buf, sizeof(buf));  // should be "+OK"
            if (strncmp(buf, "+OK", 3) != 0) {
                fprintf(stderr, "LIST failed: %s", buf);
            } else {
                // read until "." terminator
                while (1) {
                    read_line(sockfp, buf, sizeof(buf));
                    if (strcmp(buf, ".\n") == 0 || strcmp(buf, ".") == 0)
                        break;
                    printf("  %s", buf);
                }
            }
        }

        // GET
        else if (choice == 2) {
            char filename[256], buf[1024];
            unsigned char chunk[1024];
            long remaining;

            // prompt user for filename
            printf("Enter filename: ");
            if (scanf("%255s", filename) != 1) {
                fprintf(stderr, "Failed to read filename\n");
                continue;
            }

            // asking user if they want to view/save txt file
            int view_only = 0;
            if (strstr(filename, ".txt")) {
                char resp;
                printf("%s looks like a text file. View (V) or Save (S)? ", filename);
                scanf(" %c", &resp);
                view_only = (resp=='V' || resp=='v');
            }

            // asks user if they want to overwrite, if saving a text file
            if (!view_only && access(filename, F_OK) == 0) {
                char resp;
                printf("%s already exists. Overwrite? (y/n): ", filename);
                scanf(" %c", &resp);
                if (resp!='y' && resp!='Y') {
                    printf("Skipping %s\n", filename);
                    continue;   // back to menu
                }
            }

            // SIZE command
            snprintf(buf, sizeof(buf), "SIZE %s", filename);
            send_cmd(sockfp, buf);
            read_line(sockfp, buf, sizeof(buf));
            if (strncmp(buf, "+OK", 3) != 0) {
                fprintf(stderr, "SIZE failed: %s", buf);
                continue;
            }
            remaining = atol(buf + 3);

            // GET command
            snprintf(buf, sizeof(buf), "GET %s", filename);
            send_cmd(sockfp, buf);
            read_line(sockfp, buf, sizeof(buf));
            if (strncmp(buf, "+OK", 3) != 0) {
                fprintf(stderr, "GET failed: %s", buf);
                continue;
            }

            // open output if saving
            FILE *out = NULL;
            if (!view_only) {
                out = fopen(filename, "wb");
                if (!out) {
                    perror("fopen");
                    continue;
                }
            }

            // download loop
            while (remaining > 0) {
                size_t want = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
                size_t got  = fread(chunk, 1, want, sockfp);
                if (got == 0) {
                    fprintf(stderr, "Unexpected EOF\n");
                    break;
                }
                if (view_only)
                    fwrite(chunk, 1, got, stdout);
                else
                    fwrite(chunk, 1, got, out);
                remaining -= got;
            }

            // dleanup & feedback
            if (view_only) {
                printf("\n[End of %s]\n", filename);
            } else {
                fclose(out);
                if (remaining == 0)
                    printf("Downloaded %s successfully.\n", filename);
                else
                    printf("Download incomplete: %ld bytes left.\n", remaining);
            }
        }

        // QUIT
        else if (choice == 3) {
            // send QUIT and stop
            fprintf(sockfp, "QUIT\n");
            fflush(sockfp);
            printf("\nSwap hopes to see you again soon!\n");
            break;
        }
        else {
            printf("Invalid choice.\n");
        }
    }

    // released resources
    fclose(sockfp);
}
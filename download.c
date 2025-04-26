#include <stdio.h>
int main(void) {
    int choice;
    printf("Which server?\n1) Newark\n2) London\n> ");
    if (scanf("%d", &choice)!=1) {
        fprintf(stderr, "scan failed\n");
        return 1;
    }
    printf("You chose option %d\n", choice);
    return 0;
}
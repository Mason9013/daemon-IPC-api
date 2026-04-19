#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    printf("demo_worker started\n");
    printf("argc=%d\n", argc);

    for (int i = 0; i < argc; i++) {
        printf("argv[%d]=%s\n", i, argv[i]);
    }

    if (argc >= 3 && strcmp(argv[1], "echo") == 0) {
        printf("result=%s\n", argv[2]);
    }
    else if (argc >= 3 && strcmp(argv[1], "reverse") == 0) {
        char *s = argv[2];
        int n = (int)strlen(s);
        printf("result=");
        for (int i = n - 1; i >= 0; i--) {
            putchar(s[i]);
        }
        putchar('\n');
    }
    else if (argc >= 3 && strcmp(argv[1], "count") == 0) {
        printf("result=%zu\n", strlen(argv[2]));
    }
    else {
        printf("result=unknown command\n");
    }

    fflush(stdout);
    return 0;
}
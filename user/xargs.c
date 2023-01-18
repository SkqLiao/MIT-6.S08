#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(2, "Usage: xargs command (arg ...)\n");
        exit(1);
    }
    char *command = malloc(strlen(argv[1]) + 1);
    memmove(command, argv[1], strlen(argv[1]));
    char *new_argv[MAXARG];
    new_argv[0] = (char *)malloc(1024 + 1);
    for (int i = 2; i < argc; ++i) {
        new_argv[i - 1] = (char *)malloc(strlen(argv[i]) + 1);
        memmove(new_argv[i - 1], argv[i], strlen(argv[i]));
    }

    new_argv[argc - 1] = (char *)malloc(1024 + 1);

    while (1) {
        char *p = new_argv[argc - 1];
        while (read(0, p, 1)) {
            if (*p == '\n') {
                *p = 0;
                break;
            } else
                p++;
        }
        if (p == new_argv[argc - 1]) break;
        if (fork() == 0) {
            exec(command, new_argv);
        }
        wait(0);
    }
    exit(0);
}
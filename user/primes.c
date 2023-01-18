#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void run(int p[]) {
    int prime;
    close(p[1]);
    read(p[0], &prime, 4);
    printf("prime %d\n", prime);
    int n;
    if (read(p[0], &n, 4) == 4) {
        int newp[2];
        pipe(newp);
        if (fork() == 0) {
            run(newp);
        } else {
            close(newp[0]);
            do {
                if (n % prime) {
                    write(newp[1], &n, 4);
                }
            } while (read(p[0], &n, 4) == 4);
            close(p[0]);
            close(newp[1]);
            wait(0);
        }
    }
}

int main(int argc, char *argv[]) {
    int p[2];
    pipe(p);
    if (fork() == 0) {
        run(p);
    } else {
        close(p[0]);
        for (int i = 2; i <= 35; ++i) {
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}

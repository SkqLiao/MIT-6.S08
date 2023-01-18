#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p[2];
    char buf[2];
    pipe(p);
    if (fork() == 0) {
		if (read(p[0], buf, 1) != 1) {
			fprintf(2, "failed to read in child\n");
			exit(1);
		}
		close(p[0]);
		printf("%d: received ping\n", getpid());
		if (write(p[1], buf, 1) != 1) {
			fprintf(2, "failed to write in child\n");
			exit(1);
		}
		close(p[1]);
	} else {
		if (write(p[1], "a", 1) != 1) {
			fprintf(2, "failed to write in parent\n");
			exit(1);
		}
		close(p[1]);
		wait(0);
		if (read(p[0], buf, 1) != 1) {
			fprintf(2, "failed to read in parent\n");
			exit(1);
		}
		printf("%d: received pong\n", getpid());
		close(p[0]);
	}
    exit(0);
}

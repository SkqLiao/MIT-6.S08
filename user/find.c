#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

const char *fmtname(const char *path) {
    static char buf[DIRSIZ + 1];
    const char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ) return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

void find(const char *path, const char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "ls: cannot open %s\n", path);
        exit(1);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        exit(1);
        return;
    }
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        fprintf(2, "ls: path too long\n");
        exit(1);
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        if (strcmp(de.name, ".") == 0) continue;
        if (strcmp(de.name, "..") == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
            printf("ls: cannot stat %s\n", buf);
            continue;
        }
        if (st.type == T_DIR) {
            find(buf, name);
        } else if (strcmp(de.name, name) == 0) {
            printf("%s\n", buf);
        }
    }
    close(fd);
}
int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find dir file\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
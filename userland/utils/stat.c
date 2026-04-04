#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static void fmt_mode(char *buf, mode_t m) {
    buf[0] = S_ISDIR(m)?'d':S_ISLNK(m)?'l':'-';
    buf[1]=(m&S_IRUSR)?'r':'-'; buf[2]=(m&S_IWUSR)?'w':'-'; buf[3]=(m&S_IXUSR)?'x':'-';
    buf[4]=(m&S_IRGRP)?'r':'-'; buf[5]=(m&S_IWGRP)?'w':'-'; buf[6]=(m&S_IXGRP)?'x':'-';
    buf[7]=(m&S_IROTH)?'r':'-'; buf[8]=(m&S_IWOTH)?'w':'-'; buf[9]=(m&S_IXOTH)?'x':'-';
    buf[10]='\0';
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "stat: missing operand\n"); return 1; }
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) < 0) { perror(argv[i]); ret = 1; continue; }
        char mode[12]; fmt_mode(mode, st.st_mode);
        printf("  File: %s\n", argv[i]);
        printf("  Size: %-12lld Blocks: %-8lld\n", (long long)st.st_size, (long long)st.st_blocks);
        printf(" Inode: %-12lld Links: %d\n", (long long)st.st_ino, (int)st.st_nlink);
        printf("Access: %s  Uid: %d  Gid: %d\n", mode, (int)st.st_uid, (int)st.st_gid);
        printf("Modify: %lld\nChange: %lld\n", (long long)st.st_mtime, (long long)st.st_ctime);
    }
    return ret;
}
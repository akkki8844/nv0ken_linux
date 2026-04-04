#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

static int flag_a = 0;
static int flag_u = 0;
static int flag_x = 0;

static void read_proc(const char *pidstr) {
    char path[256], buf[4096];
    snprintf(path, sizeof(path), "/proc/%s/status", pidstr);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    int n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    char name[256] = "?";
    int  pid = atoi(pidstr);
    int  ppid = 0;
    char state = '?';

    char *line = buf;
    while (*line) {
        char *end = strchr(line, '\n');
        if (!end) break;
        *end = '\0';
        if (strncmp(line, "Name:", 5) == 0) sscanf(line+5, " %255s", name);
        if (strncmp(line, "State:", 6) == 0) sscanf(line+6, " %c", &state);
        if (strncmp(line, "PPid:", 5) == 0) sscanf(line+5, " %d", &ppid);
        line = end + 1;
    }

    printf("%5d %5d %c   %s\n", pid, ppid, state, name);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'a': flag_a = 1; break;
                case 'u': flag_u = 1; break;
                case 'x': flag_x = 1; break;
            }
        }
    }
    printf("  PID  PPID S   NAME\n");
    DIR *d = opendir("/proc");
    if (!d) { perror("/proc"); return 1; }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] >= '1' && de->d_name[0] <= '9')
            read_proc(de->d_name);
    }
    closedir(d);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int fd = open("/dev/klog", O_RDONLY);
    if (fd < 0) fd = open("/proc/kmsg", O_RDONLY|O_NONBLOCK);
    if (fd < 0) { perror("dmesg"); return 1; }
    char buf[4096];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, n);
    close(fd);
    return 0;
}
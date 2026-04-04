#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "sleep: missing operand\n"); return 1; }
    double secs = atof(argv[1]);
    unsigned int s = (unsigned int)secs;
    unsigned int us = (unsigned int)((secs - s) * 1000000);
    if (s)  sleep(s);
    if (us) usleep(us);
    return 0;
}
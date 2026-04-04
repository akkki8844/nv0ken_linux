#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int newline = 1, interp = 0, start = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) { newline = 0; start = 2; }
    if (argc > 1 && strcmp(argv[1], "-e") == 0) { interp  = 1; start = 2; }
    if (argc > 2 && strcmp(argv[1], "-n") == 0 && strcmp(argv[2], "-e") == 0)
        { newline = 0; interp = 1; start = 3; }

    for (int i = start; i < argc; i++) {
        if (i > start) putchar(' ');
        const char *s = argv[i];
        while (*s) {
            if (interp && *s == '\\' && *(s+1)) {
                s++;
                switch (*s) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case 'e': putchar('\033'); break;
                    case '\\': putchar('\\'); break;
                    case '0': putchar('\0'); break;
                    default: putchar('\\'); putchar(*s); break;
                }
            } else {
                putchar(*s);
            }
            s++;
        }
    }
    if (newline) putchar('\n');
    return 0;
}
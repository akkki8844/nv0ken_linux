#include <stdio.h>
#include <string.h>

static int flag_a = 0, flag_s = 0, flag_n = 0;
static int flag_r = 0, flag_m = 0;

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'a': flag_a=1; break;
                case 's': flag_s=1; break;
                case 'n': flag_n=1; break;
                case 'r': flag_r=1; break;
                case 'm': flag_m=1; break;
            }
        }
    }
    if (!flag_a && !flag_s && !flag_n && !flag_r && !flag_m) flag_s = 1;
    int first = 1;
#define P(val) do { if (!first) putchar(' '); puts(val); first=0; } while(0)
    if (flag_a || flag_s) P("nv0ken_linux");
    if (flag_a || flag_n) P("nv0ken");
    if (flag_a || flag_r) P("1.0.0");
    if (flag_a || flag_m) P("x86_64");
    if (!first) putchar('\n');
    return 0;
}
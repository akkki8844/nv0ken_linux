#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    long long t;
    sys_time(&t);

    long long s = t % 60; t /= 60;
    long long m = t % 60; t /= 60;
    long long h = t % 24; t /= 24;
    long long days = t;
    long long y = 1970;
    while (1) {
        int leap = (y%4==0 && (y%100!=0 || y%400==0));
        int diy  = leap ? 366 : 365;
        if (days < diy) break;
        days -= diy; y++;
    }
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = (y%4==0 && (y%100!=0 || y%400==0));
    int mo = 0;
    while (1) {
        int d = dm[mo]; if (mo==1 && leap) d++;
        if (days < d) break;
        days -= d; mo++;
    }
    static const char *mons[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                  "Jul","Aug","Sep","Oct","Nov","Dec"};
    static const char *wdays[] = {"Thu","Fri","Sat","Sun","Mon","Tue","Wed"};
    long long epoch_days = (long long)(time(NULL)) / 86400;
    printf("%s %s %lld %02lld:%02lld:%02lld UTC %lld\n",
           wdays[epoch_days % 7], mons[mo], days+1, h, m, s, y);
    return 0;
}
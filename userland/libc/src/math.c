#include <math.h>
#include <errno.h>

double fabs(double value) {
    return value < 0.0 ? -value : value;
}

double fmod(double numerator, double denominator) {
    if (denominator == 0.0) { errno = EDOM; return NAN; }
    long long quotient = (long long)(numerator / denominator);
    return numerator - (double)quotient * denominator;
}

double sqrt(double value) {
    if (value < 0.0) {
        errno = EDOM;
        return NAN;
    }
    if (value == 0.0 || value == INFINITY) return value;
    double estimate = value >= 1.0 ? value : 1.0;
    for (int iteration = 0; iteration < 32; iteration++)
        estimate = 0.5 * (estimate + value / estimate);
    return estimate;
}

double exp(double value) {
    if (value > 709.0) return HUGE_VAL;
    if (value < -745.0) return 0.0;
    int power = 0;
    while (value > 0.5) { value *= 0.5; power++; }
    while (value < -0.5) { value *= 0.5; power++; }
    double term = 1.0;
    double result = 1.0;
    for (int index = 1; index < 28; index++) {
        term *= value / index;
        result += term;
    }
    while (power-- > 0) result *= result;
    return result;
}

double log(double value) {
    if (value < 0.0) { errno = EDOM; return NAN; }
    if (value == 0.0) { errno = ERANGE; return -HUGE_VAL; }
    int exponent = 0;
    while (value >= 2.0) { value *= 0.5; exponent++; }
    while (value < 1.0) { value *= 2.0; exponent--; }
    double ratio = (value - 1.0) / (value + 1.0);
    double ratio_squared = ratio * ratio;
    double term = ratio;
    double result = 0.0;
    for (int divisor = 1; divisor < 40; divisor += 2) {
        result += term / divisor;
        term *= ratio_squared;
    }
    return 2.0 * result + exponent * 0.69314718055994530942;
}

double pow(double base, double exponent) {
    long long integer = (long long)exponent;
    if ((double)integer == exponent) {
        int negative = integer < 0;
        unsigned long long count = negative ? (unsigned long long)(-(integer + 1)) + 1 : (unsigned long long)integer;
        double result = 1.0;
        while (count) {
            if (count & 1) result *= base;
            base *= base;
            count >>= 1;
        }
        return negative ? 1.0 / result : result;
    }
    if (base <= 0.0) { errno = EDOM; return NAN; }
    return exp(exponent * log(base));
}

#ifndef MATH_H
#define MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define NAN (__builtin_nan(""))
#define INFINITY (__builtin_inff())

double fabs(double value);
double fmod(double numerator, double denominator);
double sqrt(double value);
double exp(double value);
double log(double value);
double pow(double base, double exponent);

#endif

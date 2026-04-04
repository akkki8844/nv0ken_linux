#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned long long size_t;
typedef long long          ssize_t;
typedef long long          ptrdiff_t;
typedef unsigned long long uintptr_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

typedef int wchar_t;

#endif
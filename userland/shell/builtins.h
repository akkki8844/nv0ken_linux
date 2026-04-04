#ifndef BUILTINS_H
#define BUILTINS_H

void builtins_init(void);
int  is_builtin(const char *name);
int  run_builtin(char **argv, int argc);

#endif
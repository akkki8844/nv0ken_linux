#ifndef EXPAND_H
#define EXPAND_H

void  expand_init(void);
char *expand_vars(const char *input);
void  expand_argv(char **argv, int argc);
char *expand_path(const char *name);
char *expand_tilde(const char *path);

#endif
#ifndef READLINE_H
#define READLINE_H

void  readline_init(void);
char *readline_read(const char *prompt);
void  readline_history_add(const char *line);
int   readline_history_count(void);
const char *readline_history_get(int index);
void  readline_history_save(const char *path);
void  readline_history_load(const char *path);

#endif
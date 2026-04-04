#include "readline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE    4096
#define MAX_HISTORY 512

static char  *history[MAX_HISTORY];
static int    hcount = 0;
static int    hpos   = 0;

void readline_init(void) {
    memset(history, 0, sizeof(history));
    hcount = 0;
    hpos   = 0;

    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.nv0sh_history", home);
        readline_history_load(path);
    }
}

void readline_history_add(const char *line) {
    if (!line || !line[0]) return;
    if (hcount > 0 && strcmp(history[hcount - 1], line) == 0) return;

    if (hcount >= MAX_HISTORY) {
        free(history[0]);
        memmove(&history[0], &history[1], sizeof(char *) * (MAX_HISTORY - 1));
        hcount--;
    }
    history[hcount++] = strdup(line);
    hpos = hcount;
}

int readline_history_count(void) { return hcount; }

const char *readline_history_get(int index) {
    if (index < 0 || index >= hcount) return NULL;
    return history[index];
}

void readline_history_save(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    int start = hcount > 256 ? hcount - 256 : 0;
    for (int i = start; i < hcount; i++)
        if (history[i]) fprintf(f, "%s\n", history[i]);
    fclose(f);
}

void readline_history_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len) readline_history_add(line);
    }
    fclose(f);
}

/* -----------------------------------------------------------------------
 * Line editor
 * --------------------------------------------------------------------- */

static int  is_tty = 0;
static char linebuf[MAX_LINE];
static int  linelen = 0;
static int  linepos = 0;

static void refresh_line(const char *prompt) {
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, "\033[K", 3);
    write(STDOUT_FILENO, prompt, strlen(prompt));
    write(STDOUT_FILENO, linebuf, linelen);

    if (linepos < linelen) {
        char move[16];
        int back = linelen - linepos;
        int n = snprintf(move, sizeof(move), "\033[%dD", back);
        write(STDOUT_FILENO, move, n);
    }
}

static int handle_escape(void) {
    char seq[4] = {0};
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return 0;
    if (seq[0] != '[') return 0;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return 0;

    switch (seq[1]) {
        case 'A':
            if (hpos > 0) {
                hpos--;
                if (history[hpos]) {
                    strncpy(linebuf, history[hpos], MAX_LINE - 1);
                    linelen = linepos = strlen(linebuf);
                }
            }
            return 1;
        case 'B':
            if (hpos < hcount) {
                hpos++;
                if (hpos == hcount) {
                    linebuf[0] = '\0';
                    linelen = linepos = 0;
                } else if (history[hpos]) {
                    strncpy(linebuf, history[hpos], MAX_LINE - 1);
                    linelen = linepos = strlen(linebuf);
                }
            }
            return 1;
        case 'C':
            if (linepos < linelen) linepos++;
            return 1;
        case 'D':
            if (linepos > 0) linepos--;
            return 1;
        case 'H': case '1':
            linepos = 0;
            return 1;
        case 'F': case '4':
            linepos = linelen;
            return 1;
        case '3':
            if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0;
            if (seq[2] == '~' && linepos < linelen) {
                memmove(linebuf + linepos, linebuf + linepos + 1,
                        linelen - linepos - 1);
                linelen--;
                linebuf[linelen] = '\0';
            }
            return 1;
        default:
            return 1;
    }
}

char *readline_read(const char *prompt) {
    is_tty = isatty(STDIN_FILENO);

    if (!is_tty) {
        write(STDOUT_FILENO, prompt, strlen(prompt));
        char *line = malloc(MAX_LINE);
        if (!line) return NULL;
        if (!fgets(line, MAX_LINE, stdin)) { free(line); return NULL; }
        return line;
    }

    linebuf[0] = '\0';
    linelen = 0;
    linepos = 0;
    hpos    = hcount;

    write(STDOUT_FILENO, prompt, strlen(prompt));

    while (1) {
        unsigned char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            if (n == 0 || errno == EIO) {
                write(STDOUT_FILENO, "\n", 1);
                return NULL;
            }
            continue;
        }

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        if (c == 4) {
            if (linelen == 0) {
                write(STDOUT_FILENO, "\n", 1);
                return NULL;
            }
            continue;
        }

        if (c == 3) {
            linebuf[0] = '\0';
            linelen = linepos = 0;
            write(STDOUT_FILENO, "^C\n", 3);
            write(STDOUT_FILENO, prompt, strlen(prompt));
            continue;
        }

        if (c == 12) {
            write(STDOUT_FILENO, "\033[2J\033[H", 7);
            refresh_line(prompt);
            continue;
        }

        if (c == 127 || c == 8) {
            if (linepos > 0) {
                memmove(linebuf + linepos - 1, linebuf + linepos,
                        linelen - linepos + 1);
                linepos--;
                linelen--;
                linebuf[linelen] = '\0';
                refresh_line(prompt);
            }
            continue;
        }

        if (c == 23) {
            while (linepos > 0 && linebuf[linepos-1] == ' ') {
                memmove(linebuf + linepos - 1, linebuf + linepos,
                        linelen - linepos + 1);
                linepos--; linelen--;
            }
            while (linepos > 0 && linebuf[linepos-1] != ' ') {
                memmove(linebuf + linepos - 1, linebuf + linepos,
                        linelen - linepos + 1);
                linepos--; linelen--;
            }
            linebuf[linelen] = '\0';
            refresh_line(prompt);
            continue;
        }

        if (c == 1) { linepos = 0; refresh_line(prompt); continue; }
        if (c == 5) { linepos = linelen; refresh_line(prompt); continue; }

        if (c == '\033') {
            handle_escape();
            refresh_line(prompt);
            continue;
        }

        if (c >= 32 && linelen < MAX_LINE - 1) {
            memmove(linebuf + linepos + 1, linebuf + linepos,
                    linelen - linepos + 1);
            linebuf[linepos++] = c;
            linelen++;
            linebuf[linelen] = '\0';
            refresh_line(prompt);
            continue;
        }
    }

    char *out = strdup(linebuf);
    return out;
}
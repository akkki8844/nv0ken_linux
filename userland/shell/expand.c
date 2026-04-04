#include "expand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define EXPAND_BUF 8192

void expand_init(void) {}

/* -----------------------------------------------------------------------
 * Tilde expansion: ~ → $HOME, ~user not supported
 * --------------------------------------------------------------------- */

char *expand_tilde(const char *path) {
    if (!path || path[0] != '~') return strdup(path);
    const char *home = getenv("HOME");
    if (!home) return strdup(path);
    char buf[EXPAND_BUF];
    snprintf(buf, sizeof(buf), "%s%s", home, path + 1);
    return strdup(buf);
}

/* -----------------------------------------------------------------------
 * Variable expansion: $VAR, ${VAR}, $?, $$, $0
 * --------------------------------------------------------------------- */

char *expand_vars(const char *input) {
    if (!input) return NULL;
    char out[EXPAND_BUF];
    int  op = 0;

    for (const char *p = input; *p && op < (int)sizeof(out) - 1; ) {
        if (*p == '\\' && *(p+1)) {
            out[op++] = *(p+1);
            p += 2;
            continue;
        }
        if (*p == '$') {
            p++;
            if (*p == '{') {
                p++;
                char name[256];
                int ni = 0;
                while (*p && *p != '}' && ni < 255) name[ni++] = *p++;
                name[ni] = '\0';
                if (*p == '}') p++;
                const char *val = getenv(name);
                if (val) {
                    size_t vl = strlen(val);
                    if (op + (int)vl < (int)sizeof(out) - 1) {
                        memcpy(out + op, val, vl);
                        op += vl;
                    }
                }
            } else if (*p == '?') {
                char status[8];
                extern int g_last_status;
                snprintf(status, sizeof(status), "%d", g_last_status);
                int sl = strlen(status);
                if (op + sl < (int)sizeof(out) - 1) {
                    memcpy(out + op, status, sl);
                    op += sl;
                }
                p++;
            } else if (*p == '$') {
                char pid[16];
                snprintf(pid, sizeof(pid), "%d", getpid());
                int pl = strlen(pid);
                if (op + pl < (int)sizeof(out) - 1) {
                    memcpy(out + op, pid, pl);
                    op += pl;
                }
                p++;
            } else if (*p == '0') {
                const char *name = getenv("0");
                if (!name) name = "nv0sh";
                int nl = strlen(name);
                if (op + nl < (int)sizeof(out) - 1) {
                    memcpy(out + op, name, nl);
                    op += nl;
                }
                p++;
            } else if (*p == '#') {
                out[op++] = '0';
                p++;
            } else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                        *p == '_') {
                char name[256];
                int ni = 0;
                while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                       (*p >= '0' && *p <= '9') || *p == '_') {
                    name[ni++] = *p++;
                    if (ni >= 255) break;
                }
                name[ni] = '\0';
                const char *val = getenv(name);
                if (val) {
                    size_t vl = strlen(val);
                    if (op + (int)vl < (int)sizeof(out) - 1) {
                        memcpy(out + op, val, vl);
                        op += vl;
                    }
                }
            } else {
                out[op++] = '$';
            }
            continue;
        }
        out[op++] = *p++;
    }
    out[op] = '\0';
    return strdup(out);
}

/* -----------------------------------------------------------------------
 * Glob expansion — simple *, ? support for a single word
 * --------------------------------------------------------------------- */

static int glob_match(const char *pat, const char *str) {
    while (*pat && *str) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*str) {
                if (glob_match(pat, str)) return 1;
                str++;
            }
            return 0;
        }
        if (*pat == '?') { pat++; str++; continue; }
        if (*pat != *str) return 0;
        pat++; str++;
    }
    while (*pat == '*') pat++;
    return !*pat && !*str;
}

static char **glob_expand(const char *word, int *count) {
    *count = 0;
    char **results = NULL;
    int cap = 0;

    char dir_part[EXPAND_BUF] = ".";
    char pat_part[EXPAND_BUF];
    strncpy(pat_part, word, sizeof(pat_part) - 1);

    char *slash = strrchr(word, '/');
    if (slash) {
        int dlen = slash - word;
        strncpy(dir_part, word, dlen); dir_part[dlen] = '\0';
        strncpy(pat_part, slash + 1, sizeof(pat_part) - 1);
    }

    struct dirent *de;
    int fd = opendir(dir_part) ? 1 : 0;
    (void)fd;

    char tmppath[EXPAND_BUF];
    snprintf(tmppath, sizeof(tmppath), "%s", dir_part);

    DIR *d = opendir(dir_part);
    if (!d) return NULL;

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.' && pat_part[0] != '.') continue;
        if (!glob_match(pat_part, de->d_name)) continue;

        if (*count >= cap) {
            cap = cap == 0 ? 8 : cap * 2;
            char **nr = realloc(results, sizeof(char *) * cap);
            if (!nr) break;
            results = nr;
        }
        char full[EXPAND_BUF];
        if (strcmp(dir_part, ".") == 0)
            snprintf(full, sizeof(full), "%s", de->d_name);
        else
            snprintf(full, sizeof(full), "%s/%s", dir_part, de->d_name);
        results[(*count)++] = strdup(full);
    }
    closedir(d);
    return results;
}

/* -----------------------------------------------------------------------
 * Public: expand each word in argv in place
 * --------------------------------------------------------------------- */

void expand_argv(char **argv, int argc) {
    for (int i = 0; i < argc && argv[i]; i++) {
        char *word = argv[i];
        char *tilded = expand_tilde(word);
        char *expanded = expand_vars(tilded ? tilded : word);
        free(tilded);

        if (!expanded) continue;

        int has_glob = strchr(expanded, '*') || strchr(expanded, '?');
        if (has_glob) {
            int count;
            char **matches = glob_expand(expanded, &count);
            if (matches && count > 0) {
                free(argv[i]);
                argv[i] = matches[0];
                free(matches);
            } else {
                free(argv[i]);
                argv[i] = expanded;
                expanded = NULL;
            }
        } else {
            free(argv[i]);
            argv[i] = expanded;
            expanded = NULL;
        }
        free(expanded);
    }
}

/* -----------------------------------------------------------------------
 * PATH lookup
 * --------------------------------------------------------------------- */

char *expand_path(const char *name) {
    if (!name) return NULL;
    if (strchr(name, '/')) return strdup(name);

    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";

    char paths[EXPAND_BUF];
    strncpy(paths, path, sizeof(paths) - 1);

    char *dir = strtok(paths, ":");
    static char result[EXPAND_BUF];

    while (dir) {
        snprintf(result, sizeof(result), "%s/%s", dir, name);
        if (access(result, X_OK) == 0) return result;
        dir = strtok(NULL, ":");
    }
    return NULL;
}
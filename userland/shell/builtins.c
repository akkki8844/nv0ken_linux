#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_ALIASES  64
#define MAX_HISTORY  512

typedef struct { char *name; char *value; } Alias;

static Alias   aliases[MAX_ALIASES];
static int     alias_count = 0;
static char   *history[MAX_HISTORY];
static int     history_count = 0;
static int     last_exit = 0;

void builtins_init(void) {}

/* -----------------------------------------------------------------------
 * Individual builtins
 * --------------------------------------------------------------------- */

static int builtin_cd(char **argv, int argc) {
    const char *dir;
    if (argc < 2) {
        dir = getenv("HOME");
        if (!dir) dir = "/";
    } else if (strcmp(argv[1], "-") == 0) {
        dir = getenv("OLDPWD");
        if (!dir) { fprintf(stderr, "cd: OLDPWD not set\n"); return 1; }
        printf("%s\n", dir);
    } else {
        dir = argv[1];
    }

    char oldpwd[512];
    getcwd(oldpwd, sizeof(oldpwd));

    if (chdir(dir) < 0) { perror("cd"); return 1; }

    setenv("OLDPWD", oldpwd, 1);

    char newpwd[512];
    if (getcwd(newpwd, sizeof(newpwd)))
        setenv("PWD", newpwd, 1);

    return 0;
}

static int builtin_echo(char **argv, int argc) {
    int newline = 1;
    int start   = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        start   = 2;
    }

    for (int i = start; i < argc; i++) {
        if (i > start) putchar(' ');
        const char *s = argv[i];
        while (*s) {
            if (*s == '\\' && *(s+1)) {
                s++;
                switch (*s) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case '\\': putchar('\\'); break;
                    case 'e': putchar('\033'); break;
                    default: putchar('\\'); putchar(*s); break;
                }
            } else {
                putchar(*s);
            }
            s++;
        }
    }
    if (newline) putchar('\n');
    return 0;
}

static int builtin_pwd(char **argv, int argc) {
    (void)argv; (void)argc;
    char buf[1024];
    if (!getcwd(buf, sizeof(buf))) { perror("pwd"); return 1; }
    puts(buf);
    return 0;
}

static int builtin_exit(char **argv, int argc) {
    int code = argc > 1 ? atoi(argv[1]) : last_exit;
    exit(code);
    return 0;
}

static int builtin_export(char **argv, int argc) {
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            setenv(argv[i], eq + 1, 1);
            *eq = '=';
        } else {
            char *val = getenv(argv[i]);
            if (val) setenv(argv[i], val, 1);
        }
    }
    return 0;
}

static int builtin_unset(char **argv, int argc) {
    for (int i = 1; i < argc; i++) unsetenv(argv[i]);
    return 0;
}

static int builtin_alias(char **argv, int argc) {
    if (argc == 1) {
        for (int i = 0; i < alias_count; i++)
            printf("alias %s='%s'\n", aliases[i].name, aliases[i].value);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            for (int j = 0; j < alias_count; j++) {
                if (strcmp(aliases[j].name, argv[i]) == 0) {
                    printf("alias %s='%s'\n", aliases[j].name, aliases[j].value);
                    break;
                }
            }
            continue;
        }
        char name[256];
        int nlen = eq - argv[i];
        strncpy(name, argv[i], nlen); name[nlen] = '\0';
        const char *val = eq + 1;

        for (int j = 0; j < alias_count; j++) {
            if (strcmp(aliases[j].name, name) == 0) {
                free(aliases[j].value);
                aliases[j].value = strdup(val);
                return 0;
            }
        }
        if (alias_count < MAX_ALIASES) {
            aliases[alias_count].name  = strdup(name);
            aliases[alias_count].value = strdup(val);
            alias_count++;
        }
    }
    return 0;
}

static int builtin_unalias(char **argv, int argc) {
    for (int i = 1; i < argc; i++) {
        for (int j = 0; j < alias_count; j++) {
            if (strcmp(aliases[j].name, argv[i]) == 0) {
                free(aliases[j].name);
                free(aliases[j].value);
                for (int k = j; k < alias_count - 1; k++)
                    aliases[k] = aliases[k+1];
                alias_count--;
                break;
            }
        }
    }
    return 0;
}

static int builtin_history(char **argv, int argc) {
    (void)argv; (void)argc;
    for (int i = 0; i < history_count; i++)
        printf("%4d  %s\n", i + 1, history[i]);
    return 0;
}

static int builtin_source(char **argv, int argc) {
    if (argc < 2) { fprintf(stderr, "source: missing filename\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    char line[4096];
    extern int execute(const char *);
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len && line[0] != '#') execute(line);
    }
    fclose(f);
    return 0;
}

static int builtin_type(char **argv, int argc) {
    for (int i = 1; i < argc; i++) {
        if (is_builtin(argv[i])) {
            printf("%s is a shell builtin\n", argv[i]);
        } else {
            char *path = getenv("PATH");
            if (!path) { printf("%s not found\n", argv[i]); continue; }
            char paths[4096];
            strncpy(paths, path, sizeof(paths) - 1);
            char *dir = strtok(paths, ":");
            int found = 0;
            while (dir) {
                char full[1024];
                snprintf(full, sizeof(full), "%s/%s", dir, argv[i]);
                if (access(full, X_OK) == 0) {
                    printf("%s is %s\n", argv[i], full);
                    found = 1; break;
                }
                dir = strtok(NULL, ":");
            }
            if (!found) printf("%s: not found\n", argv[i]);
        }
    }
    return 0;
}

static int builtin_true(char **argv, int argc) {
    (void)argv; (void)argc; return 0;
}

static int builtin_false(char **argv, int argc) {
    (void)argv; (void)argc; return 1;
}

static int builtin_colon(char **argv, int argc) {
    (void)argv; (void)argc; return 0;
}

static int builtin_read(char **argv, int argc) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return 1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    if (argc > 1) setenv(argv[1], buf, 1);
    return 0;
}

static int builtin_shift(char **argv, int argc) {
    (void)argv; (void)argc; return 0;
}

static int builtin_set(char **argv, int argc) {
    if (argc == 1) {
        extern char **environ;
        for (char **e = environ; *e; e++) printf("%s\n", *e);
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Dispatch table
 * --------------------------------------------------------------------- */

typedef struct {
    const char *name;
    int (*fn)(char **argv, int argc);
} BuiltinEntry;

static BuiltinEntry table[] = {
    { "cd",       builtin_cd       },
    { "echo",     builtin_echo     },
    { "pwd",      builtin_pwd      },
    { "exit",     builtin_exit     },
    { "export",   builtin_export   },
    { "unset",    builtin_unset    },
    { "alias",    builtin_alias    },
    { "unalias",  builtin_unalias  },
    { "history",  builtin_history  },
    { "source",   builtin_source   },
    { ".",        builtin_source   },
    { "type",     builtin_type     },
    { "true",     builtin_true     },
    { "false",    builtin_false    },
    { ":",        builtin_colon    },
    { "read",     builtin_read     },
    { "shift",    builtin_shift    },
    { "set",      builtin_set      },
    { NULL,       NULL             },
};

int is_builtin(const char *name) {
    for (int i = 0; table[i].name; i++)
        if (strcmp(table[i].name, name) == 0) return 1;
    return 0;
}

int run_builtin(char **argv, int argc) {
    if (!argv || !argv[0]) return 1;
    for (int i = 0; table[i].name; i++) {
        if (strcmp(table[i].name, argv[0]) == 0)
            return table[i].fn(argv, argc);
    }
    return 127;
}
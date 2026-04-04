#include "parser.h"
#include "builtins.h"
#include "job_control.h"
#include "readline.h"
#include "expand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define PROMPT_MAX  256
#define INPUT_MAX   4096

static int g_last_status = 0;
static int g_interactive  = 0;

/* -----------------------------------------------------------------------
 * Prompt
 * --------------------------------------------------------------------- */

static void build_prompt(char *buf, size_t size) {
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "?");

    const char *home = getenv("HOME");
    if (home) {
        size_t hlen = strlen(home);
        if (strncmp(cwd, home, hlen) == 0) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "~%s", cwd + hlen);
            strcpy(cwd, tmp);
        }
    }

    snprintf(buf, size, "\033[1;32mnv0sh\033[0m:\033[1;34m%s\033[0m%s ",
             cwd, getuid() == 0 ? "#" : "$");
}

/* -----------------------------------------------------------------------
 * Redirect setup
 * --------------------------------------------------------------------- */

static int setup_redirects(Redirect *redirs, int count) {
    for (int i = 0; i < count; i++) {
        Redirect *r = &redirs[i];
        int fd = -1;

        switch (r->type) {
            case REDIR_IN:
                fd = open(r->file, O_RDONLY);
                if (fd < 0) { perror(r->file); return -1; }
                dup2(fd, STDIN_FILENO);
                close(fd);
                break;
            case REDIR_OUT:
                fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
            case REDIR_APPEND:
                fd = open(r->file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
            case REDIR_ERR:
                fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
            case REDIR_ERR_APPEND:
                fd = open(r->file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
            case REDIR_ERR_OUT:
                dup2(STDOUT_FILENO, STDERR_FILENO);
                break;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Execute a single simple command
 * --------------------------------------------------------------------- */

static int exec_simple(SimpleCmd *cmd, int pgid, int foreground,
                        int in_fd, int out_fd) {
    if (!cmd || cmd->argc == 0) return 0;

    expand_argv(cmd->argv, cmd->argc);

    if (is_builtin(cmd->argv[0])) {
        int saved_in  = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        int saved_err = dup(STDERR_FILENO);

        if (in_fd  != STDIN_FILENO)  dup2(in_fd,  STDIN_FILENO);
        if (out_fd != STDOUT_FILENO) dup2(out_fd, STDOUT_FILENO);
        setup_redirects(cmd->redirs, cmd->redir_count);

        int ret = run_builtin(cmd->argv, cmd->argc);
        g_last_status = ret;

        dup2(saved_in,  STDIN_FILENO);
        dup2(saved_out, STDOUT_FILENO);
        dup2(saved_err, STDERR_FILENO);
        close(saved_in); close(saved_out); close(saved_err);
        return ret;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        if (pgid == 0) pgid = getpid();
        setpgid(0, pgid);
        if (foreground && g_interactive)
            tcsetpgrp(STDIN_FILENO, pgid);

        if (in_fd  != STDIN_FILENO)  { dup2(in_fd,  STDIN_FILENO);  close(in_fd); }
        if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }

        if (setup_redirects(cmd->redirs, cmd->redir_count) < 0) _exit(1);

        char *path = expand_path(cmd->argv[0]);
        execve(path ? path : cmd->argv[0], cmd->argv, environ);
        fprintf(stderr, "nv0sh: %s: command not found\n", cmd->argv[0]);
        _exit(127);
    }

    if (pgid == 0) pgid = pid;
    setpgid(pid, pgid);
    return pid;
}

/* -----------------------------------------------------------------------
 * Execute a pipeline
 * --------------------------------------------------------------------- */

static int exec_pipeline(Pipeline *pl) {
    if (!pl || pl->cmd_count == 0) return 0;

    if (pl->cmd_count == 1) {
        int pid = exec_simple(&pl->cmds[0], 0, !pl->background,
                              STDIN_FILENO, STDOUT_FILENO);
        if (pl->background) {
            job_add(pid, pl->cmds[0].argv[0]);
            printf("[%d] %d\n", job_count(), pid);
            return 0;
        }
        if (pid > 0) {
            int status;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                job_add(pid, pl->cmds[0].argv[0]);
                printf("\n[%d]+ Stopped  %s\n", job_count(),
                       pl->cmds[0].argv[0]);
                return 0;
            }
            g_last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        } else {
            g_last_status = pid;
        }
        return g_last_status;
    }

    int pipes[pl->cmd_count - 1][2];
    for (int i = 0; i < pl->cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return 1; }
    }

    pid_t pgid = 0;
    pid_t pids[pl->cmd_count];

    for (int i = 0; i < pl->cmd_count; i++) {
        int in_fd  = (i == 0) ? STDIN_FILENO  : pipes[i-1][0];
        int out_fd = (i == pl->cmd_count - 1) ? STDOUT_FILENO : pipes[i][1];

        pid_t pid = exec_simple(&pl->cmds[i], pgid, !pl->background,
                                in_fd, out_fd);
        if (i == 0) pgid = pid;
        pids[i] = pid;
    }

    for (int i = 0; i < pl->cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (pl->background) {
        job_add(pgid, pl->cmds[0].argv[0]);
        printf("[%d] %d\n", job_count(), pgid);
        return 0;
    }

    if (g_interactive) tcsetpgrp(STDIN_FILENO, pgid);

    int status = 0;
    for (int i = 0; i < pl->cmd_count; i++) {
        if (pids[i] > 0) waitpid(pids[i], &status, WUNTRACED);
    }

    if (g_interactive) tcsetpgrp(STDIN_FILENO, getpgrp());

    g_last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return g_last_status;
}

/* -----------------------------------------------------------------------
 * Execute a command list (&&, ||, ;)
 * --------------------------------------------------------------------- */

static int exec_list(CmdList *list);

static int exec_node(ASTNode *node) {
    if (!node) return 0;
    switch (node->type) {
        case NODE_PIPELINE:
            return exec_pipeline(&node->pipeline);
        case NODE_AND:
            if (exec_node(node->left) == 0)
                return exec_node(node->right);
            return g_last_status;
        case NODE_OR:
            if (exec_node(node->left) != 0)
                return exec_node(node->right);
            return g_last_status;
        case NODE_SEQ:
            exec_node(node->left);
            return exec_node(node->right);
        case NODE_SUBSHELL: {
            pid_t pid = fork();
            if (pid == 0) {
                exec_node(node->left);
                _exit(g_last_status);
            }
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        }
        default:
            return 0;
    }
}

/* -----------------------------------------------------------------------
 * Execute a full parsed command string
 * --------------------------------------------------------------------- */

static int execute(const char *input) {
    if (!input || !input[0]) return 0;

    char *expanded = expand_vars(input);
    const char *src = expanded ? expanded : input;

    ASTNode *ast = parse(src);
    if (!ast) { free(expanded); return 0; }

    int ret = exec_node(ast);
    ast_free(ast);
    free(expanded);
    return ret;
}

/* -----------------------------------------------------------------------
 * Source a file
 * --------------------------------------------------------------------- */

static int source_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[INPUT_MAX];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (line[0] && line[0] != '#') execute(line);
    }
    fclose(f);
    return 0;
}

/* -----------------------------------------------------------------------
 * Startup files
 * --------------------------------------------------------------------- */

static void load_startup(void) {
    source_file("/etc/profile");
    const char *home = getenv("HOME");
    if (home) {
        char rc[512];
        snprintf(rc, sizeof(rc), "%s/.nv0shrc", home);
        source_file(rc);
    }
}

/* -----------------------------------------------------------------------
 * Interactive REPL
 * --------------------------------------------------------------------- */

static void repl(void) {
    char prompt[PROMPT_MAX];
    char *line;

    while (1) {
        job_reap_all();

        build_prompt(prompt, sizeof(prompt));
        line = readline_read(prompt);

        if (!line) {
            printf("\n");
            break;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (!len) { free(line); continue; }

        readline_history_add(line);
        execute(line);
        free(line);
    }
}

/* -----------------------------------------------------------------------
 * Non-interactive: execute from file or stdin
 * --------------------------------------------------------------------- */

static int run_script(const char *path) {
    FILE *f;
    if (strcmp(path, "-") == 0) {
        f = stdin;
    } else {
        f = fopen(path, "r");
        if (!f) { perror(path); return 1; }
    }

    char line[INPUT_MAX];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (!len || line[0] == '#') continue;
        execute(line);
    }

    if (f != stdin) fclose(f);
    return g_last_status;
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
    setenv("SHELL", "/bin/nv0sh", 0);
    setenv("NV0SH_VERSION", "1.0", 0);

    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    setenv("$", pid_str, 1);
    setenv("0", argv[0], 1);

    builtins_init();
    job_init();
    readline_init();
    expand_init();

    if (argc == 1 && isatty(STDIN_FILENO)) {
        g_interactive = 1;
        load_startup();
        repl();
        return g_last_status;
    }

    if (argc == 2) {
        return run_script(argv[1]);
    }

    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        execute(argv[2]);
        return g_last_status;
    }

    fprintf(stderr, "usage: nv0sh [script | -c command]\n");
    return 1;
}
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define INITTAB_PATH    "/etc/inittab"
#define MOTD_PATH       "/etc/motd"
#define SHELL_PATH      "/bin/nv0sh"
#define TTY_PATH        "/dev/tty0"
#define LOG_PATH        "/var/log/init.log"
#define MAX_SERVICES    32
#define MAX_PATH_LEN    256
#define MAX_ARGS        16

typedef enum {
    SVC_ONCE,
    SVC_RESPAWN,
    SVC_BOOT,
    SVC_SHUTDOWN,
} SvcType;

typedef struct {
    char     path[MAX_PATH_LEN];
    char    *argv[MAX_ARGS];
    SvcType  type;
    pid_t    pid;
    int      running;
} Service;

static Service services[MAX_SERVICES];
static int     service_count = 0;
static int     log_fd = -1;

/* -----------------------------------------------------------------------
 * Logging
 * --------------------------------------------------------------------- */

static void init_log(const char *msg) {
    if (log_fd >= 0) {
        write(log_fd, "[init] ", 7);
        write(log_fd, msg, strlen(msg));
        write(log_fd, "\n", 1);
    }
    write(STDOUT_FILENO, "[init] ", 7);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\n", 1);
}

static void init_log_pid(const char *msg, pid_t pid) {
    char buf[128];
    int n = 0;
    const char *p = msg;
    while (*p && n < 100) buf[n++] = *p++;
    buf[n++] = ' ';
    char pid_str[16];
    int pid_len = 0;
    pid_t tmp = pid;
    if (tmp == 0) { pid_str[pid_len++] = '0'; }
    else {
        while (tmp > 0) { pid_str[pid_len++] = '0' + (tmp % 10); tmp /= 10; }
        for (int i = 0, j = pid_len - 1; i < j; i++, j--) {
            char c = pid_str[i]; pid_str[i] = pid_str[j]; pid_str[j] = c;
        }
    }
    for (int i = 0; i < pid_len && n < 127; i++) buf[n++] = pid_str[i];
    buf[n] = '\0';
    init_log(buf);
}

/* -----------------------------------------------------------------------
 * Filesystem setup
 * --------------------------------------------------------------------- */

static void mount_filesystems(void) {
    mkdir("/proc",   0755);
    mkdir("/sys",    0755);
    mkdir("/dev",    0755);
    mkdir("/tmp",    0777);
    mkdir("/var",    0755);
    mkdir("/var/log",0755);
    mkdir("/run",    0755);
    mkdir("/etc",    0755);
    mkdir("/bin",    0755);
    mkdir("/home",   0755);
    mkdir("/mnt",    0755);

    if (mount("proc",    "/proc",   "procfs",  0) < 0)
        init_log("warning: could not mount procfs");
    if (mount("tmpfs",   "/tmp",    "tmpfs",   0) < 0)
        init_log("warning: could not mount tmpfs on /tmp");
    if (mount("tmpfs",   "/run",    "tmpfs",   0) < 0)
        init_log("warning: could not mount tmpfs on /run");
    if (mount("sysfs",   "/sys",    "sysfs",   0) < 0)
        init_log("warning: could not mount sysfs");
    if (mount("devfs",   "/dev",    "devfs",   0) < 0)
        init_log("warning: could not mount devfs");

    init_log("filesystems mounted");
}

/* -----------------------------------------------------------------------
 * /dev node setup
 * --------------------------------------------------------------------- */

static void setup_dev(void) {
    int fd;

    fd = open("/dev/null",   O_CREAT | O_WRONLY, 0666);
    if (fd >= 0) close(fd);
    fd = open("/dev/zero",   O_CREAT | O_WRONLY, 0666);
    if (fd >= 0) close(fd);
    fd = open("/dev/tty0",   O_CREAT | O_RDWR,   0620);
    if (fd >= 0) close(fd);
    fd = open("/dev/tty1",   O_CREAT | O_RDWR,   0620);
    if (fd >= 0) close(fd);
    fd = open("/dev/stdin",  O_CREAT | O_RDONLY,  0444);
    if (fd >= 0) close(fd);
    fd = open("/dev/stdout", O_CREAT | O_WRONLY,  0222);
    if (fd >= 0) close(fd);
    fd = open("/dev/stderr", O_CREAT | O_WRONLY,  0222);
    if (fd >= 0) close(fd);
    fd = open("/dev/sda",    O_CREAT | O_RDWR,    0660);
    if (fd >= 0) close(fd);
    fd = open("/dev/random", O_CREAT | O_RDONLY,  0444);
    if (fd >= 0) close(fd);
    fd = open("/dev/urandom",O_CREAT | O_RDONLY,  0444);
    if (fd >= 0) close(fd);

    init_log("dev nodes created");
}

/* -----------------------------------------------------------------------
 * inittab parser
 * Format: type:path:arg1:arg2:...
 * Types:  boot, once, respawn, shutdown
 * --------------------------------------------------------------------- */

static SvcType parse_type(const char *s) {
    if (strcmp(s, "boot")     == 0) return SVC_BOOT;
    if (strcmp(s, "once")     == 0) return SVC_ONCE;
    if (strcmp(s, "respawn")  == 0) return SVC_RESPAWN;
    if (strcmp(s, "shutdown") == 0) return SVC_SHUTDOWN;
    return SVC_ONCE;
}

static void parse_inittab(void) {
    int fd = open(INITTAB_PATH, O_RDONLY);
    if (fd < 0) {
        init_log("no inittab found — using defaults");
        return;
    }

    char buf[4096];
    int  n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    char *line = buf;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') end++;
        *end = '\0';

        if (line[0] != '#' && line[0] != '\0' && service_count < MAX_SERVICES) {
            Service *svc = &services[service_count];
            memset(svc, 0, sizeof(Service));

            char *tok = strtok(line, ":");
            if (!tok) { line = end + 1; continue; }
            svc->type = parse_type(tok);

            tok = strtok(NULL, ":");
            if (!tok) { line = end + 1; continue; }
            strncpy(svc->path, tok, MAX_PATH_LEN - 1);

            int argc = 0;
            svc->argv[argc++] = svc->path;
            while ((tok = strtok(NULL, ":")) != NULL && argc < MAX_ARGS - 1)
                svc->argv[argc++] = tok;
            svc->argv[argc] = NULL;

            svc->pid     = -1;
            svc->running = 0;
            service_count++;
        }

        line = end + 1;
    }

    init_log("inittab parsed");
}

/* -----------------------------------------------------------------------
 * Service spawning
 * --------------------------------------------------------------------- */

static pid_t spawn_service(Service *svc) {
    pid_t pid = fork();
    if (pid < 0) {
        init_log("fork failed");
        return -1;
    }

    if (pid == 0) {
        int tty = open(TTY_PATH, O_RDWR);
        if (tty >= 0) {
            dup2(tty, STDIN_FILENO);
            dup2(tty, STDOUT_FILENO);
            dup2(tty, STDERR_FILENO);
            if (tty > 2) close(tty);
        }

        char *envp[] = {
            "PATH=/bin:/usr/bin",
            "HOME=/home",
            "USER=root",
            "TERM=xterm-256color",
            "SHELL=" SHELL_PATH,
            NULL
        };

        execve(svc->path, svc->argv, envp);
        write(STDERR_FILENO, "init: execve failed: ", 21);
        write(STDERR_FILENO, svc->path, strlen(svc->path));
        write(STDERR_FILENO, "\n", 1);
        _exit(127);
    }

    svc->pid     = pid;
    svc->running = 1;
    init_log_pid("spawned", pid);
    return pid;
}

/* -----------------------------------------------------------------------
 * Boot services — run once at startup, in order
 * --------------------------------------------------------------------- */

static void run_boot_services(void) {
    for (int i = 0; i < service_count; i++) {
        if (services[i].type != SVC_BOOT) continue;
        pid_t pid = spawn_service(&services[i]);
        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            services[i].running = 0;
        }
    }
}

/* -----------------------------------------------------------------------
 * Once services — run once, don't respawn
 * --------------------------------------------------------------------- */

static void run_once_services(void) {
    for (int i = 0; i < service_count; i++) {
        if (services[i].type != SVC_ONCE) continue;
        spawn_service(&services[i]);
    }
}

/* -----------------------------------------------------------------------
 * Respawn services — restart on exit
 * --------------------------------------------------------------------- */

static void start_respawn_services(void) {
    for (int i = 0; i < service_count; i++) {
        if (services[i].type != SVC_RESPAWN) continue;
        spawn_service(&services[i]);
    }
}

/* -----------------------------------------------------------------------
 * Default shell if no inittab
 * --------------------------------------------------------------------- */

static void spawn_default_shell(void) {
    if (service_count > 0) return;

    init_log("spawning default shell");

    Service *svc = &services[service_count++];
    memset(svc, 0, sizeof(Service));
    strncpy(svc->path, SHELL_PATH, MAX_PATH_LEN - 1);
    svc->argv[0] = svc->path;
    svc->argv[1] = NULL;
    svc->type    = SVC_RESPAWN;
    spawn_service(svc);
}

/* -----------------------------------------------------------------------
 * MOTD
 * --------------------------------------------------------------------- */

static void print_motd(void) {
    int fd = open(MOTD_PATH, O_RDONLY);
    if (fd < 0) {
        const char *default_motd =
            "\n"
            "  nv0ken_linux\n"
            "  welcome\n"
            "\n";
        write(STDOUT_FILENO, default_motd, strlen(default_motd));
        return;
    }
    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, n);
    close(fd);
}

/* -----------------------------------------------------------------------
 * Reap loop — wait for children, respawn if needed
 * --------------------------------------------------------------------- */

static void reap_loop(void) {
    while (1) {
        int status;
        pid_t pid = wait(&status);
        if (pid < 0) continue;

        for (int i = 0; i < service_count; i++) {
            if (services[i].pid != pid) continue;
            services[i].running = 0;
            services[i].pid     = -1;

            if (services[i].type == SVC_RESPAWN) {
                init_log_pid("respawning", pid);
                spawn_service(&services[i]);
            } else {
                init_log_pid("process exited", pid);
            }
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * Entry point — PID 1
 * --------------------------------------------------------------------- */

int main(void) {
    if (getpid() != 1) {
        write(STDERR_FILENO, "init: must be PID 1\n", 20);
        return 1;
    }

    log_fd = open(LOG_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);

    init_log("nv0ken init starting");

    mount_filesystems();
    setup_dev();
    parse_inittab();
    run_boot_services();
    print_motd();
    run_once_services();
    start_respawn_services();
    spawn_default_shell();

    init_log("entering reap loop");
    reap_loop();

    return 0;
}
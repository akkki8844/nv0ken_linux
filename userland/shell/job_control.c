#include "job_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_JOBS 64

static Job jobs[MAX_JOBS];
static int job_next_id = 1;
static int njobs = 0;

void job_init(void) {
    memset(jobs, 0, sizeof(jobs));
    job_next_id = 1;
    njobs = 0;
}

void job_add(pid_t pgid, const char *name) {
    if (njobs >= MAX_JOBS) return;
    Job *j = &jobs[njobs++];
    j->id    = job_next_id++;
    j->pgid  = pgid;
    j->state = JOB_RUNNING;
    j->exit_status = 0;
    strncpy(j->name, name ? name : "?", sizeof(j->name) - 1);
}

void job_remove(int id) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].id == id) {
            memmove(&jobs[i], &jobs[i+1], sizeof(Job) * (njobs - i - 1));
            njobs--;
            return;
        }
    }
}

void job_reap_all(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (int i = 0; i < njobs; i++) {
            if (jobs[i].pgid == pid || jobs[i].pgid == -pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    jobs[i].state = JOB_DONE;
                    jobs[i].exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
                    printf("[%d]+ Done     %s\n", jobs[i].id, jobs[i].name);
                    job_remove(jobs[i].id);
                    i--;
                } else if (WIFSTOPPED(status)) {
                    jobs[i].state = JOB_STOPPED;
                    printf("[%d]+ Stopped  %s\n", jobs[i].id, jobs[i].name);
                }
                break;
            }
        }
    }
}

void job_list(void) {
    for (int i = 0; i < njobs; i++) {
        const char *state = jobs[i].state == JOB_RUNNING ? "Running" :
                            jobs[i].state == JOB_STOPPED ? "Stopped" : "Done";
        printf("[%d]%c %s\t%s\n", jobs[i].id,
               i == njobs - 1 ? '+' : ' ',
               state, jobs[i].name);
    }
}

int job_count(void) { return njobs; }

Job *job_find_pgid(pid_t pgid) {
    for (int i = 0; i < njobs; i++)
        if (jobs[i].pgid == pgid) return &jobs[i];
    return NULL;
}

int job_fg(int id) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].id == id) {
            printf("%s\n", jobs[i].name);
            jobs[i].state = JOB_RUNNING;
            tcsetpgrp(STDIN_FILENO, jobs[i].pgid);
            kill(-jobs[i].pgid, SIGCONT);
            int status;
            waitpid(-jobs[i].pgid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if (WIFSTOPPED(status)) {
                jobs[i].state = JOB_STOPPED;
                printf("\n[%d]+ Stopped  %s\n", jobs[i].id, jobs[i].name);
            } else {
                job_remove(id);
            }
            return 0;
        }
    }
    fprintf(stderr, "fg: %%  %d: no such job\n", id);
    return 1;
}

int job_bg(int id) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].id == id) {
            jobs[i].state = JOB_RUNNING;
            kill(-jobs[i].pgid, SIGCONT);
            printf("[%d]+ %s &\n", jobs[i].id, jobs[i].name);
            return 0;
        }
    }
    fprintf(stderr, "bg: %%  %d: no such job\n", id);
    return 1;
}
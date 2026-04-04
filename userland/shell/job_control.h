#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
} JobState;

typedef struct {
    int      id;
    pid_t    pgid;
    char     name[256];
    JobState state;
    int      exit_status;
} Job;

void  job_init(void);
void  job_add(pid_t pgid, const char *name);
void  job_remove(int id);
void  job_reap_all(void);
void  job_list(void);
int   job_count(void);
Job  *job_find_pgid(pid_t pgid);
int   job_fg(int id);
int   job_bg(int id);

#endif
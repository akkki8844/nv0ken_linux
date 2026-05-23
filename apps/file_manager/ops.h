#ifndef FILE_MANAGER_OPS_H
#define FILE_MANAGER_OPS_H

#include "../../../libnv0/include/nv0/fs.h"

typedef enum {
    OPS_STATE_IDLE,
    OPS_STATE_RUNNING,
    OPS_STATE_DONE,
    OPS_STATE_ERROR,
} OpsState;

typedef struct OpsProgress {
    OpsState state;
    long long bytes_total;
    long long bytes_done;
    int percent;
    char *current_src;
    char *current_dst;
    char *error_msg;
} OpsProgress;

typedef void (*OpsCompleteFn)(int status, OpsProgress *progress);

OpsProgress *ops_progress_new(void);
void         ops_progress_free(OpsProgress *progress);

long long ops_dir_size(const char *path);
int       ops_copy(const char *src, const char *dst,
                   OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_move(const char *src, const char *dst,
                   OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_delete(const char *path,
                     OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_rename(const char *src, const char *new_name,
                     OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_mkdir(const char *path,
                    OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_mkdir_p(const char *path);
int       ops_symlink(const char *target, const char *link_path,
                      OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_chmod(const char *path, int mode,
                    OpsProgress *progress, OpsCompleteFn on_complete);
int       ops_exists(const char *path);
int       ops_is_dir(const char *path);
long long ops_file_size(const char *path);

#endif

#include "ops.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define COPY_BUF_SIZE   65536
#define MAX_PATH_LEN    1024

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* -----------------------------------------------------------------------
 * OpsProgress
 * --------------------------------------------------------------------- */

OpsProgress *ops_progress_new(void) {
    OpsProgress *p = xmalloc(sizeof(OpsProgress));
    if (!p) return NULL;
    memset(p, 0, sizeof(OpsProgress));
    p->state = OPS_STATE_IDLE;
    return p;
}

void ops_progress_free(OpsProgress *p) {
    if (!p) return;
    free(p->current_src);
    free(p->current_dst);
    free(p->error_msg);
    free(p);
}

static void progress_set_files(OpsProgress *p,
                                const char *src, const char *dst) {
    if (!p) return;
    free(p->current_src);
    free(p->current_dst);
    p->current_src = xstrdup(src);
    p->current_dst = xstrdup(dst);
}

static void progress_set_error(OpsProgress *p, const char *msg) {
    if (!p) return;
    free(p->error_msg);
    p->error_msg = xstrdup(msg);
    p->state     = OPS_STATE_ERROR;
}

/* -----------------------------------------------------------------------
 * Recursive directory size
 * --------------------------------------------------------------------- */

long long ops_dir_size(const char *path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;

    if (!S_ISDIR(st.st_mode)) return (long long)st.st_size;

    long long total = 0;
    NvDir *dir = nv_opendir(path);
    if (!dir) return 0;

    const char *entry;
    while ((entry = nv_readdir(dir)) != NULL) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        char child[MAX_PATH_LEN];
        snprintf(child, sizeof(child), "%s/%s", path, entry);
        total += ops_dir_size(child);
    }
    nv_closedir(dir);
    return total;
}

/* -----------------------------------------------------------------------
 * Single file copy
 * --------------------------------------------------------------------- */

static int copy_file(const char *src, const char *dst,
                     OpsProgress *prog) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        if (prog) progress_set_error(prog, "cannot open source");
        return -1;
    }

    struct stat st;
    fstat(src_fd, &st);

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dst_fd < 0) {
        close(src_fd);
        if (prog) progress_set_error(prog, "cannot open destination");
        return -1;
    }

    char *buf = xmalloc(COPY_BUF_SIZE);
    if (!buf) {
        close(src_fd);
        close(dst_fd);
        if (prog) progress_set_error(prog, "out of memory");
        return -1;
    }

    long long copied = 0;
    int ret = 0;

    while (1) {
        ssize_t n = read(src_fd, buf, COPY_BUF_SIZE);
        if (n == 0) break;
        if (n < 0) { ret = -1; break; }

        ssize_t w = 0;
        while (w < n) {
            ssize_t written = write(dst_fd, buf + w, n - w);
            if (written < 0) { ret = -1; goto done; }
            w += written;
        }

        copied += n;
        if (prog) {
            prog->bytes_done += n;
            if (prog->bytes_total > 0)
                prog->percent = (int)(prog->bytes_done * 100 / prog->bytes_total);
        }
    }

done:
    free(buf);
    close(src_fd);
    close(dst_fd);

    if (ret < 0 && prog)
        progress_set_error(prog, "write error during copy");

    return ret;
}

/* -----------------------------------------------------------------------
 * Recursive copy
 * --------------------------------------------------------------------- */

static int copy_recursive(const char *src, const char *dst,
                           OpsProgress *prog) {
    struct stat st;
    if (stat(src, &st) != 0) {
        if (prog) progress_set_error(prog, "stat failed");
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST) {
            if (prog) progress_set_error(prog, "mkdir failed");
            return -1;
        }

        NvDir *dir = nv_opendir(src);
        if (!dir) {
            if (prog) progress_set_error(prog, "opendir failed");
            return -1;
        }

        const char *entry;
        int ret = 0;
        while ((entry = nv_readdir(dir)) != NULL) {
            if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;

            char child_src[MAX_PATH_LEN];
            char child_dst[MAX_PATH_LEN];
            snprintf(child_src, sizeof(child_src), "%s/%s", src, entry);
            snprintf(child_dst, sizeof(child_dst), "%s/%s", dst, entry);

            if (prog) progress_set_files(prog, child_src, child_dst);
            if (copy_recursive(child_src, child_dst, prog) < 0) {
                ret = -1;
                break;
            }
        }
        nv_closedir(dir);
        return ret;
    }

    if (S_ISLNK(st.st_mode)) {
        char link_target[MAX_PATH_LEN];
        ssize_t n = readlink(src, link_target, sizeof(link_target) - 1);
        if (n < 0) return -1;
        link_target[n] = '\0';
        return symlink(link_target, dst);
    }

    if (prog) progress_set_files(prog, src, dst);
    return copy_file(src, dst, prog);
}

/* -----------------------------------------------------------------------
 * Recursive delete
 * --------------------------------------------------------------------- */

static int delete_recursive(const char *path, OpsProgress *prog) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;

    if (!S_ISDIR(st.st_mode)) {
        if (prog) progress_set_files(prog, path, NULL);
        return unlink(path);
    }

    NvDir *dir = nv_opendir(path);
    if (!dir) return -1;

    const char *entry;
    int ret = 0;
    while ((entry = nv_readdir(dir)) != NULL) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        char child[MAX_PATH_LEN];
        snprintf(child, sizeof(child), "%s/%s", path, entry);
        if (prog) progress_set_files(prog, child, NULL);
        if (delete_recursive(child, prog) < 0) {
            ret = -1;
            break;
        }
    }
    nv_closedir(dir);

    if (ret == 0) ret = rmdir(path);
    return ret;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

int ops_copy(const char *src, const char *dst,
             OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!src || !dst) return -1;

    if (prog) {
        prog->state        = OPS_STATE_RUNNING;
        prog->bytes_total  = ops_dir_size(src);
        prog->bytes_done   = 0;
        prog->percent      = 0;
        progress_set_files(prog, src, dst);
    }

    int ret = copy_recursive(src, dst, prog);

    if (prog && ret == 0)
        prog->state = OPS_STATE_DONE;

    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_move(const char *src, const char *dst,
             OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!src || !dst) return -1;

    if (prog) {
        prog->state = OPS_STATE_RUNNING;
        progress_set_files(prog, src, dst);
    }

    int ret = rename(src, dst);

    if (ret < 0 && errno == EXDEV) {
        ret = ops_copy(src, dst, prog, NULL);
        if (ret == 0)
            ret = delete_recursive(src, prog);
    }

    if (prog) prog->state = ret == 0 ? OPS_STATE_DONE : OPS_STATE_ERROR;
    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_delete(const char *path,
               OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!path) return -1;

    if (prog) {
        prog->state       = OPS_STATE_RUNNING;
        prog->bytes_total = ops_dir_size(path);
        prog->bytes_done  = 0;
        progress_set_files(prog, path, NULL);
    }

    int ret = delete_recursive(path, prog);

    if (prog) prog->state = ret == 0 ? OPS_STATE_DONE : OPS_STATE_ERROR;
    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_rename(const char *src, const char *new_name,
               OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!src || !new_name) return -1;

    char dst[MAX_PATH_LEN];
    const char *last_slash = strrchr(src, '/');
    if (last_slash) {
        size_t dir_len = last_slash - src + 1;
        if (dir_len + strlen(new_name) >= MAX_PATH_LEN) return -1;
        memcpy(dst, src, dir_len);
        strcpy(dst + dir_len, new_name);
    } else {
        strncpy(dst, new_name, MAX_PATH_LEN - 1);
        dst[MAX_PATH_LEN - 1] = '\0';
    }

    return ops_move(src, dst, prog, on_complete);
}

int ops_mkdir(const char *path, OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!path) return -1;
    int ret = mkdir(path, 0755);
    if (prog) prog->state = ret == 0 ? OPS_STATE_DONE : OPS_STATE_ERROR;
    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_mkdir_p(const char *path) {
    if (!path || !path[0]) return -1;

    char buf[MAX_PATH_LEN];
    strncpy(buf, path, MAX_PATH_LEN - 1);
    buf[MAX_PATH_LEN - 1] = '\0';

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755);
            *p = '/';
        }
    }
    return mkdir(buf, 0755);
}

int ops_symlink(const char *target, const char *link_path,
                OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!target || !link_path) return -1;
    int ret = symlink(target, link_path);
    if (prog) prog->state = ret == 0 ? OPS_STATE_DONE : OPS_STATE_ERROR;
    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_chmod(const char *path, int mode,
              OpsProgress *prog, OpsCompleteFn on_complete) {
    if (!path) return -1;
    int ret = chmod(path, (mode_t)mode);
    if (prog) prog->state = ret == 0 ? OPS_STATE_DONE : OPS_STATE_ERROR;
    if (on_complete) on_complete(ret, prog);
    return ret;
}

int ops_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int ops_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

long long ops_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}
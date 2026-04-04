# VFS interface

## overview

the virtual filesystem layer (`kernel/fs/vfs.c`) defines a uniform interface that all concrete filesystems implement. no code outside `fs/` calls ext2 or tmpfs functions directly — everything goes through VFS ops tables.

## core types

```c
typedef struct vnode    vnode_t;
typedef struct dentry   dentry_t;
typedef struct file     file_t;
typedef struct mount    mount_t;
typedef struct vfs_ops  vfs_ops_t;
typedef struct file_ops file_ops_t;
```

### vnode_t

represents a file or directory in any filesystem.

```c
struct vnode {
    uint64_t     ino;
    uint32_t     mode;
    uint32_t     uid;
    uint32_t     gid;
    uint64_t     size;
    uint64_t     mtime;
    uint64_t     ctime;
    int          type;
    mount_t     *mount;
    file_ops_t  *ops;
    void        *fs_data;
    int          refcount;
};
```

### file_ops_t — what each filesystem must implement

```c
struct file_ops {
    int      (*open)   (vnode_t *vn, file_t *f, int flags);
    int      (*close)  (file_t *f);
    ssize_t  (*read)   (file_t *f, void *buf, size_t len, off_t off);
    ssize_t  (*write)  (file_t *f, const void *buf, size_t len, off_t off);
    int      (*readdir)(file_t *f, struct dirent *de, int idx);
    int      (*stat)   (vnode_t *vn, struct stat *st);
    int      (*create) (vnode_t *dir, const char *name, int mode, vnode_t **out);
    int      (*mkdir)  (vnode_t *dir, const char *name, int mode);
    int      (*unlink) (vnode_t *dir, const char *name);
    int      (*rename) (vnode_t *old_dir, const char *old_name,
                        vnode_t *new_dir, const char *new_name);
    int      (*truncate)(vnode_t *vn, off_t len);
    int      (*lookup) (vnode_t *dir, const char *name, vnode_t **out);
    int      (*symlink)(vnode_t *dir, const char *name, const char *target);
    int      (*readlink)(vnode_t *vn, char *buf, size_t len);
    int      (*chmod)  (vnode_t *vn, int mode);
    int      (*chown)  (vnode_t *vn, int uid, int gid);
    void     (*release)(vnode_t *vn);
};
```

every function may return a negative errno on failure. returning `-ENOSYS` is valid for operations the filesystem does not support.

### vfs_ops_t — filesystem-level operations

```c
struct vfs_ops {
    const char *name;
    int (*mount)  (mount_t *mnt, const char *device, int flags);
    int (*unmount)(mount_t *mnt);
    int (*sync)   (mount_t *mnt);
    int (*statfs) (mount_t *mnt, struct statfs *buf);
};
```

## registering a filesystem

```c
#include "kernel/fs/vfs.h"

void vfs_register(vfs_ops_t *ops);
```

call this once during kernel init (or driver init). the name field must be unique (e.g. `"ext2"`, `"tmpfs"`, `"initrd"`).

## mounting

```c
int vfs_mount(const char *device, const char *target,
              const char *fstype, int flags);
int vfs_unmount(const char *target);
```

`vfs_mount` finds the registered `vfs_ops_t` by `fstype`, allocates a `mount_t`, calls `ops->mount`, and inserts into the mount table. `target` must be an existing directory in the VFS tree.

## path resolution

```c
int vfs_lookup(const char *path, vnode_t **out);
int vfs_lookup_at(vnode_t *base, const char *path, vnode_t **out);
```

`vfs_lookup` resolves from the root. `vfs_lookup_at` resolves relative to `base`. both handle `.` and `..` traversal, symlink following (up to 8 hops), and mount-point crossing.

## file descriptor table

each process has a `fd_table_t` with up to 256 open file descriptors. a file descriptor is an index into this table mapping to a `file_t *`. `file_t` holds the vnode pointer, current offset, open flags, and reference count.

```c
int  fd_alloc(process_t *proc, file_t *f);
void fd_free(process_t *proc, int fd);
file_t *fd_get(process_t *proc, int fd);
```

## dentry cache

the dentry cache stores recently resolved path components as `(parent_vnode, name) → child_vnode` mappings. it is a fixed-size open-addressing hashmap. a cache miss falls through to `ops->lookup`.

## implementing a new filesystem

1. define a `vfs_ops_t` with your mount/unmount/sync/statfs
2. define a `file_ops_t` with all required ops
3. in your `mount` function, allocate your superblock, read it from the device, and set `mnt->root_vnode`
4. every vnode you create must set `vn->ops = &your_file_ops`
5. call `vfs_register(&your_vfs_ops)` during init

minimum viable implementation requires: `lookup`, `open`, `read`, `readdir`, `stat`, `close`, `release`.
/*
 * Declares the operating system abstraction used by GEM for lifecycle,
 * memory allocation, timing, sleeping, and simple file descriptor based
 * I/O on the host platform.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_OS_H
#define GEM_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEM_OS_PATH_MAX 512

enum {
    GEM_OS_ACCESS_EXISTS = 0,
    GEM_OS_ACCESS_EXECUTE = 1,
    GEM_OS_ACCESS_READ = 2
};

typedef int gem_os_pid_t;

typedef struct gem_os_mutex {
    void *handle;
} gem_os_mutex_t;

typedef struct gem_os_file_info {
    uint64_t size_bytes;
    uint64_t mtime_ms;
    int is_directory;
    int is_executable;
    int is_symlink;
    int is_hidden;
    int is_read_only;
} gem_os_file_info_t;

typedef struct gem_os_dir {
    void *handle;
    char path[GEM_OS_PATH_MAX];
} gem_os_dir_t;

typedef struct gem_os_dirent {
    char name[GEM_OS_PATH_MAX];
    gem_os_file_info_t info;
} gem_os_dirent_t;

typedef struct gem_os_volume {
    char mount_path[GEM_OS_PATH_MAX];
    char label[GEM_OS_PATH_MAX];
} gem_os_volume_t;

typedef struct gem_os_volume_iter {
    void *handle;
} gem_os_volume_iter_t;

typedef struct gem_os_mount_iter {
    void *handle;
} gem_os_mount_iter_t;

typedef struct gem_os_pty {
    int master_fd;
    int child_pid;
} gem_os_pty_t;

/* Initializes a host mutex. Returns nonzero on success. */
int gem_os_mutex_init(gem_os_mutex_t *mutex);

/* Locks a mutex initialized by `gem_os_mutex_init`. */
void gem_os_mutex_lock(gem_os_mutex_t *mutex);

/* Unlocks a mutex initialized by `gem_os_mutex_init`. */
void gem_os_mutex_unlock(gem_os_mutex_t *mutex);

/* Destroys a host mutex and clears its wrapper. */
void gem_os_mutex_destroy(gem_os_mutex_t *mutex);

/* Returns the current host process identifier. */
gem_os_pid_t gem_os_getpid(void);

/* Returns the numeric identity of the current host user. */
uint32_t gem_os_getuid(void);

/*
 * Copies environment variable `name` into `dst`. A trailing '=' in `name`
 * is accepted for classic GEM compatibility. Returns nonzero when found.
 */
int gem_os_getenv(const char *name, char *dst, size_t dst_size);

/*
 * Returns an OS-owned pointer to environment variable `name`, or NULL.
 * A trailing '=' in `name` is accepted.
 */
const char *gem_os_getenv_ref(const char *name);

/*
 * Copies environment variable `name` from process `pid` into `dst`.
 * Returns nonzero when that process exposes the variable.
 */
int gem_os_process_getenv(gem_os_pid_t pid, const char *name, char *dst,
                          size_t dst_size);

/* Tests a path using one of the GEM_OS_ACCESS_* modes. */
int gem_os_access(const char *path, int mode);

/*
 * Resolves a readable file through the GEMix search order and writes its
 * absolute path to `out`. Returns nonzero on success.
 */
int gem_os_find_file(const char *name, char *out, size_t out_size);

/* Resolves an executable through the GEMix search order. */
int gem_os_find_executable(const char *name, char *out, size_t out_size);

/* Resolves an accessory name using the platform's accessory search path. */
int gem_os_find_accessory(const char *name, char *out, size_t out_size);

/* Resolves a startup program from the platform's AUTO search path. */
int gem_os_find_auto_program(const char *name, char *out, size_t out_size);

/* Returns the platform's per-user configuration directory. */
int gem_os_user_config_dir(char *out, size_t out_size);

/* Returns the platform's ordered system configuration directories. */
int gem_os_system_config_dirs(char *out, size_t out_size);

/* Returns the platform's per-user data directory. */
int gem_os_user_data_dir(char *out, size_t out_size);

/*
 * Starts a GEM child with a TOS-tail payload and child-only launch metadata.
 * `appl_id` is the AES id reserved before exec. Returns nonzero on success.
 */
int gem_os_spawn_gem(const char *path, const char *cmd, const void *tail,
                     size_t tail_len, int appl_id, gem_os_pid_t *out_pid);

/*
 * Retrieves launch command and TOS-tail bytes for `pid` from child metadata
 * or the host process command line. Returns nonzero on success.
 */
int gem_os_process_launch(gem_os_pid_t pid, char *cmd, size_t cmd_size,
                          void *tail, size_t tail_size, size_t *tail_len);

/*
 * Reaps `pid` without blocking. Returns 1 when reaped, 0 while running and
 * -1 when it is not a child or another error occurs.
 */
int gem_os_reap_process(gem_os_pid_t pid);

/*
 * Initializes operating system services required by the GEM runtime.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_init(void);

/*
 * Shuts down operating system services initialized by `gem_os_init()`.
 */
void gem_os_shutdown(void);

/*
 * Allocates `size` bytes of host memory.
 * Returns a pointer to the allocated block, or `NULL` on failure.
 */
void *gem_os_alloc(size_t size);

/*
 * Releases a block previously returned by `gem_os_alloc()`.
 * `ptr` may be `NULL`.
 */
void gem_os_free(void *ptr);

/*
 * Returns a monotonically increasing millisecond tick count.
 */
uint32_t gem_os_ticks_ms(void);

/*
 * Suspends the current thread for approximately `ms` milliseconds.
 */
void gem_os_sleep_ms(uint32_t ms);

/*
 * Opens `path` for reading and returns a file descriptor, or a negative
 * value on failure.
 */
int gem_os_open_read(const char *path);

/*
 * Opens `path` for writing and returns a file descriptor, or a negative
 * value on failure.
 */
int gem_os_open_write(const char *path);

/*
 * Opens an existing `path` with GEMDOS-compatible access mode: zero for
 * read-only, one for write-only, or two for read/write. The file is never
 * created or truncated. Returns a file descriptor, or a negative value.
 */
int gem_os_open_existing(const char *path, int mode);

/*
 * Closes the file descriptor `fd`.
 * Returns zero on success, or a negative value on failure.
 */
int gem_os_close(int fd);

/*
 * Reads up to `size` bytes from `fd` into `buf`.
 * Returns the number of bytes read, or a negative value on failure.
 */
int32_t gem_os_read(int fd, void *buf, uint32_t size);

/*
 * Writes up to `size` bytes from `buf` to `fd`.
 * Returns the number of bytes written, or a negative value on failure.
 */
int32_t gem_os_write(int fd, const void *buf, uint32_t size);

/*
 * Seeks `fd` to `offset` according to `whence` and returns the resulting
 * absolute byte position, or a negative value on failure.
 * `whence` matches POSIX semantics: 0 = start, 1 = current, 2 = end.
 */
int64_t gem_os_seek(int fd, int64_t offset, int whence);

/*
 * Returns the current working directory in `buf`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_getcwd(char *buf, size_t size);

/*
 * Changes the current working directory to `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_chdir(const char *path);

/*
 * Creates a directory at `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_mkdir(const char *path);

/*
 * Creates `path` and all missing parents. Newly created directories are
 * private to the current user. Returns nonzero on success.
 */
int gem_os_mkdir_p(const char *path);

/*
 * Removes an empty directory at `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_rmdir(const char *path);

/*
 * Deletes the file at `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_unlink(const char *path);

/*
 * Renames `old_path` to `new_path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_rename(const char *old_path, const char *new_path);

/*
 * Copies one file, symbolic link or directory tree to `new_path` without
 * following symbolic links. The destination must not exist. Returns nonzero
 * on success and removes a partial destination after failure.
 */
int gem_os_copy_path(const char *old_path, const char *new_path);

/*
 * Moves one file, symbolic link or directory tree to `new_path`. It uses an
 * atomic rename when possible and a no-follow copy/remove fallback across
 * filesystems. The destination must not exist. Returns nonzero on success.
 */
int gem_os_move_path(const char *old_path, const char *new_path);

/*
 * Reads a complete small file into `buf`, NUL-terminates it and optionally
 * returns its byte count. Returns nonzero when the contents fit.
 */
int gem_os_read_file(const char *path, char *buf, size_t buf_size,
                     size_t *size_out);

/*
 * Atomically replaces `path` with `size` bytes from `data`, using a private
 * temporary file in the same directory. Returns nonzero on success.
 */
int gem_os_write_file_atomic(const char *path, const void *data, size_t size);

/*
 * Populates `info` with host metadata for `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_stat_path(const char *path, gem_os_file_info_t *info);

/* Populates info without following a final symbolic link. */
int gem_os_lstat_path(const char *path, gem_os_file_info_t *info);

/* Returns nonzero when path names an entry, including a dangling symlink. */
int gem_os_path_exists(const char *path);

/* Returns nonzero when existing `path` and `other` are on one filesystem. */
int gem_os_same_device(const char *path, const char *other);

/*
 * Finds the mount point containing existing `path` and copies it to `out`.
 * Returns nonzero when the path and mount point can be resolved.
 */
int gem_os_mount_for_path(const char *path, char *out, size_t out_size);

/*
 * Resolves existing `path` and verifies that it is strictly below or equal to
 * existing `root`. The canonical path is copied to `out` on success.
 */
int gem_os_resolve_under(const char *path, const char *root, char *out,
                         size_t out_size);

/*
 * Resolves and validates path's parent beneath root without following the
 * final entry. The normalized entry path is copied to out.
 */
int gem_os_resolve_entry_under(const char *path, const char *root, char *out,
                               size_t out_size);

/*
 * Recursively removes `path` without following symbolic links. The resolved
 * target must be strictly below `root`; deleting `root` itself is refused.
 */
int gem_os_remove_tree_under(const char *path, const char *root);

/* Writes the current local time as `YYYY-MM-DDThh:mm:ss`. */
int gem_os_local_timestamp(char *out, size_t out_size);

/* Formats seconds since the host epoch in local time using format. */
int gem_os_format_timestamp(int64_t seconds, const char *format, char *out,
                            size_t out_size);

/*
 * Opens a directory stream rooted at `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_dir_open(const char *path, gem_os_dir_t *dir);

/*
 * Reads the next entry from `dir` into `entry`.
 * Returns non-zero when an entry is produced, or zero on end/failure.
 */
int gem_os_dir_read(gem_os_dir_t *dir, gem_os_dirent_t *entry);

/*
 * Closes a directory stream previously opened by `gem_os_dir_open()`.
 */
void gem_os_dir_close(gem_os_dir_t *dir);

/*
 * Returns filesystem capacity and available free space for `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_space(const char *path, uint64_t *total_bytes,
                 uint64_t *avail_bytes);

/*
 * Updates the read-only bit for `path`.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_set_read_only(const char *path, int read_only);

/*
 * Opens an iterator over mounted host volumes.
 * Returns non-zero on success and zero on failure.
 */
int gem_os_volume_iter_open(gem_os_volume_iter_t *iter);

/*
 * Reads the next mounted host volume into `volume`.
 * Returns non-zero when a volume is produced, or zero on end/failure.
 */
int gem_os_volume_iter_read(gem_os_volume_iter_t *iter,
                            gem_os_volume_t *volume);

/*
 * Closes a mounted-volume iterator previously opened by
 * `gem_os_volume_iter_open()`.
 */
void gem_os_volume_iter_close(gem_os_volume_iter_t *iter);

/* Opens an iterator over all host mount points for filesystem services. */
int gem_os_mount_iter_open(gem_os_mount_iter_t *iter);

/* Reads the next absolute mount path into path. */
int gem_os_mount_iter_read(gem_os_mount_iter_t *iter, char *path,
                           size_t path_size);

/* Closes an iterator opened by gem_os_mount_iter_open. */
void gem_os_mount_iter_close(gem_os_mount_iter_t *iter);

/* Starts a shell attached to a new PTY with the requested size and cwd.
 * Returns nonzero on success; pty owns the child and descriptor until close. */
int gem_os_pty_spawn_shell(gem_os_pty_t *pty, const char *shell_path,
                           const char *cwd, uint16_t columns, uint16_t rows);
/* Sets the PTY terminal size; returns nonzero on success. */
int gem_os_pty_resize(gem_os_pty_t *pty, uint16_t columns, uint16_t rows);
/* Reads up to size bytes into buf; returns bytes read or a negative error. */
int32_t gem_os_pty_read(gem_os_pty_t *pty, void *buf, uint32_t size);
/* Writes up to size bytes from buf; returns bytes written or a negative error.
 */
int32_t gem_os_pty_write(gem_os_pty_t *pty, const void *buf, uint32_t size);
/* Reaps an exited PTY child and returns nonzero while it is running. */
int gem_os_pty_is_alive(gem_os_pty_t *pty);
/* Closes the PTY and terminates/reaps its owned shell process. */
void gem_os_pty_close(gem_os_pty_t *pty);

#ifdef __cplusplus
}
#endif

#endif

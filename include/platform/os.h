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

typedef struct gem_os_file_info {
    uint64_t size_bytes;
    uint64_t mtime_ms;
    int is_directory;
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

typedef struct gem_os_pty {
    int master_fd;
    int child_pid;
} gem_os_pty_t;

/*
 * Initializes operating system services required by the GEM runtime.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_init(void);

/*
 * Shuts down operating system services initialized by `gem_os_init()`.
 */
void     gem_os_shutdown(void);

/*
 * Allocates `size` bytes of host memory.
 * Returns a pointer to the allocated block, or `NULL` on failure.
 */
void    *gem_os_alloc(size_t size);

/*
 * Releases a block previously returned by `gem_os_alloc()`.
 * `ptr` may be `NULL`.
 */
void     gem_os_free(void *ptr);

/*
 * Returns a monotonically increasing millisecond tick count.
 */
uint32_t gem_os_ticks_ms(void);

/*
 * Suspends the current thread for approximately `ms` milliseconds.
 */
void     gem_os_sleep_ms(uint32_t ms);

/*
 * Opens `path` for reading and returns a file descriptor, or a negative
 * value on failure.
 */
int      gem_os_open_read(const char *path);

/*
 * Opens `path` for writing and returns a file descriptor, or a negative
 * value on failure.
 */
int      gem_os_open_write(const char *path);

/*
 * Closes the file descriptor `fd`.
 * Returns zero on success, or a negative value on failure.
 */
int      gem_os_close(int fd);

/*
 * Reads up to `size` bytes from `fd` into `buf`.
 * Returns the number of bytes read, or a negative value on failure.
 */
int32_t  gem_os_read(int fd, void *buf, uint32_t size);

/*
 * Writes up to `size` bytes from `buf` to `fd`.
 * Returns the number of bytes written, or a negative value on failure.
 */
int32_t  gem_os_write(int fd, const void *buf, uint32_t size);

/*
 * Seeks `fd` to `offset` according to `whence` and returns the resulting
 * absolute byte position, or a negative value on failure.
 * `whence` matches POSIX semantics: 0 = start, 1 = current, 2 = end.
 */
int64_t  gem_os_seek(int fd, int64_t offset, int whence);

/*
 * Returns the current working directory in `buf`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_getcwd(char *buf, size_t size);

/*
 * Changes the current working directory to `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_chdir(const char *path);

/*
 * Creates a directory at `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_mkdir(const char *path);

/*
 * Removes an empty directory at `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_rmdir(const char *path);

/*
 * Deletes the file at `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_unlink(const char *path);

/*
 * Renames `old_path` to `new_path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_rename(const char *old_path, const char *new_path);

/*
 * Populates `info` with host metadata for `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_stat_path(const char *path, gem_os_file_info_t *info);

/*
 * Opens a directory stream rooted at `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_dir_open(const char *path, gem_os_dir_t *dir);

/*
 * Reads the next entry from `dir` into `entry`.
 * Returns non-zero when an entry is produced, or zero on end/failure.
 */
int      gem_os_dir_read(gem_os_dir_t *dir, gem_os_dirent_t *entry);

/*
 * Closes a directory stream previously opened by `gem_os_dir_open()`.
 */
void     gem_os_dir_close(gem_os_dir_t *dir);

/*
 * Returns filesystem capacity and available free space for `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_space(const char *path, uint64_t *total_bytes,
                      uint64_t *avail_bytes);

/*
 * Updates the read-only bit for `path`.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_set_read_only(const char *path, int read_only);

/*
 * Opens an iterator over mounted host volumes.
 * Returns non-zero on success and zero on failure.
 */
int      gem_os_volume_iter_open(gem_os_volume_iter_t *iter);

/*
 * Reads the next mounted host volume into `volume`.
 * Returns non-zero when a volume is produced, or zero on end/failure.
 */
int      gem_os_volume_iter_read(gem_os_volume_iter_t *iter,
                                 gem_os_volume_t *volume);

/*
 * Closes a mounted-volume iterator previously opened by
 * `gem_os_volume_iter_open()`.
 */
void     gem_os_volume_iter_close(gem_os_volume_iter_t *iter);

int      gem_os_pty_spawn_shell(gem_os_pty_t *pty,
                                const char *shell_path,
                                const char *cwd,
                                uint16_t columns,
                                uint16_t rows);
int      gem_os_pty_resize(gem_os_pty_t *pty, uint16_t columns, uint16_t rows);
int32_t  gem_os_pty_read(gem_os_pty_t *pty, void *buf, uint32_t size);
int32_t  gem_os_pty_write(gem_os_pty_t *pty, const void *buf, uint32_t size);
int      gem_os_pty_is_alive(gem_os_pty_t *pty);
void     gem_os_pty_close(gem_os_pty_t *pty);

#ifdef __cplusplus
}
#endif

#endif

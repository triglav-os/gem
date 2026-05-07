/*
 * Implements the GEM operating system abstraction for Linux. The
 * backend provides allocation, timing, sleeping, and simple POSIX file
 * descriptor wrappers used by higher GEM layers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "platform/os.h"

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int gem_os_init(void)
{
    return 1;
}

void gem_os_shutdown(void)
{
}

void *gem_os_alloc(size_t size)
{
    return malloc(size);
}

void gem_os_free(void *ptr)
{
    free(ptr);
}

uint32_t gem_os_ticks_ms(void)
{
    struct timespec ts;
    uint64_t ms;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0u;
    }

    ms = (uint64_t) ts.tv_sec * 1000u;
    ms += (uint64_t) ts.tv_nsec / 1000000u;
    return (uint32_t) (ms & 0xffffffffu);
}

void gem_os_sleep_ms(uint32_t ms)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = (time_t) (ms / 1000u);
    req.tv_nsec = (long) (ms % 1000u) * 1000000l;

    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) {
            return;
        }
        req = rem;
    }
}

int gem_os_open_read(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    return open(path, O_RDONLY);
}

int gem_os_open_write(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

int gem_os_close(int fd)
{
    return close(fd);
}

int32_t gem_os_read(int fd, void *buf, uint32_t size)
{
    ssize_t rc;
    size_t count = size;

    if (buf == NULL && size != 0u) {
        errno = EINVAL;
        return -1;
    }
    if (count > 0x7fffffffu) {
        count = 0x7fffffffu;
    }

    rc = read(fd, buf, count);
    if (rc < 0) {
        return -1;
    }

    return (int32_t) rc;
}

int32_t gem_os_write(int fd, const void *buf, uint32_t size)
{
    ssize_t rc;
    size_t count = size;

    if (buf == NULL && size != 0u) {
        errno = EINVAL;
        return -1;
    }
    if (count > 0x7fffffffu) {
        count = 0x7fffffffu;
    }

    rc = write(fd, buf, count);
    if (rc < 0) {
        return -1;
    }

    return (int32_t) rc;
}

int64_t gem_os_seek(int fd, int64_t offset, int whence)
{
    int seek_whence;
    off_t rc;

    switch (whence) {
    case 0:
        seek_whence = SEEK_SET;
        break;
    case 1:
        seek_whence = SEEK_CUR;
        break;
    case 2:
        seek_whence = SEEK_END;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    rc = lseek(fd, (off_t) offset, seek_whence);
    if (rc < 0) {
        return -1;
    }
    return (int64_t) rc;
}

int gem_os_getcwd(char *buf, size_t size)
{
    if (buf == NULL || size == 0u) {
        errno = EINVAL;
        return 0;
    }
    return (getcwd(buf, size) != NULL) ? 1 : 0;
}

int gem_os_chdir(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    return (chdir(path) == 0) ? 1 : 0;
}

int gem_os_mkdir(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    return (mkdir(path, 0755) == 0) ? 1 : 0;
}

int gem_os_rmdir(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    return (rmdir(path) == 0) ? 1 : 0;
}

int gem_os_unlink(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    return (unlink(path) == 0) ? 1 : 0;
}

int gem_os_rename(const char *old_path, const char *new_path)
{
    if (old_path == NULL || new_path == NULL) {
        errno = EINVAL;
        return 0;
    }
    return (rename(old_path, new_path) == 0) ? 1 : 0;
}

static int gem_os_fill_info_from_stat(const char *name,
                                      const struct stat *st,
                                      gem_os_file_info_t *info)
{
    if (st == NULL || info == NULL) {
        errno = EINVAL;
        return 0;
    }

    info->size_bytes = (uint64_t) st->st_size;
    info->mtime_ms = (uint64_t) st->st_mtim.tv_sec * 1000u +
        (uint64_t) st->st_mtim.tv_nsec / 1000000u;
    info->is_directory = S_ISDIR(st->st_mode) ? 1 : 0;
    info->is_hidden = (name != NULL && name[0] == '.') ? 1 : 0;
    info->is_read_only = ((st->st_mode & S_IWUSR) == 0) ? 1 : 0;
    return 1;
}

int gem_os_stat_path(const char *path, gem_os_file_info_t *info)
{
    struct stat st;

    if (path == NULL || info == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return gem_os_fill_info_from_stat(path, &st, info);
}

int gem_os_dir_open(const char *path, gem_os_dir_t *dir)
{
    DIR *handle;

    if (path == NULL || dir == NULL) {
        errno = EINVAL;
        return 0;
    }

    handle = opendir(path);
    if (handle == NULL) {
        return 0;
    }

    dir->handle = handle;
    strncpy(dir->path, path, sizeof(dir->path) - 1u);
    dir->path[sizeof(dir->path) - 1u] = '\0';
    return 1;
}

int gem_os_dir_read(gem_os_dir_t *dir, gem_os_dirent_t *entry)
{
    DIR *handle;
    struct dirent *dent;

    if (dir == NULL || entry == NULL || dir->handle == NULL) {
        errno = EINVAL;
        return 0;
    }

    handle = (DIR *) dir->handle;
    for (;;) {
        char full_path[PATH_MAX];
        struct stat st;
        int rc;

        errno = 0;
        dent = readdir(handle);
        if (dent == NULL) {
            return 0;
        }

        strncpy(entry->name, dent->d_name, sizeof(entry->name) - 1u);
        entry->name[sizeof(entry->name) - 1u] = '\0';

        rc = snprintf(full_path, sizeof(full_path), "%s/%s",
            dir->path, dent->d_name);
        if (rc < 0 || (size_t) rc >= sizeof(full_path)) {
            continue;
        }
        if (stat(full_path, &st) != 0) {
            continue;
        }
        if (gem_os_fill_info_from_stat(dent->d_name, &st, &entry->info) != 0) {
            return 1;
        }
    }
}

void gem_os_dir_close(gem_os_dir_t *dir)
{
    if (dir == NULL || dir->handle == NULL) {
        return;
    }

    (void) closedir((DIR *) dir->handle);
    dir->handle = NULL;
    dir->path[0] = '\0';
}

int gem_os_space(const char *path, uint64_t *total_bytes, uint64_t *avail_bytes)
{
    struct statvfs st;

    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (statvfs(path, &st) != 0) {
        return 0;
    }
    if (total_bytes != NULL) {
        *total_bytes = (uint64_t) st.f_blocks * (uint64_t) st.f_frsize;
    }
    if (avail_bytes != NULL) {
        *avail_bytes = (uint64_t) st.f_bavail * (uint64_t) st.f_frsize;
    }
    return 1;
}

int gem_os_set_read_only(const char *path, int read_only)
{
    struct stat st;
    mode_t mode;

    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }

    mode = st.st_mode;
    if (read_only != 0) {
        mode &= (mode_t) ~(S_IWUSR | S_IWGRP | S_IWOTH);
    } else {
        mode |= S_IWUSR;
    }
    return (chmod(path, mode) == 0) ? 1 : 0;
}

static int gem_os_should_expose_mount(const struct mntent *mnt)
{
    static const char *const ignored_fs[] = {
        "proc", "sysfs", "tmpfs", "devtmpfs", "devpts", "cgroup",
        "cgroup2", "overlay", "squashfs", "nsfs", "mqueue", "debugfs",
        "tracefs", "securityfs", "pstore", "autofs", "fusectl",
        "configfs", "binfmt_misc", "ramfs"
    };
    size_t i;

    if (mnt == NULL || mnt->mnt_dir == NULL || mnt->mnt_type == NULL) {
        return 0;
    }
    if (strcmp(mnt->mnt_dir, "/") == 0) {
        return 1;
    }
    if (strncmp(mnt->mnt_dir, "/snap/", 6) == 0 ||
        strncmp(mnt->mnt_dir, "/proc", 5) == 0 ||
        strncmp(mnt->mnt_dir, "/sys", 4) == 0 ||
        strncmp(mnt->mnt_dir, "/dev", 4) == 0 ||
        strncmp(mnt->mnt_dir, "/run", 4) == 0) {
        return 0;
    }
    for (i = 0; i < sizeof(ignored_fs) / sizeof(ignored_fs[0]); ++i) {
        if (strcmp(mnt->mnt_type, ignored_fs[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static void gem_os_capitalize_label(char *label, size_t size)
{
    size_t i;
    int new_word = 1;

    if (label == NULL || size == 0u) {
        return;
    }

    for (i = 0; i < size && label[i] != '\0'; ++i) {
        if (label[i] == ' ' || label[i] == '_' || label[i] == '-') {
            new_word = 1;
            continue;
        }
        if (new_word != 0 && label[i] >= 'a' && label[i] <= 'z') {
            label[i] = (char) (label[i] - ('a' - 'A'));
        } else if (new_word == 0 && label[i] >= 'A' && label[i] <= 'Z') {
            label[i] = (char) (label[i] + ('a' - 'A'));
        }
        new_word = 0;
    }
}

int gem_os_volume_iter_open(gem_os_volume_iter_t *iter)
{
    FILE *fp;

    if (iter == NULL) {
        errno = EINVAL;
        return 0;
    }

    fp = setmntent("/proc/mounts", "r");
    if (fp == NULL) {
        return 0;
    }

    iter->handle = fp;
    return 1;
}

int gem_os_volume_iter_read(gem_os_volume_iter_t *iter,
    gem_os_volume_t *volume)
{
    FILE *fp;
    struct mntent *mnt;
    const char *base;

    if (iter == NULL || volume == NULL || iter->handle == NULL) {
        errno = EINVAL;
        return 0;
    }

    fp = (FILE *) iter->handle;
    for (;;) {
        mnt = getmntent(fp);
        if (mnt == NULL) {
            return 0;
        }
        if (!gem_os_should_expose_mount(mnt)) {
            continue;
        }

        strncpy(volume->mount_path, mnt->mnt_dir,
            sizeof(volume->mount_path) - 1u);
        volume->mount_path[sizeof(volume->mount_path) - 1u] = '\0';

        if (strcmp(mnt->mnt_dir, "/") == 0) {
            strncpy(volume->label, "Root", sizeof(volume->label) - 1u);
            volume->label[sizeof(volume->label) - 1u] = '\0';
        } else {
            base = strrchr(mnt->mnt_dir, '/');
            if (base != NULL && base[1] != '\0') {
                ++base;
            } else {
                base = mnt->mnt_dir;
            }
            strncpy(volume->label, base, sizeof(volume->label) - 1u);
            volume->label[sizeof(volume->label) - 1u] = '\0';
            gem_os_capitalize_label(volume->label, sizeof(volume->label));
        }
        return 1;
    }
}

void gem_os_volume_iter_close(gem_os_volume_iter_t *iter)
{
    if (iter == NULL || iter->handle == NULL) {
        return;
    }

    (void) endmntent((FILE *) iter->handle);
    iter->handle = NULL;
}

static int gem_os_pty_apply_size(int fd, uint16_t columns, uint16_t rows)
{
    struct winsize ws;

    if (fd < 0) {
        errno = EINVAL;
        return 0;
    }

    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short) ((columns > 0u) ? columns : 80u);
    ws.ws_row = (unsigned short) ((rows > 0u) ? rows : 25u);
    return (ioctl(fd, TIOCSWINSZ, &ws) == 0) ? 1 : 0;
}

int gem_os_pty_spawn_shell(gem_os_pty_t *pty,
                           const char *shell_path,
                           const char *cwd,
                           uint16_t columns,
                           uint16_t rows)
{
    int master_fd;
    int slave_fd = -1;
    char *slave_name;
    pid_t pid;
    const char *shell;

    if (pty == NULL) {
        errno = EINVAL;
        return 0;
    }

    pty->master_fd = -1;
    pty->child_pid = -1;
    master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        return 0;
    }
    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        (void) close(master_fd);
        return 0;
    }

    slave_name = ptsname(master_fd);
    if (slave_name == NULL) {
        (void) close(master_fd);
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        (void) close(master_fd);
        return 0;
    }

    if (pid == 0) {
        shell = shell_path;
        if (shell == NULL || shell[0] == '\0') {
            shell = getenv("SHELL");
        }
        if (shell == NULL || shell[0] == '\0') {
            shell = "/bin/sh";
        }

        (void) signal(SIGINT, SIG_DFL);
        (void) signal(SIGTERM, SIG_DFL);
        (void) signal(SIGHUP, SIG_DFL);
        (void) signal(SIGCHLD, SIG_DFL);

        (void) close(master_fd);
        if (setsid() < 0) {
            _exit(127);
        }

        slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            _exit(127);
        }
        (void) gem_os_pty_apply_size(slave_fd, columns, rows);
        (void) ioctl(slave_fd, TIOCSCTTY, 0);
        (void) dup2(slave_fd, STDIN_FILENO);
        (void) dup2(slave_fd, STDOUT_FILENO);
        (void) dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO) {
            (void) close(slave_fd);
        }

        if (cwd != NULL && cwd[0] != '\0') {
            (void) chdir(cwd);
        }
        (void) setenv("TERM", "dumb", 1);
        (void) setenv("LINES", "25", 1);
        (void) setenv("COLUMNS", "80", 1);
        execl(shell, shell, "-i", (char *) NULL);
        _exit(127);
    }

    pty->master_fd = master_fd;
    pty->child_pid = (int) pid;
    (void) gem_os_pty_apply_size(master_fd, columns, rows);
    return 1;
}

int gem_os_pty_resize(gem_os_pty_t *pty, uint16_t columns, uint16_t rows)
{
    if (pty == NULL || pty->master_fd < 0) {
        errno = EINVAL;
        return 0;
    }
    return gem_os_pty_apply_size(pty->master_fd, columns, rows);
}

int32_t gem_os_pty_read(gem_os_pty_t *pty, void *buf, uint32_t size)
{
    ssize_t rc;

    if (pty == NULL || pty->master_fd < 0 || (buf == NULL && size != 0u)) {
        errno = EINVAL;
        return -1;
    }

    rc = read(pty->master_fd, buf, (size_t) size);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return (int32_t) rc;
}

int32_t gem_os_pty_write(gem_os_pty_t *pty, const void *buf, uint32_t size)
{
    ssize_t rc;

    if (pty == NULL || pty->master_fd < 0 || (buf == NULL && size != 0u)) {
        errno = EINVAL;
        return -1;
    }

    rc = write(pty->master_fd, buf, (size_t) size);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return (int32_t) rc;
}

int gem_os_pty_is_alive(gem_os_pty_t *pty)
{
    pid_t rc;
    int status;

    if (pty == NULL || pty->child_pid <= 0) {
        return 0;
    }

    rc = waitpid((pid_t) pty->child_pid, &status, WNOHANG);
    if (rc == 0) {
        return 1;
    }
    if (rc == (pid_t) pty->child_pid) {
        pty->child_pid = -1;
        return 0;
    }
    return 0;
}

void gem_os_pty_close(gem_os_pty_t *pty)
{
    if (pty == NULL) {
        return;
    }

    if (pty->child_pid > 0) {
        (void) kill((pid_t) pty->child_pid, SIGHUP);
        (void) waitpid((pid_t) pty->child_pid, NULL, 0);
        pty->child_pid = -1;
    }
    if (pty->master_fd >= 0) {
        (void) close(pty->master_fd);
        pty->master_fd = -1;
    }
}

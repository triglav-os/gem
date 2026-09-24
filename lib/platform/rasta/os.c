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
#include <pthread.h>
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

extern char **environ;

enum {
    GEM_OS_ENV_NAME_MAX = 128,
    GEM_OS_ENV_VALUE_MAX = 65536,
    GEM_OS_TAIL_MAX = 127,
    GEM_OS_ARG_MAX = 32
};

static int gem_os_env_name(const char *name,
                           char normalized[GEM_OS_ENV_NAME_MAX])
{
    size_t length;

    if (name == NULL) {
        errno = EINVAL;
        return 0;
    }
    length = strlen(name);
    if (length > 0u && name[length - 1u] == '=') {
        --length;
    }
    if (length == 0u || length >= GEM_OS_ENV_NAME_MAX) {
        errno = EINVAL;
        return 0;
    }
    memcpy(normalized, name, length);
    normalized[length] = '\0';
    return 1;
}

int gem_os_mutex_init(gem_os_mutex_t *mutex)
{
    pthread_mutex_t *native;

    if (mutex == NULL || mutex->handle != NULL) {
        errno = EINVAL;
        return 0;
    }
    native = malloc(sizeof(*native));
    if (native == NULL) {
        return 0;
    }
    if (pthread_mutex_init(native, NULL) != 0) {
        free(native);
        return 0;
    }
    mutex->handle = native;
    return 1;
}

void gem_os_mutex_lock(gem_os_mutex_t *mutex)
{
    if (mutex != NULL && mutex->handle != NULL) {
        (void)pthread_mutex_lock((pthread_mutex_t *)mutex->handle);
    }
}

void gem_os_mutex_unlock(gem_os_mutex_t *mutex)
{
    if (mutex != NULL && mutex->handle != NULL) {
        (void)pthread_mutex_unlock((pthread_mutex_t *)mutex->handle);
    }
}

void gem_os_mutex_destroy(gem_os_mutex_t *mutex)
{
    if (mutex != NULL && mutex->handle != NULL) {
        pthread_mutex_t *native = (pthread_mutex_t *)mutex->handle;

        (void)pthread_mutex_destroy(native);
        free(native);
        mutex->handle = NULL;
    }
}

gem_os_pid_t gem_os_getpid(void)
{
    return (gem_os_pid_t)getpid();
}

uint32_t gem_os_getuid(void)
{
    return (uint32_t)getuid();
}

const char *gem_os_getenv_ref(const char *name)
{
    char normalized[GEM_OS_ENV_NAME_MAX];

    if (gem_os_env_name(name, normalized) == 0) {
        return NULL;
    }
    return getenv(normalized);
}

int gem_os_getenv(const char *name, char *dst, size_t dst_size)
{
    const char *value = gem_os_getenv_ref(name);
    size_t length;

    if (value == NULL || dst == NULL || dst_size == 0u) {
        return 0;
    }
    length = strlen(value);
    if (length >= dst_size) {
        errno = ENAMETOOLONG;
        return 0;
    }
    memcpy(dst, value, length + 1u);
    return 1;
}

static int gem_os_read_process_file(gem_os_pid_t pid, const char *leaf,
                                    unsigned char **data_out, size_t *size_out)
{
    char path[64];
    unsigned char *data;
    size_t used = 0u;
    int fd;
    int rc;

    if (pid <= 0 || leaf == NULL || data_out == NULL || size_out == NULL) {
        errno = EINVAL;
        return 0;
    }
    rc = snprintf(path, sizeof(path), "/proc/%d/%s", pid, leaf);
    if (rc <= 0 || (size_t)rc >= sizeof(path)) {
        return 0;
    }
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return 0;
    }
    data = malloc(GEM_OS_ENV_VALUE_MAX);
    if (data == NULL) {
        (void)close(fd);
        return 0;
    }
    while (used < GEM_OS_ENV_VALUE_MAX) {
        ssize_t count = read(fd, data + used, GEM_OS_ENV_VALUE_MAX - used);

        if (count > 0) {
            used += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            free(data);
            (void)close(fd);
            return 0;
        }
        break;
    }
    (void)close(fd);
    *data_out = data;
    *size_out = used;
    return 1;
}

int gem_os_process_getenv(gem_os_pid_t pid, const char *name, char *dst,
                          size_t dst_size)
{
    char normalized[GEM_OS_ENV_NAME_MAX];
    unsigned char *data;
    size_t size;
    size_t offset;

    if (dst == NULL || dst_size == 0u ||
        gem_os_env_name(name, normalized) == 0 ||
        gem_os_read_process_file(pid, "environ", &data, &size) == 0) {
        return 0;
    }
    for (offset = 0u; offset < size;) {
        const char *entry = (const char *)data + offset;
        size_t remaining = size - offset;
        size_t length = strnlen(entry, remaining);
        size_t name_length = strlen(normalized);

        if (length == remaining) {
            break;
        }
        if (length > name_length && entry[name_length] == '=' &&
            memcmp(entry, normalized, name_length) == 0) {
            const char *value = entry + name_length + 1u;
            size_t value_length = length - name_length - 1u;

            if (value_length < dst_size) {
                memcpy(dst, value, value_length);
                dst[value_length] = '\0';
                free(data);
                return 1;
            }
            errno = ENAMETOOLONG;
            break;
        }
        offset += length + 1u;
    }
    free(data);
    return 0;
}

int gem_os_access(const char *path, int mode)
{
    int native_mode;

    if (path == NULL) {
        errno = EINVAL;
        return 0;
    }
    switch (mode) {
        case GEM_OS_ACCESS_EXISTS:
            native_mode = F_OK;
            break;
        case GEM_OS_ACCESS_EXECUTE:
            native_mode = X_OK;
            break;
        case GEM_OS_ACCESS_READ:
            native_mode = R_OK;
            break;
        default:
            errno = EINVAL;
            return 0;
    }
    return (access(path, native_mode) == 0) ? 1 : 0;
}

static int gem_os_try_search_path(const char *directory, const char *name,
                                  int mode, char *out, size_t out_size)
{
    char candidate[GEM_OS_PATH_MAX];
    char absolute[PATH_MAX];
    const char *dir = directory;
    int rc;

    if (dir == NULL || dir[0] == '\0') {
        dir = ".";
    }
    if (name[0] == '/') {
        rc = snprintf(candidate, sizeof(candidate), "%s", name);
    } else {
        rc = snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);
    }
    if (rc <= 0 || (size_t)rc >= sizeof(candidate) ||
        gem_os_access(candidate, mode) == 0 ||
        realpath(candidate, absolute) == NULL || strlen(absolute) >= out_size) {
        return 0;
    }
    strcpy(out, absolute);
    return 1;
}

static int gem_os_search_list(const char *list, const char *name, int mode,
                              char *out, size_t out_size)
{
    const char *start;

    if (list == NULL) {
        return 0;
    }
    start = list;
    for (;;) {
        const char *end = strchr(start, ':');
        size_t length = (end != NULL) ? (size_t)(end - start) : strlen(start);
        char directory[GEM_OS_PATH_MAX];

        if (length < sizeof(directory)) {
            memcpy(directory, start, length);
            directory[length] = '\0';
            if (gem_os_try_search_path(directory, name, mode, out, out_size)) {
                return 1;
            }
        }
        if (end == NULL) {
            return 0;
        }
        start = end + 1;
    }
}

static int gem_os_find_path(const char *name, int mode, char *out,
                            size_t out_size)
{
    const char *home;
    const char *path;
    const char *aes_path;
    char apps[GEM_OS_PATH_MAX];
    int length;

    if (name == NULL || name[0] == '\0' || out == NULL || out_size == 0u) {
        errno = EINVAL;
        return 0;
    }
    if (strchr(name, '/') != NULL) {
        return gem_os_try_search_path(".", name, mode, out, out_size);
    }
    if (gem_os_try_search_path(".", name, mode, out, out_size)) {
        return 1;
    }
    home = gem_os_getenv_ref("GEMIX_HOME");
    if (home != NULL && home[0] != '\0') {
        if (gem_os_try_search_path(home, name, mode, out, out_size)) {
            return 1;
        }
        length = snprintf(apps, sizeof(apps), "%s/apps", home);
        if (length > 0 && (size_t)length < sizeof(apps) &&
            gem_os_try_search_path(apps, name, mode, out, out_size)) {
            return 1;
        }
    }
    path = gem_os_getenv_ref("PATH");
    if (gem_os_search_list(path, name, mode, out, out_size)) {
        return 1;
    }
    aes_path = gem_os_getenv_ref("GEM_AES_PATH");
    return gem_os_search_list(aes_path, name, mode, out, out_size);
}

static int gem_os_copy_text(char *out, size_t out_size, const char *value)
{
    size_t length;

    if (out == NULL || out_size == 0u || value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length >= out_size) {
        return 0;
    }
    memcpy(out, value, length + 1u);
    return 1;
}

int gem_os_find_file(const char *name, char *out, size_t out_size)
{
    return gem_os_find_path(name, GEM_OS_ACCESS_READ, out, out_size);
}

int gem_os_find_executable(const char *name, char *out, size_t out_size)
{
    return gem_os_find_path(name, GEM_OS_ACCESS_EXECUTE, out, out_size);
}

static int gem_os_find_special_program(const char *name, const char *env_name,
                                       const char *first_suffix,
                                       const char *second_suffix,
                                       char *out, size_t out_size)
{
    const char *configured = gem_os_getenv_ref(env_name);
    const char *home = gem_os_getenv_ref("HOME");
    const char *gemix = gem_os_getenv_ref("GEMIX_HOME");
    char directories[GEM_OS_PATH_MAX * 4u];
    int length;

    if (name == NULL || name[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    if (configured != NULL &&
        gem_os_search_list(configured, name, GEM_OS_ACCESS_READ, out,
                           out_size)) {
        return 1;
    }
    if (gemix != NULL && gemix[0] != '\0') {
        length = snprintf(directories, sizeof(directories),
                          "%s/%s:%s/%s", gemix, first_suffix, gemix,
                          second_suffix);
        if (length > 0 && (size_t)length < sizeof(directories) &&
            gem_os_search_list(directories, name, GEM_OS_ACCESS_READ, out,
                               out_size)) {
            return 1;
        }
    }
    if (home != NULL && home[0] == '/') {
        length = snprintf(directories, sizeof(directories),
                          "%s/.gem/%s:%s/.gem/%s", home, first_suffix, home,
                          second_suffix);
        if (length > 0 && (size_t)length < sizeof(directories) &&
            gem_os_search_list(directories, name, GEM_OS_ACCESS_READ, out,
                               out_size)) {
            return 1;
        }
    }
    if (strcmp(first_suffix, "accessories") == 0) {
        return gem_os_search_list(
            "/usr/local/share/gem/accessories:/usr/share/gem/accessories",
            name, GEM_OS_ACCESS_READ, out, out_size);
    }
    return gem_os_search_list("/usr/local/share/gem/auto:/usr/share/gem/auto",
                              name, GEM_OS_ACCESS_READ, out, out_size);
}

int gem_os_find_accessory(const char *name, char *out, size_t out_size)
{
    return gem_os_find_special_program(name, "GEM_ACC_PATH", "accessories",
                                        "acc", out, out_size);
}

int gem_os_find_auto_program(const char *name, char *out, size_t out_size)
{
    return gem_os_find_special_program(name, "GEM_AUTO_PATH", "auto", "AUTO",
                                        out, out_size);
}

int gem_os_user_config_dir(char *out, size_t out_size)
{
    const char *xdg = gem_os_getenv_ref("XDG_CONFIG_HOME");
    const char *home;
    int length;

    if (out == NULL || out_size == 0u) {
        return 0;
    }
    if (xdg != NULL && xdg[0] == '/') {
        return gem_os_copy_text(out, out_size, xdg);
    }
    home = gem_os_getenv_ref("HOME");
    if (home == NULL || home[0] != '/') {
        return 0;
    }
    length = snprintf(out, out_size, "%s/.config", home);
    return length > 0 && (size_t)length < out_size;
}

int gem_os_system_config_dirs(char *out, size_t out_size)
{
    const char *xdg = gem_os_getenv_ref("XDG_CONFIG_DIRS");

    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return gem_os_copy_text(out, out_size,
                            xdg != NULL && xdg[0] != '\0' ? xdg : "/etc/xdg");
}

int gem_os_user_data_dir(char *out, size_t out_size)
{
    const char *xdg = gem_os_getenv_ref("XDG_DATA_HOME");
    const char *home;
    int length;

    if (out == NULL || out_size == 0u) {
        return 0;
    }
    if (xdg != NULL && xdg[0] == '/') {
        return gem_os_copy_text(out, out_size, xdg);
    }
    home = gem_os_getenv_ref("HOME");
    if (home == NULL || home[0] != '/') {
        return 0;
    }
    length = snprintf(out, out_size, "%s/.local/share", home);
    return length > 0 && (size_t)length < out_size;
}

static size_t gem_os_tail_arguments(char *tail, size_t tail_len, char *argv[],
                                    size_t argv_size)
{
    size_t argc = 1u;
    size_t index = 0u;

    while (index < tail_len && argc + 1u < argv_size) {
        char quote = '\0';
        char *write;

        while (index < tail_len &&
               (tail[index] == ' ' || tail[index] == '\t')) {
            ++index;
        }
        if (index == tail_len) {
            break;
        }
        write = tail + index;
        argv[argc++] = write;
        while (index < tail_len) {
            char value = tail[index++];

            if (quote == '\0' && (value == '\'' || value == '"')) {
                quote = value;
                continue;
            }
            if (quote != '\0' && value == quote) {
                quote = '\0';
                continue;
            }
            if (quote == '\0' && (value == ' ' || value == '\t')) {
                break;
            }
            *write++ = value;
        }
        *write = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

static void gem_os_tail_hex(const unsigned char *tail, size_t tail_len,
                            char hex[GEM_OS_TAIL_MAX * 2u + 1u])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;

    for (index = 0u; index < tail_len; ++index) {
        hex[index * 2u] = digits[tail[index] >> 4];
        hex[index * 2u + 1u] = digits[tail[index] & 15u];
    }
    hex[tail_len * 2u] = '\0';
}

static int gem_os_shell_env_entry(const char *entry)
{
    static const char *names[] = {"GEM_SHEL_CMD", "GEM_SHEL_TAIL",
                                  "GEM_SHEL_TAIL_HEX", "GEM_SHEL_TAIL_LEN",
                                  "GEM_APPL_ID"};
    size_t index;

    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        size_t length = strlen(names[index]);

        if (strncmp(entry, names[index], length) == 0 && entry[length] == '=') {
            return 1;
        }
    }
    return 0;
}

int gem_os_spawn_gem(const char *path, const char *cmd, const void *tail,
                     size_t tail_len, int appl_id, gem_os_pid_t *out_pid)
{
    char resolved[GEM_OS_PATH_MAX];
    char tail_copy[GEM_OS_TAIL_MAX + 1u];
    char command_env[sizeof("GEM_SHEL_CMD=") + 255u];
    char tail_env[sizeof("GEM_SHEL_TAIL=") + GEM_OS_TAIL_MAX];
    char tail_hex_env[sizeof("GEM_SHEL_TAIL_HEX=") + GEM_OS_TAIL_MAX * 2u];
    char tail_length_env[sizeof("GEM_SHEL_TAIL_LEN=") + 15u];
    char app_id_env[sizeof("GEM_APPL_ID=") + 15u];
    char tail_hex[GEM_OS_TAIL_MAX * 2u + 1u];
    char *argv[GEM_OS_ARG_MAX];
    char **child_env;
    size_t child_env_count = 0u;
    size_t environment_count = 0u;
    size_t index;
    int error_pipe[2];
    pid_t pid;
    int child_error = 0;
    ssize_t error_size;

    if (path == NULL || cmd == NULL || tail_len > GEM_OS_TAIL_MAX ||
        (tail == NULL && tail_len != 0u) || out_pid == NULL || appl_id <= 0 ||
        strlen(cmd) > 255u ||
        gem_os_find_executable(path, resolved, sizeof(resolved)) == 0) {
        return 0;
    }
    if (tail_len > 0u) {
        memcpy(tail_copy, tail, tail_len);
    }
    tail_copy[tail_len] = '\0';
    gem_os_tail_hex((const unsigned char *)tail_copy, tail_len, tail_hex);
    (void)snprintf(command_env, sizeof(command_env), "GEM_SHEL_CMD=%s", cmd);
    memcpy(tail_env, "GEM_SHEL_TAIL=", sizeof("GEM_SHEL_TAIL=") - 1u);
    if (tail_len != 0u) {
        memcpy(tail_env + sizeof("GEM_SHEL_TAIL=") - 1u, tail, tail_len);
    }
    tail_env[sizeof("GEM_SHEL_TAIL=") - 1u + tail_len] = '\0';
    (void)snprintf(tail_hex_env, sizeof(tail_hex_env), "GEM_SHEL_TAIL_HEX=%s",
                   tail_hex);
    (void)snprintf(tail_length_env, sizeof(tail_length_env),
                   "GEM_SHEL_TAIL_LEN=%zu", tail_len);
    (void)snprintf(app_id_env, sizeof(app_id_env), "GEM_APPL_ID=%d", appl_id);
    argv[0] = resolved;
    (void)gem_os_tail_arguments(tail_copy, tail_len, argv, GEM_OS_ARG_MAX);
    while (environ[environment_count] != NULL) {
        ++environment_count;
    }
    child_env = malloc((environment_count + 6u) * sizeof(*child_env));
    if (child_env == NULL) {
        return 0;
    }
    for (index = 0u; index < environment_count; ++index) {
        if (gem_os_shell_env_entry(environ[index]) == 0) {
            child_env[child_env_count++] = environ[index];
        }
    }
    child_env[child_env_count++] = command_env;
    child_env[child_env_count++] = tail_env;
    child_env[child_env_count++] = tail_hex_env;
    child_env[child_env_count++] = tail_length_env;
    child_env[child_env_count++] = app_id_env;
    child_env[child_env_count] = NULL;
    if (pipe2(error_pipe, O_CLOEXEC) != 0) {
        free(child_env);
        return 0;
    }
    pid = fork();
    if (pid < 0) {
        (void)close(error_pipe[0]);
        (void)close(error_pipe[1]);
        free(child_env);
        return 0;
    }
    if (pid == 0) {
        (void)close(error_pipe[0]);
        execve(resolved, argv, child_env);
        child_error = errno;
        do {
            error_size =
                write(error_pipe[1], &child_error, sizeof(child_error));
        } while (error_size < 0 && errno == EINTR);
        _exit(127);
    }
    (void)close(error_pipe[1]);
    do {
        error_size = read(error_pipe[0], &child_error, sizeof(child_error));
    } while (error_size < 0 && errno == EINTR);
    (void)close(error_pipe[0]);
    free(child_env);
    if (error_size != 0) {
        (void)waitpid(pid, NULL, 0);
        if (error_size > 0) {
            errno = child_error;
        }
        return 0;
    }
    *out_pid = (gem_os_pid_t)pid;
    return 1;
}

static int gem_os_hex_digit(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static int gem_os_process_tail(gem_os_pid_t pid, unsigned char *tail,
                               size_t tail_size, size_t *tail_len)
{
    char length_text[16];
    char hex[GEM_OS_TAIL_MAX * 2u + 1u];
    char *end;
    unsigned long length;
    size_t index;

    if (gem_os_process_getenv(pid, "GEM_SHEL_TAIL_LEN", length_text,
                              sizeof(length_text)) == 0) {
        return 0;
    }
    errno = 0;
    length = strtoul(length_text, &end, 10);
    if (errno != 0 || *end != '\0' || length > GEM_OS_TAIL_MAX ||
        length > tail_size ||
        gem_os_process_getenv(pid, "GEM_SHEL_TAIL_HEX", hex, sizeof(hex)) ==
            0 ||
        strlen(hex) != length * 2u) {
        return 0;
    }
    for (index = 0u; index < (size_t)length; ++index) {
        int high = gem_os_hex_digit(hex[index * 2u]);
        int low = gem_os_hex_digit(hex[index * 2u + 1u]);

        if (high < 0 || low < 0) {
            return 0;
        }
        tail[index] = (unsigned char)((high << 4) | low);
    }
    *tail_len = (size_t)length;
    return 1;
}

int gem_os_process_launch(gem_os_pid_t pid, char *cmd, size_t cmd_size,
                          void *tail, size_t tail_size, size_t *tail_len)
{
    unsigned char *data;
    unsigned char *tail_bytes = (unsigned char *)tail;
    size_t size;
    size_t first;
    size_t offset;
    size_t used = 0u;

    if (cmd == NULL || cmd_size == 0u || tail == NULL || tail_len == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (gem_os_process_getenv(pid, "GEM_SHEL_CMD", cmd, cmd_size) != 0 &&
        gem_os_process_tail(pid, tail_bytes, tail_size, tail_len) != 0) {
        return 1;
    }
    if (gem_os_read_process_file(pid, "cmdline", &data, &size) == 0 ||
        size == 0u) {
        return 0;
    }
    first = strnlen((const char *)data, size);
    if (first == size || first >= cmd_size) {
        free(data);
        return 0;
    }
    memcpy(cmd, data, first + 1u);
    offset = first + 1u;
    while (offset < size && used < tail_size) {
        size_t length = strnlen((const char *)data + offset, size - offset);

        if (length == 0u || length == size - offset) {
            break;
        }
        if (used != 0u) {
            tail_bytes[used++] = ' ';
            if (used == tail_size) {
                break;
            }
        }
        if (length > tail_size - used) {
            length = tail_size - used;
        }
        memcpy(tail_bytes + used, data + offset, length);
        used += length;
        offset += length + 1u;
    }
    free(data);
    *tail_len = used;
    return 1;
}

int gem_os_reap_process(gem_os_pid_t pid)
{
    pid_t result;

    if (pid <= 0) {
        errno = EINVAL;
        return -1;
    }
    do {
        result = waitpid((pid_t)pid, NULL, WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result == 0) {
        return 0;
    }
    return (result == (pid_t)pid) ? 1 : -1;
}

int gem_os_init(void)
{
    return 1;
}

void gem_os_shutdown(void) {}

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

    ms = (uint64_t)ts.tv_sec * 1000u;
    ms += (uint64_t)ts.tv_nsec / 1000000u;
    return (uint32_t)(ms & 0xffffffffu);
}

void gem_os_sleep_ms(uint32_t ms)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = (time_t)(ms / 1000u);
    req.tv_nsec = (long)(ms % 1000u) * 1000000l;

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

int gem_os_open_existing(const char *path, int mode)
{
    int flags;

    if (path == NULL || mode < 0 || mode > 2) {
        errno = EINVAL;
        return -1;
    }
    flags = mode == 0 ? O_RDONLY : mode == 1 ? O_WRONLY : O_RDWR;
    return open(path, flags);
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

    return (int32_t)rc;
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

    return (int32_t)rc;
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

    rc = lseek(fd, (off_t)offset, seek_whence);
    if (rc < 0) {
        return -1;
    }
    return (int64_t)rc;
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

int gem_os_mkdir_p(const char *path)
{
    char copy[GEM_OS_PATH_MAX];
    char *cursor;
    size_t length;

    if (path == NULL || path[0] != '/') {
        errno = EINVAL;
        return 0;
    }
    length = strlen(path);
    if (length == 0u || length >= sizeof(copy)) {
        errno = ENAMETOOLONG;
        return 0;
    }
    memcpy(copy, path, length + 1u);
    for (cursor = copy + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir(copy, 0700) != 0 && errno != EEXIST) {
            return 0;
        }
        *cursor = '/';
    }
    if (mkdir(copy, 0700) != 0 && errno != EEXIST) {
        return 0;
    }
    return 1;
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

static int gem_os_copy_regular_path(const char *source,
                                    const char *destination, mode_t mode)
{
    char buffer[16384];
    int input;
    int output;
    int okay = 1;

    input = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (input < 0) {
        return 0;
    }
    output = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                  mode & 0777);
    if (output < 0) {
        (void)close(input);
        return 0;
    }
    for (;;) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        size_t written = 0u;

        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            okay = 0;
            break;
        }
        while (written < (size_t)count) {
            ssize_t result =
                write(output, buffer + written, (size_t)count - written);

            if (result > 0) {
                written += (size_t)result;
            } else if (result < 0 && errno == EINTR) {
                continue;
            } else {
                okay = 0;
                break;
            }
        }
        if (okay == 0) {
            break;
        }
    }
    if (okay != 0 && (fsync(output) != 0 || fchmod(output, mode & 0777) != 0)) {
        okay = 0;
    }
    if (close(input) != 0) {
        okay = 0;
    }
    if (close(output) != 0) {
        okay = 0;
    }
    if (okay == 0) {
        (void)unlink(destination);
    }
    return okay;
}

static int gem_os_copy_path_join(const char *directory, const char *name,
                                 char *out, size_t out_size)
{
    int length = snprintf(out, out_size, "%s/%s", directory, name);

    if (length <= 0 || (size_t)length >= out_size) {
        errno = ENAMETOOLONG;
        return 0;
    }
    return 1;
}

static int gem_os_copy_entry_path(const char *source, const char *destination)
{
    struct stat info;

    if (lstat(source, &info) != 0) {
        return 0;
    }
    if (S_ISREG(info.st_mode)) {
        return gem_os_copy_regular_path(source, destination, info.st_mode);
    }
    if (S_ISLNK(info.st_mode)) {
        char target[PATH_MAX];
        ssize_t length = readlink(source, target, sizeof(target) - 1u);

        if (length < 0 || (size_t)length >= sizeof(target) - 1u) {
            return 0;
        }
        target[length] = '\0';
        return symlink(target, destination) == 0;
    }
    if (S_ISDIR(info.st_mode)) {
        DIR *directory;
        struct dirent *entry;
        int okay = 1;

        if (mkdir(destination, (info.st_mode & 0777) | S_IRWXU) != 0) {
            return 0;
        }
        directory = opendir(source);
        if (directory == NULL) {
            (void)rmdir(destination);
            return 0;
        }
        while ((entry = readdir(directory)) != NULL) {
            char source_child[PATH_MAX];
            char destination_child[PATH_MAX];

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (gem_os_copy_path_join(source, entry->d_name, source_child,
                                      sizeof(source_child)) == 0 ||
                gem_os_copy_path_join(destination, entry->d_name,
                                      destination_child,
                                      sizeof(destination_child)) == 0 ||
                gem_os_copy_entry_path(source_child, destination_child) == 0) {
                okay = 0;
                break;
            }
        }
        if (closedir(directory) != 0) {
            okay = 0;
        }
        if (okay != 0 && chmod(destination, info.st_mode & 0777) != 0) {
            okay = 0;
        }
        return okay;
    }
    errno = ENOTSUP;
    return 0;
}

static int gem_os_parent_path(const char *path, char *parent,
                              size_t parent_size)
{
    char *slash;
    size_t length;

    if (path == NULL || parent == NULL || parent_size == 0u) {
        errno = EINVAL;
        return 0;
    }
    length = strlen(path);
    if (length == 0u || length >= parent_size) {
        errno = ENAMETOOLONG;
        return 0;
    }
    memcpy(parent, path, length + 1u);
    slash = strrchr(parent, '/');
    if (slash == NULL) {
        return gem_os_getcwd(parent, parent_size);
    }
    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    return 1;
}

int gem_os_copy_path(const char *old_path, const char *new_path)
{
    char new_parent[PATH_MAX];
    char resolved_source[PATH_MAX];
    char resolved_parent[PATH_MAX];
    char resolved_destination[PATH_MAX];
    const char *name;
    struct stat source_info;
    struct stat destination_info;
    int length;
    int saved_error;

    if (old_path == NULL || new_path == NULL ||
        lstat(old_path, &source_info) != 0 ||
        gem_os_parent_path(new_path, new_parent, sizeof(new_parent)) == 0) {
        if (old_path == NULL || new_path == NULL) errno = EINVAL;
        return 0;
    }
    if (lstat(new_path, &destination_info) == 0) {
        errno = EEXIST;
        return 0;
    }
    if (errno != ENOENT) return 0;

    if (S_ISDIR(source_info.st_mode)) {
        name = strrchr(new_path, '/');
        name = name != NULL ? name + 1u : new_path;
        if (name[0] == '\0' || realpath(old_path, resolved_source) == NULL ||
            realpath(new_parent, resolved_parent) == NULL) return 0;
        length = snprintf(resolved_destination,
                          sizeof(resolved_destination), "%s/%s",
                          resolved_parent, name);
        if (length <= 0 || (size_t)length >= sizeof(resolved_destination)) {
            errno = ENAMETOOLONG;
            return 0;
        }
        length = (int)strlen(resolved_source);
        if (strncmp(resolved_destination, resolved_source,
                    (size_t)length) == 0 &&
            (resolved_destination[length] == '\0' ||
             resolved_destination[length] == '/')) {
            errno = EINVAL;
            return 0;
        }
    }
    if (gem_os_copy_entry_path(old_path, new_path) != 0) return 1;
    saved_error = errno;
    if (saved_error != EEXIST)
        (void)gem_os_remove_tree_under(new_path, new_parent);
    errno = saved_error;
    return 0;
}

static int gem_os_rename_no_replace(const char *old_path,
                                    const char *new_path)
{
    struct stat destination_info;

    if (renameat2(AT_FDCWD, old_path, AT_FDCWD, new_path,
                  RENAME_NOREPLACE) == 0) return 1;
    if (errno != ENOSYS && errno != EINVAL) return 0;
    if (lstat(new_path, &destination_info) == 0) {
        errno = EEXIST;
        return 0;
    }
    if (errno != ENOENT) return 0;
    return rename(old_path, new_path) == 0;
}

int gem_os_move_path(const char *old_path, const char *new_path)
{
    char old_parent[PATH_MAX];
    char new_parent[PATH_MAX];
    char temporary[PATH_MAX];
    unsigned int attempt;
    int saved_error;

    if (old_path == NULL || new_path == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (gem_os_rename_no_replace(old_path, new_path) != 0) {
        return 1;
    }
    if (errno != EXDEV ||
        gem_os_parent_path(old_path, old_parent, sizeof(old_parent)) == 0 ||
        gem_os_parent_path(new_path, new_parent, sizeof(new_parent)) == 0) {
        return 0;
    }
    for (attempt = 0u; attempt < 100u; ++attempt) {
        int length = snprintf(temporary, sizeof(temporary),
                              "%s/.gem-move-%lu-%u", old_parent,
                              (unsigned long)getpid(), attempt);

        if (length <= 0 || (size_t)length >= sizeof(temporary)) {
            errno = ENAMETOOLONG;
            return 0;
        }
        struct stat temporary_info;

        if (lstat(temporary, &temporary_info) == 0) {
            continue;
        }
        if (errno != ENOENT) {
            return 0;
        }
        if (gem_os_rename_no_replace(old_path, temporary) != 0) {
            break;
        }
        if (errno == EEXIST) continue;
        return 0;
    }
    if (attempt == 100u) {
        return 0;
    }
    if (gem_os_copy_entry_path(temporary, new_path) == 0) {
        saved_error = errno;
        (void)rename(temporary, old_path);
        if (saved_error != EEXIST)
            (void)gem_os_remove_tree_under(new_path, new_parent);
        errno = saved_error;
        return 0;
    }
    /* The original name is already gone and the complete destination exists.
     * A cleanup failure can leave only a hidden source-side staging entry;
     * retaining the destination avoids data loss after a partial removal. */
    (void)gem_os_remove_tree_under(temporary, old_parent);
    return 1;
}

int gem_os_read_file(const char *path, char *buf, size_t buf_size,
                     size_t *size_out)
{
    size_t used = 0u;
    int fd;

    if (path == NULL || buf == NULL || buf_size == 0u) {
        errno = EINVAL;
        return 0;
    }
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return 0;
    }
    while (used + 1u < buf_size) {
        ssize_t count = read(fd, buf + used, buf_size - used - 1u);

        if (count > 0) {
            used += (size_t)count;
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            (void)close(fd);
            return 0;
        }
    }
    if (used + 1u == buf_size) {
        char extra;
        ssize_t count;

        do {
            count = read(fd, &extra, 1u);
        } while (count < 0 && errno == EINTR);
        if (count != 0) {
            (void)close(fd);
            errno = EOVERFLOW;
            return 0;
        }
    }
    if (close(fd) != 0) {
        return 0;
    }
    buf[used] = '\0';
    if (size_out != NULL) {
        *size_out = used;
    }
    return 1;
}

int gem_os_write_file_atomic(const char *path, const void *data, size_t size)
{
    char temporary[GEM_OS_PATH_MAX];
    unsigned int attempt;
    int fd = -1;
    size_t used = 0u;

    if (path == NULL || data == NULL) {
        errno = EINVAL;
        return 0;
    }
    for (attempt = 0u; attempt < 100u; ++attempt) {
        int length = snprintf(temporary, sizeof(temporary), "%s.tmp.%lu.%u",
                              path, (unsigned long)getpid(), attempt);

        if (length <= 0 || (size_t)length >= sizeof(temporary)) {
            errno = ENAMETOOLONG;
            return 0;
        }
        fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd >= 0) {
            break;
        }
        if (errno != EEXIST) {
            return 0;
        }
    }
    if (fd < 0) {
        return 0;
    }
    while (used < size) {
        ssize_t count = write(fd, (const char *)data + used, size - used);

        if (count > 0) {
            used += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            (void)close(fd);
            (void)unlink(temporary);
            return 0;
        }
    }
    if (fsync(fd) != 0 || close(fd) != 0) {
        (void)unlink(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        (void)unlink(temporary);
        return 0;
    }
    return 1;
}

static int gem_os_fill_info_from_stat(const char *name, const struct stat *st,
                                      gem_os_file_info_t *info)
{
    if (st == NULL || info == NULL) {
        errno = EINVAL;
        return 0;
    }

    info->size_bytes = (uint64_t)st->st_size;
    info->mtime_ms = (uint64_t)st->st_mtim.tv_sec * 1000u +
                     (uint64_t)st->st_mtim.tv_nsec / 1000000u;
    info->is_directory = S_ISDIR(st->st_mode) ? 1 : 0;
    info->is_executable =
        ((st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) ? 1 : 0;
    info->is_symlink = S_ISLNK(st->st_mode) ? 1 : 0;
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

int gem_os_lstat_path(const char *path, gem_os_file_info_t *info)
{
    struct stat st;

    if (path == NULL || info == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (lstat(path, &st) != 0) {
        return 0;
    }
    return gem_os_fill_info_from_stat(path, &st, info);
}

int gem_os_path_exists(const char *path)
{
    struct stat st;

    return path != NULL && lstat(path, &st) == 0;
}

int gem_os_same_device(const char *left_path, const char *right_path)
{
    struct stat left;
    struct stat right;

    if (left_path == NULL || right_path == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (lstat(left_path, &left) != 0 || lstat(right_path, &right) != 0) {
        return 0;
    }
    return left.st_dev == right.st_dev;
}

static int gem_os_path_prefix(const char *root, const char *path)
{
    size_t length;

    if (root == NULL || path == NULL) {
        return 0;
    }
    length = strlen(root);
    if (strncmp(root, path, length) != 0) {
        return 0;
    }
    return length == 1u || path[length] == '\0' || path[length] == '/';
}

int gem_os_mount_for_path(const char *path, char *out, size_t out_size)
{
    struct stat source_stat;
    char resolved[PATH_MAX];
    char parent[PATH_MAX];
    char best[PATH_MAX] = "";
    char name[PATH_MAX];
    char *slash;
    FILE *mounts;
    struct mntent *mount;
    int length;

    if (path == NULL || out == NULL || out_size == 0u ||
        strlen(path) >= sizeof(parent)) {
        return 0;
    }
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (slash == NULL || slash[1] == '\0' ||
        strlen(slash + 1) >= sizeof(name)) {
        return 0;
    }
    strcpy(name, slash + 1);
    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    if (realpath(parent, resolved) == NULL) {
        return 0;
    }
    length = snprintf(parent, sizeof(parent), strcmp(resolved, "/") == 0
                                                  ? "/%s"
                                                  : "%s/%s",
                      resolved, name);
    if (length <= 0 || (size_t)length >= sizeof(parent) ||
        lstat(parent, &source_stat) != 0) {
        return 0;
    }
    strcpy(resolved, parent);
    mounts = setmntent("/proc/mounts", "r");
    if (mounts == NULL) {
        return 0;
    }
    while ((mount = getmntent(mounts)) != NULL) {
        struct stat mount_stat;
        size_t current_length;

        if (mount->mnt_dir == NULL ||
            gem_os_path_prefix(mount->mnt_dir, resolved) == 0 ||
            stat(mount->mnt_dir, &mount_stat) != 0 ||
            mount_stat.st_dev != source_stat.st_dev) {
            continue;
        }
        current_length = strlen(mount->mnt_dir);
        if (current_length > strlen(best) && current_length < sizeof(best)) {
            memcpy(best, mount->mnt_dir, current_length + 1u);
        }
    }
    (void)endmntent(mounts);
    if (best[0] == '\0' || strlen(best) >= out_size) {
        errno = ENAMETOOLONG;
        return 0;
    }
    strcpy(out, best);
    return 1;
}

int gem_os_resolve_under(const char *path, const char *allowed_root, char *out,
                         size_t out_size)
{
    char root[PATH_MAX];
    char resolved[PATH_MAX];
    size_t length;

    if (path == NULL || allowed_root == NULL || out == NULL || out_size == 0u ||
        realpath(allowed_root, root) == NULL || realpath(path, resolved) == NULL ||
        gem_os_path_prefix(root, resolved) == 0) {
        return 0;
    }
    length = strlen(resolved);
    if (length >= out_size) {
        errno = ENAMETOOLONG;
        return 0;
    }
    memcpy(out, resolved, length + 1u);
    return 1;
}

int gem_os_resolve_entry_under(const char *path, const char *allowed_root,
                               char *out, size_t out_size)
{
    char root[PATH_MAX];
    char parent[PATH_MAX];
    char resolved_parent[PATH_MAX];
    char name[PATH_MAX];
    char *slash;
    struct stat st;
    int length;

    if (path == NULL || allowed_root == NULL || out == NULL || out_size == 0u ||
        strlen(path) >= sizeof(parent) || realpath(allowed_root, root) == NULL) {
        return 0;
    }
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (slash == NULL || slash[1] == '\0' ||
        strlen(slash + 1) >= sizeof(name)) {
        errno = EINVAL;
        return 0;
    }
    strcpy(name, slash + 1);
    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    if (realpath(parent, resolved_parent) == NULL ||
        gem_os_path_prefix(root, resolved_parent) == 0) {
        return 0;
    }
    length = snprintf(out, out_size, strcmp(resolved_parent, "/") == 0
                                         ? "/%s"
                                         : "%s/%s",
                      resolved_parent, name);
    if (length <= 0 || (size_t)length >= out_size || lstat(out, &st) != 0) {
        return 0;
    }
    return 1;
}

static int gem_os_remove_tree_at(int parent_fd, const char *name)
{
    struct stat st;

    if (fstatat(parent_fd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        return 0;
    }
    if (S_ISDIR(st.st_mode) != 0) {
        int child_fd = openat(parent_fd, name,
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        DIR *dir;
        struct dirent *entry;
        int okay = 1;

        if (child_fd < 0) {
            return 0;
        }
        dir = fdopendir(child_fd);
        if (dir == NULL) {
            (void)close(child_fd);
            return 0;
        }
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (gem_os_remove_tree_at(dirfd(dir), entry->d_name) == 0) {
                okay = 0;
                break;
            }
        }
        (void)closedir(dir);
        return okay != 0 && unlinkat(parent_fd, name, AT_REMOVEDIR) == 0;
    }
    return unlinkat(parent_fd, name, 0) == 0;
}

int gem_os_remove_tree_under(const char *path, const char *allowed_root)
{
    char root[PATH_MAX];
    char resolved[PATH_MAX];
    char relative[PATH_MAX];
    char *part;
    char *next;
    char *save = NULL;
    struct stat st;
    int directory_fd;

    if (path == NULL || allowed_root == NULL ||
        realpath(allowed_root, root) == NULL || lstat(path, &st) != 0) {
        return 0;
    }
    if (S_ISLNK(st.st_mode) != 0) {
        char parent[PATH_MAX];
        char *slash;

        if (strlen(path) >= sizeof(parent)) {
            errno = ENAMETOOLONG;
            return 0;
        }
        strcpy(parent, path);
        slash = strrchr(parent, '/');
        if (slash == NULL) {
            errno = EINVAL;
            return 0;
        }
        if (slash == parent) {
            slash[1] = '\0';
        } else {
            *slash = '\0';
        }
        if (realpath(parent, resolved) == NULL ||
            gem_os_path_prefix(root, resolved) == 0) {
            return 0;
        }
        return unlink(path) == 0;
    }
    if (realpath(path, resolved) == NULL || strcmp(root, resolved) == 0 ||
        gem_os_path_prefix(root, resolved) == 0) {
        errno = EINVAL;
        return 0;
    }
    if (strlen(resolved) >= sizeof(relative)) {
        errno = ENAMETOOLONG;
        return 0;
    }
    if (strcmp(root, "/") == 0) {
        strcpy(relative, resolved + 1);
    } else {
        strcpy(relative, resolved + strlen(root) + 1u);
    }
    directory_fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) {
        return 0;
    }
    part = strtok_r(relative, "/", &save);
    if (part == NULL) {
        (void)close(directory_fd);
        return 0;
    }
    next = strtok_r(NULL, "/", &save);
    while (next != NULL) {
        int next_fd = openat(directory_fd, part,
                             O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

        (void)close(directory_fd);
        if (next_fd < 0) {
            return 0;
        }
        directory_fd = next_fd;
        part = next;
        next = strtok_r(NULL, "/", &save);
    }
    {
        int result = gem_os_remove_tree_at(directory_fd, part);

        (void)close(directory_fd);
        return result;
    }
}

int gem_os_local_timestamp(char *out, size_t out_size)
{
    time_t now;
    struct tm local;

    if (out == NULL || out_size == 0u) {
        errno = EINVAL;
        return 0;
    }
    now = time(NULL);
    if (now == (time_t)-1 || localtime_r(&now, &local) == NULL) {
        return 0;
    }
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%S", &local) != 0u;
}

int gem_os_format_timestamp(int64_t seconds, const char *format, char *out,
                            size_t out_size)
{
    time_t value = (time_t)seconds;
    struct tm local;

    if (format == NULL || out == NULL || out_size == 0u ||
        localtime_r(&value, &local) == NULL) {
        return 0;
    }
    return strftime(out, out_size, format, &local) != 0u;
}

int gem_os_mount_iter_open(gem_os_mount_iter_t *iter)
{
    FILE *mounts;

    if (iter == NULL) {
        errno = EINVAL;
        return 0;
    }
    mounts = setmntent("/proc/mounts", "r");
    if (mounts == NULL) {
        return 0;
    }
    iter->handle = mounts;
    return 1;
}

int gem_os_mount_iter_read(gem_os_mount_iter_t *iter, char *path,
                           size_t path_size)
{
    struct mntent *mount;
    size_t length;

    if (iter == NULL || iter->handle == NULL || path == NULL ||
        path_size == 0u) {
        errno = EINVAL;
        return 0;
    }
    for (;;) {
        mount = getmntent((FILE *)iter->handle);
        if (mount == NULL) {
            return 0;
        }
        if (mount->mnt_dir == NULL) {
            continue;
        }
        length = strlen(mount->mnt_dir);
        if (length < path_size) {
            break;
        }
    }
    memcpy(path, mount->mnt_dir, length + 1u);
    return 1;
}

void gem_os_mount_iter_close(gem_os_mount_iter_t *iter)
{
    if (iter == NULL || iter->handle == NULL) {
        return;
    }
    (void)endmntent((FILE *)iter->handle);
    iter->handle = NULL;
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

    handle = (DIR *)dir->handle;
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

        rc = snprintf(full_path, sizeof(full_path), "%s/%s", dir->path,
                      dent->d_name);
        if (rc < 0 || (size_t)rc >= sizeof(full_path)) {
            continue;
        }
        if (lstat(full_path, &st) != 0) {
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

    (void)closedir((DIR *)dir->handle);
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
        *total_bytes = (uint64_t)st.f_blocks * (uint64_t)st.f_frsize;
    }
    if (avail_bytes != NULL) {
        *avail_bytes = (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
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

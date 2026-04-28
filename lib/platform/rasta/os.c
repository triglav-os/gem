/*
 * Implements the GEM operating system abstraction for Linux. The
 * backend provides allocation, timing, sleeping, and simple POSIX file
 * descriptor wrappers used by higher GEM layers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "platform/os.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
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

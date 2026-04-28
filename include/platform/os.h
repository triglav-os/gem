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

#ifdef __cplusplus
}
#endif

#endif

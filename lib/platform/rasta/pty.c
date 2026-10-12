/*
 * Runs an interactive shell behind a pseudo-terminal for the GEM terminal
 * sample: spawning with a controlling tty and window size, nonblocking
 * reads and writes, liveness polling and bounded termination.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE
#include "platform/os.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int gem_os_pty_apply_size(int fd, uint16_t columns, uint16_t rows)
{
    struct winsize ws;

    if (fd < 0) {
        errno = EINVAL;
        return 0;
    }

    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)((columns > 0u) ? columns : 80u);
    ws.ws_row = (unsigned short)((rows > 0u) ? rows : 25u);
    return (ioctl(fd, TIOCSWINSZ, &ws) == 0) ? 1 : 0;
}

static void gem_os_pty_clear_host_terminal(void)
{
    static const char *const names[] = {
        "COLORTERM",          "COLORFGBG",       "TERM_PROGRAM",
        "TERM_PROGRAM_VERSION", "VTE_VERSION",    "KONSOLE_VERSION",
        "KONSOLE_DBUS_SERVICE", "KITTY_WINDOW_ID", "KITTY_PID",
        "WEZTERM_EXECUTABLE", "WEZTERM_PANE",    "WT_SESSION",
        "TMUX",               "TMUX_PANE",       "STY"};
    size_t index;

    for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        (void)unsetenv(names[index]);
    }
}

int gem_os_pty_spawn_shell(gem_os_pty_t *pty, const char *shell_path,
                           const char *cwd, uint16_t columns, uint16_t rows)
{
    int master_fd;
    int slave_fd = -1;
    char *slave_name;
    pid_t pid;
    const char *shell;
    char lines_text[16];
    char columns_text[16];

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
        (void)close(master_fd);
        return 0;
    }

    slave_name = ptsname(master_fd);
    if (slave_name == NULL) {
        (void)close(master_fd);
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        (void)close(master_fd);
        return 0;
    }

    if (pid == 0) {
        shell = shell_path;
        if (shell == NULL || shell[0] == '\0') {
            shell = getenv("SHELL");
        }
        /*
         * Prefer bash when nothing is requested: Gemix ships bash as
         * the interactive shell, and a bare /bin/sh (dash/busybox)
         * is a poor default for the GEM terminal.
         */
        if (shell == NULL || shell[0] == '\0') {
            if (access("/bin/bash", X_OK) == 0) {
                shell = "/bin/bash";
            } else {
                shell = "/bin/sh";
            }
        }

        (void)signal(SIGINT, SIG_DFL);
        (void)signal(SIGTERM, SIG_DFL);
        (void)signal(SIGHUP, SIG_DFL);
        (void)signal(SIGCHLD, SIG_DFL);

        (void)close(master_fd);
        if (setsid() < 0) {
            _exit(127);
        }

        slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            _exit(127);
        }
        (void)gem_os_pty_apply_size(slave_fd, columns, rows);
        (void)ioctl(slave_fd, TIOCSCTTY, 0);
        (void)dup2(slave_fd, STDIN_FILENO);
        (void)dup2(slave_fd, STDOUT_FILENO);
        (void)dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO) {
            (void)close(slave_fd);
        }

        if (cwd != NULL && cwd[0] != '\0' && chdir(cwd) != 0) {
            _exit(127);
        }
        (void)snprintf(lines_text, sizeof(lines_text), "%u", (unsigned)rows);
        (void)snprintf(columns_text, sizeof(columns_text), "%u",
                       (unsigned)columns);
        gem_os_pty_clear_host_terminal();
        (void)setenv("TERM", "vt100", 1);
        (void)setenv("LINES", lines_text, 1);
        (void)setenv("COLUMNS", columns_text, 1);
        execl(shell, shell, "-i", (char *)NULL);
        _exit(127);
    }

    pty->master_fd = master_fd;
    pty->child_pid = (int)pid;
    (void)gem_os_pty_apply_size(master_fd, columns, rows);
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

    rc = read(pty->master_fd, buf, (size_t)size);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return (int32_t)rc;
}

int32_t gem_os_pty_write(gem_os_pty_t *pty, const void *buf, uint32_t size)
{
    ssize_t rc;

    if (pty == NULL || pty->master_fd < 0 || (buf == NULL && size != 0u)) {
        errno = EINVAL;
        return -1;
    }

    rc = write(pty->master_fd, buf, (size_t)size);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return (int32_t)rc;
}

int gem_os_pty_is_alive(gem_os_pty_t *pty)
{
    pid_t rc;
    int status;

    if (pty == NULL || pty->child_pid <= 0) {
        return 0;
    }

    rc = waitpid((pid_t)pty->child_pid, &status, WNOHANG);
    if (rc == 0) {
        return 1;
    }
    if (rc == (pid_t)pty->child_pid) {
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
        pid_t child = (pid_t)pty->child_pid;
        unsigned attempt;

        /* A shell that traps SIGHUP (or a job under nohup) would otherwise
         * block the closing application forever. Escalate after a grace
         * period; the pty slave is going away regardless. */
        (void)kill(child, SIGHUP);
        for (attempt = 0; attempt < 50u; ++attempt) {
            if (waitpid(child, NULL, WNOHANG) != 0) {
                break;
            }
            gem_os_sleep_ms(10u);
        }
        if (attempt == 50u) {
            (void)kill(child, SIGKILL);
            (void)waitpid(child, NULL, 0);
        }
        pty->child_pid = -1;
    }
    if (pty->master_fd >= 0) {
        (void)close(pty->master_fd);
        pty->master_fd = -1;
    }
}

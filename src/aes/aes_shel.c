/*
 * Implements GEM AES shell services with a mutex-protected DESKTOP.INF view,
 * per-child launch records and host process work delegated to platform/os.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_shel.h"

#include "aes_internal.h"
#include "desktop_inf.h"
#include "platform/os.h"

#include <stdint.h>
#include <string.h>

enum { AES_SHEL_LAUNCH_MAX = 32, AES_SHEL_TAIL_MAX = 127 };

typedef struct aes_shel_state {
    gem_os_mutex_t mutex;
    int initialized;
    int caller_pid;
    char default_cmd[256];
    char default_dir[AES_PATH_LEN];
    shel_launch_t launches[AES_SHEL_LAUNCH_MAX];
} aes_shel_state_t;

static aes_shel_state_t shell_state;

void (*aes_shel_config_changed_hook)(void);

static int aes_shel_ensure_initialized(void)
{
    if (shell_state.initialized != 0) {
        return 1;
    }
    if (gem_os_mutex_init(&shell_state.mutex) == 0) {
        return 0;
    }
    shell_state.initialized = 1;
    return 1;
}

void aes_shel_reset(void)
{
    if (aes_shel_ensure_initialized() == 0) {
        return;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    memset(shell_state.launches, 0, sizeof(shell_state.launches));
    shell_state.caller_pid = 0;
    shell_state.default_cmd[0] = '\0';
    shell_state.default_dir[0] = '\0';
    gem_os_mutex_unlock(&shell_state.mutex);
}

void aes_shel_set_caller_pid(int pid)
{
    shell_state.caller_pid = pid;
}

static int aes_shel_id_is_reserved(WORD id)
{
    size_t index;

    for (index = 0u; index < AES_SHEL_LAUNCH_MAX; ++index) {
        if (shell_state.launches[index].appl_id == id) {
            return 1;
        }
    }
    return 0;
}

static WORD aes_shel_reserve_id(void)
{
    WORD candidate = aes_state.next_app_id;
    unsigned attempts;

    if (candidate <= 0) {
        candidate = 1;
    }
    for (attempts = 0u; attempts < 32767u; ++attempts) {
        if (aes_find_app_by_id(candidate) == NULL &&
            aes_shel_id_is_reserved(candidate) == 0) {
            aes_state.next_app_id =
                (candidate == 32767) ? 1 : (WORD)(candidate + 1);
            return candidate;
        }
        candidate = (candidate == 32767) ? 1 : (WORD)(candidate + 1);
    }
    return 0;
}

static int aes_shel_parse_id(const char *text)
{
    int value = 0;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    while (*text != '\0') {
        if (*text < '0' || *text > '9' || value > 3276) {
            return 0;
        }
        value = value * 10 + (*text - '0');
        ++text;
    }
    return (value > 0 && value <= 32767) ? value : 0;
}

WORD aes_shel_claim_app_id(void)
{
    char text[16];
    int pid = shell_state.caller_pid;
    int requested = 0;
    size_t index;
    WORD result = 0;

    if (aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    if (pid <= 0) {
        pid = gem_os_getpid();
    }
    if (gem_os_process_getenv(pid, "GEM_APPL_ID", text, sizeof(text)) != 0) {
        requested = aes_shel_parse_id(text);
    }
    gem_os_mutex_lock(&shell_state.mutex);
    for (index = 0u; index < AES_SHEL_LAUNCH_MAX; ++index) {
        shel_launch_t *launch = &shell_state.launches[index];

        if ((requested != 0 && launch->appl_id == requested &&
             (launch->pid == 0 || launch->pid == pid)) ||
            (requested == 0 && launch->pid == pid)) {
            launch->pid = pid;
            result = (WORD)launch->appl_id;
            break;
        }
    }
    gem_os_mutex_unlock(&shell_state.mutex);
    return result;
}

void aes_shel_reap_children(void)
{
    size_t index;

    if (aes_shel_ensure_initialized() == 0) {
        return;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    for (index = 0u; index < AES_SHEL_LAUNCH_MAX; ++index) {
        shel_launch_t *launch = &shell_state.launches[index];

        if (launch->pid > 0 && gem_os_reap_process(launch->pid) != 0) {
            memset(launch, 0, sizeof(*launch));
        }
    }
    gem_os_mutex_unlock(&shell_state.mutex);
}

static shel_launch_t *aes_shel_new_launch(void)
{
    size_t index;

    for (index = 0u; index < AES_SHEL_LAUNCH_MAX; ++index) {
        if (shell_state.launches[index].appl_id == 0) {
            return &shell_state.launches[index];
        }
    }
    return NULL;
}

WORD shel_write(WORD doex, WORD isgr, WORD isover, char *cmd, char *tail)
{
    shel_launch_t *launch;
    gem_os_pid_t pid;
    size_t cmd_length;
    size_t tail_length;
    WORD app_id;

    (void)isgr;
    (void)isover;
    if (doex == 0) {
        return 1;
    }
    if (doex != 1 || cmd == NULL || cmd[0] == '\0' ||
        aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    cmd_length = strlen(cmd);
    tail_length = (tail != NULL) ? (unsigned char)tail[0] : 0u;
    if (cmd_length >= sizeof(shell_state.launches[0].cmd) ||
        tail_length > AES_SHEL_TAIL_MAX) {
        return 0;
    }
    aes_shel_reap_children();
    gem_os_mutex_lock(&shell_state.mutex);
    launch = aes_shel_new_launch();
    app_id = aes_shel_reserve_id();
    if (launch == NULL || app_id == 0) {
        gem_os_mutex_unlock(&shell_state.mutex);
        return 0;
    }
    memset(launch, 0, sizeof(*launch));
    launch->appl_id = app_id;
    memcpy(launch->cmd, cmd, cmd_length + 1u);
    launch->tail[0] = (char)tail_length;
    if (tail_length != 0u) {
        memcpy(launch->tail + 1, tail + 1, tail_length);
    }
    gem_os_mutex_unlock(&shell_state.mutex);

    if (gem_os_spawn_gem(cmd, cmd, launch->tail + 1, tail_length, app_id,
                         &pid) == 0) {
        gem_os_mutex_lock(&shell_state.mutex);
        memset(launch, 0, sizeof(*launch));
        gem_os_mutex_unlock(&shell_state.mutex);
        return 0;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    launch->pid = pid;
    gem_os_mutex_unlock(&shell_state.mutex);
    return 1;
}

WORD shel_read(char *cmd, char *tail)
{
    shel_launch_t launch;
    unsigned char raw_tail[AES_SHEL_TAIL_MAX];
    size_t raw_length = 0u;
    size_t index;
    int pid = shell_state.caller_pid;
    int found = 0;

    if (cmd == NULL || tail == NULL || aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    if (pid <= 0) {
        pid = gem_os_getpid();
    }
    memset(&launch, 0, sizeof(launch));
    gem_os_mutex_lock(&shell_state.mutex);
    for (index = 0u; index < AES_SHEL_LAUNCH_MAX; ++index) {
        const shel_launch_t *candidate = &shell_state.launches[index];

        if ((aes_state.current_app_id > 0 &&
             candidate->appl_id == aes_state.current_app_id) ||
            candidate->pid == pid) {
            launch = *candidate;
            found = 1;
            break;
        }
    }
    gem_os_mutex_unlock(&shell_state.mutex);
    if (found != 0) {
        memcpy(cmd, launch.cmd, sizeof(launch.cmd));
        memcpy(tail, launch.tail, sizeof(launch.tail));
        return 1;
    }
    if (gem_os_process_launch(pid, cmd, 256u, raw_tail, sizeof(raw_tail),
                              &raw_length) == 0 ||
        raw_length > AES_SHEL_TAIL_MAX) {
        cmd[0] = '\0';
        tail[0] = 0;
        return 0;
    }
    tail[0] = (char)raw_length;
    if (raw_length != 0u) {
        memcpy(tail + 1, raw_tail, raw_length);
    }
    return 1;
}

WORD shel_get(char *buf, WORD length)
{
    unsigned char *desktop_inf;
    size_t desktop_inf_size = 0u;
    size_t copy_size;
    WORD result = 0;

    if (aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    desktop_inf = gem_os_alloc(DESKTOP_INF_DATA_MAX);
    if (desktop_inf == NULL) {
        return 0;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    if (!desktop_inf_config_get(desktop_inf, DESKTOP_INF_DATA_MAX,
                                &desktop_inf_size)) {
        goto done;
    }
    if (length == SHEL_BUFSIZE) {
        result = (WORD)desktop_inf_size;
        goto done;
    }
    if (buf == NULL || length < 0) {
        goto done;
    }
    copy_size = (size_t)length;
    if (copy_size > desktop_inf_size) {
        copy_size = desktop_inf_size;
    }
    if (copy_size != 0u) {
        memcpy(buf, desktop_inf, copy_size);
    }
    result = 1;

done:
    gem_os_mutex_unlock(&shell_state.mutex);
    gem_os_free(desktop_inf);
    return result;
}

WORD shel_put(char *buf, WORD length)
{
    int result;

    if (length <= 0 || buf == NULL || aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    result = desktop_inf_config_put((const unsigned char *)buf, (size_t)length);
    gem_os_mutex_unlock(&shell_state.mutex);
    if (result != 0 && aes_shel_config_changed_hook != NULL)
        aes_shel_config_changed_hook();
    return result != 0 ? 1 : 0;
}

WORD shel_find(char *path)
{
    char resolved[AES_PATH_LEN];

    if (path == NULL ||
        gem_os_find_file(path, resolved, sizeof(resolved)) == 0) {
        return 0;
    }
    strcpy(path, resolved);
    return 1;
}

WORD shel_envrn(char **env, char *var)
{
    const char *value;

    if (env == NULL || var == NULL) {
        return 0;
    }
    value = gem_os_getenv_ref(var);
    if (value == NULL) {
        *env = NULL;
        return 0;
    }
    *env = (char *)value;
    return 1;
}

WORD shel_rdef(char *lpcmd, char *lpdir)
{
    if (aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    if (lpcmd != NULL) {
        strcpy(lpcmd, shell_state.default_cmd);
    }
    if (lpdir != NULL) {
        strcpy(lpdir, shell_state.default_dir);
    }
    gem_os_mutex_unlock(&shell_state.mutex);
    return 1;
}

WORD shel_wdef(char *lpcmd, char *lpdir)
{
    if (aes_shel_ensure_initialized() == 0) {
        return 0;
    }
    gem_os_mutex_lock(&shell_state.mutex);
    if (lpcmd != NULL) {
        strncpy(shell_state.default_cmd, lpcmd,
                sizeof(shell_state.default_cmd) - 1u);
        shell_state.default_cmd[sizeof(shell_state.default_cmd) - 1u] = '\0';
    }
    if (lpdir != NULL) {
        strncpy(shell_state.default_dir, lpdir,
                sizeof(shell_state.default_dir) - 1u);
        shell_state.default_dir[sizeof(shell_state.default_dir) - 1u] = '\0';
    }
    gem_os_mutex_unlock(&shell_state.mutex);
    return 1;
}

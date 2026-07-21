/*
 * Exposes the core hosted AES application, scrap, and shell
 * entry points while leaving windowing and object logic in smaller
 * companion modules.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "platform/os.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
WORD contrl[12] __attribute__((weak));
WORD intin[128] __attribute__((weak));
WORD ptsin[256] __attribute__((weak));
WORD intout[128] __attribute__((weak));
WORD ptsout[256] __attribute__((weak));

WORD global[15];

static void _aes_union_message_rects(WORD target[8], const WORD incoming[8]);

WORD _aes_dequeue_message(WORD msg[8]);
WORD appl_init(void);
WORD appl_exit(void);
WORD appl_find(char *name);
WORD appl_bvset(WORD bvdisk, WORD bvhard);
WORD appl_write(WORD ap_wid, WORD ap_wlength, void *ap_wpbuff);
WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff);
WORD appl_tplay(void *pbuff, WORD length, WORD scale);
WORD appl_trecord(void *pbuff, WORD length);
WORD appl_yield(void);
WORD scrp_read(char *pscrap);
WORD scrp_write(char *pscrap);
WORD scrp_clear(void);
WORD shel_read(char *cmd, char *tail);
WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail);
WORD shel_get(char *buf, WORD length);
WORD shel_put(char *buf, WORD length);
WORD shel_find(char *path);
WORD shel_envrn(char **env, char *var);
WORD shel_rdef(char *lpcmd, char *lpdir);
WORD shel_wdef(char *lpcmd, char *lpdir);

WORD _aes_dequeue_message(WORD msg[8])
{
    if (msg == NULL) {
        return 0;
    }

    return appl_read(_aes.current_app_id, 8, msg);
}

WORD appl_init(void)
{
    size_t i;

    if (_aes.initialized == 0) {
        _aes_reset_state();
        _aes.initialized = 1;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used == 0) {
            _aes.apps[i].used = 1;
            _aes.apps[i].id = _aes.next_app_id++;
            strcpy(_aes.apps[i].name, "APP");
            _aes.current_app_id = _aes.apps[i].id;
            global[2] = _aes.current_app_id;
            _aes_trace("appl_init return id=%d", _aes.current_app_id);
            return _aes.current_app_id;
        }
    }
    _aes_trace("appl_init failed");
    return 0;
}

WORD appl_exit(void)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);
    size_t i;
    int apps_left = 0;

    if (app != NULL) {
        memset(app, 0, sizeof(*app));
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0) {
            apps_left = 1;
            break;
        }
    }

    if (!apps_left && _aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
        v_clsvwk(_aes.vdi_handle);
        _aes.vdi_handle = 0;
        _aes.vdi_ready = 0;
    }
    _aes.current_app_id = 0;
    global[2] = 0;
    return 1;
}

WORD appl_find(char *name)
{
    size_t i;

    if (name == NULL) {
        return 0;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0 &&
            strncmp(_aes.apps[i].name, name, 8) == 0) {
            return _aes.apps[i].id;
        }
    }
    return 0;
}

WORD appl_bvset(WORD bvdisk, WORD bvhard)
{
    _aes.desktop_bvdisk = bvdisk;
    _aes.desktop_bvhard = bvhard;
    return 1;
}

WORD appl_write(WORD ap_wid, WORD ap_wlength, void *ap_wpbuff)
{
    size_t i;
    WORD *words = (WORD *) ap_wpbuff;

    if (ap_wpbuff == NULL || ap_wlength < 8) {
        return 0;
    }

    if (words[0] == WM_REDRAW) {
        for (i = 0; i < AES_MAX_MESSAGES; ++i) {
            if (_aes.messages[i].used != 0 &&
                _aes.messages[i].dest == ap_wid &&
                _aes.messages[i].data[0] == WM_REDRAW &&
                _aes.messages[i].data[3] == words[3]) {
                _aes_union_message_rects(_aes.messages[i].data, words);
                return 1;
            }
        }
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used == 0) {
            _aes.messages[i].used = 1;
            _aes.messages[i].dest = ap_wid;
            _aes.messages[i].seq = _aes.next_message_seq++;
            memcpy(_aes.messages[i].data, words,
                sizeof(_aes.messages[i].data));
            return 1;
        }
    }
    _aes_trace("appl_write queue full: dropped message dest=%d type=%d",
        ap_wid, words[0]);
    return 0;
}

WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff)
{
    size_t i;
    size_t best_control = AES_MAX_MESSAGES;
    size_t best_redraw = AES_MAX_MESSAGES;
    uint32_t best_control_seq = 0u;
    uint32_t best_redraw_seq = 0u;

    if (ap_rpbuff == NULL || ap_rlength < 8) {
        return 0;
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used == 0 || _aes.messages[i].dest != ap_rwid) {
            continue;
        }

        if (_aes.messages[i].data[0] == WM_REDRAW) {
            if (best_redraw == AES_MAX_MESSAGES ||
                _aes.messages[i].seq < best_redraw_seq) {
                best_redraw = i;
                best_redraw_seq = _aes.messages[i].seq;
            }
        } else {
            if (best_control == AES_MAX_MESSAGES ||
                _aes.messages[i].seq < best_control_seq) {
                best_control = i;
                best_control_seq = _aes.messages[i].seq;
            }
        }
    }

    if (best_control != AES_MAX_MESSAGES) {
        memcpy(ap_rpbuff, _aes.messages[best_control].data,
            sizeof(_aes.messages[best_control].data));
        _aes.messages[best_control].used = 0;
        _aes.messages[best_control].seq = 0u;
        return 1;
    }

    if (best_redraw != AES_MAX_MESSAGES) {
        memcpy(ap_rpbuff, _aes.messages[best_redraw].data,
            sizeof(_aes.messages[best_redraw].data));
        _aes.messages[best_redraw].used = 0;
        _aes.messages[best_redraw].seq = 0u;
        return 1;
    }

    return 0;
}

static void _aes_union_message_rects(WORD target[8], const WORD incoming[8])
{
    WORD target_x1;
    WORD target_y1;
    WORD incoming_x1;
    WORD incoming_y1;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (target == NULL || incoming == NULL) {
        return;
    }

    target_x1 = (WORD) (target[4] + target[6] - 1);
    target_y1 = (WORD) (target[5] + target[7] - 1);
    incoming_x1 = (WORD) (incoming[4] + incoming[6] - 1);
    incoming_y1 = (WORD) (incoming[5] + incoming[7] - 1);
    x0 = _aes_min_word(target[4], incoming[4]);
    y0 = _aes_min_word(target[5], incoming[5]);
    x1 = _aes_max_word(target_x1, incoming_x1);
    y1 = _aes_max_word(target_y1, incoming_y1);
    target[4] = x0;
    target[5] = y0;
    target[6] = (WORD) (x1 - x0 + 1);
    target[7] = (WORD) (y1 - y0 + 1);
}

WORD appl_tplay(void *pbuff, WORD length, WORD scale)
{
    (void) pbuff;
    (void) length;
    (void) scale;
    return 1;
}

WORD appl_trecord(void *pbuff, WORD length)
{
    if (pbuff != NULL && length > 0) {
        memset(pbuff, 0, (size_t) length);
    }
    return 1;
}

WORD appl_yield(void)
{
    gem_os_sleep_ms(1u);
    return 1;
}

WORD scrp_read(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strcpy(pscrap, _aes.scrap_path);
    return (_aes.scrap_path[0] != '\0') ? 1 : 0;
}

WORD scrp_write(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strncpy(_aes.scrap_path, pscrap, sizeof(_aes.scrap_path) - 1u);
    _aes.scrap_path[sizeof(_aes.scrap_path) - 1u] = '\0';
    return 1;
}

WORD scrp_clear(void)
{
    _aes.scrap_path[0] = '\0';
    return 1;
}

WORD shel_read(char *cmd, char *tail)
{
    if (cmd != NULL) {
        strcpy(cmd, _aes.shell_cmd);
    }
    if (tail != NULL) {
        strcpy(tail, _aes.shell_tail);
    }
    return 1;
}

WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail)
{
    (void) doex;
    (void) isgr;
    (void) iscr;

    if (cmd != NULL) {
        strncpy(_aes.shell_cmd, cmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (tail != NULL) {
        strncpy(_aes.shell_tail, tail, sizeof(_aes.shell_tail) - 1u);
        _aes.shell_tail[sizeof(_aes.shell_tail) - 1u] = '\0';
    }
    return 1;
}

WORD shel_get(char *buf, WORD length)
{
    if (_aes.shell_buf_len == 0) {
        void *data = NULL;
        size_t size = 0u;
        char resolved[260];

        if (_aes_try_resolve_path("DESKTOP.INF", resolved, sizeof(resolved)) &&
            _aes_load_file(resolved, &data, &size)) {
            if (size >= sizeof(_aes.shell_buf)) {
                size = sizeof(_aes.shell_buf) - 1u;
            }
            memcpy(_aes.shell_buf, data, size);
            _aes.shell_buf[size] = '\0';
            _aes.shell_buf_len = (WORD) size;
            gem_os_free(data);
        }
    }

    if (buf == NULL || length <= 0) {
        return 0;
    }

    if ((size_t) length > sizeof(_aes.shell_buf)) {
        length = (WORD) sizeof(_aes.shell_buf);
    }
    memcpy(buf, _aes.shell_buf, (size_t) length);
    return 1;
}

WORD shel_put(char *buf, WORD length)
{
    int fd;

    if (buf == NULL || length < 0) {
        return 0;
    }

    if ((size_t) length >= sizeof(_aes.shell_buf)) {
        length = (WORD) (sizeof(_aes.shell_buf) - 1u);
    }
    memcpy(_aes.shell_buf, buf, (size_t) length);
    _aes.shell_buf[length] = '\0';
    _aes.shell_buf_len = length;

    fd = gem_os_open_write("bin/desktop.inf");
    if (fd >= 0) {
        (void) gem_os_write(fd, _aes.shell_buf,
            (uint32_t) _aes.shell_buf_len);
        (void) gem_os_close(fd);
    }

    return 1;
}

WORD shel_find(char *path)
{
    char resolved[260];

    if (path == NULL) {
        return 0;
    }
    if (_aes_try_resolve_path(path, resolved, sizeof(resolved))) {
        strcpy(path, resolved);
        return 1;
    }
    return 0;
}

WORD shel_envrn(char **env, char *var)
{
    char *value;

    if (env == NULL || var == NULL) {
        return 0;
    }

    value = getenv(var);
    if (value == NULL) {
        return 0;
    }
    *env = value;
    return 1;
}

WORD shel_rdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strcpy(lpcmd, _aes.shell_cmd);
    }
    if (lpdir != NULL) {
        strcpy(lpdir, _aes.shell_dir);
    }
    return 1;
}

WORD shel_wdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strncpy(_aes.shell_cmd, lpcmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (lpdir != NULL) {
        strncpy(_aes.shell_dir, lpdir, sizeof(_aes.shell_dir) - 1u);
        _aes.shell_dir[sizeof(_aes.shell_dir) - 1u] = '\0';
    }
    return 1;
}

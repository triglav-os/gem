/*
 * Implements hosted GEM VDI input, mouse, and callback entry points.
 * These routines isolate locator, string, and cursor handling from the
 * drawing-heavy parts of the public VDI surface.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include "platform/hid.h"
#include "platform/os.h"
#include "platform/raster.h"

VOID v_hide_c(VDI_HANDLE handle)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    if (_vdi.cursor_hidden == 0) {
        _vdi.cursor_hidden = 1;
        _vdi_prepare_screen_write();
        if (_vdi.update_depth == 0) {
            gem_raster_present();
        } else {
            _vdi.present_pending = 1;
        }
    }
}

VOID v_show_c(VDI_HANDLE handle, WORD reset)
{
    (void) reset;

    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    if (_vdi.cursor_hidden != 0) {
        _vdi.cursor_hidden = 0;
        if (_vdi.update_depth == 0) {
            _vdi_present_screen();
        } else {
            _vdi.present_pending = 1;
        }
    }
}

VOID vq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        if (status != NULL) {
            *status = 0;
        }
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        return;
    }

    _vdi_pump_events();
    if (status != NULL) {
        *status = _vdi.mouse_status;
    }
    if (x != NULL) {
        *x = _vdi.mouse_x;
    }
    if (y != NULL) {
        *y = _vdi.mouse_y;
    }
}

VOID vrq_string(VDI_HANDLE handle, WORD max_length, WORD echo_mode,
    WORD *echo_xy, BYTE *out_string)
{
    WORD length = 0;
    WORD echo_x = 0;
    WORD echo_y = 0;

    if (handle != _vdi_handle_screen || !_vdi.open || out_string == NULL ||
        max_length < 0) {
        return;
    }

    if (echo_xy != NULL) {
        echo_x = echo_xy[0];
        echo_y = echo_xy[1];
    }

    FOREVER {
        gem_hid_event_t evt;

        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            _vdi.mouse_x = evt.x;
            _vdi.mouse_y = evt.y;
            _vdi.mouse_status = (WORD) evt.flags;
            continue;
        }
        if (evt.type != GEM_HID_KEY || (evt.flags & 1u) == 0u) {
            continue;
        }

        {
            char ch = (char) (evt.key & 0xffu);

            if (ch == '\0') {
                continue;
            }
            if (ch == '\n') {
                out_string[length] = '\0';
                return;
            }
            if (ch == '\b') {
                if (length > 0) {
                    --length;
                    out_string[length] = '\0';
                    if (echo_mode != 0 && echo_xy != NULL) {
                        _vdi_fill_rect(echo_x, (WORD) (echo_y - 7),
                            (WORD) (echo_x + max_length *
                            _vdi_font_cell_width()), echo_y, 0);
                        v_gtext(handle, echo_x, echo_y, out_string);
                    }
                }
                continue;
            }
            if (length >= max_length) {
                continue;
            }

            out_string[length++] = (BYTE) ch;
            out_string[length] = '\0';
            if (echo_mode != 0 && echo_xy != NULL) {
                v_gtext(handle, echo_x, echo_y, out_string);
            }
        }
    }
}

WORD vrq_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
    WORD *term)
{
    (void) x;
    (void) y;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_pump_events();
    if (xout != NULL) {
        *xout = _vdi.mouse_x;
    }
    if (yout != NULL) {
        *yout = _vdi.mouse_y;
    }
    if (term != NULL) {
        *term = _vdi.mouse_status;
    }
    return 1;
}

WORD vsm_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
    WORD *term)
{
    return vrq_locator(handle, x, y, xout, yout, term);
}

WORD vrq_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }
    if (val_out != NULL) {
        *val_out = val_in;
    }
    if (term != NULL) {
        *term = 1;
    }
    return 1;
}

WORD vsm_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term)
{
    return vrq_valuator(handle, val_in, val_out, term);
}

WORD vrq_choice(WORD handle, WORD ch_in, WORD *ch_out)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }
    if (ch_out != NULL) {
        *ch_out = ch_in;
    }
    return 1;
}

WORD vsm_choice(WORD handle, WORD *choice)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }
    if (choice != NULL) {
        *choice = 1;
    }
    return 1;
}

WORD vsm_string(WORD handle, WORD length, WORD echo_mode, WORD *echoxy,
    BYTE *string)
{
    if (!_vdi_valid_handle(handle) || string == NULL) {
        return 0;
    }

    vrq_string(handle, length, echo_mode, echoxy, string);
    return 1;
}

WORD vsin_mode(WORD handle, WORD dev_type, WORD mode)
{
    if (!_vdi_valid_handle(handle) || dev_type < 0 ||
        dev_type >= VDI_INPUT_DEVICES) {
        return 0;
    }

    _vdi_compat.input_mode[dev_type] = mode;
    return mode;
}

WORD vqin_mode(WORD handle, WORD dev_type, WORD *input_mode)
{
    if (!_vdi_valid_handle(handle) || input_mode == NULL || dev_type < 0 ||
        dev_type >= VDI_INPUT_DEVICES) {
        return 0;
    }

    *input_mode = _vdi_compat.input_mode[dev_type];
    return 1;
}

WORD vsc_form(WORD handle, MFORM *cur_form)
{
    if (!_vdi_valid_handle(handle) || cur_form == NULL) {
        return 0;
    }

    return _vdi_set_mouse_form(cur_form);
}

WORD vex_timv(WORD handle, VOID (*tim_addr)(void), VOID (**old_addr)(void),
    WORD *scale)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (old_addr != NULL) {
        *old_addr = _vdi_compat.timv;
    }
    _vdi_compat.timv = tim_addr;
    if (scale != NULL) {
        *scale = 1;
    }
    return 1;
}

WORD vex_butv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void))
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (sav_code != NULL) {
        *sav_code = _vdi_compat.butv;
    }
    _vdi_compat.butv = usr_code;
    return 1;
}

WORD vex_motv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void))
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (sav_code != NULL) {
        *sav_code = _vdi_compat.motv;
    }
    _vdi_compat.motv = usr_code;
    return 1;
}

WORD vex_curv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void))
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (sav_code != NULL) {
        *sav_code = _vdi_compat.curv;
    }
    _vdi_compat.curv = usr_code;
    return 1;
}

WORD vq_key_s(WORD handle, WORD *status)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (status != NULL) {
        *status = 0;
    }
    return 1;
}

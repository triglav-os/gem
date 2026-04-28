/*
 * Implements a minimal GEM VDI subset on top of the platform raster,
 * input, and operating system abstractions. This workstation is limited
 * to one monochrome screen-backed virtual workstation and the small set
 * of primitives declared in `gem/vdi.h`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_vdi.h"

#include "platform/hid.h"
#include "platform/os.h"
#include "platform/raster.h"

#include <stdlib.h>
#include <string.h>

/*
 * Opens the single virtual workstation supported by this minimal VDI
 * layer.
 * `work_in` is accepted for GEM compatibility but is currently ignored.
 * `handle` receives the workstation handle on success, or zero on
 * failure.
 * `work_out` receives basic device capabilities such as screen size and
 * text metrics.
 * Side effects: initializes the OS, raster, and input backends if they
 * are not already open, clears the screen to black, and presents the
 * initial frame.
 */
VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57])
{
    WORD width = _vdi_parse_env_word("GEM_VDI_WIDTH", _vdi_default_width);
    WORD height = _vdi_parse_env_word("GEM_VDI_HEIGHT",
        _vdi_default_height);

    (void) work_in;

    if (handle == NULL || work_out == NULL) {
        return;
    }

    if (!_vdi.open) {
        if (!gem_os_init()) {
            *handle = 0;
            return;
        }
        if (!gem_raster_init((uint16_t) width, (uint16_t) height,
            GEM_RASTER_INDEX8)) {
            gem_os_shutdown();
            *handle = 0;
            return;
        }
        (void) gem_hid_init();

        memset(&_vdi, 0, sizeof(_vdi));
        _vdi.surface = gem_raster_surface();
        _vdi.width = width;
        _vdi.height = height;
        _vdi.line_color = 1;
        _vdi.fill_color = 1;
        _vdi.text_color = 1;
        _vdi.open = 1;

        gem_raster_set_palette(0u, 0u, 0u, 0u);
        gem_raster_set_palette(1u, 255u, 255u, 255u);
        _vdi_clear_screen(0);
        _vdi_present_screen();
    }

    *handle = _vdi_handle_screen;
    _vdi_fill_work_out(work_out);
}

/*
 * Closes the virtual workstation identified by `handle`.
 * `handle` must be the single screen workstation returned by
 * `v_opnvwk()`.
 * Returns no value.
 * Side effects: shuts down input, raster, and OS services and resets
 * all private VDI state.
 */
VOID v_clsvwk(VDI_HANDLE handle)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    gem_hid_shutdown();
    gem_raster_shutdown();
    gem_os_shutdown();
    memset(&_vdi, 0, sizeof(_vdi));
}

/*
 * Clears the current workstation to color index zero.
 * `handle` selects the workstation to clear.
 * Returns no value.
 * Side effects: overwrites the full screen surface and immediately
 * presents the updated frame.
 */
VOID v_clrwk(VDI_HANDLE handle)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    _vdi_clear_screen(0);
    _vdi_present_screen();
}

/*
 * Draws a connected polyline using the current line color.
 * `handle` selects the workstation.
 * `count` is the number of points in `pxy`.
 * `pxy` contains `count` x,y point pairs laid out as
 * `{x0, y0, x1, y1, ...}`.
 * Returns no value.
 * Side effects: draws each segment onto the screen and presents the
 * result immediately.
 * Current limit: uses a simple unclipped line helper.
 */
VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy)
{
    WORD i;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL ||
        count < 2) {
        return;
    }

    for (i = 0; i < (WORD) (count - 1); ++i) {
        WORD x0 = pxy[i * 2];
        WORD y0 = pxy[i * 2 + 1];
        WORD x1 = pxy[i * 2 + 2];
        WORD y1 = pxy[i * 2 + 3];

        _vdi_draw_line_segment(x0, y0, x1, y1, _vdi.line_color);
    }

    _vdi_present_screen();
}

/*
 * Draws a filled inclusive rectangle using the current fill color.
 * `handle` selects the workstation.
 * `pxy` is `{x0, y0, x1, y1}` in GEM inclusive coordinates.
 * Returns no value.
 * Side effects: fills the rectangle on the screen and presents the
 * result immediately.
 * Current limit: this is a solid monochrome fill only.
 */
VOID v_bar(VDI_HANDLE handle, CONST WORD pxy[4])
{
    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }

    _vdi_fill_rect(pxy[0], pxy[1], pxy[2], pxy[3], _vdi.fill_color);
    _vdi_present_screen();
}

/*
 * Draws a NUL-terminated text string at baseline position `x`,`y`.
 * `handle` selects the workstation.
 * `x` and `y` specify the GEM baseline origin of the first character.
 * `text` points to the string to draw and must remain valid for the
 * duration of the call.
 * Returns no value.
 * Side effects: renders characters onto the screen and presents the
 * result immediately.
 * Current limit: uses the small built-in fallback font in `_vdi.c`.
 */
VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text)
{
    WORD pen_x = x;
    WORD top_y = (WORD) (y - 7);
    const char *cursor = (const char *) text;

    if (handle != _vdi_handle_screen || !_vdi.open || text == NULL) {
        return;
    }

    while (*cursor != '\0') {
        _vdi_draw_glyph(pen_x, top_y, *cursor, _vdi.text_color);
        pen_x = (WORD) (pen_x + _vdi_text_width);
        ++cursor;
    }

    _vdi_present_screen();
}

/*
 * Sets the current text drawing color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps zero to black and any non-zero
 * value to white.
 */
VOID vst_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.text_color = (WORD) ((color_index != 0) ? 1 : 0);
    }
}

/*
 * Sets the current filled-area color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps zero to black and any non-zero
 * value to white.
 */
VOID vsf_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.fill_color = (WORD) ((color_index != 0) ? 1 : 0);
    }
}

/*
 * Sets the current line drawing color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps zero to black and any non-zero
 * value to white.
 */
VOID vsl_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.line_color = (WORD) ((color_index != 0) ? 1 : 0);
    }
}

/*
 * Increments the cursor hide count for `handle`.
 * Returns no value.
 * Current limit: the hide count is tracked internally, but no cursor is
 * actually drawn by this VDI layer yet.
 */
VOID v_hide_c(VDI_HANDLE handle)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        ++_vdi.cursor_hidden;
    }
}

/*
 * Decrements or resets the cursor hide count for `handle`.
 * `reset` forces the hide count to zero when non-zero.
 * Returns no value.
 * Current limit: the counter only tracks logical cursor visibility.
 */
VOID v_show_c(VDI_HANDLE handle, WORD reset)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    if (reset != 0) {
        _vdi.cursor_hidden = 0;
    } else if (_vdi.cursor_hidden > 0) {
        --_vdi.cursor_hidden;
    }
}

/*
 * Queries the current mouse button state and position.
 * `handle` selects the workstation.
 * `status` receives the current button bitmask when not `NULL`.
 * `x` receives the current mouse x coordinate when not `NULL`.
 * `y` receives the current mouse y coordinate when not `NULL`.
 * Returns no value.
 * Side effects: pumps pending platform input events before reporting
 * state.
 */
VOID vq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
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

/*
 * Requests a string from keyboard input.
 * `handle` selects the workstation.
 * `max_length` is the maximum character count excluding the trailing
 * NUL terminator.
 * `echo_mode` enables on-screen echo when non-zero.
 * `echo_xy` optionally supplies the baseline position `{x, y}` used for
 * echoed text.
 * `out_string` receives the typed NUL-terminated result.
 * Returns no value.
 * Side effects: blocks until Enter is pressed, updates stored mouse
 * state while waiting, and may redraw echoed text.
 * Current limit: only a small subset of keyboard scancodes is converted
 * to ASCII.
 */
VOID vrq_string(VDI_HANDLE handle,
                WORD max_length,
                WORD echo_mode,
                WORD *echo_xy,
                BYTE *out_string)
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
            char ch = _vdi_scancode_to_ascii(evt.key);

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
                            (WORD) (echo_x + max_length * _vdi_text_width),
                            echo_y, 0);
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

/*
 * Copies raster data from `src` to `dst` using inclusive GEM
 * coordinates.
 * `handle` selects the workstation.
 * `mode` is the VDI raster operation mode and is accepted for
 * compatibility.
 * `pxy` contains eight values:
 * `{src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1, dst_y1}`.
 * `src` describes the source MFDB, or the screen when `src->fd_addr` is
 * `NULL`.
 * `dst` describes the destination MFDB, or the screen when `dst` is
 * `NULL`.
 * Returns no value.
 * Side effects: writes the destination bitmap and presents the screen if
 * the destination is the physical screen.
 * Current limit: implements only a simple replace-style per-pixel copy
 * with nearest-neighbor scaling when source and destination sizes
 * differ.
 */
VOID vro_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst)
{
    WORD src_x0;
    WORD src_y0;
    WORD src_w;
    WORD src_h;
    WORD dst_x0;
    WORD dst_y0;
    WORD dst_w;
    WORD dst_h;
    WORD y;

    (void) mode;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }

    src_x0 = (pxy[0] < pxy[2]) ? pxy[0] : pxy[2];
    src_y0 = (pxy[1] < pxy[3]) ? pxy[1] : pxy[3];
    src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    dst_x0 = (pxy[4] < pxy[6]) ? pxy[4] : pxy[6];
    dst_y0 = (pxy[5] < pxy[7]) ? pxy[5] : pxy[7];
    dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);

    for (y = 0; y < dst_h; ++y) {
        WORD x;
        WORD src_y = (WORD) (src_y0 + (LONG) y * src_h / dst_h);

        for (x = 0; x < dst_w; ++x) {
            WORD src_x = (WORD) (src_x0 + (LONG) x * src_w / dst_w);
            WORD color = _vdi_mfdb_get_pixel(src, src_x, src_y);

            _vdi_mfdb_set_pixel(dst, (WORD) (dst_x0 + x), (WORD) (dst_y0 + y),
                color);
        }
    }

    if (_vdi_uses_screen(dst)) {
        _vdi_present_screen();
    }
}

/*
 * Copies raster data from `src` to `dst` transparently.
 * `handle` selects the workstation.
 * `mode` is the VDI raster operation mode and is accepted for
 * compatibility.
 * `pxy` contains eight inclusive GEM coordinates laid out like
 * `vro_cpyfm()`.
 * `src` describes the source MFDB.
 * `dst` describes the destination MFDB.
 * `colors[0]` is the requested background color and `colors[1]` is the
 * requested foreground color when `colors` is not `NULL`.
 * Returns no value.
 * Side effects: writes only non-zero source pixels into the destination
 * and presents the screen if the destination is the physical screen.
 * Current limit: only the foreground color is honored, and monochrome
 * semantics are assumed throughout.
 */
VOID vrt_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst,
               CONST WORD colors[2])
{
    WORD src_x0;
    WORD src_y0;
    WORD src_w;
    WORD src_h;
    WORD dst_x0;
    WORD dst_y0;
    WORD dst_w;
    WORD dst_h;
    WORD foreground = 1;
    WORD y;

    (void) mode;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }
    if (colors != NULL) {
        foreground = (WORD) ((colors[1] != 0) ? 1 : 0);
    }

    src_x0 = (pxy[0] < pxy[2]) ? pxy[0] : pxy[2];
    src_y0 = (pxy[1] < pxy[3]) ? pxy[1] : pxy[3];
    src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    dst_x0 = (pxy[4] < pxy[6]) ? pxy[4] : pxy[6];
    dst_y0 = (pxy[5] < pxy[7]) ? pxy[5] : pxy[7];
    dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);

    for (y = 0; y < dst_h; ++y) {
        WORD x;
        WORD src_y = (WORD) (src_y0 + (LONG) y * src_h / dst_h);

        for (x = 0; x < dst_w; ++x) {
            WORD src_x = (WORD) (src_x0 + (LONG) x * src_w / dst_w);

            if (_vdi_mfdb_get_pixel(src, src_x, src_y) != 0) {
                _vdi_mfdb_set_pixel(dst, (WORD) (dst_x0 + x),
                    (WORD) (dst_y0 + y), foreground);
            }
        }
    }

    if (_vdi_uses_screen(dst)) {
        _vdi_present_screen();
    }
}

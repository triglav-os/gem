/*
 * Implements the public GEM VDI layer for the Linux/rasta port. The
 * code exposes the historical GEM workstation entry points while
 * backing them with a single monochrome screen workstation and a
 * compatibility-oriented software renderer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_vdi.h"

#include "platform/hid.h"
#include "platform/os.h"
#include "platform/raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _vdi_prepare_defaults(void);
static void _vdi_reset_workstation_state(void);
static WORD _vdi_color_to_pixel(WORD color_index);
static WORD _vdi_min_word(WORD left, WORD right);
static WORD _vdi_max_word(WORD left, WORD right);

static WORD _vdi_color_to_pixel(WORD color_index)
{
    return (color_index == 0) ? 1 : 0;
}

/*
 * Opens the single virtual workstation used by this port's VDI layer.
 * `work_in` is accepted for GEM compatibility, but the current backend
 * only uses a small fixed subset of workstation attributes.
 * `handle` receives the workstation handle on success, or zero on
 * failure.
 * `work_out` receives device capabilities such as screen size and text
 * metrics.
 * Side effects: initializes the OS, raster, and input backends if they
 * are not already open, establishes historical VDI default attributes,
 * clears the screen, and presents the initial frame.
 */
VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57])
{
    WORD width = _vdi_parse_env_word("GEM_VDI_WIDTH",
        _vdi_default_width);
    WORD height = _vdi_parse_env_word("GEM_VDI_HEIGHT",
        _vdi_default_height);

    (void) work_in;

    if (handle == NULL || work_out == NULL) {
        return;
    }

    _vdi_prepare_defaults();
    if (!_vdi.open) {
        if (!_vdi_load_fonts()) {
            *handle = 0;
            return;
        }
        if (!gem_os_init()) {
            _vdi_unload_fonts();
            *handle = 0;
            return;
        }
        if (!gem_raster_init((uint16_t) width, (uint16_t) height,
            GEM_RASTER_MONO1)) {
            gem_os_shutdown();
            _vdi_unload_fonts();
            *handle = 0;
            return;
        }
        (void) gem_hid_init();
        gem_os_sleep_ms(20u);
        (void) gem_raster_resync();
        gem_os_sleep_ms(20u);
        (void) gem_raster_resync();

        memset(&_vdi, 0, sizeof(_vdi));
        _vdi.surface = gem_raster_surface();
        _vdi.width = width;
        _vdi.height = height;
        _vdi.line_color = _vdi_color_to_pixel(1);
        _vdi.fill_color = _vdi_color_to_pixel(1);
        _vdi.text_color = _vdi_color_to_pixel(1);
        _vdi.cursor_hidden = 1;
        _vdi.open = 1;
        (void) _vdi_load_standard_mouse_forms();
        gem_raster_set_palette(0u, 0u, 0u, 0u);
        gem_raster_set_palette(1u, 255u, 255u, 255u);
        _vdi_clear_screen(0);
        _vdi_present_screen();
    }

    _vdi_reset_workstation_state();
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
    _vdi_unload_fonts();
    memset(&_vdi, 0, sizeof(_vdi));
}

/*
 * Clears the current workstation to the current background color.
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
 * Current limit: line styles and widths are stored as workstation
 * attributes, but the current software renderer draws a one-pixel solid
 * line.
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
 * Current limit: this is a monochrome solid fill implementation.
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
 * Current limit: text uses the currently selected loaded bitmap font.
 */
VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text)
{
    WORD pen_x = x;
    WORD top_y = (WORD) (y - _vdi_font_ascent());
    const char *cursor = (const char *) text;

    if (handle != _vdi_handle_screen || !_vdi.open || text == NULL) {
        return;
    }

    while (*cursor != '\0') {
        _vdi_draw_glyph(pen_x, top_y, *cursor, _vdi.text_color);
        pen_x = (WORD) (pen_x + _vdi_char_cell_width(*cursor));
        ++cursor;
    }

    _vdi_present_screen();
}

/*
 * Sets the current text drawing color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps GEM white to the lit pixel and
 * all other indices to black.
 */
VOID vst_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.text_color = _vdi_color_to_pixel(color_index);
    }
}

/*
 * Sets the current filled-area color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps GEM white to the lit pixel and
 * all other indices to black.
 */
VOID vsf_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.fill_color = _vdi_color_to_pixel(color_index);
    }
}

/*
 * Sets the current line drawing color for `handle`.
 * `color_index` is a VDI color index.
 * Returns no value.
 * Current limit: monochrome output maps GEM white to the lit pixel and
 * all other indices to black.
 */
VOID vsl_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.line_color = _vdi_color_to_pixel(color_index);
    }
}

/*
 * Increments the cursor hide count for `handle`.
 * Returns no value.
 * Current limit: the hide count is tracked internally, but the cursor
 * is not yet rendered by this VDI layer.
 */
VOID v_hide_c(VDI_HANDLE handle)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi_prepare_screen_write();
        ++_vdi.cursor_hidden;
        if (_vdi.update_depth == 0) {
            gem_raster_present();
        } else {
            _vdi.present_pending = 1;
        }
    }
}

/*
 * Decrements or resets the cursor hide count for `handle`.
 * `reset` forces the hide count to zero when non-zero.
 * Returns no value.
 * Current limit: this only updates logical cursor visibility state.
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
    _vdi_present_screen();
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
 * Current limit: only a small subset of keyboard scancodes is
 * translated into ASCII.
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
                            (WORD) (echo_x + max_length * _vdi_font_cell_width()),
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

static uint8_t _vdi_sample_bitmap_direct(const MFDB *mfdb, WORD x, WORD y)
{
    if (_vdi_uses_screen(mfdb)) {
        if (x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
            return 0;
        }
        return (uint8_t) (_vdi_get_screen_pixel(x, y) != 0);
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    {
        const UWORD *row = (const UWORD *) mfdb->fd_addr +
            (size_t) y * (size_t) mfdb->fd_wdwidth;
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));

        return (row[(size_t) x / 16u] & mask) != 0u ? 1u : 0u;
    }
}

static void _vdi_get_destination_bounds(MFDB *dst, vdi_rect_t *bounds)
{
    if (_vdi_uses_screen(dst)) {
        _vdi_get_active_clip_rect(bounds);
        return;
    }

    bounds->x0 = 0;
    bounds->y0 = 0;
    bounds->x1 = (WORD) (dst->fd_w - 1);
    bounds->y1 = (WORD) (dst->fd_h - 1);
}

static void _vdi_store_pixel_raw(MFDB *dst, WORD x, WORD y, WORD color)
{
    if (_vdi_uses_screen(dst)) {
        if (_vdi_point_visible(x, y)) {
            uint8_t *row = (uint8_t *) _vdi.surface->pixels +
                (size_t) y * _vdi.surface->pitch;
            uint8_t mask = (uint8_t) (1u << (7u - ((unsigned int) x & 7u)));

            if (color != 0) {
                row[(size_t) x / 8u] |= mask;
            } else {
                row[(size_t) x / 8u] &= (uint8_t) ~mask;
            }
        }
        return;
    }

    if (dst == NULL || dst->fd_addr == NULL || dst->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= dst->fd_w || y >= dst->fd_h) {
        return;
    }

    {
        UWORD *row = (UWORD *) dst->fd_addr +
            (size_t) y * (size_t) dst->fd_wdwidth;
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));

        if (color != 0) {
            row[(size_t) x / 16u] |= mask;
        } else {
            row[(size_t) x / 16u] &= (UWORD) ~mask;
        }
    }
}

static void _vdi_blit_1to1_transparent(MFDB *src,
                                        const vdi_rect_t *dst_rect,
                                        WORD src_x0,
                                        WORD src_y0,
                                        WORD dst_x0,
                                        WORD dst_y0,
                                        WORD foreground)
{
    size_t src_pitch = (size_t) src->fd_wdwidth * 2u;
    size_t dst_pitch = _vdi.surface->pitch;
    WORD dx_clipped = (WORD) (dst_rect->x0 - dst_x0);
    size_t src_byte0 = (size_t) (src_x0 + dx_clipped) / 8u;
    size_t dst_byte0 = (size_t) dst_rect->x0 / 8u;
    size_t dst_byte1 = (size_t) dst_rect->x1 / 8u;
    size_t row_bytes = dst_byte1 - dst_byte0 + 1u;
    uint8_t left_mask = (uint8_t) (0xffu >> ((unsigned int) dst_rect->x0 & 7u));
    uint8_t right_mask = (uint8_t) (0xffu << (7u - ((unsigned int) dst_rect->x1 & 7u)));
    WORD rel_y;

    for (rel_y = 0; rel_y <= dst_rect->y1 - dst_rect->y0; ++rel_y) {
        WORD sy = (WORD) (src_y0 + (dst_rect->y0 - dst_y0) + rel_y);
        WORD dy = (WORD) (dst_rect->y0 + rel_y);
        const uint8_t *sr = (const uint8_t *) src->fd_addr +
            (size_t) sy * src_pitch + src_byte0;
        uint8_t *dr = (uint8_t *) _vdi.surface->pixels +
            (size_t) dy * dst_pitch + dst_byte0;
        size_t i;

        if (row_bytes == 1u) {
            uint8_t mask = left_mask & right_mask;

            if (foreground != 0) {
                *dr |= (*sr & mask);
            } else {
                *dr &= (uint8_t) ~(*sr & mask);
            }
            continue;
        }

        /* Left edge */
        if (foreground != 0) {
            *dr |= (*sr & left_mask);
        } else {
            *dr &= (uint8_t) ~(*sr & left_mask);
        }
        ++sr;
        ++dr;

        /* Middle full bytes */
        for (i = 1u; i < row_bytes - 1u; ++i, ++sr, ++dr) {
            if (foreground != 0) {
                *dr |= *sr;
            } else {
                *dr &= (uint8_t) ~(*sr);
            }
        }

        /* Right edge */
        if (foreground != 0) {
            *dr |= (*sr & right_mask);
        } else {
            *dr &= (uint8_t) ~(*sr & right_mask);
        }
    }
}

static void _vdi_blit_bitmap(MFDB *src,
                             MFDB *dst,
                             CONST WORD pxy[8],
                             WORD mode,
                             WORD transparent,
                             WORD foreground)
{
    vdi_rect_t dst_rect;
    vdi_rect_t dst_bounds;
    WORD src_x0 = _vdi_min_word(pxy[0], pxy[2]);
    WORD src_y0 = _vdi_min_word(pxy[1], pxy[3]);
    WORD src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    WORD src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    WORD dst_x0 = _vdi_min_word(pxy[4], pxy[6]);
    WORD dst_y0 = _vdi_min_word(pxy[5], pxy[7]);
    WORD dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    WORD dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);
    WORD y;

    dst_rect.x0 = dst_x0;
    dst_rect.y0 = dst_y0;
    dst_rect.x1 = (WORD) (dst_x0 + dst_w - 1);
    dst_rect.y1 = (WORD) (dst_y0 + dst_h - 1);
    _vdi_get_destination_bounds(dst, &dst_bounds);
    if (!_vdi_intersect_rects(&dst_rect, &dst_bounds, &dst_rect)) {
        return;
    }

    /* Fast path: 1:1 scale, non-transparent mode 1, screen-to-screen same alignment */
    if (src_w == dst_w && src_h == dst_h &&
        transparent == 0 && mode == 1 && mode != 6 &&
        _vdi_uses_screen(src) && _vdi_uses_screen(dst) &&
        ((unsigned int) src_x0 & 7u) == ((unsigned int) dst_x0 & 7u)) {
        size_t pitch = _vdi.surface->pitch;
        size_t left_byte = (size_t) dst_rect.x0 / 8u;
        size_t right_byte = (size_t) dst_rect.x1 / 8u;
        size_t row_bytes = right_byte - left_byte + 1u;
        WORD rel_y;

        for (rel_y = 0; rel_y <= dst_rect.y1 - dst_rect.y0; ++rel_y) {
            WORD sy = (WORD) (src_y0 + (dst_rect.y0 - dst_y0) + rel_y);
            WORD dy = (WORD) (dst_rect.y0 + rel_y);
            const uint8_t *sr = (const uint8_t *) _vdi.surface->pixels +
                (size_t) sy * pitch + left_byte;
            uint8_t *dr = (uint8_t *) _vdi.surface->pixels +
                (size_t) dy * pitch + left_byte;

            memmove(dr, sr, row_bytes);
        }
        _vdi_present_screen();
        return;
    }

    for (y = dst_rect.y0; y <= dst_rect.y1; ++y) {
        WORD rel_y = (WORD) (y - dst_y0);
        WORD sample_y = (WORD) (src_y0 + ((LONG) rel_y * src_h) / dst_h);
        WORD x = dst_rect.x0;

        if (mode == 6) {
            for (; x <= dst_rect.x1; ++x) {
                WORD rel_x = (WORD) (x - dst_x0);
                WORD sample_x = (WORD) (src_x0 + ((LONG) rel_x * src_w) / dst_w);

                if (_vdi_sample_bitmap_direct(src, sample_x, sample_y) != 0u) {
                    WORD dest = _vdi_mfdb_get_pixel(dst, x, y);

                    _vdi_store_pixel_raw(dst, x, y, (WORD) ((dest == 0) ? 1 : 0));
                }
            }
            continue;
        }

        while (x <= dst_rect.x1) {
            WORD rel_x = (WORD) (x - dst_x0);
            WORD sample_x = (WORD) (src_x0 + ((LONG) rel_x * src_w) / dst_w);
            uint8_t source = _vdi_sample_bitmap_direct(src, sample_x, sample_y);
            WORD run_color;
            WORD run_start;

            if (transparent != 0 && source == 0u) {
                ++x;
                continue;
            }

            run_color = (transparent != 0) ? foreground :
                (WORD) ((source != 0u) ? 1 : 0);
            run_start = x;
            ++x;
            while (x <= dst_rect.x1) {
                rel_x = (WORD) (x - dst_x0);
                sample_x = (WORD) (src_x0 + ((LONG) rel_x * src_w) / dst_w);
                source = _vdi_sample_bitmap_direct(src, sample_x, sample_y);
                if (transparent != 0) {
                    if (source == 0u) {
                        break;
                    }
                } else if ((WORD) ((source != 0u) ? 1 : 0) != run_color) {
                    break;
                }
                ++x;
            }
            _vdi_mfdb_draw_hline(dst, y, run_start, (WORD) (x - 1), run_color);
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
 * Side effects: clips the destination rectangle geometrically, starts
 * sampling at the corresponding clipped source offset, and presents the
 * screen if the destination is the physical screen.
 */
VOID vro_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst)
{
    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }

    _vdi_blit_bitmap(src, dst, pxy, mode, 0, 0);
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
 * Side effects: clips the destination rectangle geometrically, writes
 * only non-zero source samples into the destination, and presents the
 * screen if the destination is the physical screen.
 */
VOID vrt_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst,
               CONST WORD colors[2])
{
    WORD foreground = 1;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }
    if (colors != NULL) {
        foreground = _vdi_color_to_pixel(colors[1]);
    }

    _vdi_blit_bitmap(src, dst, pxy, mode, 1, foreground);
    if (_vdi_uses_screen(dst)) {
        _vdi_present_screen();
    }
}

/*
 * Historical GEM VDI compatibility layer.
 */

enum {
    VDI_COMPAT_COLORS = 16,
    VDI_INPUT_DEVICES = 8,
    VDI_META_NAME_LEN = 260,
    VDI_POLYGON_MAX_POINTS = 256
};

#define VDI_PI 3.14159265358979323846

typedef struct vdi_compat_state {
    WORD clip_enabled;
    WORD clip[4];
    WORD output_window[4];
    WORD write_mode;
    WORD line_style;
    WORD line_width;
    WORD line_beg_style;
    WORD line_end_style;
    WORD line_pattern;
    WORD marker_type;
    WORD marker_height;
    WORD marker_color;
    WORD text_font;
    WORD text_height;
    WORD text_rotation;
    WORD text_alignment_h;
    WORD text_alignment_v;
    WORD text_background;
    WORD text_effects;
    WORD fill_interior;
    WORD fill_style;
    WORD fill_perimeter;
    WORD fill_pattern[16];
    WORD input_mode[VDI_INPUT_DEVICES];
    WORD cursor_row;
    WORD cursor_column;
    WORD reverse_video;
    WORD palette[VDI_COMPAT_COLORS][3];
    VOID (*timv)(void);
    VOID (*butv)(void);
    VOID (*motv)(void);
    VOID (*curv)(void);
    BYTE meta_filename[VDI_META_NAME_LEN];
} vdi_compat_state_t;

static vdi_compat_state_t _vdi_compat;
static int _vdi_compat_initialized;

static int _vdi_valid_handle(WORD handle)
{
    return _vdi.open != 0 && handle == _vdi_handle_screen;
}

static WORD _vdi_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

static WORD _vdi_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

static void _vdi_rect_from_xy(const WORD xy[4],
                              WORD *x0,
                              WORD *y0,
                              WORD *x1,
                              WORD *y1)
{
    *x0 = _vdi_min_word(xy[0], xy[2]);
    *y0 = _vdi_min_word(xy[1], xy[3]);
    *x1 = _vdi_max_word(xy[0], xy[2]);
    *y1 = _vdi_max_word(xy[1], xy[3]);
}

void _vdi_get_active_clip_rect(vdi_rect_t *rect)
{
    vdi_rect_t screen;
    vdi_rect_t requested;

    if (rect == NULL) {
        return;
    }

    screen.x0 = 0;
    screen.y0 = 0;
    screen.x1 = (WORD) (_vdi.width - 1);
    screen.y1 = (WORD) (_vdi.height - 1);
    *rect = screen;
    if (_vdi_compat.clip_enabled == 0) {
        return;
    }

    _vdi_rect_from_xy(_vdi_compat.clip, &requested.x0, &requested.y0,
        &requested.x1, &requested.y1);
    if (!_vdi_intersect_rects(&screen, &requested, rect)) {
        *rect = screen;
        rect->x1 = -1;
        rect->y1 = -1;
    }
}

int _vdi_intersect_rects(const vdi_rect_t *left,
                         const vdi_rect_t *right,
                         vdi_rect_t *out)
{
    if (left == NULL || right == NULL || out == NULL) {
        return 0;
    }

    out->x0 = _vdi_max_word(left->x0, right->x0);
    out->y0 = _vdi_max_word(left->y0, right->y0);
    out->x1 = _vdi_min_word(left->x1, right->x1);
    out->y1 = _vdi_min_word(left->y1, right->y1);
    return out->x0 <= out->x1 && out->y0 <= out->y1;
}

int _vdi_point_visible(WORD x, WORD y)
{
    vdi_rect_t clip;

    if (!_vdi.open) {
        return 0;
    }

    _vdi_get_active_clip_rect(&clip);
    return x >= clip.x0 && x <= clip.x1 && y >= clip.y0 && y <= clip.y1;
}

void _vdi_plot_pixel(WORD x, WORD y, WORD color)
{
    if (_vdi_point_visible(x, y)) {
        _vdi_set_screen_pixel(x, y, color);
    }
}

static void _vdi_draw_line(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_draw_line_segment(x0, y0, x1, y1, color);
}

static void _vdi_fill_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_fill_rect(x0, y0, x1, y1, color);
}

static void _vdi_outline_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_draw_line(x0, y0, x1, y0, color);
    _vdi_draw_line(x1, y0, x1, y1, color);
    _vdi_draw_line(x1, y1, x0, y1, color);
    _vdi_draw_line(x0, y1, x0, y0, color);
}

static double _vdi_deci_degrees_to_radians(WORD angle)
{
    return (double) angle * (VDI_PI / 1800.0);
}

static WORD _vdi_normalize_angle(WORD angle)
{
    int a = (int) angle % 3600;

    if (a < 0) {
        a += 3600;
    }
    return (WORD) a;
}

static int _vdi_angle_in_sweep(WORD angle, WORD start, WORD end)
{
    angle = _vdi_normalize_angle(angle);
    start = _vdi_normalize_angle(start);
    end = _vdi_normalize_angle(end);

    if (start <= end) {
        return angle >= start && angle <= end;
    }
    return angle >= start || angle <= end;
}

static WORD _vdi_point_count_from_radius(WORD xrad, WORD yrad)
{
    WORD radius = _vdi_max_word(xrad, yrad);
    WORD points = (WORD) (radius * 8);

    if (points < 32) {
        points = 32;
    }
    if (points > (WORD) (VDI_POLYGON_MAX_POINTS - 2)) {
        points = (WORD) (VDI_POLYGON_MAX_POINTS - 2);
    }
    return points;
}

static void _vdi_fill_polygon_points(const WORD *points,
                                     WORD count,
                                     WORD color)
{
    vdi_rect_t clip;
    WORD min_y;
    WORD max_y;
    WORD y;

    if (points == NULL || count < 3) {
        return;
    }

    min_y = max_y = points[1];
    for (y = 1; y < count; ++y) {
        WORD py = points[y * 2 + 1];

        if (py < min_y) {
            min_y = py;
        }
        if (py > max_y) {
            max_y = py;
        }
    }

    _vdi_get_active_clip_rect(&clip);
    if (clip.x0 > clip.x1 || clip.y0 > clip.y1) {
        return;
    }

    if (min_y < clip.y0) {
        min_y = clip.y0;
    }
    if (max_y > clip.y1) {
        max_y = clip.y1;
    }

    for (y = min_y; y <= max_y; ++y) {
        double intersections[VDI_POLYGON_MAX_POINTS];
        WORD intersections_count = 0;
        WORD i;

        for (i = 0; i < count; ++i) {
            WORD next = (WORD) ((i + 1 < count) ? i + 1 : 0);
            WORD x0 = points[i * 2];
            WORD y0 = points[i * 2 + 1];
            WORD x1 = points[next * 2];
            WORD y1 = points[next * 2 + 1];

            if (y0 == y1) {
                continue;
            }
            if ((y < _vdi_min_word(y0, y1)) || (y >= _vdi_max_word(y0, y1))) {
                continue;
            }

            if (intersections_count < VDI_POLYGON_MAX_POINTS) {
                intersections[intersections_count++] =
                    (double) x0 + ((double) (y - y0) * (double) (x1 - x0)) /
                    (double) (y1 - y0);
            }
        }

        if (intersections_count < 2) {
            continue;
        }

        for (i = 1; i < intersections_count; ++i) {
            double key = intersections[i];
            int j = (int) i - 1;

            while (j >= 0 && intersections[j] > key) {
                intersections[j + 1] = intersections[j];
                --j;
            }
            intersections[j + 1] = key;
        }

        for (i = 0; i + 1 < intersections_count; i += 2) {
            WORD left = (WORD) ceil(intersections[i]);
            WORD right = (WORD) floor(intersections[i + 1]);

            if (left < clip.x0) {
                left = clip.x0;
            }
            if (right > clip.x1) {
                right = clip.x1;
            }
            if (left <= right) {
                _vdi_draw_screen_hline_direct(y, left, right, color);
            }
        }
    }
}

static WORD _vdi_build_arc_points(WORD cx,
                                  WORD cy,
                                  WORD xrad,
                                  WORD yrad,
                                  WORD start,
                                  WORD end,
                                  WORD *points,
                                  WORD close_polygon)
{
    WORD samples;
    WORD point_count = 0;
    WORD sample;

    if (points == NULL || xrad < 0 || yrad < 0) {
        return 0;
    }

    if (close_polygon != 0) {
        points[point_count * 2] = cx;
        points[point_count * 2 + 1] = cy;
        ++point_count;
    }

    samples = _vdi_point_count_from_radius(xrad, yrad);
    for (sample = 0; sample <= samples; ++sample) {
        WORD angle = (WORD) ((LONG) sample * 3600L / samples);

        if (_vdi_angle_in_sweep(angle, start, end)) {
            double radians = _vdi_deci_degrees_to_radians(angle);
            WORD px = (WORD) lround((double) cx + cos(radians) * xrad);
            WORD py = (WORD) lround((double) cy - sin(radians) * yrad);

            if (point_count == 0 ||
                points[(point_count - 1) * 2] != px ||
                points[(point_count - 1) * 2 + 1] != py) {
                points[point_count * 2] = px;
                points[point_count * 2 + 1] = py;
                ++point_count;
            }
        }
    }

    return point_count;
}

static void _vdi_draw_polyline_points(const WORD *points,
                                      WORD count,
                                      WORD color,
                                      WORD closed)
{
    WORD i;

    if (points == NULL || count < 2) {
        return;
    }

    for (i = 0; i < (WORD) (count - 1); ++i) {
        _vdi_draw_line(points[i * 2], points[i * 2 + 1],
            points[i * 2 + 2], points[i * 2 + 3], color);
    }
    if (closed != 0) {
        _vdi_draw_line(points[(count - 1) * 2], points[(count - 1) * 2 + 1],
            points[0], points[1], color);
    }
}

static void _vdi_prepare_defaults(void)
{
    if (_vdi_compat_initialized != 0) {
        return;
    }

    memset(&_vdi_compat, 0, sizeof(_vdi_compat));
    _vdi_compat.write_mode = 1;
    _vdi_compat.line_style = 1;
    _vdi_compat.line_width = 1;
    _vdi_compat.line_beg_style = 0;
    _vdi_compat.line_end_style = 0;
    _vdi_compat.marker_type = 3;
    _vdi_compat.marker_height = _vdi_font_text_height();
    _vdi_compat.marker_color = 1;
    _vdi_compat.text_font = 3;
    _vdi_compat.text_height = _vdi_font_text_height();
    _vdi_compat.fill_interior = 1;
    _vdi_compat.fill_style = 1;
    _vdi_compat.fill_perimeter = 1;
    _vdi_compat.palette[0][0] = 0;
    _vdi_compat.palette[0][1] = 0;
    _vdi_compat.palette[0][2] = 0;
    _vdi_compat.palette[1][0] = 1000;
    _vdi_compat.palette[1][1] = 1000;
    _vdi_compat.palette[1][2] = 1000;
    _vdi_compat_initialized = 1;
}

static void _vdi_reset_workstation_state(void)
{
    _vdi_compat.clip[0] = 0;
    _vdi_compat.clip[1] = 0;
    _vdi_compat.clip[2] = (WORD) (_vdi.width - 1);
    _vdi_compat.clip[3] = (WORD) (_vdi.height - 1);
    _vdi_compat.output_window[0] = _vdi_compat.clip[0];
    _vdi_compat.output_window[1] = _vdi_compat.clip[1];
    _vdi_compat.output_window[2] = _vdi_compat.clip[2];
    _vdi_compat.output_window[3] = _vdi_compat.clip[3];
    _vdi_compat.clip_enabled = 0;
}

static WORD _vdi_string_length(const char *string)
{
    size_t length;

    if (string == NULL) {
        return 0;
    }

    length = strlen(string);
    if (length > 32767u) {
        length = 32767u;
    }
    return (WORD) length;
}

static void _vdi_set_extent_for_string(const char *string, WORD extent[8])
{
    WORD width = _vdi_string_width(string);
    WORD text_height = _vdi_font_text_height();

    extent[0] = 0;
    extent[1] = 0;
    extent[2] = (WORD) (width - ((width > 0) ? 1 : 0));
    extent[3] = 0;
    extent[4] = (WORD) (width - ((width > 0) ? 1 : 0));
    extent[5] = (WORD) (text_height - 1);
    extent[6] = 0;
    extent[7] = (WORD) (text_height - 1);
}

static void _vdi_store_rgb(WORD index, const WORD rgb[3])
{
    if (index < 0 || index >= VDI_COMPAT_COLORS || rgb == NULL) {
        return;
    }

    _vdi_compat.palette[index][0] = rgb[0];
    _vdi_compat.palette[index][1] = rgb[1];
    _vdi_compat.palette[index][2] = rgb[2];
}

static void _vdi_copy_rgb(WORD index, WORD rgb[3])
{
    if (rgb == NULL) {
        return;
    }
    if (index < 0 || index >= VDI_COMPAT_COLORS) {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
        return;
    }

    rgb[0] = _vdi_compat.palette[index][0];
    rgb[1] = _vdi_compat.palette[index][1];
    rgb[2] = _vdi_compat.palette[index][2];
}

WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[])
{
    VDI_HANDLE wk_handle = 0;

    _vdi_prepare_defaults();
    v_opnvwk(work_in, &wk_handle, work_out);
    if (handle != NULL) {
        *handle = wk_handle;
    }
    return wk_handle;
}

WORD v_opnrwk(WORD work_in[], WORD *handle, WORD work_out[])
{
    return v_opnwk(work_in, handle, work_out);
}

WORD v_clswk(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    v_clsvwk(handle);
    return 1;
}

WORD _vdi_text_background_mode(void)
{
    return _vdi_compat.text_background;
}

WORD _vdi_write_mode(void)
{
    return _vdi_compat.write_mode;
}

WORD v_updwk(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_present_screen();
    return 1;
}

VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        _vdi_compat.clip_enabled = 0;
        return;
    }

    _vdi_compat.clip_enabled = (clip_flag != 0) ? 1 : 0;
    memcpy(_vdi_compat.clip, xy, sizeof(_vdi_compat.clip));
}

VOID v_pmarker(WORD handle, WORD count, WORD xy[])
{
    WORD i;

    if (!_vdi_valid_handle(handle) || xy == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        WORD x = xy[i * 2];
        WORD y = xy[i * 2 + 1];

        _vdi_draw_line((WORD) (x - 2), y, (WORD) (x + 2), y,
            _vdi_compat.marker_color);
        _vdi_draw_line(x, (WORD) (y - 2), x, (WORD) (y + 2),
            _vdi_compat.marker_color);
    }
    _vdi_present_screen();
}

VOID v_fillarea(WORD handle, WORD count, WORD xy[])
{
    if (!_vdi_valid_handle(handle) || xy == NULL || count <= 0) {
        return;
    }

    _vdi_fill_polygon_points(xy, count, _vdi.fill_color);
    if (_vdi_compat.fill_perimeter != 0) {
        _vdi_draw_polyline_points(xy, count, _vdi.line_color, 1);
    }
    _vdi_present_screen();
}

WORD v_cellarray(WORD handle,
                 WORD xy[],
                 WORD row_length,
                 WORD el_per_row,
                 WORD num_rows,
                 WORD wr_mode,
                 WORD colors[])
{
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;
    WORD cols;
    WORD rows;
    WORD cell_width;
    WORD cell_height;
    WORD row;

    (void) row_length;
    (void) wr_mode;

    if (!_vdi_valid_handle(handle) || xy == NULL || colors == NULL ||
        el_per_row <= 0 || num_rows <= 0) {
        return 0;
    }

    _vdi_rect_from_xy(xy, &left, &top, &right, &bottom);
    cols = (el_per_row > 0) ? el_per_row : 1;
    rows = (num_rows > 0) ? num_rows : 1;
    cell_width = (WORD) (((right - left) + cols) / cols);
    cell_height = (WORD) (((bottom - top) + rows) / rows);

    for (row = 0; row < rows; ++row) {
        WORD col;

        for (col = 0; col < cols; ++col) {
            WORD color = colors[row * cols + col];
            WORD x0 = (WORD) (left + col * cell_width);
            WORD y0 = (WORD) (top + row * cell_height);
            WORD x1 = (WORD) (x0 + cell_width - 1);
            WORD y1 = (WORD) (y0 + cell_height - 1);

            _vdi_fill_box(x0, y0, x1, y1, (WORD) ((color != 0) ? 1 : 0));
        }
    }
    _vdi_present_screen();
    return 1;
}

VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, begang, endang,
        points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 0);
    _vdi_present_screen();
}

VOID v_pieslice(WORD handle,
                WORD x,
                WORD y,
                WORD radius,
                WORD begang,
                WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, begang, endang,
        points, 1);
    _vdi_fill_polygon_points(points, count, _vdi.fill_color);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_pie(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang)
{
    v_pieslice(handle, x, y, radius, begang, endang);
}

VOID v_circle(WORD handle, WORD x, WORD y, WORD radius)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, 0, 3599, points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, 0, 3599, points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_ellarc(WORD handle,
              WORD x,
              WORD y,
              WORD xrad,
              WORD yrad,
              WORD begang,
              WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, begang, endang, points,
        0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 0);
    _vdi_present_screen();
}

VOID v_ellpie(WORD handle,
              WORD x,
              WORD y,
              WORD xrad,
              WORD yrad,
              WORD begang,
              WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, begang, endang, points,
        1);
    _vdi_fill_polygon_points(points, count, _vdi.fill_color);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_rbox(WORD handle, WORD xy[4])
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    _vdi_rect_from_xy(xy, &x0, &y0, &x1, &y1);
    _vdi_outline_box(x0, y0, x1, y1, _vdi.line_color);
    _vdi_present_screen();
}

VOID v_rfbox(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    vr_recfl(handle, xy);
}

VOID v_justified(WORD handle,
                 WORD x,
                 WORD y,
                 char *string,
                 WORD length,
                 WORD wordspace,
                 WORD charspace)
{
    BYTE buffer[256];
    size_t copy_len;

    (void) wordspace;
    (void) charspace;

    if (!_vdi_valid_handle(handle) || string == NULL) {
        return;
    }

    copy_len = strlen(string);
    if (length >= 0 && (size_t) length < copy_len) {
        copy_len = (size_t) length;
    }
    if (copy_len >= sizeof(buffer)) {
        copy_len = sizeof(buffer) - 1u;
    }
    memcpy(buffer, string, copy_len);
    buffer[copy_len] = '\0';
    v_gtext(handle, x, y, buffer);
}

WORD vst_height(WORD handle,
                WORD height,
                WORD *charw,
                WORD *charh,
                WORD *cellw,
                WORD *cellh)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_height = (height > 0) ? height : _vdi_font_text_height();
    if (charw != NULL) {
        *charw = _vdi_font_cell_width();
    }
    if (charh != NULL) {
        *charh = _vdi_compat.text_height;
    }
    if (cellw != NULL) {
        *cellw = _vdi_font_cell_width();
    }
    if (cellh != NULL) {
        *cellh = _vdi_compat.text_height;
    }
    return _vdi_compat.text_height;
}

WORD vst_rotation(WORD handle, WORD angle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_rotation = angle;
    return angle;
}

WORD vs_color(WORD handle, WORD index, WORD rgb[3])
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_store_rgb(index, rgb);
    return 1;
}

WORD vsl_type(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_style = style;
    return style;
}

WORD vsl_width(WORD handle, WORD width)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_width = (width > 0) ? width : 1;
    return _vdi_compat.line_width;
}

WORD vsm_type(WORD handle, WORD symbol)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_type = symbol;
    return symbol;
}

WORD vsm_height(WORD handle, WORD height)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_height = (height > 0) ? height : _vdi_font_text_height();
    return _vdi_compat.marker_height;
}

WORD vsm_color(WORD handle, WORD color_index)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_color = _vdi_color_to_pixel(color_index);
    return color_index;
}

WORD vst_font(WORD handle, WORD font)
{
    WORD selected;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    selected = _vdi_select_font(font);
    if (selected == 0) {
        return 0;
    }
    _vdi_compat.text_font = font;
    return font;
}

WORD vsf_interior(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_interior = style;
    return style;
}

WORD vsf_style(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_style = style;
    return style;
}

WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[])
{
    (void) setflag;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_copy_rgb(index, rgb);
    return 1;
}

WORD vq_cellarray(WORD handle,
                  WORD xy[],
                  WORD row_length,
                  WORD num_requested,
                  WORD *el_used,
                  WORD *rows_used,
                  WORD status[])
{
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;
    WORD cols;
    WORD rows;
    WORD cell_width;
    WORD cell_height;
    WORD row;

    (void) row_length;

    if (!_vdi_valid_handle(handle) || xy == NULL || status == NULL ||
        num_requested <= 0) {
        return 0;
    }

    _vdi_rect_from_xy(xy, &left, &top, &right, &bottom);
    cols = (num_requested > 0) ? num_requested : 1;
    rows = 1;
    cell_width = (WORD) (((right - left) + cols) / cols);
    cell_height = (WORD) (((bottom - top) + rows) / rows);

    for (row = 0; row < rows; ++row) {
        WORD col;

        for (col = 0; col < cols; ++col) {
            WORD sample_x = (WORD) (left + col * cell_width);
            WORD sample_y = (WORD) (top + row * cell_height);

            status[row * cols + col] = _vdi_get_screen_pixel(sample_x,
                sample_y);
        }
    }
    if (el_used != NULL) {
        *el_used = cols;
    }
    if (rows_used != NULL) {
        *rows_used = rows;
    }
    return 1;
}

WORD vrq_locator(WORD handle,
                 WORD x,
                 WORD y,
                 WORD *xout,
                 WORD *yout,
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

WORD vsm_locator(WORD handle,
                 WORD x,
                 WORD y,
                 WORD *xout,
                 WORD *yout,
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

WORD vsm_string(WORD handle,
                WORD length,
                WORD echo_mode,
                WORD *echoxy,
                BYTE *string)
{
    if (!_vdi_valid_handle(handle) || string == NULL) {
        return 0;
    }

    vrq_string(handle, length, echo_mode, echoxy, string);
    return 1;
}

WORD vswr_mode(WORD handle, WORD mode)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.write_mode = mode;
    return mode;
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

WORD vql_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.line_style;
    attrib[1] = _vdi.line_color;
    attrib[2] = _vdi_compat.write_mode;
    attrib[3] = _vdi_compat.line_width;
    return 1;
}

WORD vqm_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.marker_type;
    attrib[1] = _vdi_compat.marker_color;
    attrib[2] = _vdi_compat.write_mode;
    attrib[3] = _vdi_compat.marker_height;
    return 1;
}

WORD vqf_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.fill_interior;
    attrib[1] = _vdi.fill_color;
    attrib[2] = _vdi_compat.fill_style;
    attrib[3] = _vdi_compat.fill_perimeter;
    return 1;
}

WORD vqt_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.text_font;
    attrib[1] = _vdi.text_color;
    attrib[2] = _vdi_compat.text_rotation;
    attrib[3] = _vdi_compat.text_height;
    attrib[4] = _vdi_compat.text_alignment_h;
    attrib[5] = _vdi_compat.text_alignment_v;
    return 1;
}

WORD vst_alignment(WORD handle, WORD horiz, WORD vert)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_alignment_h = horiz;
    _vdi_compat.text_alignment_v = vert;
    return 1;
}

WORD vq_extnd(WORD handle, WORD owflag, WORD work_out[])
{
    (void) owflag;

    if (!_vdi_valid_handle(handle) || work_out == NULL) {
        return 0;
    }

    _vdi_fill_work_out(work_out);
    return 1;
}

VOID v_contourfill(WORD handle, WORD x, WORD y, WORD index)
{
    typedef struct vdi_seed {
        WORD x;
        WORD y;
    } vdi_seed_t;

    vdi_rect_t clip;
    WORD target;
    WORD fill_color = _vdi_color_to_pixel(index);
    size_t capacity;
    vdi_seed_t *stack;
    size_t stack_size = 0;

    if (!_vdi_valid_handle(handle) || !_vdi_point_visible(x, y)) {
        return;
    }

    _vdi_get_active_clip_rect(&clip);
    target = _vdi_get_screen_pixel(x, y);
    if (target == fill_color) {
        return;
    }

    capacity = (size_t) _vdi.width * (size_t) _vdi.height;
    stack = gem_os_alloc(sizeof(vdi_seed_t) * capacity);
    if (stack == NULL) {
        return;
    }

    {
        size_t pitch = _vdi.surface->pitch;
        const uint8_t *pixels = (const uint8_t *) _vdi.surface->pixels;

#define _CF_ROW(yy) (pixels + (size_t)(yy) * pitch)
#define _CF_PIX(row, xx) \
    (((row)[(size_t)(xx) / 8u] & (uint8_t)(0x80u >> ((unsigned int)(xx) & 7u))) != 0u)

        stack[stack_size].x = x;
        stack[stack_size].y = y;
        ++stack_size;
        while (stack_size > 0u) {
            WORD left;
            WORD right;
            WORD scan_y;
            vdi_seed_t seed = stack[--stack_size];
            const uint8_t *seed_row;

            if (seed.x < clip.x0 || seed.x > clip.x1 ||
                seed.y < clip.y0 || seed.y > clip.y1) {
                continue;
            }
            seed_row = _CF_ROW(seed.y);
            if ((WORD) _CF_PIX(seed_row, seed.x) != target) {
                continue;
            }

            left = seed.x;
            right = seed.x;
            while (left > clip.x0 &&
                (WORD) _CF_PIX(seed_row, left - 1) == target) {
                --left;
            }
            while (right < clip.x1 &&
                (WORD) _CF_PIX(seed_row, right + 1) == target) {
                ++right;
            }

            _vdi_draw_screen_hline_direct(seed.y, left, right, fill_color);

            for (scan_y = (WORD) (seed.y - 1); scan_y <= (WORD) (seed.y + 1);
                scan_y += 2) {
                const uint8_t *scan_row;
                WORD scan_x = left;

                if (scan_y < clip.y0 || scan_y > clip.y1) {
                    continue;
                }
                scan_row = _CF_ROW(scan_y);

                while (scan_x <= right) {
                    while (scan_x <= right &&
                        (WORD) _CF_PIX(scan_row, scan_x) != target) {
                        ++scan_x;
                    }
                    if (scan_x > right) {
                        break;
                    }
                    if (stack_size < capacity) {
                        stack[stack_size].x = scan_x;
                        stack[stack_size].y = scan_y;
                        ++stack_size;
                    }
                    while (scan_x <= right &&
                        (WORD) _CF_PIX(scan_row, scan_x) == target) {
                        ++scan_x;
                    }
                }
            }
        }

#undef _CF_ROW
#undef _CF_PIX
    }

    gem_os_free(stack);
    _vdi_present_screen();
}

WORD vsf_perimeter(WORD handle, WORD per_vis)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_perimeter = per_vis;
    return per_vis;
}

WORD vst_background(WORD handle, WORD mode)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_background = mode;
    return mode;
}

WORD vst_effects(WORD handle, WORD effect)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_effects = effect;
    return effect;
}

WORD vst_point(WORD handle,
               WORD point,
               WORD *charw,
               WORD *charh,
               WORD *cellw,
               WORD *cellh)
{
    return vst_height(handle, point, charw, charh, cellw, cellh);
}

WORD vsl_ends(WORD handle, WORD begstyle, WORD endstyle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_beg_style = begstyle;
    _vdi_compat.line_end_style = endstyle;
    return 1;
}

WORD vsl_end_style(WORD handle, WORD begstyle, WORD endstyle)
{
    return vsl_ends(handle, begstyle, endstyle);
}

WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst)
{
    WORD xy[8];

    if (!_vdi_valid_handle(handle) || src == NULL || dst == NULL) {
        return 0;
    }

    xy[0] = 0;
    xy[1] = 0;
    xy[2] = (WORD) (src->fd_w - 1);
    xy[3] = (WORD) (src->fd_h - 1);
    xy[4] = 0;
    xy[5] = 0;
    xy[6] = (WORD) (dst->fd_w - 1);
    xy[7] = (WORD) (dst->fd_h - 1);
    vro_cpyfm(handle, 1, xy, src, dst);
    return 1;
}

WORD vsc_form(WORD handle, MFORM *cur_form)
{
    if (!_vdi_valid_handle(handle) || cur_form == NULL) {
        return 0;
    }

    return _vdi_set_mouse_form(cur_form);
}

WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes)
{
    size_t count = sizeof(_vdi_compat.fill_pattern) /
        sizeof(_vdi_compat.fill_pattern[0]);

    (void) planes;

    if (!_vdi_valid_handle(handle) || fill_pat == NULL) {
        return 0;
    }

    memcpy(_vdi_compat.fill_pattern, fill_pat, sizeof(WORD) * count);
    return 1;
}

WORD vsl_udsty(WORD handle, WORD pattern)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_pattern = pattern;
    return pattern;
}

VOID vr_recfl(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    v_bar(handle, xy);
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

WORD vqt_extent(WORD handle, char *string, WORD extent[8])
{
    if (!_vdi_valid_handle(handle) || extent == NULL) {
        return 0;
    }

    _vdi_set_extent_for_string(string, extent);
    return 1;
}

WORD vqt_width(WORD handle,
               WORD character,
               WORD *cell_width,
               WORD *left_delta,
               WORD *right_delta)
{
    WORD width;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    width = _vdi_char_cell_width((char) character);

    if (cell_width != NULL) {
        *cell_width = width;
    }
    if (left_delta != NULL) {
        *left_delta = 0;
    }
    if (right_delta != NULL) {
        *right_delta = 0;
    }
    return width;
}

WORD vex_timv(WORD handle,
              VOID (*tim_addr)(void),
              VOID (**old_addr)(void),
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

WORD vst_load_fonts(WORD handle, WORD select)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    (void) select;
    return _vdi_load_external_fonts();
}

WORD vst_unload_fonts(WORD handle, WORD select)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    (void) select;
    _vdi_compat.text_font = _vdi_default_font_id();
    return _vdi_unload_external_fonts();
}

WORD vex_butv(WORD handle,
              VOID (*usr_code)(void),
              VOID (**sav_code)(void))
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

WORD vex_motv(WORD handle,
              VOID (*usr_code)(void),
              VOID (**sav_code)(void))
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

WORD vex_curv(WORD handle,
              VOID (*usr_code)(void),
              VOID (**sav_code)(void))
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

WORD vqt_name(WORD handle, WORD element_num, BYTE *name)
{
    const char *font_name;
    WORD font_id;

    if (!_vdi_valid_handle(handle) || name == NULL) {
        return 0;
    }

    font_id = _vdi_font_id_for_element(element_num);
    font_name = _vdi_font_name(element_num);
    if (font_id == 0 || font_name == NULL) {
        return 0;
    }
    strcpy((char *) name, font_name);
    return font_id;
}

WORD vqt_fontinfo(WORD handle,
                  WORD *min_ade,
                  WORD *max_ade,
                  WORD distances[],
                  WORD *max_width,
                  WORD effects[])
{
    WORD text_height = _vdi_font_text_height();

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (min_ade != NULL) {
        *min_ade = _vdi_font_first_ade();
    }
    if (max_ade != NULL) {
        *max_ade = _vdi_font_last_ade();
    }
    if (distances != NULL) {
        distances[0] = 0;
        distances[1] = (WORD) (_vdi_font_ascent() - 2);
        distances[2] = 2;
        distances[3] = text_height;
        distances[4] = text_height;
    }
    if (max_width != NULL) {
        *max_width = _vdi_font_cell_width();
    }
    if (effects != NULL) {
        effects[0] = 0;
        effects[1] = 0;
        effects[2] = 0;
    }
    return 1;
}

WORD vq_chcells(WORD handle, WORD *rows, WORD *columns)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (rows != NULL) {
        *rows = (WORD) (_vdi.height / _vdi_font_text_height());
    }
    if (columns != NULL) {
        *columns = (WORD) (_vdi.width / _vdi_font_cell_width());
    }
    return 1;
}

WORD v_exit_cur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_enter_cur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_curup(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (_vdi_compat.cursor_row > 0) {
        --_vdi_compat.cursor_row;
    }
    return 1;
}

WORD v_curdown(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    ++_vdi_compat.cursor_row;
    return 1;
}

WORD v_curright(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    ++_vdi_compat.cursor_column;
    return 1;
}

WORD v_curleft(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (_vdi_compat.cursor_column > 0) {
        --_vdi_compat.cursor_column;
    }
    return 1;
}

WORD v_curhome(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_row = 0;
    _vdi_compat.cursor_column = 0;
    return 1;
}

WORD v_eeos(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    v_clrwk(handle);
    return 1;
}

WORD v_eeol(WORD handle)
{
    WORD y0;
    WORD y1;
    WORD x0;
    WORD x1;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    x0 = (WORD) (_vdi_compat.cursor_column * _vdi_font_cell_width());
    x1 = (WORD) (_vdi.width - 1);
    y0 = (WORD) (_vdi_compat.cursor_row * _vdi_font_text_height());
    y1 = (WORD) (y0 + _vdi_font_text_height() - 1);
    _vdi_fill_box(x0, y0, x1, y1, 0);
    _vdi_present_screen();
    return 1;
}

WORD vs_curaddress(WORD handle, WORD row, WORD column)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_row = (row > 0) ? row - 1 : 0;
    _vdi_compat.cursor_column = (column > 0) ? column - 1 : 0;
    return 1;
}

WORD v_curtext(WORD handle, BYTE *string)
{
    WORD x;
    WORD y;

    if (!_vdi_valid_handle(handle) || string == NULL) {
        return 0;
    }

    x = (WORD) (_vdi_compat.cursor_column * _vdi_font_cell_width());
    y = (WORD) ((_vdi_compat.cursor_row + 1) * _vdi_font_text_height() - 1);
    v_gtext(handle, x, y, string);
    _vdi_compat.cursor_column = (WORD) (_vdi_compat.cursor_column +
        _vdi_string_length((const char *) string));
    return 1;
}

WORD v_rvon(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.reverse_video = 1;
    return 1;
}

WORD v_rvoff(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.reverse_video = 0;
    return 1;
}

WORD vq_curaddress(WORD handle, WORD *row, WORD *column)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (row != NULL) {
        *row = (WORD) (_vdi_compat.cursor_row + 1);
    }
    if (column != NULL) {
        *column = (WORD) (_vdi_compat.cursor_column + 1);
    }
    return 1;
}

WORD vq_tabstatus(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_hardcopy(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_dspcur(WORD handle, WORD x, WORD y)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_column = (WORD) (x / _vdi_font_cell_width());
    _vdi_compat.cursor_row = (WORD) (y / _vdi_font_text_height());
    return 1;
}

WORD v_rmcur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_form_adv(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_output_window(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return 0;
    }

    memcpy(_vdi_compat.output_window, xy, sizeof(_vdi_compat.output_window));
    memcpy(_vdi_compat.clip, xy, sizeof(_vdi_compat.clip));
    _vdi_compat.clip_enabled = 1;
    return 1;
}

WORD v_clear_disp_list(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_bit_image(WORD handle,
                 BYTE *filename,
                 WORD aspect,
                 WORD x_scale,
                 WORD y_scale,
                 WORD h_align,
                 WORD v_align,
                 WORD xy[4])
{
    FILE *stream;

    (void) aspect;
    (void) x_scale;
    (void) y_scale;
    (void) h_align;
    (void) v_align;
    (void) xy;

    if (!_vdi_valid_handle(handle) || filename == NULL) {
        return 0;
    }

    stream = fopen((const char *) filename, "rb");
    if (stream == NULL) {
        return 0;
    }
    fclose(stream);
    return 1;
}

WORD vs_palette(WORD handle, WORD palette)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    return palette;
}

WORD v_meta_extents(WORD handle, WORD min_x, WORD min_y, WORD max_x, WORD max_y)
{
    (void) min_x;
    (void) min_y;
    (void) max_x;
    (void) max_y;
    return _vdi_valid_handle(handle);
}

WORD v_write_meta(WORD handle,
                  WORD num_intin,
                  WORD *intin_values,
                  WORD num_ptsin,
                  WORD *ptsin_values)
{
    (void) num_intin;
    (void) intin_values;
    (void) num_ptsin;
    (void) ptsin_values;
    return _vdi_valid_handle(handle);
}

WORD vm_filename(WORD handle, BYTE *filename)
{
    size_t length;

    if (!_vdi_valid_handle(handle) || filename == NULL) {
        return 0;
    }

    length = strlen((const char *) filename);
    if (length >= sizeof(_vdi_compat.meta_filename)) {
        length = sizeof(_vdi_compat.meta_filename) - 1u;
    }
    memcpy(_vdi_compat.meta_filename, filename, length);
    _vdi_compat.meta_filename[length] = '\0';
    return 1;
}

WORD v_get_pixel(WORD handle, WORD x, WORD y, WORD *pel, WORD *index)
{
    WORD color;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    color = _vdi_get_screen_pixel(x, y);
    if (pel != NULL) {
        *pel = color;
    }
    if (index != NULL) {
        *index = color;
    }
    return 1;
}

WORD v_escape(WORD handle,
              WORD function,
              WORD count_in,
              WORD count_out,
              WORD *intin_values,
              WORD *ptsin_values)
{
    (void) function;
    (void) count_in;
    (void) count_out;
    (void) intin_values;
    (void) ptsin_values;
    return _vdi_valid_handle(handle);
}

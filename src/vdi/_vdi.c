/*
 * Implements private helpers used by the minimal GEM VDI layer. These
 * routines keep drawing, font lookup, event pumping, and MFDB access
 * separate from the public workstation entry points in `vdi.c`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_vdi.h"

#include "platform/hid.h"
#include "platform/raster.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

vdi_state_t _vdi;

static const uint8_t _vdi_font_digits[10][7] = {
    { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e },
    { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f },
    { 0x1e, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1e },
    { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 },
    { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e },
    { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e },
    { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x1c }
};

static const uint8_t _vdi_font_upper[26][7] = {
    { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e },
    { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e },
    { 0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0f },
    { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e },
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f },
    { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 },
    { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 },
    { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d },
    { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 },
    { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e },
    { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 },
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a },
    { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 },
    { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f }
};

static const uint8_t _vdi_font_space[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t _vdi_font_period[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c
};

static const uint8_t _vdi_font_comma[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x08
};

static const uint8_t _vdi_font_colon[7] = {
    0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00
};

static const uint8_t _vdi_font_dash[7] = {
    0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00
};

static const uint8_t _vdi_font_slash[7] = {
    0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10
};

static const uint8_t _vdi_font_lparen[7] = {
    0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02
};

static const uint8_t _vdi_font_rparen[7] = {
    0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08
};

static const uint8_t _vdi_font_unknown[7] = {
    0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04
};

static WORD _vdi_clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static const uint8_t *_vdi_glyph_for_char(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return _vdi_font_digits[ch - '0'];
    }
    if (ch >= 'a' && ch <= 'z') {
        ch = (char) (ch - 'a' + 'A');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return _vdi_font_upper[ch - 'A'];
    }

    switch (ch) {
    case ' ':
        return _vdi_font_space;
    case '.':
        return _vdi_font_period;
    case ',':
        return _vdi_font_comma;
    case ':':
        return _vdi_font_colon;
    case '-':
    case '_':
        return _vdi_font_dash;
    case '/':
        return _vdi_font_slash;
    case '(':
        return _vdi_font_lparen;
    case ')':
        return _vdi_font_rparen;
    default:
        return _vdi_font_unknown;
    }
}

WORD _vdi_parse_env_word(const char *name, WORD fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 32767L) {
        return fallback;
    }

    return (WORD) parsed;
}

void _vdi_pump_events(void)
{
    gem_hid_event_t evt;

    while (gem_hid_poll(&evt) != 0) {
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            _vdi.mouse_x = evt.x;
            _vdi.mouse_y = evt.y;
            _vdi.mouse_status = (WORD) evt.flags;
        }
    }
}

void _vdi_present_screen(void)
{
    gem_raster_present();
    _vdi_pump_events();
}

void _vdi_set_screen_pixel(WORD x, WORD y, WORD color)
{
    uint8_t *row;

    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL) {
        return;
    }
    if (x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
        return;
    }

    row = (uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
    row[x] = (uint8_t) ((color != 0) ? 1u : 0u);
}

WORD _vdi_get_screen_pixel(WORD x, WORD y)
{
    const uint8_t *row;

    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
        return 0;
    }

    row = (const uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
    return (WORD) ((row[x] != 0u) ? 1 : 0);
}

void _vdi_clear_screen(WORD color)
{
    uint8_t value = (uint8_t) ((color != 0) ? 1u : 0u);

    memset(_vdi.surface->pixels, value,
        (size_t) _vdi.surface->pitch * (size_t) _vdi.height);
}

void _vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD dx = (WORD) abs(x1 - x0);
    WORD sx = (WORD) ((x0 < x1) ? 1 : -1);
    WORD dy = (WORD) -abs(y1 - y0);
    WORD sy = (WORD) ((y0 < y1) ? 1 : -1);
    WORD err = (WORD) (dx + dy);

    FOREVER {
        WORD e2 = (WORD) (2 * err);

        _vdi_set_screen_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }

        if (e2 >= dy) {
            err = (WORD) (err + dy);
            x0 = (WORD) (x0 + sx);
        }
        if (e2 <= dx) {
            err = (WORD) (err + dx);
            y0 = (WORD) (y0 + sy);
        }
    }
}

void _vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD x_min = (x0 < x1) ? x0 : x1;
    WORD x_max = (x0 < x1) ? x1 : x0;
    WORD y_min = (y0 < y1) ? y0 : y1;
    WORD y_max = (y0 < y1) ? y1 : y0;
    WORD y;

    x_min = _vdi_clamp_word(x_min, 0, (WORD) (_vdi.width - 1));
    x_max = _vdi_clamp_word(x_max, 0, (WORD) (_vdi.width - 1));
    y_min = _vdi_clamp_word(y_min, 0, (WORD) (_vdi.height - 1));
    y_max = _vdi_clamp_word(y_max, 0, (WORD) (_vdi.height - 1));

    for (y = y_min; y <= y_max; ++y) {
        WORD x;

        for (x = x_min; x <= x_max; ++x) {
            _vdi_set_screen_pixel(x, y, color);
        }
    }
}

void _vdi_draw_glyph(WORD x, WORD y, char ch, WORD color)
{
    const uint8_t *glyph = _vdi_glyph_for_char(ch);
    WORD row_index;

    for (row_index = 0; row_index < 7; ++row_index) {
        uint8_t row_bits = glyph[row_index];
        WORD col_index;

        for (col_index = 0; col_index < 5; ++col_index) {
            if ((row_bits & (uint8_t) (1u << (4 - col_index))) != 0u) {
                _vdi_set_screen_pixel((WORD) (x + col_index),
                    (WORD) (y + row_index), color);
            }
        }
    }
}

char _vdi_scancode_to_ascii(uint16_t key)
{
    if (key >= 4u && key <= 29u) {
        return (char) ('a' + (char) (key - 4u));
    }
    if (key >= 30u && key <= 38u) {
        return (char) ('1' + (char) (key - 30u));
    }

    switch (key) {
    case 39u:
        return '0';
    case 40u:
        return '\n';
    case 42u:
        return '\b';
    case 44u:
        return ' ';
    case 45u:
        return '-';
    case 47u:
        return '[';
    case 48u:
        return ']';
    case 51u:
        return ';';
    case 52u:
        return '\'';
    case 54u:
        return ',';
    case 55u:
        return '.';
    case 56u:
        return '/';
    default:
        return '\0';
    }
}

int _vdi_uses_screen(const MFDB *mfdb)
{
    return mfdb == NULL || mfdb->fd_addr == NULL;
}

WORD _vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y)
{
    const uint8_t *bytes;
    size_t row_bytes;
    size_t byte_index;
    uint8_t mask;

    if (_vdi_uses_screen(mfdb)) {
        return _vdi_get_screen_pixel(x, y);
    }
    if (mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    bytes = (const uint8_t *) mfdb->fd_addr;
    row_bytes = (size_t) mfdb->fd_wdwidth * 2u;
    byte_index = (size_t) y * row_bytes + (size_t) x / 8u;
    mask = (uint8_t) (1u << (7 - ((unsigned int) x & 7u)));
    return (WORD) ((bytes[byte_index] & mask) != 0u);
}

void _vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color)
{
    uint8_t *bytes;
    size_t row_bytes;
    size_t byte_index;
    uint8_t mask;

    if (_vdi_uses_screen(mfdb)) {
        _vdi_set_screen_pixel(x, y, color);
        return;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return;
    }

    bytes = (uint8_t *) mfdb->fd_addr;
    row_bytes = (size_t) mfdb->fd_wdwidth * 2u;
    byte_index = (size_t) y * row_bytes + (size_t) x / 8u;
    mask = (uint8_t) (1u << (7 - ((unsigned int) x & 7u)));
    if (color != 0) {
        bytes[byte_index] |= mask;
    } else {
        bytes[byte_index] &= (uint8_t) ~mask;
    }
}

void _vdi_fill_work_out(WORD work_out[57])
{
    memset(work_out, 0, sizeof(WORD) * 57u);
    work_out[0] = (WORD) (_vdi.width - 1);
    work_out[1] = (WORD) (_vdi.height - 1);
    work_out[13] = 2;
    work_out[35] = 1;
    work_out[39] = 1;
    work_out[45] = _vdi_text_width;
    work_out[46] = _vdi_text_height;
}

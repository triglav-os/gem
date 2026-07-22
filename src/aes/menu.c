/*
 * Implements hosted AES menus, object trees, forms, and
 * classic alert boxes on top of the private AES runtime helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"
#include "alert_icons.h"

#include "platform/os.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum {
    AES_ALERT_MAX_LINES = 5,
    AES_ALERT_MAX_BUTTONS = 3,
    AES_ALERT_TEXT_LEN = 80
};

typedef struct aes_alert {
    WORD icon;
    WORD default_button;
    WORD line_count;
    WORD button_count;
    char lines[AES_ALERT_MAX_LINES][AES_ALERT_TEXT_LEN];
    char buttons[AES_ALERT_MAX_BUTTONS][AES_ALERT_TEXT_LEN];
    GRECT outer;
    GRECT button_rects[AES_ALERT_MAX_BUTTONS];
} aes_alert_t;

static WORD _aes_form_last_object(OBJECT *tree);
static WORD _aes_form_last_reachable_object(const OBJECT *tree,
    WORD object, uint8_t visited[256]);
static int _aes_form_is_editable(const OBJECT *tree, WORD object);
static WORD _aes_form_find_next_editable(OBJECT *tree,
    WORD current, int backwards);
static void _aes_form_object_rect(OBJECT *tree, WORD object, GRECT *rect);
static void _aes_form_redraw_tree(OBJECT *tree);
static void _aes_form_redraw_object(OBJECT *tree, WORD object);
static void _aes_form_set_active_field(OBJECT *tree, WORD object, WORD *idx);
static void _aes_form_set_caret_from_click(OBJECT *tree, WORD object,
    WORD mouse_x, WORD *idx);
static void _aes_form_delete_backward(OBJECT *tree, WORD object, WORD *idx);
static void _aes_form_delete_forward(OBJECT *tree, WORD object, WORD *idx);
static int _aes_form_apply_key(OBJECT *tree, WORD object, WORD key,
    WORD *idx);
static WORD _aes_form_find_default_exit(OBJECT *tree);
static void _aes_form_flash_object(OBJECT *tree, WORD object);

static int _aes_alert_parse_group(const char **cursor,
    char *out, size_t out_size);
static void _aes_alert_split_items(const char *text,
    char items[][AES_ALERT_TEXT_LEN], WORD *count, WORD max_items);
static int _aes_parse_alert(const char *str, WORD defbtn, aes_alert_t *alert);
static void _aes_alert_compute_layout(aes_alert_t *alert);
static void _aes_alert_draw_text(WORD x, WORD y, const char *text);
static void _aes_alert_draw_frame(const GRECT *rect);
static void _aes_alert_draw_button(const GRECT *rect,
    const char *label, int is_default);
static const aes_alert_icon_asset_t *_aes_alert_icon_asset(WORD icon);
static int _aes_alert_icon_bit(const UWORD *bits,
    const aes_alert_icon_asset_t *asset, WORD x, WORD y);
static void _aes_alert_draw_icon(const aes_alert_icon_asset_t *asset,
    WORD x, WORD y);
static void _aes_draw_alert(const aes_alert_t *alert);
static WORD _aes_run_alert(aes_alert_t *alert);
WORD menu_bar(OBJECT *tree, WORD show);
WORD menu_icheck(OBJECT *tree, WORD item, WORD check);
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable);
WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal);
WORD menu_text(OBJECT *tree, WORD item, char *text);
WORD menu_register(WORD apid, char *name);
WORD menu_unregister(WORD mid);
WORD menu_click(WORD click, WORD setit);
WORD objc_add(OBJECT *tree, WORD parent, WORD child);
WORD objc_delete(OBJECT *tree, WORD object);
WORD objc_draw(OBJECT *tree,
               WORD startob,
               WORD depth,
               WORD xc,
               WORD yc,
               WORD wc,
               WORD hc);
WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my);
WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y);
WORD objc_order(OBJECT *tree, WORD object, WORD newpos);
WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx, WORD kind);
WORD objc_change(OBJECT *tree,
                 WORD object,
                 WORD depth,
                 WORD xc,
                 WORD yc,
                 WORD wc,
                 WORD hc,
                 WORD newstate,
                 WORD redraw);
WORD form_do(OBJECT *tree, WORD startob);
WORD form_dial(WORD flag,
               WORD x1,
               WORD y1,
               WORD w1,
               WORD h1,
               WORD x2,
               WORD y2,
               WORD w2,
               WORD h2);
WORD form_alert(WORD defbtn, char *str);
WORD form_error(WORD errnum);
WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch);
WORD form_keybd(OBJECT *tree,
                WORD object,
                WORD next,
                WORD thechar,
                WORD *newobj,
                WORD *newchar);
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj);

static int _aes_alert_parse_group(const char **cursor,
    char *out, size_t out_size)
{
    const char *scan;
    size_t len = 0;

    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0u) {
        return 0;
    }

    scan = *cursor;
    while (*scan != '\0' && *scan != '[') {
        ++scan;
    }
    if (*scan != '[') {
        return 0;
    }
    ++scan;
    while (*scan != '\0' && *scan != ']') {
        if (len + 1u < out_size) {
            out[len++] = *scan;
        }
        ++scan;
    }
    if (*scan != ']') {
        return 0;
    }

    out[len] = '\0';
    *cursor = scan + 1;
    return 1;
}

static void _aes_alert_split_items(const char *text,
    char items[][AES_ALERT_TEXT_LEN], WORD *count, WORD max_items)
{
    WORD item = 0;
    size_t len = 0;

    if (count == NULL) {
        return;
    }
    *count = 0;
    if (text == NULL || items == NULL || max_items <= 0) {
        return;
    }

    memset(items, 0, (size_t) max_items * AES_ALERT_TEXT_LEN);
    while (*text != '\0' && item < max_items) {
        if (*text == '|') {
            items[item][len] = '\0';
            ++item;
            len = 0;
            ++text;
            continue;
        }
        if (len + 1u < AES_ALERT_TEXT_LEN) {
            items[item][len++] = *text;
        }
        ++text;
    }
    if (item < max_items) {
        items[item][len] = '\0';
        *count = (WORD) (item + 1);
    }
}

static WORD _aes_form_last_object(OBJECT *tree)
{
    uint8_t visited[256] = {0};

    if (tree == NULL) {
        return ROOT;
    }

    return _aes_form_last_reachable_object(tree, ROOT, visited);
}

static WORD _aes_form_last_reachable_object(const OBJECT *tree,
    WORD object, uint8_t visited[256])
{
    WORD last;
    WORD child;

    if (tree == NULL || visited == NULL || object < ROOT || object >= 256) {
        return ROOT;
    }
    if (visited[object] != 0u) {
        return ROOT;
    }

    visited[object] = 1u;
    last = object;
    child = tree[object].ob_head;
    if (child == NIL || child == object) {
        return last;
    }

    while (child != NIL && child != object) {
        WORD child_last;
        WORD next;

        child_last = _aes_form_last_reachable_object(tree, child, visited);
        if (child_last > last) {
            last = child_last;
        }

        next = tree[child].ob_next;
        if (next == child || next == object) {
            break;
        }
        child = next;
    }

    return last;
}

static int _aes_form_is_editable(const OBJECT *tree, WORD object)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    return tree[object].ob_type == G_FTEXT ||
        tree[object].ob_type == G_FBOXTEXT;
}

static WORD _aes_form_find_next_editable(OBJECT *tree,
                                         WORD current,
                                         int backwards)
{
    WORD first = NIL;
    WORD last = _aes_form_last_object(tree);
    WORD i;

    if (tree == NULL) {
        return NIL;
    }

    for (i = ROOT; i <= last; ++i) {
        if (_aes_form_is_editable(tree, i) != 0) {
            first = i;
            break;
        }
    }
    if (first == NIL) {
        return NIL;
    }
    if (current == NIL || current < ROOT || current > last) {
        return first;
    }

    if (backwards == 0) {
        for (i = (WORD) (current + 1); i <= last; ++i) {
            if (_aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
        for (i = ROOT; i < current; ++i) {
            if (_aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
    } else {
        for (i = (WORD) (current - 1); i >= ROOT; --i) {
            if (_aes_form_is_editable(tree, i) != 0) {
                return i;
            }
            if (i == ROOT) {
                break;
            }
        }
        for (i = last; i > current; --i) {
            if (_aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
    }

    return current;
}

static void _aes_form_object_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    WORD x;
    WORD y;

    if (rect == NULL) {
        return;
    }

    _aes_set_rect(rect, 0, 0, 0, 0);
    if (tree == NULL || object < 0) {
        return;
    }

    objc_offset(tree, object, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = tree[object].ob_width;
    rect->g_h = tree[object].ob_height;
}

static void _aes_form_redraw_tree(OBJECT *tree)
{
    if (tree == NULL) {
        return;
    }

    objc_draw(tree, ROOT, MAX_DEPTH, tree[ROOT].ob_x, tree[ROOT].ob_y,
        tree[ROOT].ob_width, tree[ROOT].ob_height);
}

static void _aes_form_redraw_object(OBJECT *tree, WORD object)
{
    GRECT rect;

    if (tree == NULL || object < 0) {
        return;
    }

    _aes_form_object_rect(tree, object, &rect);
    objc_draw(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h);
}

static void _aes_form_set_active_field(OBJECT *tree, WORD object, WORD *idx)
{
    WORD current = _aes.edit_object;

    if (tree == NULL || object < 0 || _aes_form_is_editable(tree, object) == 0 ||
        idx == NULL) {
        return;
    }

    if (_aes.edit_tree == tree && current >= 0 &&
        _aes_form_is_editable(tree, current) != 0 && current != object) {
        tree[current].ob_state &= (UWORD) ~SELECTED;
        _aes_form_redraw_object(tree, current);
    }

    tree[object].ob_state |= SELECTED;
    objc_edit(tree, object, 0, idx, EDINIT);
    _aes_form_redraw_object(tree, object);
}

static void _aes_form_set_caret_from_click(OBJECT *tree, WORD object,
                                           WORD mouse_x, WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    GRECT rect;
    WORD length;
    WORD rel_x;
    WORD index;
    char prefix[MAX_LEN];

    if (tree == NULL || object < 0 || idx == NULL ||
        _aes_form_is_editable(tree, object) == 0 ||
        tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    buffer = (char *) (intptr_t) ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    _aes_form_object_rect(tree, object, &rect);
    length = (WORD) strlen(buffer);
    rel_x = (WORD) (mouse_x - rect.g_x -
        (tree[object].ob_type == G_FBOXTEXT ? 3 : 2));
    if (rel_x <= 0) {
        *idx = 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return;
    }

    for (index = 0; index < length; ++index) {
        WORD left_width;
        WORD right_width;
        WORD midpoint;

        memcpy(prefix, buffer, (size_t) index);
        prefix[index] = '\0';
        left_width = (WORD) _vdi_string_width(prefix);

        memcpy(prefix, buffer, (size_t) (index + 1));
        prefix[index + 1] = '\0';
        right_width = (WORD) _vdi_string_width(prefix);

        midpoint = (WORD) (left_width + (right_width - left_width) / 2);
        if (rel_x <= midpoint) {
            *idx = index;
            objc_edit(tree, object, 0, idx, EDINIT);
            return;
        }
    }

    *idx = length;
    objc_edit(tree, object, 0, idx, EDINIT);
}

static void _aes_form_delete_backward(OBJECT *tree, WORD object, WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL || *idx <= 0 ||
        tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    buffer = (char *) (intptr_t) ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    length = strlen(buffer);
    --(*idx);
    memmove(&buffer[*idx], &buffer[*idx + 1], length - (size_t) *idx);
    objc_edit(tree, object, 0, idx, EDINIT);
}

static void _aes_form_delete_forward(OBJECT *tree, WORD object, WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL ||
        tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    buffer = (char *) (intptr_t) ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    length = strlen(buffer);
    if ((size_t) *idx >= length) {
        return;
    }

    memmove(&buffer[*idx], &buffer[*idx + 1], length - (size_t) *idx);
    objc_edit(tree, object, 0, idx, EDINIT);
}

static int _aes_form_apply_key(OBJECT *tree, WORD object, WORD key,
                               WORD *idx)
{
    WORD ascii;
    WORD scancode;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }

    ascii = (WORD) (key & 0xffu);
    scancode = (WORD) ((key >> 8) & 0xffu);

    if (ascii == '\b' || scancode == 42 || scancode == 14) {
        _aes_form_delete_backward(tree, object, idx);
        return 1;
    }
    if (scancode == 76 || scancode == 83) {
        _aes_form_delete_forward(tree, object, idx);
        return 1;
    }
    if (scancode == 80 || scancode == 75) {
        if (*idx > 0) {
            --(*idx);
            objc_edit(tree, object, 0, idx, EDINIT);
        }
        return 1;
    }
    if (scancode == 79) {
        TEDINFO *ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
        char *buffer = (char *) (intptr_t) ted->te_ptext;
        WORD length = (buffer != NULL) ? (WORD) strlen(buffer) : 0;

        if (*idx < length) {
            ++(*idx);
            objc_edit(tree, object, 0, idx, EDINIT);
        }
        return 1;
    }
    if (scancode == 74 || scancode == 71) {
        *idx = 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return 1;
    }
    if (scancode == 77) {
        TEDINFO *ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
        char *buffer = (char *) (intptr_t) ted->te_ptext;

        *idx = (buffer != NULL) ? (WORD) strlen(buffer) : 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return 1;
    }
    if (ascii >= 32 && ascii <= 126) {
        objc_edit(tree, object, ascii, idx, EDCHAR);
        return 1;
    }

    return 0;
}

static WORD _aes_form_find_default_exit(OBJECT *tree)
{
    WORD i;
    WORD last = _aes_form_last_object(tree);

    if (tree == NULL) {
        return NIL;
    }

    for (i = ROOT; i <= last; ++i) {
        if ((tree[i].ob_flags & DEFAULT) != 0u &&
            (tree[i].ob_flags & EXIT) != 0u) {
            return i;
        }
    }
    return NIL;
}

static void _aes_form_flash_object(OBJECT *tree, WORD object)
{
    GRECT rect;
    UWORD old_state;

    if (tree == NULL || object < 0) {
        return;
    }

    _aes_form_object_rect(tree, object, &rect);
    old_state = tree[object].ob_state;
    objc_change(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        (WORD) (old_state | SELECTED), 1);
    evnt_timer(90, 0);
    objc_change(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        old_state, 1);
}

static int _aes_parse_alert(const char *str, WORD defbtn, aes_alert_t *alert)
{
    char icon_text[AES_ALERT_TEXT_LEN];
    char line_text[AES_ALERT_TEXT_LEN];
    char button_text[AES_ALERT_TEXT_LEN];
    const char *cursor = str;

    if (alert == NULL) {
        return 0;
    }

    memset(alert, 0, sizeof(*alert));
    alert->default_button = (defbtn > 0) ? defbtn : 1;
    if (str == NULL) {
        strcpy(alert->lines[0], "Alert");
        strcpy(alert->buttons[0], "OK");
        alert->line_count = 1;
        alert->button_count = 1;
        return 1;
    }

    if (_aes_alert_parse_group(&cursor, icon_text, sizeof(icon_text)) == 0 ||
        _aes_alert_parse_group(&cursor, line_text, sizeof(line_text)) == 0 ||
        _aes_alert_parse_group(&cursor, button_text,
            sizeof(button_text)) == 0) {
        strncpy(alert->lines[0], str, AES_ALERT_TEXT_LEN - 1u);
        strcpy(alert->buttons[0], "OK");
        alert->line_count = 1;
        alert->button_count = 1;
        return 1;
    }

    alert->icon = (WORD) atoi(icon_text);
    _aes_alert_split_items(line_text, alert->lines, &alert->line_count,
        AES_ALERT_MAX_LINES);
    _aes_alert_split_items(button_text, alert->buttons, &alert->button_count,
        AES_ALERT_MAX_BUTTONS);
    if (alert->line_count <= 0) {
        strcpy(alert->lines[0], "Alert");
        alert->line_count = 1;
    }
    if (alert->button_count <= 0) {
        strcpy(alert->buttons[0], "OK");
        alert->button_count = 1;
    }
    if (alert->default_button > alert->button_count) {
        alert->default_button = alert->button_count;
    }
    return 1;
}

static void _aes_alert_compute_layout(aes_alert_t *alert)
{
    GRECT desktop;
    WORD text_h;
    WORD text_w = 0;
    WORD line_gap = 4;
    WORD icon_w = 0;
    WORD icon_h = 0;
    WORD button_h;
    WORD button_gap = 8;
    WORD button_total = 0;
    WORD content_w;
    WORD content_h;
    WORD x;
    WORD i;

    if (alert == NULL) {
        return;
    }

    text_h = _vdi_font_text_height();
    if (text_h <= 0) {
        text_h = AES_CHAR_HEIGHT;
    }
    button_h = (WORD) (text_h + 12);
    if (_aes_alert_icon_asset(alert->icon) != NULL) {
        icon_w = 32;
        icon_h = 32;
    }

    for (i = 0; i < alert->line_count; ++i) {
        WORD width = (WORD) _vdi_string_width(alert->lines[i]);

        if (width > text_w) {
            text_w = width;
        }
    }

    for (i = 0; i < alert->button_count; ++i) {
        WORD width = (WORD) (_vdi_string_width(alert->buttons[i]) + 18);

        if (width < 48) {
            width = 48;
        }
        alert->button_rects[i].g_w = width;
        alert->button_rects[i].g_h = button_h;
        button_total = (WORD) (button_total + width);
        if (i + 1 < alert->button_count) {
            button_total = (WORD) (button_total + button_gap);
        }
    }

    content_w = (WORD) (text_w + icon_w + ((icon_w > 0) ? 12 : 0));
    if (button_total > content_w) {
        content_w = button_total;
    }
    content_h = (WORD) (alert->line_count * text_h +
        (alert->line_count - 1) * line_gap);
    if (icon_h > content_h) {
        content_h = icon_h;
    }

    _aes_desktop_rect(&desktop);
    alert->outer.g_w = (WORD) (content_w + 32);
    alert->outer.g_h = (WORD) (content_h + button_h + 40);
    alert->outer.g_x = (WORD) (desktop.g_x +
        (desktop.g_w - alert->outer.g_w) / 2);
    alert->outer.g_y = (WORD) (desktop.g_y +
        (desktop.g_h - alert->outer.g_h) / 2);

    x = (WORD) (alert->outer.g_x +
        (alert->outer.g_w - button_total) / 2);
    for (i = 0; i < alert->button_count; ++i) {
        alert->button_rects[i].g_x = x;
        alert->button_rects[i].g_y = (WORD) (alert->outer.g_y +
            alert->outer.g_h - button_h - 12);
        x = (WORD) (x + alert->button_rects[i].g_w + button_gap);
    }
}

static void _aes_alert_draw_text(WORD x, WORD y, const char *text)
{
    vst_color(_aes.vdi_handle, WHITE);
    v_gtext(_aes.vdi_handle, x, y, (CONST BYTE *) text);
}

static void _aes_alert_draw_frame(const GRECT *rect)
{
    WORD fill[4];
    WORD border[10];

    if (rect == NULL) {
        return;
    }

    fill[0] = rect->g_x;
    fill[1] = rect->g_y;
    fill[2] = (WORD) (rect->g_x + rect->g_w - 1);
    fill[3] = (WORD) (rect->g_y + rect->g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    vr_recfl(_aes.vdi_handle, fill);

    border[0] = fill[0];
    border[1] = fill[1];
    border[2] = fill[2];
    border[3] = fill[1];
    border[4] = fill[2];
    border[5] = fill[3];
    border[6] = fill[0];
    border[7] = fill[3];
    border[8] = fill[0];
    border[9] = fill[1];
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, border);
}

static void _aes_alert_draw_button(const GRECT *rect,
    const char *label, int is_default)
{
    WORD fill[4];
    WORD border[10];
    WORD inner[10];
    WORD text_x;
    WORD text_y;
    WORD width;

    if (rect == NULL || label == NULL) {
        return;
    }

    fill[0] = rect->g_x;
    fill[1] = rect->g_y;
    fill[2] = (WORD) (rect->g_x + rect->g_w - 1);
    fill[3] = (WORD) (rect->g_y + rect->g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    vr_recfl(_aes.vdi_handle, fill);

    border[0] = fill[0];
    border[1] = fill[1];
    border[2] = fill[2];
    border[3] = fill[1];
    border[4] = fill[2];
    border[5] = fill[3];
    border[6] = fill[0];
    border[7] = fill[3];
    border[8] = fill[0];
    border[9] = fill[1];
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, border);

    if (is_default != 0 && rect->g_w > 4 && rect->g_h > 4) {
        inner[0] = (WORD) (fill[0] + 2);
        inner[1] = (WORD) (fill[1] + 2);
        inner[2] = (WORD) (fill[2] - 2);
        inner[3] = inner[1];
        inner[4] = inner[2];
        inner[5] = (WORD) (fill[3] - 2);
        inner[6] = inner[0];
        inner[7] = inner[5];
        inner[8] = inner[0];
        inner[9] = inner[1];
        v_pline(_aes.vdi_handle, 5, inner);
    }

    width = (WORD) _vdi_string_width(label);
    text_x = (WORD) (rect->g_x + (rect->g_w - width) / 2);
    text_y = (WORD) (rect->g_y +
        (rect->g_h - _vdi_font_text_height()) / 2 + _vdi_font_ascent());
    _aes_alert_draw_text(text_x, text_y, label);
}

static const aes_alert_icon_asset_t *_aes_alert_icon_asset(WORD icon)
{
    switch (icon) {
    case 1:
        return &aes_alert_exclamation_icon_asset;
    case 2:
        return &aes_alert_question_icon_asset;
    case 3:
        return &aes_alert_stop_icon_asset;
    default:
        return NULL;
    }
}

static int _aes_alert_icon_bit(const UWORD *bits,
    const aes_alert_icon_asset_t *asset, WORD x, WORD y)
{
    size_t row_words;
    size_t index;
    UWORD word;

    if (bits == NULL || asset == NULL || x < 0 || y < 0 ||
        x >= asset->width || y >= asset->height) {
        return 0;
    }

    row_words = (size_t) ((asset->width + 15) / 16);
    index = (size_t) y * row_words + (size_t) x / 16u;
    word = bits[index];
    return (word & (WORD) (0x8000u >> (x % 16))) != 0;
}

static void _aes_alert_draw_icon(const aes_alert_icon_asset_t *asset,
    WORD x, WORD y)
{
    WORD row;

    if (asset == NULL) {
        return;
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;
            WORD rect[4];

            while (col < asset->width &&
                _aes_alert_icon_bit(asset->mask_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                _aes_alert_icon_bit(asset->mask_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD) (x + start);
            rect[1] = (WORD) (y + row);
            rect[2] = (WORD) (x + col - 1);
            rect[3] = rect[1];
            vsf_color(_aes.vdi_handle, BLACK);
            v_bar(_aes.vdi_handle, rect);
        }
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;
            WORD rect[4];

            while (col < asset->width &&
                _aes_alert_icon_bit(asset->data_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                _aes_alert_icon_bit(asset->data_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD) (x + start);
            rect[1] = (WORD) (y + row);
            rect[2] = (WORD) (x + col - 1);
            rect[3] = rect[1];
            vsf_color(_aes.vdi_handle, WHITE);
            v_bar(_aes.vdi_handle, rect);
        }
    }
}

static void _aes_draw_alert(const aes_alert_t *alert)
{
    WORD clip[4];
    WORD text_x;
    WORD text_y;
    WORD text_h;
    WORD line_gap = 4;
    WORD i;

    if (alert == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    clip[0] = alert->outer.g_x;
    clip[1] = alert->outer.g_y;
    clip[2] = (WORD) (alert->outer.g_x + alert->outer.g_w - 1);
    clip[3] = (WORD) (alert->outer.g_y + alert->outer.g_h - 1);

    wind_update(BEG_UPDATE);
    v_hide_c(_aes.vdi_handle);
    vs_clip(_aes.vdi_handle, 1, clip);
    (void) vswr_mode(_aes.vdi_handle, MD_REPLACE);

    _aes_alert_draw_frame(&alert->outer);
    text_x = (WORD) (alert->outer.g_x + 12);
    if (_aes_alert_icon_asset(alert->icon) != NULL) {
        _aes_alert_draw_icon(_aes_alert_icon_asset(alert->icon),
            text_x, (WORD) (alert->outer.g_y + 12));
        text_x = (WORD) (text_x + 44);
    }

    text_h = _vdi_font_text_height();
    for (i = 0; i < alert->line_count; ++i) {
        text_y = (WORD) (alert->outer.g_y + 12 +
            i * (text_h + line_gap) + _vdi_font_ascent());
        _aes_alert_draw_text(text_x, text_y, alert->lines[i]);
    }
    for (i = 0; i < alert->button_count; ++i) {
        _aes_alert_draw_button(&alert->button_rects[i], alert->buttons[i],
            i + 1 == alert->default_button);
    }

    vs_clip(_aes.vdi_handle, 0, clip);
    v_show_c(_aes.vdi_handle, 0);
    wind_update(END_UPDATE);
}

static WORD _aes_run_alert(aes_alert_t *alert)
{
    gem_hid_event_t evt;
    uint8_t *saved_pixels = NULL;
    WORD i;

    if (alert == NULL) {
        return 1;
    }

    _vdi_begin_update();
    v_hide_c(_aes.vdi_handle);
    (void) _aes_save_region_pixels(&alert->outer, &saved_pixels);
    v_show_c(_aes.vdi_handle, 0);
    _vdi_end_update();
    _aes_draw_alert(alert);

    for (;;) {
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_QUIT) {
            break;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE) {
            _aes_store_mouse_state(&evt);
        }
        if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
            WORD ch = (WORD) (evt.key & 0xffu);

            if (ch == 13 || ch == '\n' || ch == ' ') {
                break;
            }
            if (ch >= '1' && ch < '1' + alert->button_count) {
                wind_update(BEG_UPDATE);
                v_hide_c(_aes.vdi_handle);
                _aes_restore_region_pixels(&alert->outer, saved_pixels);
                v_show_c(_aes.vdi_handle, 0);
                wind_update(END_UPDATE);
                if (saved_pixels != NULL) {
                    gem_os_free(saved_pixels);
                }
                return (WORD) (ch - '0');
            }
            if (ch == 27) {
                break;
            }
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
            for (i = 0; i < alert->button_count; ++i) {
                if (_aes_point_in_rect((WORD) evt.x, (WORD) evt.y,
                        &alert->button_rects[i]) != 0) {
                    wind_update(BEG_UPDATE);
                    v_hide_c(_aes.vdi_handle);
                    _aes_restore_region_pixels(&alert->outer, saved_pixels);
                    v_show_c(_aes.vdi_handle, 0);
                    wind_update(END_UPDATE);
                    if (saved_pixels != NULL) {
                        gem_os_free(saved_pixels);
                    }
                    return (WORD) (i + 1);
                }
            }
        }
    }

    wind_update(BEG_UPDATE);
    v_hide_c(_aes.vdi_handle);
    _aes_restore_region_pixels(&alert->outer, saved_pixels);
    v_show_c(_aes.vdi_handle, 0);
    wind_update(END_UPDATE);
    if (saved_pixels != NULL) {
        gem_os_free(saved_pixels);
    }
    return alert->default_button;
}

WORD menu_bar(OBJECT *tree, WORD show)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);

    _aes.menu_tree = tree;
    _aes.menu_visible = (show != 0) ? 1 : 0;
    _aes.menu_owner_app_id = _aes.current_app_id;
    _aes.active_app_id = _aes.current_app_id;
    if (show != 0 && tree != NULL && _aes.desktop_owner_app_id == 0) {
        _aes.desktop_owner_app_id = _aes.current_app_id;
    }
    if (app != NULL) {
        app->menu_tree = tree;
        app->menu_visible = _aes.menu_visible;
    }
    if (show != 0 && tree != NULL) {
        _aes_menu_prepare_tree(tree);
        _aes_menu_hide_popups(tree);
        _aes_menu_redraw_tree(tree);
    } else {
        _aes_menu_clear_saved_region();
    }
    _aes_trace("menu_bar tree=%p show=%d", (void *) tree, show);
    return 1;
}

WORD menu_icheck(OBJECT *tree, WORD item, WORD check)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (check != 0) {
        tree[item].ob_state |= CHECKED;
    } else {
        tree[item].ob_state &= (UWORD) ~CHECKED;
    }
    return 1;
}

WORD menu_ienable(OBJECT *tree, WORD item, WORD enable)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (enable != 0) {
        tree[item].ob_state &= (UWORD) ~DISABLED;
    } else {
        tree[item].ob_state |= DISABLED;
    }
    return 1;
}

WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    if (tree == NULL || title < 0) {
        return 0;
    }
    if (normal != 0) {
        tree[title].ob_state &= (UWORD) ~SELECTED;
    } else {
        tree[title].ob_state |= SELECTED;
    }
    return 1;
}

WORD menu_text(OBJECT *tree, WORD item, char *text)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    tree[item].ob_spec = (LONG) (intptr_t) text;
    return 1;
}

WORD menu_register(WORD apid, char *name)
{
    aes_app_t *app = _aes_find_app_by_id(apid);

    if (app == NULL || name == NULL) {
        return 0;
    }
    strncpy(app->name, name, sizeof(app->name) - 1u);
    app->name[sizeof(app->name) - 1u] = '\0';
    return apid;
}

WORD menu_unregister(WORD mid)
{
    return (_aes_find_app_by_id(mid) != NULL) ? 1 : 0;
}

WORD menu_click(WORD click, WORD setit)
{
    if (setit != 0) {
        _aes.menu_click = click;
    }
    return _aes.menu_click;
}

WORD objc_add(OBJECT *tree, WORD parent, WORD child)
{
    WORD last;

    if (tree == NULL || parent < 0 || child < 0) {
        return 0;
    }

    if (tree[parent].ob_head == NIL) {
        tree[parent].ob_head = child;
    } else {
        last = tree[parent].ob_tail;
        tree[last].ob_next = child;
    }
    tree[parent].ob_tail = child;
    tree[child].ob_next = parent;
    return 1;
}

WORD objc_delete(OBJECT *tree, WORD object)
{
    size_t i;

    if (tree == NULL || object < 0) {
        return 0;
    }

    for (i = 0; i < 1024u; ++i) {
        if (tree[i].ob_head == object) {
            tree[i].ob_head = tree[object].ob_next;
            if (tree[i].ob_tail == object) {
                tree[i].ob_tail = NIL;
            }
            break;
        }
        if (tree[i].ob_head != NIL) {
            WORD node = tree[i].ob_head;

            while (node != NIL && node != (WORD) i) {
                if (tree[node].ob_next == object) {
                    tree[node].ob_next = tree[object].ob_next;
                    if (tree[i].ob_tail == object) {
                        tree[i].ob_tail = node;
                    }
                    break;
                }
                node = tree[node].ob_next;
            }
        }
    }
    tree[object].ob_next = NIL;
    return 1;
}

WORD objc_draw(OBJECT *tree,
               WORD startob,
               WORD depth,
               WORD xc,
               WORD yc,
               WORD wc,
               WORD hc)
{
    WORD clip[4];
    WORD origin_x = 0;
    WORD origin_y = 0;
    WORD text_attrib[10];
    WORD previous_font = 0;
    WORD restore_font = 0;

    (void) depth;

    if (tree == NULL || startob < 0 || _aes_ensure_vdi() == 0) {
        _aes_trace("objc_draw skipped tree=%p start=%d vdi=%d",
            (void *) tree, startob, _aes.vdi_ready);
        return 0;
    }

    _aes_trace("objc_draw tree=%p start=%d depth=%d clip=%d,%d %dx%d type=%u",
        (void *) tree, startob, depth, xc, yc, wc, hc,
        (unsigned) tree[startob].ob_type);

    if (startob == ROOT && tree != _aes.menu_tree) {
        _aes.hover_tree = tree;
    }

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
        restore_font = 1;
    }
    (void) vst_font(_aes.vdi_handle, 1);

    clip[0] = xc;
    clip[1] = yc;
    clip[2] = (WORD) (xc + wc - 1);
    clip[3] = (WORD) (yc + hc - 1);
    _aes_object_extent(tree, startob, &origin_x, &origin_y);
    vs_clip(_aes.vdi_handle, 1, clip);
    _aes_draw_tree_recursive(tree, startob,
        (WORD) (origin_x - tree[startob].ob_x),
        (WORD) (origin_y - tree[startob].ob_y), depth, clip);
    vs_clip(_aes.vdi_handle, 0, clip);
    if (restore_font != 0) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
    return 1;
}

WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my)
{
    if (tree == NULL || startob < 0) {
        return NIL;
    }

    return _aes_find_in_subtree(tree, startob, 0, 0, depth, mx, my);
}

WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    _aes_object_extent(tree, object, x, y);
    return 1;
}

WORD objc_order(OBJECT *tree, WORD object, WORD newpos)
{
    (void) tree;
    (void) object;
    (void) newpos;
    return 1;
}

WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx, WORD kind)
{
    TEDINFO *ted;
    char *text;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }
    if (kind == EDSTART || kind == EDINIT) {
        _aes.edit_tree = tree;
        _aes.edit_object = object;
        _aes.edit_index = *idx;
        return 1;
    }
    if (kind == EDEND) {
        if (_aes.edit_tree == tree && _aes.edit_object == object) {
            _aes.edit_tree = NULL;
            _aes.edit_object = NIL;
            _aes.edit_index = 0;
        }
        return 1;
    }
    if (kind != EDCHAR || tree[object].ob_spec == 0) {
        return 1;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    text = (char *) (intptr_t) ted->te_ptext;
    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (*idx < ted->te_txtlen - 1 && charidx >= 32 && charidx <= 126) {
        if ((size_t) *idx > length) {
            *idx = (WORD) length;
        }
        if (length < (size_t) (ted->te_txtlen - 1)) {
            memmove(&text[*idx + 1], &text[*idx],
                length - (size_t) *idx + 1u);
            text[*idx] = (char) charidx;
            ++(*idx);
        } else if ((size_t) *idx < length) {
            memmove(&text[*idx + 1], &text[*idx],
                length - (size_t) *idx);
            text[*idx] = (char) charidx;
            text[ted->te_txtlen - 1] = '\0';
            ++(*idx);
        } else {
            text[*idx] = (char) charidx;
            text[*idx + 1] = '\0';
            ++(*idx);
        }
    }
    _aes.edit_tree = tree;
    _aes.edit_object = object;
    _aes.edit_index = *idx;
    return 1;
}

WORD objc_change(OBJECT *tree,
                 WORD object,
                 WORD depth,
                 WORD xc,
                 WORD yc,
                 WORD wc,
                 WORD hc,
                 WORD newstate,
                 WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    (void) depth;
    tree[object].ob_state = (UWORD) newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, 0, xc, yc, wc, hc);
    }
    return 1;
}

WORD form_do(OBJECT *tree, WORD startob)
{
    WORD active;
    WORD idx = 0;

    if (tree == NULL) {
        return 0;
    }

    active = (_aes_form_is_editable(tree, startob) != 0) ? startob :
        _aes_form_find_next_editable(tree, startob, 0);
    if (active != NIL) {
        TEDINFO *ted = (TEDINFO *) (intptr_t) tree[active].ob_spec;
        char *buffer = (ted != NULL) ? (char *) (intptr_t) ted->te_ptext : NULL;

        idx = (buffer != NULL) ? (WORD) strlen(buffer) : 0;
        _aes_form_set_active_field(tree, active, &idx);
    }

    FOREVER {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        event = evnt_multi(MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            NULL, 0, 0, &mx, &my, &mb, &ks, &kr, &br);

        if ((event & MU_KEYBD) != 0) {
            WORD ascii = (WORD) (kr & 0xffu);
            WORD scancode = (WORD) ((kr >> 8) & 0xffu);

            if (ascii == 27) {
                WORD cancel = _aes_form_find_default_exit(tree);

                if (cancel != NIL) {
                    return cancel;
                }
                return 0;
            }
            if (ascii == '\t' || scancode == 43 || scancode == 15) {
                active = _aes_form_find_next_editable(tree, active,
                    (_aes.key_state & (K_LSHIFT | K_RSHIFT)) != 0u);
                if (active != NIL) {
                    TEDINFO *ted = (TEDINFO *) (intptr_t) tree[active].ob_spec;
                    char *buffer = (ted != NULL) ?
                        (char *) (intptr_t) ted->te_ptext : NULL;

                    idx = (buffer != NULL) ? (WORD) strlen(buffer) : 0;
                    _aes_form_set_active_field(tree, active, &idx);
                }
                continue;
            }
            if (scancode == 41 || scancode == 28 ||
                ascii == '\r' || ascii == '\n') {
                WORD button = _aes_form_find_default_exit(tree);

                if (button != NIL) {
                    _aes_form_flash_object(tree, button);
                    return button;
                }
            }
            if (active != NIL &&
                _aes_form_apply_key(tree, active, kr, &idx) != 0) {
                _aes_form_redraw_object(tree, active);
            }
        }

        if ((event & MU_BUTTON) != 0) {
            WORD object = objc_find(tree, ROOT, MAX_DEPTH, mx, my);

            if (object == NIL) {
                continue;
            }
            if (_aes_form_is_editable(tree, object) != 0) {
                if (object != active) {
                    active = object;
                    idx = 0;
                    _aes_form_set_active_field(tree, active, &idx);
                }
                _aes_form_set_caret_from_click(tree, object, mx, &idx);
                _aes_form_redraw_object(tree, object);
                continue;
            }
            if ((tree[object].ob_flags & EXIT) != 0u) {
                _aes_form_flash_object(tree, object);
                return object;
            }
            if ((tree[object].ob_flags & SELECTABLE) != 0u) {
                WORD newobj = object;

                (void) form_button(tree, object, 1, &newobj);
                _aes_form_redraw_object(tree, object);
                if ((tree[object].ob_flags & RBUTTON) != 0u) {
                    _aes_form_redraw_tree(tree);
                }
            }
        }
    }
}

WORD form_dial(WORD flag,
               WORD x1,
               WORD y1,
               WORD w1,
               WORD h1,
               WORD x2,
               WORD y2,
               WORD w2,
               WORD h2)
{
    GRECT dirty;
    GRECT desktop;

    (void) flag;
    (void) x1;
    (void) y1;
    (void) w1;
    (void) h1;
    (void) x2;
    (void) y2;
    (void) w2;
    (void) h2;

    if (flag == FMD_START || flag == FMD_GROW) {
        _aes_desktop_rect(&desktop);
        if (desktop.g_w > 0 && desktop.g_h > 0) {
            _aes_redraw_region(&desktop);
        }
    }

    if (flag == FMD_FINISH || flag == FMD_SHRINK) {
        _aes.hover_tree = NULL;
        _aes_set_rect(&dirty, x2, y2, w2, h2);
        _aes_redraw_region(&dirty);
    }

    return 1;
}

WORD form_alert(WORD defbtn, char *str)
{
    aes_alert_t alert;

    if (_aes_parse_alert(str, defbtn, &alert) == 0) {
        return (defbtn > 0) ? defbtn : 1;
    }
    _aes_alert_compute_layout(&alert);
    return _aes_run_alert(&alert);
}

WORD form_error(WORD errnum)
{
    switch (errnum) {
    case 2:
        return form_alert(1, "[3][ File not found ][ OK ]");
    case 8:
        return form_alert(1, "[3][ Insufficient memory ][ OK ]");
    default:
        return form_alert(1, "[1][ AES form error ][ OK ]");
    }
}

WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch)
{
    WORD width = 200;
    WORD height = 100;

    if (tree != NULL) {
        width = tree[ROOT].ob_width;
        height = tree[ROOT].ob_height;
    }
    if (_aes_ensure_vdi() != 0) {
        if (cx != NULL) {
            *cx = (WORD) ((_aes.work_out[0] + 1 - width) / 2);
        }
        if (cy != NULL) {
            *cy = (WORD) ((_aes.work_out[1] + 1 - height) / 2);
        }
    } else {
        if (cx != NULL) {
            *cx = 0;
        }
        if (cy != NULL) {
            *cy = 0;
        }
    }
    if (cw != NULL) {
        *cw = width;
    }
    if (ch != NULL) {
        *ch = height;
    }
    return 1;
}

WORD form_keybd(OBJECT *tree,
                WORD object,
                WORD next,
                WORD thechar,
                WORD *newobj,
                WORD *newchar)
{
    (void) tree;
    if (newobj != NULL) {
        *newobj = next;
    }
    if (newchar != NULL) {
        *newchar = thechar;
    }
    return object;
}

WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj)
{
    (void) clicks;

    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state ^= SELECTED;
    if (newobj != NULL) {
        *newobj = object;
    }
    return 1;
}

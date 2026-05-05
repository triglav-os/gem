/*
 * Implements a hosted monochrome GEM clock application with a live time
 * view, a simple alarm, and editable clock fields backed by an internal
 * time offset instead of direct host clock mutation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/portab.h>

#include "platform/os.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    clock_root = 0,
    clock_mode_button,
    clock_alarm_button,
    clock_hour_button,
    clock_colon_label,
    clock_minute_button,
    clock_ampm_button,
    clock_month_button,
    clock_slash_1,
    clock_day_button,
    clock_slash_2,
    clock_year_button,
    clock_hint_label,
    clock_status_label,
    clock_object_count
};

typedef struct clock_state {
    WORD handle;
    GRECT normal_rect;
    OBJECT tree[clock_object_count];
    int64_t time_offset_seconds;
    int alarm_enabled;
    int show_alarm;
    int alarm_hour_24;
    int alarm_minute;
    int selected_field;
    int pending_digit;
    int last_alarm_minute_key;
    char hour_text[4];
    char minute_text[4];
    char ampm_text[4];
    char month_text[4];
    char day_text[4];
    char year_text[4];
    char status_text[32];
} clock_state_t;

static void clock_init_object(OBJECT *object,
                              UWORD type,
                              UWORD flags,
                              UWORD state,
                              LONG spec,
                              WORD x,
                              WORD y,
                              WORD width,
                              WORD height)
{
    if (object == NULL) {
        return;
    }

    object->ob_next = NIL;
    object->ob_head = NIL;
    object->ob_tail = NIL;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = width;
    object->ob_height = height;
}

static void clock_button(OBJECT *object,
                         const char *label,
                         WORD x,
                         WORD y,
                         WORD width,
                         WORD height)
{
    clock_init_object(object, G_BUTTON, SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) label, x, y, width, height);
}

static void clock_sync_root_rect(clock_state_t *state)
{
    GRECT work_rect;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
        &work_rect.g_w, &work_rect.g_h) == 0) {
        return;
    }

    state->tree[clock_root].ob_x = work_rect.g_x;
    state->tree[clock_root].ob_y = work_rect.g_y;
    state->tree[clock_root].ob_width = work_rect.g_w;
    state->tree[clock_root].ob_height = work_rect.g_h;
}

static void clock_draw(clock_state_t *state, const GRECT *dirty_rect)
{
    GRECT clip_rect;

    if (state == NULL) {
        return;
    }

    clock_sync_root_rect(state);
    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
        &clip_rect.g_w, &clip_rect.g_h);
    while (clip_rect.g_w > 0 && clip_rect.g_h > 0) {
        WORD x0 = clip_rect.g_x;
        WORD y0 = clip_rect.g_y;
        WORD x1 = (WORD) (clip_rect.g_x + clip_rect.g_w - 1);
        WORD y1 = (WORD) (clip_rect.g_y + clip_rect.g_h - 1);

        if (dirty_rect != NULL) {
            WORD dirty_x1 = (WORD) (dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_y1 = (WORD) (dirty_rect->g_y + dirty_rect->g_h - 1);

            if (x0 < dirty_rect->g_x) {
                x0 = dirty_rect->g_x;
            }
            if (y0 < dirty_rect->g_y) {
                y0 = dirty_rect->g_y;
            }
            if (x1 > dirty_x1) {
                x1 = dirty_x1;
            }
            if (y1 > dirty_y1) {
                y1 = dirty_y1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            objc_draw(state->tree, ROOT, MAX_DEPTH, x0, y0,
                (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }
    wind_update(END_UPDATE);
}

static void clock_redraw_object(clock_state_t *state, WORD object)
{
    WORD x;
    WORD y;
    GRECT dirty_rect;

    if (state == NULL || object < 0 || object >= clock_object_count) {
        return;
    }

    objc_offset(state->tree, object, &x, &y);
    dirty_rect.g_x = x;
    dirty_rect.g_y = y;
    dirty_rect.g_w = state->tree[object].ob_width;
    dirty_rect.g_h = state->tree[object].ob_height;
    clock_draw(state, &dirty_rect);
}

static void clock_format_field(char *buffer, size_t size, int value, int zero_pad)
{
    if (buffer == NULL || size < 3u) {
        return;
    }

    if (value < 0) {
        value = 0;
    }
    if (value > 99) {
        value = 99;
    }

    if (zero_pad) {
        snprintf(buffer, size, "%02d", value);
    } else {
        snprintf(buffer, size, "%2d", value);
    }
}

static time_t clock_effective_time(const clock_state_t *state)
{
    time_t now;

    now = time(NULL);
    if (state == NULL) {
        return now;
    }
    return (time_t) ((int64_t) now + state->time_offset_seconds);
}

static void clock_load_tm(const clock_state_t *state, struct tm *out_tm)
{
    time_t effective_time;
    struct tm *tm_ptr;

    if (out_tm == NULL) {
        return;
    }

    effective_time = clock_effective_time(state);
    tm_ptr = localtime(&effective_time);
    if (tm_ptr == NULL) {
        memset(out_tm, 0, sizeof(*out_tm));
        return;
    }

    *out_tm = *tm_ptr;
}

static void clock_store_tm(clock_state_t *state, const struct tm *tm_value)
{
    time_t desired;
    time_t now;

    if (state == NULL || tm_value == NULL) {
        return;
    }

    desired = mktime((struct tm *) tm_value);
    now = time(NULL);
    if (desired == (time_t) -1 || now == (time_t) -1) {
        return;
    }

    state->time_offset_seconds = (int64_t) desired - (int64_t) now;
}

static void clock_select_field(clock_state_t *state, int object)
{
    int ii;

    if (state == NULL) {
        return;
    }

    state->selected_field = object;
    state->pending_digit = -1;
    for (ii = 0; ii < clock_object_count; ++ii) {
        if (ii == clock_hour_button || ii == clock_minute_button ||
            ii == clock_month_button || ii == clock_day_button ||
            ii == clock_year_button) {
            if (ii == object) {
                state->tree[ii].ob_state |= SELECTED;
            } else {
                state->tree[ii].ob_state &= (UWORD) ~SELECTED;
            }
            clock_redraw_object(state, (WORD) ii);
        }
    }
}

static void clock_update_strings(clock_state_t *state)
{
    struct tm tm_value;
    int display_hour;
    int display_minute;
    int hour12;

    if (state == NULL) {
        return;
    }

    clock_load_tm(state, &tm_value);
    if (state->show_alarm) {
        display_hour = state->alarm_hour_24;
        display_minute = state->alarm_minute;
    } else {
        display_hour = tm_value.tm_hour;
        display_minute = tm_value.tm_min;
    }

    hour12 = display_hour % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }

    clock_format_field(state->hour_text, sizeof(state->hour_text), hour12, 0);
    clock_format_field(state->minute_text, sizeof(state->minute_text),
        display_minute, 1);
    snprintf(state->ampm_text, sizeof(state->ampm_text), "%s",
        (display_hour >= 12) ? "pm" : "am");
    clock_format_field(state->month_text, sizeof(state->month_text),
        tm_value.tm_mon + 1, 0);
    clock_format_field(state->day_text, sizeof(state->day_text),
        tm_value.tm_mday, 0);
    clock_format_field(state->year_text, sizeof(state->year_text),
        (tm_value.tm_year + 1900) % 100, 1);
    snprintf(state->status_text, sizeof(state->status_text), "%s",
        state->alarm_enabled ? "Alarm armed" : "Alarm off");

    state->tree[clock_mode_button].ob_spec = state->show_alarm ?
        (LONG) (intptr_t) "Show time" : (LONG) (intptr_t) "Show alarm";
    state->tree[clock_alarm_button].ob_spec = state->alarm_enabled ?
        (LONG) (intptr_t) "Alarm on" : (LONG) (intptr_t) "Alarm off";
    state->tree[clock_hour_button].ob_spec = (LONG) (intptr_t) state->hour_text;
    state->tree[clock_minute_button].ob_spec =
        (LONG) (intptr_t) state->minute_text;
    state->tree[clock_ampm_button].ob_spec = (LONG) (intptr_t) state->ampm_text;
    state->tree[clock_month_button].ob_spec =
        (LONG) (intptr_t) state->month_text;
    state->tree[clock_day_button].ob_spec = (LONG) (intptr_t) state->day_text;
    state->tree[clock_year_button].ob_spec = (LONG) (intptr_t) state->year_text;
    state->tree[clock_status_label].ob_spec =
        (LONG) (intptr_t) state->status_text;
}

static void clock_redraw_dynamic(clock_state_t *state)
{
    static const WORD objects[] = {
        clock_mode_button,
        clock_alarm_button,
        clock_hour_button,
        clock_minute_button,
        clock_ampm_button,
        clock_month_button,
        clock_day_button,
        clock_year_button,
        clock_status_label
    };
    size_t ii;

    if (state == NULL) {
        return;
    }

    clock_update_strings(state);
    for (ii = 0; ii < sizeof(objects) / sizeof(objects[0]); ++ii) {
        clock_redraw_object(state, objects[ii]);
    }
}

static void clock_beep(void)
{
    fputc('\a', stderr);
    fflush(stderr);
}

static void clock_check_alarm(clock_state_t *state)
{
    struct tm tm_value;
    int minute_key;

    if (state == NULL || !state->alarm_enabled) {
        return;
    }

    clock_load_tm(state, &tm_value);
    minute_key = tm_value.tm_yday * 1440 + tm_value.tm_hour * 60 + tm_value.tm_min;
    if (state->last_alarm_minute_key == minute_key) {
        return;
    }
    if (tm_value.tm_hour == state->alarm_hour_24 &&
        tm_value.tm_min == state->alarm_minute) {
        state->last_alarm_minute_key = minute_key;
        clock_beep();
    }
}

static int clock_apply_two_digit_value(clock_state_t *state, int field, int value)
{
    struct tm tm_value;
    int hour24;

    if (state == NULL) {
        return 0;
    }

    clock_load_tm(state, &tm_value);
    switch (field) {
    case clock_hour_button:
        if (value < 1 || value > 12) {
            return 0;
        }
        if (state->show_alarm) {
            hour24 = state->alarm_hour_24;
            hour24 %= 12;
            if (value == 12) {
                value = 0;
            }
            state->alarm_hour_24 = value + ((hour24 >= 12) ? 12 : 0);
            if (state->alarm_hour_24 >= 24) {
                state->alarm_hour_24 -= 24;
            }
        } else {
            hour24 = tm_value.tm_hour;
            if (value == 12) {
                value = 0;
            }
            tm_value.tm_hour = value + ((hour24 >= 12) ? 12 : 0);
            if (tm_value.tm_hour >= 24) {
                tm_value.tm_hour -= 24;
            }
            clock_store_tm(state, &tm_value);
        }
        return 1;
    case clock_minute_button:
        if (value < 0 || value > 59) {
            return 0;
        }
        if (state->show_alarm) {
            state->alarm_minute = value;
        } else {
            tm_value.tm_min = value;
            clock_store_tm(state, &tm_value);
        }
        return 1;
    case clock_month_button:
        if (value < 1 || value > 12) {
            return 0;
        }
        tm_value.tm_mon = value - 1;
        clock_store_tm(state, &tm_value);
        return 1;
    case clock_day_button:
        if (value < 1 || value > 31) {
            return 0;
        }
        tm_value.tm_mday = value;
        clock_store_tm(state, &tm_value);
        return 1;
    case clock_year_button:
        if (value < 0 || value > 99) {
            return 0;
        }
        tm_value.tm_year = ((value > 83) ? (1900 + value) : (2000 + value)) - 1900;
        clock_store_tm(state, &tm_value);
        return 1;
    default:
        return 0;
    }
}

static void clock_toggle_ampm(clock_state_t *state)
{
    struct tm tm_value;

    if (state == NULL) {
        return;
    }

    if (state->show_alarm) {
        state->alarm_hour_24 = (state->alarm_hour_24 + 12) % 24;
    } else {
        clock_load_tm(state, &tm_value);
        tm_value.tm_hour = (tm_value.tm_hour + 12) % 24;
        clock_store_tm(state, &tm_value);
    }
}

static void clock_handle_digit(clock_state_t *state, int digit)
{
    int value;

    if (state == NULL || state->selected_field < 0) {
        return;
    }

    if (state->pending_digit < 0) {
        state->pending_digit = digit;
        switch (state->selected_field) {
        case clock_hour_button:
            snprintf(state->hour_text, sizeof(state->hour_text), "%d ", digit);
            clock_redraw_object(state, clock_hour_button);
            break;
        case clock_minute_button:
            snprintf(state->minute_text, sizeof(state->minute_text), "%d ", digit);
            clock_redraw_object(state, clock_minute_button);
            break;
        case clock_month_button:
            snprintf(state->month_text, sizeof(state->month_text), "%d ", digit);
            clock_redraw_object(state, clock_month_button);
            break;
        case clock_day_button:
            snprintf(state->day_text, sizeof(state->day_text), "%d ", digit);
            clock_redraw_object(state, clock_day_button);
            break;
        case clock_year_button:
            snprintf(state->year_text, sizeof(state->year_text), "%d ", digit);
            clock_redraw_object(state, clock_year_button);
            break;
        default:
            break;
        }
        return;
    }

    value = state->pending_digit * 10 + digit;
    state->pending_digit = -1;
    if (!clock_apply_two_digit_value(state, state->selected_field, value)) {
        clock_beep();
    }
    clock_redraw_dynamic(state);
}

static void clock_handle_button(clock_state_t *state, WORD object)
{
    if (state == NULL) {
        return;
    }

    switch (object) {
    case clock_mode_button:
        state->show_alarm = !state->show_alarm;
        break;
    case clock_alarm_button:
        state->alarm_enabled = !state->alarm_enabled;
        break;
    case clock_hour_button:
    case clock_minute_button:
    case clock_month_button:
    case clock_day_button:
    case clock_year_button:
        clock_select_field(state, object);
        break;
    case clock_ampm_button:
        clock_toggle_ampm(state);
        break;
    default:
        break;
    }

    clock_redraw_dynamic(state);
}

static WORD clock_map_key(WORD key_code)
{
    WORD ch;

    ch = (WORD) (key_code & 0x00ff);
    if (ch >= '0' && ch <= '9') {
        return ch;
    }
    if (ch == 'a' || ch == 'A') {
        return 'a';
    }
    if (ch == 't' || ch == 'T') {
        return 't';
    }
    return -1;
}

static int clock_flash_button(clock_state_t *state, WORD object)
{
    WORD x;
    WORD y;
    WORD pressed_state;

    if (state == NULL || object < 0 || object >= clock_object_count) {
        return 0;
    }

    objc_offset(state->tree, object, &x, &y);
    pressed_state = (WORD) (state->tree[object].ob_state | SELECTED);
    (void) objc_change(state->tree, object, 0, x, y,
        state->tree[object].ob_width, state->tree[object].ob_height,
        pressed_state, 1);
    gem_os_sleep_ms(90u);
    state->tree[object].ob_state &= (UWORD) ~SELECTED;
    (void) objc_change(state->tree, object, 0, x, y,
        state->tree[object].ob_width, state->tree[object].ob_height,
        state->tree[object].ob_state, 1);
    return 1;
}

static void clock_init_tree(clock_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->selected_field = -1;
    state->pending_digit = -1;
    state->alarm_hour_24 = 7;
    state->alarm_minute = 30;
    state->last_alarm_minute_key = -1;

    clock_init_object(&state->tree[clock_root], G_BOX, LASTOB, NORMAL, 0L,
        0, 0, 300, 154);
    clock_button(&state->tree[clock_mode_button], "Show alarm", 16, 14, 112, 22);
    clock_button(&state->tree[clock_alarm_button], "Alarm off", 140, 14, 112, 22);
    clock_button(&state->tree[clock_hour_button], "12", 34, 52, 44, 24);
    clock_init_object(&state->tree[clock_colon_label], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) ":", 86, 60, 8, 8);
    clock_button(&state->tree[clock_minute_button], "00", 100, 52, 44, 24);
    clock_button(&state->tree[clock_ampm_button], "am", 158, 52, 44, 24);
    clock_button(&state->tree[clock_month_button], " 1", 34, 90, 44, 22);
    clock_init_object(&state->tree[clock_slash_1], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "/", 88, 97, 8, 8);
    clock_button(&state->tree[clock_day_button], " 1", 100, 90, 44, 22);
    clock_init_object(&state->tree[clock_slash_2], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "/", 154, 97, 8, 8);
    clock_button(&state->tree[clock_year_button], "26", 168, 90, 44, 22);
    clock_init_object(&state->tree[clock_hint_label], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Click a field, then type two digits.", 16, 122, 220, 8);
    clock_init_object(&state->tree[clock_status_label], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Alarm off", 16, 136, 120, 8);

    objc_add(state->tree, clock_root, clock_mode_button);
    objc_add(state->tree, clock_root, clock_alarm_button);
    objc_add(state->tree, clock_root, clock_hour_button);
    objc_add(state->tree, clock_root, clock_colon_label);
    objc_add(state->tree, clock_root, clock_minute_button);
    objc_add(state->tree, clock_root, clock_ampm_button);
    objc_add(state->tree, clock_root, clock_month_button);
    objc_add(state->tree, clock_root, clock_slash_1);
    objc_add(state->tree, clock_root, clock_day_button);
    objc_add(state->tree, clock_root, clock_slash_2);
    objc_add(state->tree, clock_root, clock_year_button);
    objc_add(state->tree, clock_root, clock_hint_label);
    objc_add(state->tree, clock_root, clock_status_label);

    clock_update_strings(state);
}

int main(void)
{
    WORD appl_id;
    WORD msg[8] = { 0 };
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    WORD object;
    WORD mapped_key;
    GRECT current_rect;
    GRECT full_rect;
    clock_state_t state;

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    clock_init_tree(&state);
    state.handle = wind_create((UWORD) (NAME | MOVER | CLOSER | FULLER),
        0, 0, 640, 400);
    if (state.handle <= 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.handle, WF_NAME, "Clock");
    (void) wind_open(state.handle, 80, 50, 320, 220);
    (void) wind_get(state.handle, WF_WXYWH, &state.normal_rect.g_x,
        &state.normal_rect.g_y, &state.normal_rect.g_w,
        &state.normal_rect.g_h);
    wind_get(0, WF_WXYWH, &full_rect.g_x, &full_rect.g_y, &full_rect.g_w,
        &full_rect.g_h);
    (void) graf_mouse(M_ON, NULL);
    clock_draw(&state, NULL);

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_BUTTON | MU_KEYBD |
            MU_TIMER),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            1000, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0) {
            if (msg[3] != state.handle) {
                if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
                    break;
                }
                continue;
            }

            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw_rect;

                redraw_rect.g_x = msg[4];
                redraw_rect.g_y = msg[5];
                redraw_rect.g_w = msg[6];
                redraw_rect.g_h = msg[7];
                clock_draw(&state, &redraw_rect);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                    msg[7]);
                (void) wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x != full_rect.g_x ||
                    current_rect.g_y != full_rect.g_y ||
                    current_rect.g_w != full_rect.g_w ||
                    current_rect.g_h != full_rect.g_h) {
                    state.normal_rect = current_rect;
                }
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_TOPPED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_FULLED) {
                wind_update(BEG_UPDATE);
                (void) wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x == full_rect.g_x &&
                    current_rect.g_y == full_rect.g_y &&
                    current_rect.g_w == full_rect.g_w &&
                    current_rect.g_h == full_rect.g_h) {
                    (void) wind_set(state.handle, WF_WXYWH,
                        state.normal_rect.g_x, state.normal_rect.g_y,
                        state.normal_rect.g_w, state.normal_rect.g_h);
                } else {
                    state.normal_rect = current_rect;
                    (void) wind_set(state.handle, WF_WXYWH, full_rect.g_x,
                        full_rect.g_y, full_rect.g_w, full_rect.g_h);
                }
                wind_update(END_UPDATE);
            }
        }

        if ((event_flags & MU_TIMER) != 0) {
            clock_redraw_dynamic(&state);
            clock_check_alarm(&state);
        }

        if ((event_flags & MU_BUTTON) != 0 &&
            wind_find(mouse_x, mouse_y) == state.handle) {
            object = objc_find(state.tree, ROOT, MAX_DEPTH, mouse_x, mouse_y);
            if (object >= clock_mode_button && object < clock_object_count &&
                object != clock_colon_label && object != clock_slash_1 &&
                object != clock_slash_2 && object != clock_hint_label &&
                object != clock_status_label) {
                clock_flash_button(&state, object);
                clock_handle_button(&state, object);
            }
        }

        if ((event_flags & MU_KEYBD) != 0) {
            if (key_code == 0x011b) {
                break;
            }
            mapped_key = clock_map_key(key_code);
            if (mapped_key >= '0' && mapped_key <= '9') {
                clock_handle_digit(&state, mapped_key - '0');
            } else if (mapped_key == 'a') {
                clock_flash_button(&state, clock_alarm_button);
                clock_handle_button(&state, clock_alarm_button);
            } else if (mapped_key == 't') {
                clock_flash_button(&state, clock_mode_button);
                clock_handle_button(&state, clock_mode_button);
            }
        }
    }

    if (state.handle > 0) {
        (void) wind_close(state.handle);
        (void) wind_delete(state.handle);
    }
    appl_exit();
    gem_os_shutdown();
    return 0;
}

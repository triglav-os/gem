/*
 * Demonstrates a basic AES window whose client area is drawn from one
 * GEM object tree. Uses built-in AES object types with manual click handling.
 *
 * MIT License
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <platform/os.h>
#include <stdio.h>
#include <string.h>

enum {
    demo21_root = 0,
    demo21_heading,
    demo21_intro,
    demo21_checkbox_grid,
    demo21_checkbox_tips,
    demo21_radio_caption,
    demo21_radio_classic,
    demo21_radio_compact,
    demo21_status,
    demo21_ok,
    demo21_quit,
    demo21_object_count
};

typedef struct demo21_state {
    WORD handle;
    GRECT normal_rect;
    OBJECT tree[demo21_object_count];
    int show_grid;
    int show_tips;
    int mode;
    char status[80];
} demo21_state_t;

static void demo21_init_object(OBJECT *object,
    UWORD type, UWORD flags, UWORD state, LONG spec,
    WORD x, WORD y, WORD width, WORD height)
{
    if (object == NULL) return;

    object->ob_next   = NIL;
    object->ob_head   = NIL;
    object->ob_tail   = NIL;
    object->ob_type   = type;
    object->ob_flags  = flags;
    object->ob_state  = state;
    object->ob_spec   = spec;
    object->ob_x      = x;
    object->ob_y      = y;
    object->ob_width  = width;
    object->ob_height = height;
}

static void demo21_update_labels(demo21_state_t *state)
{
    if (state == NULL) return;

    const char *mode_name = (state->mode == 0) ? "Classic" : "Compact";

    snprintf(state->status, sizeof(state->status),
             "Grid %s, tips %s, mode %s.",
             state->show_grid ? "on" : "off",
             state->show_tips ? "on" : "off",
             mode_name);

    state->tree[demo21_status].ob_spec = (LONG)(intptr_t)state->status;

    /* Update checkbox and radio states */
    if (state->show_grid)
        state->tree[demo21_checkbox_grid].ob_state |= CHECKED;
    else
        state->tree[demo21_checkbox_grid].ob_state &= (UWORD)~CHECKED;

    if (state->show_tips)
        state->tree[demo21_checkbox_tips].ob_state |= CHECKED;
    else
        state->tree[demo21_checkbox_tips].ob_state &= (UWORD)~CHECKED;

    if (state->mode == 0) {
        state->tree[demo21_radio_classic].ob_state |= CHECKED;
        state->tree[demo21_radio_compact].ob_state &= (UWORD)~CHECKED;
    } else {
        state->tree[demo21_radio_classic].ob_state &= (UWORD)~CHECKED;
        state->tree[demo21_radio_compact].ob_state |= CHECKED;
    }
}

static void demo21_init_tree(demo21_state_t *state)
{
    if (state == NULL) return;

    demo21_init_object(&state->tree[demo21_root], G_BOX, LASTOB,
        NORMAL, 0L, 0, 0, 480, 220);

    demo21_init_object(&state->tree[demo21_heading], G_STRING, NONE,
        NORMAL, (LONG)(intptr_t)"Demo 21: AES controls", 16, 14, 160, 8);

    demo21_init_object(&state->tree[demo21_intro], G_STRING, NONE,
        NORMAL, (LONG)(intptr_t)"Built-in GEM objects with sample click logic.",
        16, 30, 280, 8);

    demo21_init_object(&state->tree[demo21_checkbox_grid], G_BUTTON,
        SELECTABLE, NORMAL, (LONG)(intptr_t)"Show grid", 16, 56, 124, 22);

    demo21_init_object(&state->tree[demo21_checkbox_tips], G_BUTTON,
        SELECTABLE, NORMAL, (LONG)(intptr_t)"Show tips", 16, 84, 124, 22);

    demo21_init_object(&state->tree[demo21_radio_caption], G_STRING, NONE,
        NORMAL, (LONG)(intptr_t)"Mode", 168, 56, 40, 8);

    demo21_init_object(&state->tree[demo21_radio_classic], G_BUTTON,
        SELECTABLE | RBUTTON, NORMAL,
        (LONG)(intptr_t)"Classic", 168, 84, 124, 22);

    demo21_init_object(&state->tree[demo21_radio_compact], G_BUTTON,
        SELECTABLE | RBUTTON, NORMAL,
        (LONG)(intptr_t)"Compact", 168, 110, 124, 22);

    demo21_init_object(&state->tree[demo21_status], G_STRING, NONE,
        NORMAL, 0L, 16, 142, 280, 8);

    demo21_init_object(&state->tree[demo21_ok], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG)(intptr_t)"OK", 250, 172, 84, 22);

    demo21_init_object(&state->tree[demo21_quit], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG)(intptr_t)"Quit", 346, 172, 84, 22);

    /* Build object tree */
    objc_add(state->tree, demo21_root, demo21_heading);
    objc_add(state->tree, demo21_root, demo21_intro);
    objc_add(state->tree, demo21_root, demo21_checkbox_grid);
    objc_add(state->tree, demo21_root, demo21_checkbox_tips);
    objc_add(state->tree, demo21_root, demo21_radio_caption);
    objc_add(state->tree, demo21_root, demo21_radio_classic);
    objc_add(state->tree, demo21_root, demo21_radio_compact);
    objc_add(state->tree, demo21_root, demo21_status);
    objc_add(state->tree, demo21_root, demo21_ok);
    objc_add(state->tree, demo21_root, demo21_quit);

    state->show_grid = 1;
    state->show_tips = 0;
    state->mode = 0;
    demo21_update_labels(state);
}

static void demo21_sync_root_rect(demo21_state_t *state)
{
    GRECT work;
    if (state == NULL) return;

    if (wind_get(state->handle, WF_CXYWH,
                 &work.g_x, &work.g_y, &work.g_w, &work.g_h))
    {
        state->tree[demo21_root].ob_x = work.g_x;
        state->tree[demo21_root].ob_y = work.g_y;
        state->tree[demo21_root].ob_width = work.g_w;
        state->tree[demo21_root].ob_height = work.g_h;
    }
}

static void demo21_draw(demo21_state_t *state, const GRECT *dirty_rect)
{
    GRECT clip;
    if (state == NULL) return;

    demo21_sync_root_rect(state);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &clip.g_x, &clip.g_y,
             &clip.g_w, &clip.g_h);

    while (clip.g_w > 0 && clip.g_h > 0) {
        WORD x0 = clip.g_x;
        WORD y0 = clip.g_y;
        WORD x1 = clip.g_x + clip.g_w - 1;
        WORD y1 = clip.g_y + clip.g_h - 1;

        if (dirty_rect) {
            /* intersect with dirty rect */
            if (x0 < dirty_rect->g_x) x0 = dirty_rect->g_x;
            if (y0 < dirty_rect->g_y) y0 = dirty_rect->g_y;
            if (x1 > dirty_rect->g_x + dirty_rect->g_w - 1)
                x1 = dirty_rect->g_x + dirty_rect->g_w - 1;
            if (y1 > dirty_rect->g_y + dirty_rect->g_h - 1)
                y1 = dirty_rect->g_y + dirty_rect->g_h - 1;
        }

        if (x0 <= x1 && y0 <= y1) {
            objc_draw(state->tree, ROOT, MAX_DEPTH, x0, y0,
                      (WORD)(x1 - x0 + 1), (WORD)(y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip.g_x, &clip.g_y,
                 &clip.g_w, &clip.g_h);
    }
    wind_update(END_UPDATE);
}

static int demo21_handle_object_click(demo21_state_t *state,
    WORD object, WORD mouse_x, WORD mouse_y)
{
    GRECT work;
    GRECT dirty;
    UWORD old_state;

    (void) mouse_x;
    (void) mouse_y;

    if (state == NULL) {
        return 0;
    }

    if (!wind_get(state->handle, WF_CXYWH, &work.g_x, &work.g_y,
            &work.g_w, &work.g_h)) {
        work.g_x = state->tree[demo21_root].ob_x;
        work.g_y = state->tree[demo21_root].ob_y;
        work.g_w = state->tree[demo21_root].ob_width;
        work.g_h = state->tree[demo21_root].ob_height;
    }

    dirty.g_x = (WORD) (work.g_x + state->tree[object].ob_x);
    dirty.g_y = (WORD) (work.g_y + state->tree[object].ob_y);
    dirty.g_w = state->tree[object].ob_width;
    dirty.g_h = state->tree[object].ob_height;

    old_state = state->tree[object].ob_state;
    state->tree[object].ob_state |= SELECTED;
    demo21_draw(state, &dirty);
    gem_os_sleep_ms(120u);
    state->tree[object].ob_state = old_state;

    switch (object) {
    case demo21_checkbox_grid:
        state->show_grid = !state->show_grid;
        break;
    case demo21_checkbox_tips:
        state->show_tips = !state->show_tips;
        break;
    case demo21_radio_classic:
        state->mode = 0;
        break;
    case demo21_radio_compact:
        state->mode = 1;
        break;
    case demo21_ok:
        break;
    case demo21_quit:
        return 1;
    default:
        return 0;
    }

    demo21_update_labels(state);

    demo21_draw(state, &work);
    return 0;
}

int main(void)
{
    WORD appl_id;
    WORD vdi_handle;
    WORD work_in[11]  = {1,1,1,1,1,1,1,1,1,1,2};
    WORD work_out[57] = {0};
    WORD msg[8] = {0};
    WORD mx = 0;
    WORD my = 0;
    WORD mb = 0;
    WORD ks = 0;
    GRECT full_rect;
    GRECT current_rect;
    demo21_state_t state = {0};

    appl_id = appl_init();
    if (appl_id < 0)
        return 1;

    vdi_handle = graf_handle();
    if (vdi_handle == 0) {
        appl_exit();
        return 1;
    }

    v_opnvwk(work_in, &vdi_handle, work_out);

    state.handle = wind_create(NAME | MOVER | CLOSER | FULLER | SIZER,
                               0, 0, work_out[0]+1, work_out[1]+1);

    if (state.handle <= 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    demo21_init_tree(&state);

    wind_set(state.handle, WF_NAME, "Demo 21 Controls", 0, 0);
    wind_open(state.handle, 120, 90, 500, 260);

    wind_get(state.handle, WF_CURRXYWH,
             &state.normal_rect.g_x, &state.normal_rect.g_y,
             &state.normal_rect.g_w, &state.normal_rect.g_h);
    wind_get(0, WF_WORKXYWH, &full_rect.g_x, &full_rect.g_y,
        &full_rect.g_w, &full_rect.g_h);

    graf_mouse(M_ON, NULL);
    demo21_draw(&state, NULL);

    while (1) {
        WORD event_flags = evnt_multi(MU_MESAG | MU_BUTTON | MU_KEYBD,
                                      1,1,1, 0,0,0,0,0, 0,0,0,0,0,
                                      msg, 0,0, &mx,&my,&mb,&ks,NULL,NULL);

        if (event_flags & MU_MESAG) {
            if (msg[3] != state.handle) continue;

            if (msg[0] == WM_CLOSED)
                break;

            if (msg[0] == WM_REDRAW) {
                GRECT r = {msg[4], msg[5], msg[6], msg[7]};
                demo21_draw(&state, &r);
            }
            else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                wind_set(state.handle, WF_CURRXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
            }
            else if (msg[0] == WM_TOPPED) {
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            }
            else if (msg[0] == WM_FULLED) {
                wind_get(state.handle, WF_CURRXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x == full_rect.g_x &&
                    current_rect.g_y == full_rect.g_y &&
                    current_rect.g_w == full_rect.g_w &&
                    current_rect.g_h == full_rect.g_h) {
                    wind_set(state.handle, WF_CURRXYWH,
                        state.normal_rect.g_x, state.normal_rect.g_y,
                        state.normal_rect.g_w, state.normal_rect.g_h);
                } else {
                    state.normal_rect = current_rect;
                    wind_set(state.handle, WF_CURRXYWH,
                        full_rect.g_x, full_rect.g_y,
                        full_rect.g_w, full_rect.g_h);
                }
            }
        }

        if (event_flags & MU_BUTTON) {
            if (wind_find(mx, my) == state.handle) {
                WORD obj = objc_find(state.tree, ROOT, MAX_DEPTH, mx, my);
                if (obj > 0 && demo21_handle_object_click(&state, obj, mx, my))
                    break;
            }
        }

        if ((event_flags & MU_KEYBD) && (ks == 0x011b))  /* Esc */
            break;
    }

    /* Cleanup */
    if (state.handle > 0) {
        wind_close(state.handle);
        wind_delete(state.handle);
    }

    v_clsvwk(vdi_handle);
    appl_exit();

    return 0;
}

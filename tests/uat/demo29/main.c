/*
 * Exercises G_BOXCHAR, G_USERDEF, graf_watchbox(), and graf_rubbox().
 * The sample is intentionally old-school: a plain object tree inside a
 * window, a custom user draw callback, and a small keypad-like cluster
 * of boxchar buttons.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <stdio.h>
#include <string.h>

enum {
    D29_ROOT = 0,
    D29_HEAD,
    D29_PROMPT,
    D29_USERDEF,
    D29_BC_7,
    D29_BC_8,
    D29_BC_9,
    D29_BC_PLUS,
    D29_BC_4,
    D29_BC_5,
    D29_BC_6,
    D29_BC_MINUS,
    D29_STATUS,
    D29_WATCH,
    D29_RUBBOX,
    D29_DONE,
    D29_COUNT
};

typedef struct demo29_state {
    WORD handle;
    OBJECT tree[D29_COUNT];
    USERBLK userblk;
    char status[96];
    WORD level;
} demo29_state_t;

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
static WORD work_out[57];
static demo29_state_t *g_demo29_state;

static void init_object(OBJECT *object,
                        UWORD type,
                        UWORD flags,
                        UWORD state,
                        LONG spec,
                        WORD x,
                        WORD y,
                        WORD w,
                        WORD h)
{
    object->ob_next = NIL;
    object->ob_head = NIL;
    object->ob_tail = NIL;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = w;
    object->ob_height = h;
}

static LONG demo29_draw_user(LONG parm_long)
{
    PARMBLK *parm;
    WORD pxy[4];
    WORD fill[4];
    WORD gauge[4];
    demo29_state_t *state;

    parm = (PARMBLK *) (intptr_t) parm_long;
    if (parm == NULL) {
        return 0;
    }

    state = (demo29_state_t *) (intptr_t) parm->pb_parm;
    fill[0] = parm->pb_x;
    fill[1] = parm->pb_y;
    fill[2] = (WORD) (parm->pb_x + parm->pb_w - 1);
    fill[3] = (WORD) (parm->pb_y + parm->pb_h - 1);
    vsf_color(vdi_handle, 0);
    v_bar(vdi_handle, fill);

    pxy[0] = fill[0];
    pxy[1] = fill[1];
    pxy[2] = fill[2];
    pxy[3] = fill[1];
    v_pline(vdi_handle, 2, pxy);
    pxy[1] = fill[3];
    pxy[3] = fill[3];
    v_pline(vdi_handle, 2, pxy);
    pxy[0] = fill[0];
    pxy[1] = fill[1];
    pxy[2] = fill[0];
    pxy[3] = fill[3];
    v_pline(vdi_handle, 2, pxy);
    pxy[0] = fill[2];
    pxy[2] = fill[2];
    v_pline(vdi_handle, 2, pxy);

    gauge[0] = (WORD) (fill[0] + 4);
    gauge[1] = (WORD) (fill[1] + 4);
    gauge[2] = (WORD) (fill[0] + 4 +
        ((parm->pb_w - 9) * state->level) / 10);
    gauge[3] = (WORD) (fill[3] - 4);
    if (gauge[2] >= gauge[0] && gauge[3] >= gauge[1]) {
        vsf_color(vdi_handle, 1);
        v_bar(vdi_handle, gauge);
    }

    v_gtext(vdi_handle, (WORD) (fill[0] + 6), (WORD) (fill[1] + 14),
        (CONST BYTE *) "USERBLK");
    return 0;
}

static void update_status(demo29_state_t *state, const char *text)
{
    strncpy(state->status, text, sizeof(state->status) - 1u);
    state->status[sizeof(state->status) - 1u] = '\0';
    state->tree[D29_STATUS].ob_spec = (LONG) (intptr_t) state->status;
}

static void init_tree(demo29_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->level = 5;
    state->userblk.ab_code = (LONG) (intptr_t) demo29_draw_user;
    state->userblk.ab_parm = (LONG) (intptr_t) state;

    init_object(&state->tree[D29_ROOT], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, 340, 204);
    init_object(&state->tree[D29_HEAD], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 29 boxchar and user objects", 14, 14, 220, 8);
    init_object(&state->tree[D29_PROMPT], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Click keypad, watchbox, and rubbox probes.",
        14, 30, 240, 8);
    init_object(&state->tree[D29_USERDEF], G_USERDEF, NONE, NORMAL,
        (LONG) (intptr_t) &state->userblk, 14, 48, 126, 28);
    init_object(&state->tree[D29_BC_7], G_BOXCHAR, SELECTABLE, NORMAL,
        0x37ff1100L, 166, 48, 24, 20);
    init_object(&state->tree[D29_BC_8], G_BOXCHAR, SELECTABLE, NORMAL,
        0x38ff1100L, 194, 48, 24, 20);
    init_object(&state->tree[D29_BC_9], G_BOXCHAR, SELECTABLE, NORMAL,
        0x39ff1100L, 222, 48, 24, 20);
    init_object(&state->tree[D29_BC_PLUS], G_BOXCHAR, SELECTABLE, NORMAL,
        0x2bff1105L, 250, 48, 24, 20);
    init_object(&state->tree[D29_BC_4], G_BOXCHAR, SELECTABLE, NORMAL,
        0x34ff1100L, 166, 72, 24, 20);
    init_object(&state->tree[D29_BC_5], G_BOXCHAR, SELECTABLE, NORMAL,
        0x35ff1100L, 194, 72, 24, 20);
    init_object(&state->tree[D29_BC_6], G_BOXCHAR, SELECTABLE, NORMAL,
        0x36ff1100L, 222, 72, 24, 20);
    init_object(&state->tree[D29_BC_MINUS], G_BOXCHAR, SELECTABLE, NORMAL,
        0x2dff1105L, 250, 72, 24, 20);
    init_object(&state->tree[D29_STATUS], G_STRING, NONE, NORMAL, 0L,
        14, 116, 280, 8);
    init_object(&state->tree[D29_WATCH], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Watchbox", 14, 142, 88, 22);
    init_object(&state->tree[D29_RUBBOX], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Rubbox", 112, 142, 72, 22);
    init_object(&state->tree[D29_DONE], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "Done", 266, 142, 58, 22);

    objc_add(state->tree, D29_ROOT, D29_HEAD);
    objc_add(state->tree, D29_ROOT, D29_PROMPT);
    objc_add(state->tree, D29_ROOT, D29_USERDEF);
    objc_add(state->tree, D29_ROOT, D29_BC_7);
    objc_add(state->tree, D29_ROOT, D29_BC_8);
    objc_add(state->tree, D29_ROOT, D29_BC_9);
    objc_add(state->tree, D29_ROOT, D29_BC_PLUS);
    objc_add(state->tree, D29_ROOT, D29_BC_4);
    objc_add(state->tree, D29_ROOT, D29_BC_5);
    objc_add(state->tree, D29_ROOT, D29_BC_6);
    objc_add(state->tree, D29_ROOT, D29_BC_MINUS);
    objc_add(state->tree, D29_ROOT, D29_STATUS);
    objc_add(state->tree, D29_ROOT, D29_WATCH);
    objc_add(state->tree, D29_ROOT, D29_RUBBOX);
    objc_add(state->tree, D29_ROOT, D29_DONE);

    update_status(state, "USERBLK level 5, waiting for probe click.");
}

static void sync_root_rect(demo29_state_t *state)
{
    GRECT work;

    wind_get(state->handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state->tree[D29_ROOT].ob_x = work.g_x;
    state->tree[D29_ROOT].ob_y = work.g_y;
    state->tree[D29_ROOT].ob_width = work.g_w;
    state->tree[D29_ROOT].ob_height = work.g_h;
}

static void draw_tree(demo29_state_t *state, const GRECT *dirty)
{
    GRECT box;

    sync_root_rect(state);
    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH,
        &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);

        if (dirty != NULL) {
            WORD dx1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD) (dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) x0 = dirty->g_x;
            if (y0 < dirty->g_y) y0 = dirty->g_y;
            if (x1 > dx1) x1 = dx1;
            if (y1 > dy1) y1 = dy1;
        }

        if (x0 <= x1 && y0 <= y1) {
            WORD fill[4] = { x0, y0, x1, y1 };

            (void) vswr_mode(vdi_handle, MD_REPLACE);
            vsf_color(vdi_handle, BLACK);
            v_bar(vdi_handle, fill);
            objc_draw(state->tree, ROOT, MAX_DEPTH,
                x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
        }
        wind_get(state->handle, WF_NEXTXYWH,
            &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }
    wind_update(END_UPDATE);
}

static void object_rect(demo29_state_t *state, WORD object, GRECT *rect)
{
    WORD x;
    WORD y;

    objc_offset(state->tree, object, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = state->tree[object].ob_width;
    rect->g_h = state->tree[object].ob_height;
}

static void probe_watchbox(demo29_state_t *state)
{
    GRECT rect;

    object_rect(state, D29_WATCH, &rect);
    (void) graf_watchbox(state->tree, D29_WATCH,
        (UWORD) (state->tree[D29_WATCH].ob_state | SELECTED),
        state->tree[D29_WATCH].ob_state);
    update_status(state, "graf_watchbox() called on Watchbox button.");
    objc_draw(state->tree, ROOT, MAX_DEPTH,
        rect.g_x, rect.g_y, rect.g_w, rect.g_h);
}

static void probe_rubbox(demo29_state_t *state)
{
    WORD out_w = 0;
    WORD out_h = 0;
    char text[96];

    (void) state;
    graf_rubbox(24, 24, 40, 30, &out_w, &out_h);
    sprintf(text, "graf_rubbox() -> %d x %d", out_w, out_h);
    update_status(g_demo29_state, text);
}

static void handle_boxchar(demo29_state_t *state, WORD object)
{
    char text[96];

    if (object == D29_BC_PLUS && state->level < 10) {
        ++state->level;
    } else if (object == D29_BC_MINUS && state->level > 0) {
        --state->level;
    }

    sprintf(text, "G_BOXCHAR object %d clicked, level now %d.",
        object, state->level);
    update_status(state, text);
}

int main(void)
{
    demo29_state_t state;
    GRECT work;
    WORD msg[8];
    WORD done = 0;

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);
    form_dial(FMD_START, 0, 0, 0, 0, 0, 0, 0, 0);

    init_tree(&state);
    g_demo29_state = &state;

    wind_get(0, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state.handle = wind_create(NAME | CLOSER | MOVER, work.g_x + 48,
        work.g_y + 32, 360, 232);
    if (state.handle <= 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(state.handle, WF_NAME, "Demo 29 Probes");
    wind_open(state.handle, work.g_x + 48, work.g_y + 32, 360, 232);
    draw_tree(&state, NULL);

    while (done == 0) {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        event = evnt_multi(MU_MESAG | MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);

        if ((event & MU_KEYBD) != 0 && (kr & 0xff) == 27) {
            done = 1;
        }
        if ((event & MU_MESAG) != 0) {
            switch (msg[0]) {
            case WM_REDRAW: {
                GRECT dirty;

                dirty.g_x = msg[4];
                dirty.g_y = msg[5];
                dirty.g_w = msg[6];
                dirty.g_h = msg[7];
                draw_tree(&state, &dirty);
                break;
            }
            case WM_TOPPED:
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                break;
            case WM_CLOSED:
                done = 1;
                break;
            case WM_MOVED:
            case WM_SIZED:
                wind_set(state.handle, WF_CURRXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
                draw_tree(&state, NULL);
                break;
            default:
                break;
            }
        }

        if ((event & MU_BUTTON) != 0) {
            WORD object = objc_find(state.tree, ROOT, MAX_DEPTH, mx, my);

            switch (object) {
            case D29_BC_7:
            case D29_BC_8:
            case D29_BC_9:
            case D29_BC_PLUS:
            case D29_BC_4:
            case D29_BC_5:
            case D29_BC_6:
            case D29_BC_MINUS:
                handle_boxchar(&state, object);
                draw_tree(&state, NULL);
                break;
            case D29_WATCH:
                probe_watchbox(&state);
                draw_tree(&state, NULL);
                break;
            case D29_RUBBOX:
                probe_rubbox(&state);
                draw_tree(&state, NULL);
                break;
            case D29_DONE:
                done = 1;
                break;
            default:
                break;
            }
        }
    }

    wind_close(state.handle);
    wind_delete(state.handle);
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

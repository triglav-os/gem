/*
 * Exercises static AES object classes that are still lightly covered
 * in the hosted samples: G_TEXT, G_BOXTEXT, G_IMAGE, and G_ICON.
 * The sample uses a classic window/object tree style so hosted AES can
 * be compared directly against Atari ST GEM behavior.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

enum {
    D28_ROOT = 0,
    D28_HEAD,
    D28_TEXT_LABEL,
    D28_TEXT_VALUE,
    D28_BOXTEXT_LABEL,
    D28_BOXTEXT_VALUE,
    D28_IMAGE_LABEL,
    D28_IMAGE,
    D28_ICON_LABEL,
    D28_ICON,
    D28_STATUS,
    D28_DONE,
    D28_COUNT
};

typedef struct demo28_state {
    WORD handle;
    OBJECT tree[D28_COUNT];
    TEDINFO ted[2];
    BITBLK bitblk;
    ICONBLK iconblk;
    char status[80];
} demo28_state_t;

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
static WORD work_out[57];

static UWORD demo28_image_data[16] = {
    0x07e0, 0x1ff8, 0x3c3c, 0x781e,
    0x700e, 0xe667, 0xe667, 0xe667,
    0xe007, 0xe667, 0xe667, 0x700e,
    0x781e, 0x3c3c, 0x1ff8, 0x07e0
};

static UWORD demo28_icon_mask[16] = {
    0x03c0, 0x0ff0, 0x1ff8, 0x3ffc,
    0x7ffe, 0x7ffe, 0xffff, 0xffff,
    0xffff, 0xffff, 0x7ffe, 0x7ffe,
    0x3ffc, 0x1ff8, 0x0ff0, 0x03c0
};

static UWORD demo28_icon_data[16] = {
    0x0000, 0x0660, 0x0ff0, 0x1ff8,
    0x3c3c, 0x781e, 0x700e, 0xe667,
    0xe667, 0x700e, 0x781e, 0x3c3c,
    0x1ff8, 0x0ff0, 0x0660, 0x0000
};

static char demo28_icon_text[] = "ICON";
static char demo28_plain_text[] = "Plain TEDINFO text";
static char demo28_box_text[] = "Boxed TEDINFO";
static char demo28_text_template[] = "____________________";
static char demo28_text_valid[] = "XXXXXXXXXXXXXXXXXXXX";

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

static void update_status(demo28_state_t *state, const char *text)
{
    strncpy(state->status, text, sizeof(state->status) - 1u);
    state->status[sizeof(state->status) - 1u] = '\0';
    state->tree[D28_STATUS].ob_spec = (LONG) (intptr_t) state->status;
}

static void init_tree(demo28_state_t *state)
{
    memset(state, 0, sizeof(*state));

    state->ted[0].te_ptext = (LONG) (intptr_t) demo28_plain_text;
    state->ted[0].te_ptmplt = (LONG) (intptr_t) demo28_text_template;
    state->ted[0].te_pvalid = (LONG) (intptr_t) demo28_text_valid;
    state->ted[0].te_font = 3;
    state->ted[0].te_junk1 = 0;
    state->ted[0].te_just = 0;
    state->ted[0].te_color = 0x1170;
    state->ted[0].te_junk2 = 0;
    state->ted[0].te_thickness = 1;
    state->ted[0].te_txtlen = 21;
    state->ted[0].te_tmplen = 21;

    state->ted[1] = state->ted[0];
    state->ted[1].te_ptext = (LONG) (intptr_t) demo28_box_text;
    state->ted[1].te_txtlen = 16;
    state->ted[1].te_tmplen = 16;

    state->bitblk.bi_pdata = (LONG) (intptr_t) demo28_image_data;
    state->bitblk.bi_wb = 2;
    state->bitblk.bi_hl = 16;
    state->bitblk.bi_x = 0;
    state->bitblk.bi_y = 0;
    state->bitblk.bi_color = 1;

    state->iconblk.ib_pmask = (LONG) (intptr_t) demo28_icon_mask;
    state->iconblk.ib_pdata = (LONG) (intptr_t) demo28_icon_data;
    state->iconblk.ib_ptext = (LONG) (intptr_t) demo28_icon_text;
    state->iconblk.ib_char = 0;
    state->iconblk.ib_xchar = 0;
    state->iconblk.ib_ychar = 0;
    state->iconblk.ib_xicon = 0;
    state->iconblk.ib_yicon = 0;
    state->iconblk.ib_wicon = 16;
    state->iconblk.ib_hicon = 16;
    state->iconblk.ib_xtext = 0;
    state->iconblk.ib_ytext = 18;
    state->iconblk.ib_wtext = 32;
    state->iconblk.ib_htext = 8;

    init_object(&state->tree[D28_ROOT], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, 340, 184);
    init_object(&state->tree[D28_HEAD], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 28 object gallery", 14, 14, 150, 8);
    init_object(&state->tree[D28_TEXT_LABEL], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "G_TEXT", 14, 42, 48, 8);
    init_object(&state->tree[D28_TEXT_VALUE], G_TEXT, NONE, NORMAL,
        (LONG) (intptr_t) &state->ted[0], 84, 38, 150, 16);
    init_object(&state->tree[D28_BOXTEXT_LABEL], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "G_BOXTEXT", 14, 74, 64, 8);
    init_object(&state->tree[D28_BOXTEXT_VALUE], G_BOXTEXT, NONE, NORMAL,
        (LONG) (intptr_t) &state->ted[1], 84, 70, 128, 18);
    init_object(&state->tree[D28_IMAGE_LABEL], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "G_IMAGE", 14, 108, 54, 8);
    init_object(&state->tree[D28_IMAGE], G_IMAGE, SELECTABLE, NORMAL,
        (LONG) (intptr_t) &state->bitblk, 84, 104, 16, 16);
    init_object(&state->tree[D28_ICON_LABEL], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "G_ICON", 150, 108, 48, 8);
    init_object(&state->tree[D28_ICON], G_ICON, SELECTABLE, NORMAL,
        (LONG) (intptr_t) &state->iconblk, 218, 102, 32, 28);
    init_object(&state->tree[D28_STATUS], G_STRING, NONE, NORMAL, 0L,
        14, 146, 250, 8);
    init_object(&state->tree[D28_DONE], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "Done", 258, 142, 58, 22);

    objc_add(state->tree, D28_ROOT, D28_HEAD);
    objc_add(state->tree, D28_ROOT, D28_TEXT_LABEL);
    objc_add(state->tree, D28_ROOT, D28_TEXT_VALUE);
    objc_add(state->tree, D28_ROOT, D28_BOXTEXT_LABEL);
    objc_add(state->tree, D28_ROOT, D28_BOXTEXT_VALUE);
    objc_add(state->tree, D28_ROOT, D28_IMAGE_LABEL);
    objc_add(state->tree, D28_ROOT, D28_IMAGE);
    objc_add(state->tree, D28_ROOT, D28_ICON_LABEL);
    objc_add(state->tree, D28_ROOT, D28_ICON);
    objc_add(state->tree, D28_ROOT, D28_STATUS);
    objc_add(state->tree, D28_ROOT, D28_DONE);

    update_status(state, "Click image or icon to test object hit/draw.");
}

static void sync_root_rect(demo28_state_t *state)
{
    GRECT work;

    wind_get(state->handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state->tree[D28_ROOT].ob_x = work.g_x;
    state->tree[D28_ROOT].ob_y = work.g_y;
    state->tree[D28_ROOT].ob_width = work.g_w;
    state->tree[D28_ROOT].ob_height = work.g_h;
}

static void draw_tree(demo28_state_t *state, const GRECT *dirty)
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

static void flash_object(demo28_state_t *state, WORD object)
{
    GRECT rect;
    WORD old_state;
    WORD x;
    WORD y;

    objc_offset(state->tree, object, &x, &y);
    rect.g_x = x;
    rect.g_y = y;
    rect.g_w = state->tree[object].ob_width;
    rect.g_h = state->tree[object].ob_height;
    old_state = state->tree[object].ob_state;
    objc_change(state->tree, object, 0,
        rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        (WORD) (old_state | SELECTED), 1);
    evnt_timer(100, 0);
    objc_change(state->tree, object, 0,
        rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        old_state, 1);
}

int main(void)
{
    demo28_state_t state;
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
    wind_get(0, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state.handle = wind_create(NAME | CLOSER | MOVER, work.g_x + 32,
        work.g_y + 24, 360, 212);
    if (state.handle <= 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(state.handle, WF_NAME, "Demo 28 Gallery");
    wind_open(state.handle, work.g_x + 32, work.g_y + 24, 360, 212);
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
            case D28_IMAGE:
                update_status(&state, "G_IMAGE clicked.");
                flash_object(&state, object);
                draw_tree(&state, NULL);
                break;
            case D28_ICON:
                update_status(&state, "G_ICON clicked.");
                flash_object(&state, object);
                draw_tree(&state, NULL);
                break;
            case D28_DONE:
                flash_object(&state, object);
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

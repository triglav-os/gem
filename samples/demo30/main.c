/*
 * Exercises form_do(), form_keybd(), form_button(), and a few menu
 * compatibility calls in one place. The sample intentionally exposes
 * the return values in status text so hosted AES gaps stay visible.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <stdio.h>
#include <string.h>

enum {
    D30_ROOT = 0,
    D30_PANEL,
    D30_TITLE,
    D30_LABEL_NAME,
    D30_NAME,
    D30_LABEL_CODE,
    D30_CODE,
    D30_STATUS,
    D30_RUN_FORM_DO,
    D30_RUN_KEYBD,
    D30_RUN_BUTTON,
    D30_DONE,
    D30_COUNT
};

enum {
    D30_MENU_ROOT = 0,
    D30_MENU_BAR_BOX,
    D30_MENU_TITLES,
    D30_TITLE_DESK,
    D30_TITLE_PROBE,
    D30_DESK_BOX,
    D30_DESK_ABOUT,
    D30_DESK_QUIT,
    D30_PROBE_BOX,
    D30_PROBE_CHECKED,
    D30_PROBE_DISABLED,
    D30_PROBE_RENAME,
    D30_MENU_COUNT
};

typedef struct demo30_state {
    WORD handle;
    OBJECT tree[D30_COUNT];
    TEDINFO ted[2];
    char name[24];
    char code[16];
    char status[96];
    WORD name_index;
    WORD code_index;
    WORD menu_checked;
} demo30_state_t;

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
static WORD work_out[57];

static OBJECT menu_tree[D30_MENU_COUNT] = {
    {-1, 1, 8, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {0, 2, 2, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1},
    {1, 3, 4, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Desk ", 0, 0, 6, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Probe ", 6, 0, 7, 1},
    {0, 6, 7, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 18, 3},
    {5, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  About Demo 30 ", 0, 0, 18, 1},
    {5, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  Quit ", 0, 1, 18, 1},
    {0, 9, 11, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 28, 4},
    {8, -1, -1, G_STRING, CHECKED, NORMAL, (LONG) "  Checked item ", 0, 0, 28, 1},
    {8, -1, -1, G_STRING, NONE, DISABLED, (LONG) "  Disabled item ", 0, 1, 28, 1},
    {8, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  Rename checked text ", 0, 2, 28, 1}
};

static char menu_checked_text[] = "  Checked item ";
static char menu_renamed_text[] = "  Renamed item ";
static char demo30_name_template[] = "______________________";
static char demo30_name_valid[] = "XXXXXXXXXXXXXXXXXXXXXX";
static char demo30_code_template[] = "______________";
static char demo30_code_valid[] = "XXXXXXXXXXXXXX";

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

static void update_status(demo30_state_t *state, const char *text)
{
    strncpy(state->status, text, sizeof(state->status) - 1u);
    state->status[sizeof(state->status) - 1u] = '\0';
    state->tree[D30_STATUS].ob_spec = (LONG) (intptr_t) state->status;
}

static void init_tree(demo30_state_t *state)
{
    memset(state, 0, sizeof(*state));
    strcpy(state->name, "Atari");
    strcpy(state->code, "ST");
    state->menu_checked = 1;

    state->ted[0].te_ptext = (LONG) (intptr_t) state->name;
    state->ted[0].te_ptmplt = (LONG) (intptr_t) demo30_name_template;
    state->ted[0].te_pvalid = (LONG) (intptr_t) demo30_name_valid;
    state->ted[0].te_font = 3;
    state->ted[0].te_junk1 = 0;
    state->ted[0].te_just = 0;
    state->ted[0].te_color = 0x1170;
    state->ted[0].te_junk2 = 0;
    state->ted[0].te_thickness = 1;
    state->ted[0].te_txtlen = sizeof(state->name);
    state->ted[0].te_tmplen = 23;

    state->ted[1] = state->ted[0];
    state->ted[1].te_ptext = (LONG) (intptr_t) state->code;
    state->ted[1].te_ptmplt = (LONG) (intptr_t) demo30_code_template;
    state->ted[1].te_pvalid = (LONG) (intptr_t) demo30_code_valid;
    state->ted[1].te_txtlen = sizeof(state->code);
    state->ted[1].te_tmplen = 15;

    init_object(&state->tree[D30_ROOT], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, 320, 188);
    init_object(&state->tree[D30_TITLE], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 30 form and menu probe", 16, 14, 180, 8);
    init_object(&state->tree[D30_LABEL_NAME], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Name", 16, 42, 32, 8);
    init_object(&state->tree[D30_NAME], G_FTEXT, SELECTABLE, NORMAL,
        (LONG) (intptr_t) &state->ted[0], 74, 40, 150, 16);
    init_object(&state->tree[D30_LABEL_CODE], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Code", 16, 70, 32, 8);
    init_object(&state->tree[D30_CODE], G_BOXTEXT, SELECTABLE, NORMAL,
        (LONG) (intptr_t) &state->ted[1], 74, 66, 100, 18);
    init_object(&state->tree[D30_STATUS], G_STRING, NONE, NORMAL, 0L,
        16, 102, 280, 8);
    init_object(&state->tree[D30_RUN_FORM_DO], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "form_do", 16, 126, 74, 22);
    init_object(&state->tree[D30_RUN_KEYBD], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "form_keybd", 100, 126, 94, 22);
    init_object(&state->tree[D30_RUN_BUTTON], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "form_button", 204, 126, 94, 22);
    init_object(&state->tree[D30_DONE], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "Done", 240, 154, 58, 22);

    objc_add(state->tree, D30_ROOT, D30_TITLE);
    objc_add(state->tree, D30_ROOT, D30_LABEL_NAME);
    objc_add(state->tree, D30_ROOT, D30_NAME);
    objc_add(state->tree, D30_ROOT, D30_LABEL_CODE);
    objc_add(state->tree, D30_ROOT, D30_CODE);
    objc_add(state->tree, D30_ROOT, D30_STATUS);
    objc_add(state->tree, D30_ROOT, D30_RUN_FORM_DO);
    objc_add(state->tree, D30_ROOT, D30_RUN_KEYBD);
    objc_add(state->tree, D30_ROOT, D30_RUN_BUTTON);
    objc_add(state->tree, D30_ROOT, D30_DONE);

    state->name_index = (WORD) strlen(state->name);
    state->code_index = (WORD) strlen(state->code);
    update_status(state, "Use menu and probe buttons to inspect AES form APIs.");
}

static void sync_root_rect(demo30_state_t *state)
{
    GRECT work;

    wind_get(state->handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state->tree[D30_ROOT].ob_x = work.g_x;
    state->tree[D30_ROOT].ob_y = work.g_y;
    state->tree[D30_ROOT].ob_width = work.g_w;
    state->tree[D30_ROOT].ob_height = work.g_h;
}

static void draw_tree(demo30_state_t *state, const GRECT *dirty)
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

static void run_form_do_probe(demo30_state_t *state)
{
    WORD result;
    char text[96];

    result = form_do(state->tree, D30_NAME);
    sprintf(text, "form_do(tree, D30_NAME) returned %d.", result);
    update_status(state, text);
}

static void run_form_keybd_probe(demo30_state_t *state)
{
    WORD newobj = 0;
    WORD newchar = 0;
    WORD result;
    char text[96];

    result = form_keybd(state->tree, D30_NAME, D30_CODE, 'A',
        &newobj, &newchar);
    sprintf(text, "form_keybd -> obj=%d newobj=%d char=%d.",
        result, newobj, newchar);
    update_status(state, text);
}

static void run_form_button_probe(demo30_state_t *state)
{
    WORD newobj = 0;
    WORD result;
    char text[96];

    result = form_button(state->tree, D30_DONE, 1, &newobj);
    sprintf(text, "form_button -> ret=%d newobj=%d state=0x%04x.",
        result, newobj, state->tree[D30_DONE].ob_state);
    update_status(state, text);
}

int main(void)
{
    demo30_state_t state;
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
    menu_click(1, 1);
    menu_bar(menu_tree, 1);
    menu_ienable(menu_tree, D30_PROBE_DISABLED, 0);
    menu_icheck(menu_tree, D30_PROBE_CHECKED, 1);

    wind_get(0, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state.handle = wind_create(NAME | CLOSER | MOVER, work.g_x + 52,
        work.g_y + 44, 344, 220);
    if (state.handle <= 0) {
        menu_bar(menu_tree, 0);
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(state.handle, WF_NAME, "Demo 30 Form Probe");
    wind_open(state.handle, work.g_x + 52, work.g_y + 44, 344, 220);
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
            case MN_SELECTED:
                switch (msg[4]) {
                case D30_DESK_ABOUT:
                    form_alert(1, "[1][ Demo 30 menu probe ][ OK ]");
                    break;
                case D30_DESK_QUIT:
                    done = 1;
                    break;
                case D30_PROBE_CHECKED:
                    state.menu_checked = !state.menu_checked;
                    menu_icheck(menu_tree, D30_PROBE_CHECKED,
                        state.menu_checked);
                    update_status(&state, state.menu_checked != 0 ?
                        "menu_icheck() set CHECKED." :
                        "menu_icheck() cleared CHECKED.");
                    draw_tree(&state, NULL);
                    break;
                case D30_PROBE_RENAME:
                    if (menu_tree[D30_PROBE_CHECKED].ob_spec ==
                        (LONG) (intptr_t) menu_checked_text) {
                        menu_text(menu_tree, D30_PROBE_CHECKED,
                            menu_renamed_text);
                        update_status(&state,
                            "menu_text() renamed checked item.");
                    } else {
                        menu_text(menu_tree, D30_PROBE_CHECKED,
                            menu_checked_text);
                        update_status(&state,
                            "menu_text() restored checked item text.");
                    }
                    break;
                default:
                    break;
                }
                menu_tnormal(menu_tree, msg[3], 1);
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
            case D30_RUN_FORM_DO:
                run_form_do_probe(&state);
                draw_tree(&state, NULL);
                break;
            case D30_RUN_KEYBD:
                run_form_keybd_probe(&state);
                draw_tree(&state, NULL);
                break;
            case D30_RUN_BUTTON:
                run_form_button_probe(&state);
                draw_tree(&state, NULL);
                break;
            case D30_DONE:
                done = 1;
                break;
            default:
                break;
            }
        }
    }

    wind_close(state.handle);
    wind_delete(state.handle);
    menu_bar(menu_tree, 0);
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

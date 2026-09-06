/*
 * Demonstrates a classic GEM window hosting a small form-like editor
 * with TEDINFO fields. It exercises redraw clipping, object hit
 * testing, and incremental keyboard updates using `objc_edit()`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

enum {
    D25_ROOT = 0,
    D25_HEAD,
    D25_LABEL_NAME,
    D25_FIELD_NAME,
    D25_LABEL_CITY,
    D25_FIELD_CITY,
    D25_STATUS,
    D25_APPLY,
    D25_CLEAR,
    D25_DONE,
    D25_COUNT
};

typedef struct demo25_state {
    WORD handle;
    GRECT normal_rect;
    OBJECT tree[D25_COUNT];
    TEDINFO ted[2];
    char name[24];
    char city[24];
    char status[96];
    WORD active_field;
    WORD name_index;
    WORD city_index;
} demo25_state_t;

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];

WORD sample_string_width(const char *text)
{
    WORD extent[8];

    if (text == NULL || *text == '\0') {
        return 0;
    }

    vqt_extent(vdi_handle, (char *) text, extent);
    return (WORD) (extent[2] - extent[0]);
}

void init_object(OBJECT *object,
    UWORD type, UWORD flags, UWORD state, LONG spec,
    WORD x, WORD y, WORD w, WORD h)
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

void update_status(demo25_state_t *state, const char *prefix)
{
    strcpy(state->status, prefix);
    strcat(state->status, " ");
    strcat(state->status, state->name);
    strcat(state->status, " / ");
    strcat(state->status, state->city);
    state->tree[D25_STATUS].ob_spec = (LONG) (intptr_t) state->status;
}

void set_active_field(demo25_state_t *state, WORD object)
{
    WORD *index_ptr;

    state->active_field = object;

    state->tree[D25_FIELD_NAME].ob_state &= (UWORD) ~SELECTED;
    state->tree[D25_FIELD_CITY].ob_state &= (UWORD) ~SELECTED;
    if (object == D25_FIELD_NAME || object == D25_FIELD_CITY) {
        state->tree[object].ob_state |= SELECTED;
        index_ptr = (object == D25_FIELD_NAME) ? &state->name_index :
            &state->city_index;
        objc_edit(state->tree, object, 0, index_ptr, EDINIT);
    }
}

void init_tree(demo25_state_t *state)
{
    strcpy(state->name, "Atari");
    strcpy(state->city, "Ljubljana");

    state->ted[0].te_ptext = (LONG) (intptr_t) state->name;
    state->ted[0].te_ptmplt = (LONG) (intptr_t) "______________________";
    state->ted[0].te_pvalid = (LONG) (intptr_t) "XXXXXXXXXXXXXXXXXXXXXX";
    state->ted[0].te_font = 3;
    state->ted[0].te_junk1 = 0;
    state->ted[0].te_just = 0;
    state->ted[0].te_color = 0x1170;
    state->ted[0].te_junk2 = 0;
    state->ted[0].te_thickness = 1;
    state->ted[0].te_txtlen = sizeof(state->name);
    state->ted[0].te_tmplen = 23;

    state->ted[1] = state->ted[0];
    state->ted[1].te_ptext = (LONG) (intptr_t) state->city;

    init_object(&state->tree[D25_ROOT], G_BOX, LASTOB, NORMAL, 0L,
        0, 0, 430, 190);
    init_object(&state->tree[D25_HEAD], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 25 editor", 14, 14, 110, 8);
    init_object(&state->tree[D25_LABEL_NAME], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Name", 14, 44, 32, 8);
    init_object(&state->tree[D25_FIELD_NAME], G_FBOXTEXT,
        SELECTABLE, NORMAL, (LONG) (intptr_t) &state->ted[0],
        70, 44, 170, 16);
    init_object(&state->tree[D25_LABEL_CITY], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "City", 14, 76, 32, 8);
    init_object(&state->tree[D25_FIELD_CITY], G_FBOXTEXT,
        SELECTABLE, NORMAL, (LONG) (intptr_t) &state->ted[1],
        70, 76, 170, 16);
    init_object(&state->tree[D25_STATUS], G_STRING, NONE, NORMAL, 0L,
        14, 108, 320, 8);
    init_object(&state->tree[D25_APPLY], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "Apply", 188, 140, 64, 22);
    init_object(&state->tree[D25_CLEAR], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Clear", 262, 140, 64, 22);
    init_object(&state->tree[D25_DONE], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Done", 336, 140, 64, 22);

    objc_add(state->tree, D25_ROOT, D25_HEAD);
    objc_add(state->tree, D25_ROOT, D25_LABEL_NAME);
    objc_add(state->tree, D25_ROOT, D25_FIELD_NAME);
    objc_add(state->tree, D25_ROOT, D25_LABEL_CITY);
    objc_add(state->tree, D25_ROOT, D25_FIELD_CITY);
    objc_add(state->tree, D25_ROOT, D25_STATUS);
    objc_add(state->tree, D25_ROOT, D25_APPLY);
    objc_add(state->tree, D25_ROOT, D25_CLEAR);
    objc_add(state->tree, D25_ROOT, D25_DONE);

    state->name_index = (WORD) strlen(state->name);
    state->city_index = (WORD) strlen(state->city);
    set_active_field(state, D25_FIELD_NAME);
    update_status(state, "Editing");
}

void sync_root_rect(demo25_state_t *state)
{
    GRECT work;

    wind_get(state->handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    state->tree[D25_ROOT].ob_x = work.g_x;
    state->tree[D25_ROOT].ob_y = work.g_y;
    state->tree[D25_ROOT].ob_width = work.g_w;
    state->tree[D25_ROOT].ob_height = work.g_h;
}

void draw_tree(demo25_state_t *state, const GRECT *dirty)
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
            objc_draw(state->tree, ROOT, MAX_DEPTH,
                x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH,
            &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }

    wind_update(END_UPDATE);
}

void field_rect(demo25_state_t *state, WORD object, GRECT *rect)
{
    WORD x;
    WORD y;

    sync_root_rect(state);
    objc_offset(state->tree, object, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = state->tree[object].ob_width;
    rect->g_h = state->tree[object].ob_height;
}

void set_field_caret_from_click(demo25_state_t *state, WORD object, WORD mouse_x)
{
    GRECT rect;
    char *buffer;
    WORD *index_ptr;
    WORD length;
    WORD rel_x;
    WORD index;
    char prefix[24];

    if (object != D25_FIELD_NAME && object != D25_FIELD_CITY) {
        return;
    }

    field_rect(state, object, &rect);
    buffer = (object == D25_FIELD_NAME) ? state->name : state->city;
    index_ptr = (object == D25_FIELD_NAME) ? &state->name_index :
        &state->city_index;
    length = (WORD) strlen(buffer);
    rel_x = (WORD) (mouse_x - rect.g_x - 2);

    if (rel_x <= 0) {
        *index_ptr = 0;
        objc_edit(state->tree, object, 0, index_ptr, EDINIT);
        return;
    }

    for (index = 0; index < length; ++index) {
        WORD left_width;
        WORD right_width;
        WORD midpoint;

        memcpy(prefix, buffer, (size_t) index);
        prefix[index] = '\0';
        left_width = sample_string_width(prefix);

        memcpy(prefix, buffer, (size_t) (index + 1));
        prefix[index + 1] = '\0';
        right_width = sample_string_width(prefix);

        midpoint = (WORD) (left_width + (right_width - left_width) / 2);
        if (rel_x <= midpoint) {
            *index_ptr = index;
            objc_edit(state->tree, object, 0, index_ptr, EDINIT);
            return;
        }
    }

    *index_ptr = length;
    objc_edit(state->tree, object, 0, index_ptr, EDINIT);
}

int handle_button(demo25_state_t *state, WORD object, WORD mouse_x)
{
    GRECT dirty;
    UWORD old_state;

    if (object != D25_FIELD_NAME && object != D25_FIELD_CITY &&
        object != D25_APPLY && object != D25_CLEAR &&
        object != D25_DONE) {
        return 0;
    }

    field_rect(state, object, &dirty);
    old_state = state->tree[object].ob_state;
    objc_change(state->tree, object, 0,
        dirty.g_x, dirty.g_y, dirty.g_w, dirty.g_h,
        (WORD) (old_state | SELECTED), 1);
    evnt_timer(100, 0);
    objc_change(state->tree, object, 0,
        dirty.g_x, dirty.g_y, dirty.g_w, dirty.g_h,
        old_state, 1);

    if (object == D25_FIELD_NAME || object == D25_FIELD_CITY) {
        set_active_field(state, object);
        set_field_caret_from_click(state, object, mouse_x);
        draw_tree(state, NULL);
        return 0;
    }

    if (object == D25_APPLY) {
        update_status(state, "Applied");
        draw_tree(state, NULL);
        return 0;
    }

    if (object == D25_CLEAR) {
        state->name[0] = '\0';
        state->city[0] = '\0';
        state->name_index = 0;
        state->city_index = 0;
        update_status(state, "Cleared");
        draw_tree(state, NULL);
        return 0;
    }

    return (object == D25_DONE);
}

int main(void)
{
    demo25_state_t state;
    WORD msg[8];
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_return = 0;
    WORD dummy = 0;
    WORD events;
    GRECT full;

    memset(&state, 0, sizeof(state));

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);

    init_tree(&state);
    wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
    state.handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full.g_x, full.g_y, full.g_w, full.g_h);
    if (state.handle < 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set(state.handle, WF_NAME, "Demo 25 Editor", 0, 0);
    wind_open(state.handle, full.g_x + 40, full.g_y + 50, 440, 220);
    wind_get(state.handle, WF_CURRXYWH,
        &state.normal_rect.g_x, &state.normal_rect.g_y,
        &state.normal_rect.g_w, &state.normal_rect.g_h);

    draw_tree(&state, NULL);

    while (1) {
        events = evnt_multi(MU_MESAG | MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, 0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_return, &dummy);

        if ((events & MU_MESAG) != 0) {
            if (msg[3] != state.handle) {
                continue;
            }

            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT dirty;

                dirty.g_x = msg[4];
                dirty.g_y = msg[5];
                dirty.g_w = msg[6];
                dirty.g_h = msg[7];
                draw_tree(&state, &dirty);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                wind_set(state.handle, WF_CURRXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
            } else if (msg[0] == WM_TOPPED) {
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            } else if (msg[0] == WM_FULLED) {
                GRECT current;

                wind_get(state.handle, WF_CURRXYWH,
                    &current.g_x, &current.g_y,
                    &current.g_w, &current.g_h);
                if (current.g_x == full.g_x && current.g_y == full.g_y &&
                    current.g_w == full.g_w && current.g_h == full.g_h) {
                    wind_set(state.handle, WF_CURRXYWH,
                        state.normal_rect.g_x, state.normal_rect.g_y,
                        state.normal_rect.g_w, state.normal_rect.g_h);
                } else {
                    state.normal_rect = current;
                    wind_set(state.handle, WF_CURRXYWH,
                        full.g_x, full.g_y, full.g_w, full.g_h);
                }
            }
        }

        if ((events & MU_BUTTON) != 0) {
            WORD object = objc_find(state.tree, ROOT, MAX_DEPTH,
                mouse_x, mouse_y);

            if (object >= 0 &&
                handle_button(&state, object, mouse_x) != 0) {
                break;
            }
        }

        if ((events & MU_KEYBD) != 0) {
            WORD *index_ptr;
            GRECT dirty;

            if ((key_return & 0xff) == 27) {
                break;
            }
            if (state.active_field != D25_FIELD_NAME &&
                state.active_field != D25_FIELD_CITY) {
                continue;
            }

            index_ptr = (state.active_field == D25_FIELD_NAME) ?
                &state.name_index : &state.city_index;
            if ((key_return & 0xff) == 8) {
                char *buffer = (state.active_field == D25_FIELD_NAME) ?
                    state.name : state.city;
                size_t length = strlen(buffer);

                if (*index_ptr > 0) {
                    --(*index_ptr);
                    memmove(&buffer[*index_ptr], &buffer[*index_ptr + 1],
                        length - (size_t) *index_ptr);
                }
            } else {
                objc_edit(state.tree, state.active_field, (WORD) (key_return & 0xff),
                    index_ptr, EDCHAR);
            }

            update_status(&state, "Editing");
            field_rect(&state, state.active_field, &dirty);
            draw_tree(&state, &dirty);
        }
    }

    wind_close(state.handle);
    wind_delete(state.handle);
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

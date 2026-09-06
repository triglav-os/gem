/*
 * Demonstrates a classic Atari ST style AES form with many editable
 * fields. The sample is meant to stress shared form_do() behavior for
 * caret placement, Tab navigation, in-field editing, and the visual
 * difference between boxed and unboxed text fields.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <stdio.h>
#include <string.h>

enum {
    D33_ROOT = 0,
    D33_PANEL,
    D33_TITLE,
    D33_HINT,
    D33_LABEL_BASE = 4,
    D33_FIELD_BASE = 14,
    D33_OK = 24,
    D33_CANCEL = 25,
    D33_COUNT = 26
};

enum {
    D33_FIELD_COUNT = 10
};

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
static WORD work_out[57];

static OBJECT dialog_tree[D33_COUNT];
static TEDINFO ted[D33_FIELD_COUNT];
static char buffers[D33_FIELD_COUNT][20];

static const char *field_labels[D33_FIELD_COUNT] = {
    "Name", "Street", "City", "State", "Zip",
    "Country", "Phone", "Email", "Role", "Notes"
};

static const char *field_defaults[D33_FIELD_COUNT] = {
    "Atari", "GEM Way", "Ljubljana", "SI", "1000",
    "Slovenia", "555-123", "st@gem.dev", "Tester", "AES"
};

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

static WORD centered_text_baseline(WORD top, WORD height,
                                   WORD text_height, WORD text_ascent)
{
    WORD pad = 0;

    if (height > text_height) {
        pad = (WORD) ((height - text_height) / 2);
    }
    return (WORD) (top + pad + text_ascent);
}

static WORD object_top_from_baseline(WORD baseline, WORD height,
                                     WORD text_height, WORD text_ascent)
{
    WORD pad = 0;

    if (height > text_height) {
        pad = (WORD) ((height - text_height) / 2);
    }
    return (WORD) (baseline - pad - text_ascent);
}

static void init_dialog_tree(WORD text_height, WORD text_ascent)
{
    WORD i;
    const WORD label_height = 8;
    const WORD field_height = 16;

    init_object(&dialog_tree[D33_ROOT], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, 420, 220);
    init_object(&dialog_tree[D33_PANEL], G_BOX, NONE, NORMAL, 0L,
        0, 0, 420, 220);
    init_object(&dialog_tree[D33_TITLE], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 33 Real AES Form", 18, 14, 180, 8);
    init_object(&dialog_tree[D33_HINT], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Tab moves forward. Click places caret. Enter accepts.",
        18, 30, 320, 8);

    objc_add(dialog_tree, D33_ROOT, D33_PANEL);
    objc_add(dialog_tree, D33_ROOT, D33_TITLE);
    objc_add(dialog_tree, D33_ROOT, D33_HINT);

    for (i = 0; i < D33_FIELD_COUNT; ++i) {
        WORD column = (WORD) (i / 5);
        WORD row = (WORD) (i % 5);
        WORD label_x = (column == 0) ? 18 : 220;
        WORD field_x = (column == 0) ? 74 : 282;
        WORD y = (WORD) (56 + row * 26);
        WORD baseline = centered_text_baseline(y, field_height,
            text_height, text_ascent);
        WORD label_id = (WORD) (D33_LABEL_BASE + i);
        WORD field_id = (WORD) (D33_FIELD_BASE + i);
        UWORD field_type = (i & 1) == 0 ? G_FBOXTEXT : G_FTEXT;
        WORD field_y = object_top_from_baseline(baseline, field_height,
            text_height, text_ascent);
        WORD label_y = object_top_from_baseline(baseline, label_height,
            text_height, text_ascent);

        strncpy(buffers[i], field_defaults[i], sizeof(buffers[i]) - 1u);
        buffers[i][sizeof(buffers[i]) - 1u] = '\0';

        ted[i].te_ptext = (LONG) (intptr_t) buffers[i];
        ted[i].te_ptmplt = (LONG) (intptr_t) "___________________";
        ted[i].te_pvalid = (LONG) (intptr_t) "XXXXXXXXXXXXXXXXXXX";
        ted[i].te_font = IBM;
        ted[i].te_junk1 = 0;
        ted[i].te_just = TE_LEFT;
        ted[i].te_color = 0x1170;
        ted[i].te_junk2 = 0;
        ted[i].te_thickness = 1;
        ted[i].te_txtlen = (WORD) sizeof(buffers[i]);
        ted[i].te_tmplen = 20;

        init_object(&dialog_tree[label_id], G_STRING, NONE, NORMAL,
            (LONG) (intptr_t) field_labels[i], label_x, label_y, 48,
            label_height);
        init_object(&dialog_tree[field_id], field_type,
            SELECTABLE | EDITABLE, NORMAL, (LONG) (intptr_t) &ted[i],
            field_x, field_y, 118, field_height);
        objc_add(dialog_tree, D33_ROOT, label_id);
        objc_add(dialog_tree, D33_ROOT, field_id);
    }

    init_object(&dialog_tree[D33_OK], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "OK", 282, 184, 56, 22);
    init_object(&dialog_tree[D33_CANCEL], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Cancel", 346, 184, 64, 22);
    objc_add(dialog_tree, D33_ROOT, D33_OK);
    objc_add(dialog_tree, D33_ROOT, D33_CANCEL);
}

int main(void)
{
    WORD cx;
    WORD cy;
    WORD cw;
    WORD ch;
    WORD result;
    char alert[128];
    WORD min_ade;
    WORD max_ade;
    WORD distances[5];
    WORD max_width;
    WORD effects[3];
    WORD text_height;
    WORD text_ascent;

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);
    vqt_fontinfo(vdi_handle, &min_ade, &max_ade, distances, &max_width,
        effects);
    text_height = distances[3];
    text_ascent = (WORD) (distances[1] + distances[2]);
    init_dialog_tree(text_height, text_ascent);

    form_center(dialog_tree, &cx, &cy, &cw, &ch);
    dialog_tree[D33_ROOT].ob_x = cx;
    dialog_tree[D33_ROOT].ob_y = cy;

    form_dial(FMD_START, 0, 0, 0, 0, cx, cy, cw, ch);
    objc_draw(dialog_tree, ROOT, MAX_DEPTH, cx, cy, cw, ch);
    result = form_do(dialog_tree, D33_FIELD_BASE);
    form_dial(FMD_FINISH, 0, 0, 0, 0, cx, cy, cw, ch);

    if (result == D33_OK) {
        snprintf(alert, sizeof(alert),
            "[1][ Saved %s / %s ][ OK ]", buffers[0], buffers[2]);
        form_alert(1, alert);
    } else if (result == D33_CANCEL) {
        form_alert(1, "[1][ Demo 33 cancelled ][ OK ]");
    }

    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

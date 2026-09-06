/*
 * Exercises hosted AES message-box APIs through a simple window that
 * launches a range of form_alert() and form_error() probes from the
 * keyboard. This sample is meant to be a stable harness while alert
 * rendering and return-value behavior are implemented and refined.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <stdio.h>
#include <string.h>

enum {
    demo32_line_count = 11,
    demo32_line_gap = 6
};

typedef struct demo32_state {
    WORD handle;
    WORD char_w;
    WORD char_h;
    WORD ascent;
    char status[96];
} demo32_state_t;

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
static WORD work_out[57];

static const char *demo32_lines[demo32_line_count] = {
    "Message-box probes:",
    "1  Note icon, one button",
    "2  Question icon, two buttons",
    "3  Stop icon, three buttons",
    "4  Multi-line note",
    "5  Default button 2",
    "6  Default button 3",
    "7  Long button labels",
    "8  form_error(8)",
    "9  form_error(2)",
    "Esc quits"
};

static void demo32_update_status(demo32_state_t *state, const char *text)
{
    if (state == NULL || text == NULL) {
        return;
    }

    strncpy(state->status, text, sizeof(state->status) - 1u);
    state->status[sizeof(state->status) - 1u] = '\0';
}

static void demo32_draw(demo32_state_t *state, const GRECT *dirty)
{
    GRECT work;
    GRECT box;

    if (state == NULL) {
        return;
    }

    wind_get(state->handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH,
        &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);
        WORD clip[4];
        WORD fill[4];
        WORD line;

        if (dirty != NULL) {
            WORD dx1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD) (dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dx1) {
                x1 = dx1;
            }
            if (y1 > dy1) {
                y1 = dy1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip[0] = x0;
            clip[1] = y0;
            clip[2] = x1;
            clip[3] = y1;
            fill[0] = work.g_x;
            fill[1] = work.g_y;
            fill[2] = (WORD) (work.g_x + work.g_w - 1);
            fill[3] = (WORD) (work.g_y + work.g_h - 1);

            vs_clip(vdi_handle, 1, clip);
            (void) vswr_mode(vdi_handle, MD_REPLACE);
            vsf_color(vdi_handle, BLACK);
            vr_recfl(vdi_handle, fill);
            vst_color(vdi_handle, WHITE);

            for (line = 0; line < demo32_line_count; ++line) {
                WORD text_y = (WORD) (work.g_y + 6 +
                    line * (state->char_h + demo32_line_gap) +
                    state->ascent);

                v_gtext(vdi_handle, (WORD) (work.g_x + 8), text_y,
                    (CONST BYTE *) demo32_lines[line]);
            }

            v_gtext(vdi_handle, (WORD) (work.g_x + 8),
                (WORD) (work.g_y + work.g_h - 8), (CONST BYTE *) state->status);
            vs_clip(vdi_handle, 0, clip);
        }

        wind_get(state->handle, WF_NEXTXYWH,
            &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }
    wind_update(END_UPDATE);
}

static void demo32_run_alert(demo32_state_t *state, WORD key)
{
    const char *alert = NULL;
    WORD result = 0;
    char text[96];

    if (state == NULL) {
        return;
    }

    switch (key) {
    case '1':
        alert = "[1][ Demo 32 note ][ OK ]";
        break;
    case '2':
        alert = "[2][ Save changes? ][ Yes | No ]";
        break;
    case '3':
        alert = "[3][ Stop current probe? ][ Cancel | Retry | Ignore ]";
        break;
    case '4':
        alert = "[1][ First line | Second line | Third line ][ Continue ]";
        break;
    case '5':
        alert = "[2][ Button 2 should be default ][ One | Two ]";
        result = form_alert(2, (char *) alert);
        snprintf(text, sizeof(text), "Alert 5 returned button %d.", result);
        demo32_update_status(state, text);
        return;
    case '6':
        alert = "[3][ Button 3 should be default ][ One | Two | Three ]";
        result = form_alert(3, (char *) alert);
        snprintf(text, sizeof(text), "Alert 6 returned button %d.", result);
        demo32_update_status(state, text);
        return;
    case '7':
        alert = "[1][ Long labels probe ][ First choice | Second choice ]";
        break;
    case '8':
        result = form_error(8);
        snprintf(text, sizeof(text), "form_error(8) returned %d.", result);
        demo32_update_status(state, text);
        return;
    case '9':
        result = form_error(2);
        snprintf(text, sizeof(text), "form_error(2) returned %d.", result);
        demo32_update_status(state, text);
        return;
    default:
        return;
    }

    result = form_alert(1, (char *) alert);
    snprintf(text, sizeof(text), "Alert %c returned button %d.", key, result);
    demo32_update_status(state, text);
}

int main(void)
{
    demo32_state_t state;
    GRECT desk;
    WORD msg[8];
    WORD done = 0;
    WORD distances[5] = {0};

    memset(&state, 0, sizeof(state));

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle(&state.char_w, &state.char_h, NULL, NULL);
    v_opnvwk(work_in, &vdi_handle, work_out);
    (void) vqt_fontinfo(vdi_handle, NULL, NULL, distances, NULL, NULL);
    state.ascent = (WORD) (distances[1] + 1);
    if (state.ascent <= 0) {
        state.ascent = state.char_h;
    }

    demo32_update_status(&state, "Press 1-9 to launch a message box.");

    wind_get(0, WF_WORKXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    state.handle = wind_create(NAME | CLOSER | MOVER,
        (WORD) (desk.g_x + 40), (WORD) (desk.g_y + 32), 420, 240);
    if (state.handle <= 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(state.handle, WF_NAME, "Demo 32 Message Boxes");
    wind_open(state.handle, (WORD) (desk.g_x + 40), (WORD) (desk.g_y + 32),
        420, 240);
    demo32_draw(&state, NULL);

    while (done == 0) {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        event = evnt_multi(MU_MESAG | MU_KEYBD,
            0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);

        (void) mx;
        (void) my;
        (void) mb;
        (void) ks;
        (void) br;

        if ((event & MU_KEYBD) != 0) {
            if ((kr & 0xff) == 27) {
                done = 1;
            } else {
                demo32_run_alert(&state, (WORD) (kr & 0xff));
                demo32_draw(&state, NULL);
            }
        }

        if ((event & MU_MESAG) != 0) {
            switch (msg[0]) {
            case WM_REDRAW:
            {
                GRECT dirty;

                dirty.g_x = msg[4];
                dirty.g_y = msg[5];
                dirty.g_w = msg[6];
                dirty.g_h = msg[7];
                demo32_draw(&state, &dirty);
                break;
            }
            case WM_TOPPED:
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                break;
            case WM_MOVED:
                wind_set(state.handle, WF_CURRXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
                break;
            case WM_CLOSED:
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

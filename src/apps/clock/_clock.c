/*
 * Implements a hosted GEM analog clock with resource-loaded numeral
 * bitmaps and stylized Iskra-inspired hands. The application is
 * intentionally self-contained so the legacy alarm/editor logic is no
 * longer required.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/portab.h>
#include <gem/vdi.h>

#include "clock.h"
#include "platform/os.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    clock_work_width = 260,
    clock_work_height = 240,
    clock_number_count = 12,
    clock_face_margin = 16
};

static const double clock_pi = 3.14159265358979323846;

typedef struct clock_state {
    WORD handle;
    BITBLK numbers[clock_number_count];
    int numbers_loaded;
    int last_second;
} clock_state_t;

/*
 * Releases any app-owned bitmap plane referenced by one bit block.
 */
static void clock_free_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL) {
        return;
    }

    free((void *) (intptr_t) bitblk->bi_pdata);
    bitblk->bi_pdata = 0;
}

/*
 * Releases any app-owned bitmap planes referenced by one icon block.
 */
static void clock_free_icon(ICONBLK *icon)
{
    if (icon == NULL) {
        return;
    }

    free((void *) (intptr_t) icon->ib_pmask);
    free((void *) (intptr_t) icon->ib_pdata);
    icon->ib_pmask = 0;
    icon->ib_pdata = 0;
}

/*
 * Releases all app-owned bitmap resources held in the clock state.
 */
static void clock_free_icons(clock_state_t *state)
{
    WORD ii;

    if (state == NULL) {
        return;
    }

    for (ii = 0; ii < clock_number_count; ++ii) {
        clock_free_bitblk(&state->numbers[ii]);
    }
    state->numbers_loaded = 0;
}

/*
 * Deep-copies one resource bit block so its bitmap plane survives
 * after `rsrc_free()`.
 */
static int clock_clone_bitblk(BITBLK *dst, const BITBLK *src)
{
    size_t plane_bytes;
    void *data_copy;

    if (dst == NULL || src == NULL || src->bi_pdata == 0 ||
        src->bi_wb <= 0 || src->bi_hl <= 0) {
        return 0;
    }

    plane_bytes = (size_t) src->bi_wb * (size_t) src->bi_hl;
    data_copy = malloc(plane_bytes);
    if (data_copy == NULL) {
        return 0;
    }

    memcpy(data_copy, (const void *) (intptr_t) src->bi_pdata, plane_bytes);
    *dst = *src;
    dst->bi_pdata = (LONG) (intptr_t) data_copy;
    return 1;
}

/*
 * Deep-copies one resource icon block so its bitmap planes survive
 * after `rsrc_free()`.
 */
static int clock_clone_icon(ICONBLK *dst, const ICONBLK *src)
{
    size_t plane_bytes;
    void *mask_copy;
    void *data_copy;

    if (dst == NULL || src == NULL || src->ib_pmask == 0 ||
        src->ib_pdata == 0 || src->ib_wicon <= 0 || src->ib_hicon <= 0) {
        return 0;
    }

    plane_bytes = (size_t) ((src->ib_wicon + 15) / 16) * 2u *
        (size_t) src->ib_hicon;
    mask_copy = malloc(plane_bytes);
    data_copy = malloc(plane_bytes);
    if (mask_copy == NULL || data_copy == NULL) {
        free(mask_copy);
        free(data_copy);
        return 0;
    }

    memcpy(mask_copy, (const void *) (intptr_t) src->ib_pmask, plane_bytes);
    memcpy(data_copy, (const void *) (intptr_t) src->ib_pdata, plane_bytes);

    *dst = *src;
    dst->ib_pmask = (LONG) (intptr_t) mask_copy;
    dst->ib_pdata = (LONG) (intptr_t) data_copy;
    dst->ib_ptext = 0;
    return 1;
}

/*
 * Returns the current local broken-down time. When the host clock
 * query fails, the structure is cleared.
 */
static void clock_local_time(struct tm *out_tm)
{
    time_t now;
    struct tm *local_tm;

    if (out_tm == NULL) {
        return;
    }

    now = time(NULL);
    if (now == (time_t) -1) {
        memset(out_tm, 0, sizeof(*out_tm));
        return;
    }

    local_tm = localtime(&now);
    if (local_tm == NULL) {
        memset(out_tm, 0, sizeof(*out_tm));
        return;
    }

    *out_tm = *local_tm;
}

/*
 * Loads the external numeral resource and copies the bit blocks into
 * local state for later direct bitmap drawing.
 */
static int clock_load_bitblks_from_resource(const char *primary_path,
                                            const char *fallback_path,
                                            BITBLK *bitblks,
                                            WORD count)
{
    BITBLK *bitblk;
    WORD ii;

    if (bitblks == NULL || count <= 0) {
        return 0;
    }
    if (rsrc_load((char *) primary_path) == 0 &&
        rsrc_load((char *) fallback_path) == 0) {
        return 0;
    }

    for (ii = 0; ii < count; ++ii) {
        if (rsrc_gaddr(R_BITBLK, ii, (void **) &bitblk) == 0 ||
            bitblk == NULL) {
            rsrc_free();
            return 0;
        }
        if (!clock_clone_bitblk(&bitblks[ii], bitblk)) {
            while (--ii >= 0) {
                clock_free_bitblk(&bitblks[ii]);
            }
            rsrc_free();
            return 0;
        }
    }
    rsrc_free();
    return 1;
}

/*
 * Loads the external numeral resource and copies the icon blocks into
 * local state for later direct bitmap drawing.
 */
static int clock_load_icons_from_resource(const char *primary_path,
                                          const char *fallback_path,
                                          ICONBLK *icons,
                                          WORD count)
{
    ICONBLK *icon;
    WORD ii;

    if (icons == NULL || count <= 0) {
        return 0;
    }
    if (rsrc_load((char *) primary_path) == 0 &&
        rsrc_load((char *) fallback_path) == 0) {
        return 0;
    }

    for (ii = 0; ii < count; ++ii) {
        if (rsrc_gaddr(R_ICONBLK, ii, (void **) &icon) == 0 || icon == NULL) {
            rsrc_free();
            return 0;
        }
        if (!clock_clone_icon(&icons[ii], icon)) {
            while (--ii >= 0) {
                clock_free_icon(&icons[ii]);
            }
            rsrc_free();
            return 0;
        }
    }
    rsrc_free();
    return 1;
}

/*
 * Loads the external numeral resource and copies the icon blocks into
 * local state for later direct bitmap drawing.
 */
static int clock_load_numbers(clock_state_t *state)
{
    if (state == NULL) {
        return 0;
    }
    state->numbers_loaded = clock_load_bitblks_from_resource(
        "bin/clock_numbers.rsc", "clock_numbers.rsc",
        state->numbers, clock_number_count);
    return state->numbers_loaded;
}

/*
 * Converts a clock angle in radians into a point, with zero radians
 * pointing straight up.
 */
static void clock_point(WORD cx,
                        WORD cy,
                        double radius,
                        double angle,
                        WORD *x,
                        WORD *y)
{
    if (x == NULL || y == NULL) {
        return;
    }

    *x = (WORD) lround((double) cx + sin(angle) * radius);
    *y = (WORD) lround((double) cy - cos(angle) * radius);
}

/*
 * Draws one transparent image plane at the requested screen
 * position.
 */
static void clock_draw_bitmap_plane(WORD handle,
                                    WORD width,
                                    WORD height,
                                    WORD words_per_row,
                                    LONG plane,
                                    WORD dst_x,
                                    WORD dst_y)
{
    MFDB src;
    WORD pxy[8];
    WORD colors[2];

    if (plane == 0 || width <= 0 || height <= 0 || words_per_row <= 0) {
        return;
    }

    memset(&src, 0, sizeof(src));
    src.fd_addr = (VOID *) (intptr_t) plane;
    src.fd_w = width;
    src.fd_h = height;
    src.fd_wdwidth = words_per_row;
    src.fd_stand = 1;
    src.fd_nplanes = 1;

    pxy[0] = 0;
    pxy[1] = 0;
    pxy[2] = (WORD) (width - 1);
    pxy[3] = (WORD) (height - 1);
    pxy[4] = dst_x;
    pxy[5] = dst_y;
    pxy[6] = (WORD) (dst_x + width - 1);
    pxy[7] = (WORD) (dst_y + height - 1);
    colors[0] = BLACK;
    colors[1] = WHITE;

    vrt_cpyfm(handle, MD_REPLACE, pxy, &src, NULL, colors);
}

/*
 * Draws one transparent icon bitmap plane at the requested screen
 * position.
 */
static void clock_draw_icon_plane(WORD handle,
                                  const ICONBLK *icon,
                                  LONG plane,
                                  WORD dst_x,
                                  WORD dst_y)
{
    if (icon == NULL || plane == 0 || icon->ib_wicon <= 0 ||
        icon->ib_hicon <= 0) {
        return;
    }

    clock_draw_bitmap_plane(handle, icon->ib_wicon, icon->ib_hicon,
        (WORD) ((icon->ib_wicon + 15) / 16), plane, dst_x, dst_y);
}

/*
 * Draws one clock numeral bitmap at the requested destination.
 */
static void clock_draw_number_bitmap(WORD handle,
                                     const BITBLK *bitblk,
                                     WORD dst_x,
                                     WORD dst_y)
{
    WORD width;

    if (bitblk == NULL) {
        return;
    }
    width = (WORD) (bitblk->bi_wb * 8);
    clock_draw_bitmap_plane(handle, width, bitblk->bi_hl,
        (WORD) (bitblk->bi_wb / 2), bitblk->bi_pdata, dst_x, dst_y);
}

/*
 * Draws all twelve numerals around the clock face.
 */
static void clock_draw_numbers(WORD handle,
                               const clock_state_t *state,
                               WORD cx,
                               WORD cy,
                               WORD radius)
{
    WORD ii;

    if (state == NULL || state->numbers_loaded == 0) {
        return;
    }

    for (ii = 0; ii < clock_number_count; ++ii) {
        const BITBLK *bitblk = &state->numbers[ii];
        WORD number_x;
        WORD number_y;
        double angle = (double) (ii + 1) * (clock_pi / 6.0);

        clock_point(cx, cy, (double) radius, angle, &number_x, &number_y);
        number_x = (WORD) (number_x - (bitblk->bi_wb * 8) / 2);
        number_y = (WORD) (number_y - bitblk->bi_hl / 2);
        clock_draw_number_bitmap(handle, bitblk, number_x, number_y);
    }
}

/*
 * Fills one stepped clock hand. The silhouette is intentionally
 * asymmetric: a longer thin strip runs along one side of a shorter,
 * thicker base section.
 */
static void clock_fill_hand(WORD handle,
                            WORD cx,
                            WORD cy,
                            double angle,
                            double arm_length,
                            double thin_width,
                            double shoulder_length,
                            double thick_width)
{
    double dir_x = sin(angle);
    double dir_y = -cos(angle);
    double perp_x = -dir_y;
    double perp_y = dir_x;
    double start_x = (double) cx - dir_x * 5.0;
    double start_y = (double) cy - dir_y * 5.0;
    double long_x = (double) cx + dir_x * arm_length;
    double long_y = (double) cy + dir_y * arm_length;
    double shoulder_x = (double) cx + dir_x * shoulder_length;
    double shoulder_y = (double) cy + dir_y * shoulder_length;
    WORD points[12];

    points[0] = (WORD) lround(start_x + perp_x * thin_width);
    points[1] = (WORD) lround(start_y + perp_y * thin_width);
    points[2] = (WORD) lround(long_x + perp_x * thin_width);
    points[3] = (WORD) lround(long_y + perp_y * thin_width);
    points[4] = (WORD) lround(long_x);
    points[5] = (WORD) lround(long_y);
    points[6] = (WORD) lround(shoulder_x);
    points[7] = (WORD) lround(shoulder_y);
    points[8] = (WORD) lround(shoulder_x - perp_x * thick_width);
    points[9] = (WORD) lround(shoulder_y - perp_y * thick_width);
    points[10] = (WORD) lround(start_x - perp_x * thick_width);
    points[11] = (WORD) lround(start_y - perp_y * thick_width);
    v_fillarea(handle, 6, points);
}

/*
 * Draws the second hand as a thin line with a short tail.
 */
static void clock_draw_second_hand(WORD handle,
                                   WORD cx,
                                   WORD cy,
                                   double angle,
                                   double length,
                                   double tail)
{
    WORD pxy[4];

    clock_point(cx, cy, tail, angle + clock_pi, &pxy[0], &pxy[1]);
    clock_point(cx, cy, length, angle, &pxy[2], &pxy[3]);
    v_pline(handle, 2, pxy);
}

/*
 * Draws the center hub as a small square block.
 */
static void clock_draw_hub(WORD handle, WORD cx, WORD cy)
{
    WORD rect[4];

    rect[0] = (WORD) (cx - 5);
    rect[1] = (WORD) (cy - 5);
    rect[2] = (WORD) (cx + 5);
    rect[3] = (WORD) (cy + 5);
    v_bar(handle, rect);
}

/*
 * Paints the clock inside the current clipped work rectangle.
 */
static void clock_paint_face(const clock_state_t *state,
                             const GRECT *work_rect,
                             const GRECT *clip_rect)
{
    WORD handle;
    WORD clip[4];
    WORD fill[4];
    WORD cx;
    WORD cy;
    WORD radius;
    GRECT face_rect;
    struct tm now_tm;
    double hour_angle;
    double minute_angle;
    double second_angle;

    if (state == NULL || work_rect == NULL || clip_rect == NULL) {
        return;
    }

    handle = graf_handle(NULL, NULL, NULL, NULL);
    face_rect.g_x = (WORD) (work_rect->g_x + clock_face_margin);
    face_rect.g_y = (WORD) (work_rect->g_y + clock_face_margin);
    face_rect.g_w = (WORD) (work_rect->g_w - clock_face_margin * 2);
    face_rect.g_h = (WORD) (work_rect->g_h - clock_face_margin * 2);
    if (face_rect.g_w <= 0 || face_rect.g_h <= 0) {
        return;
    }

    clip[0] = clip_rect->g_x;
    clip[1] = clip_rect->g_y;
    clip[2] = (WORD) (clip_rect->g_x + clip_rect->g_w - 1);
    clip[3] = (WORD) (clip_rect->g_y + clip_rect->g_h - 1);
    fill[0] = clip[0];
    fill[1] = clip[1];
    fill[2] = clip[2];
    fill[3] = clip[3];

    cx = (WORD) (face_rect.g_x + face_rect.g_w / 2);
    cy = (WORD) (face_rect.g_y + face_rect.g_h / 2);
    radius = (WORD) (((face_rect.g_w < face_rect.g_h) ?
        face_rect.g_w : face_rect.g_h) / 2 - 10);

    vs_clip(handle, 1, clip);
    (void) vswr_mode(handle, MD_REPLACE);
    vsf_color(handle, BLACK);
    v_bar(handle, fill);

    clock_draw_numbers(handle, state, cx, cy, (WORD) (radius - 4));

    clock_local_time(&now_tm);
    hour_angle = ((double) (now_tm.tm_hour % 12) +
        (double) now_tm.tm_min / 60.0) * (clock_pi / 6.0);
    minute_angle = ((double) now_tm.tm_min +
        (double) now_tm.tm_sec / 60.0) * (clock_pi / 30.0);
    second_angle = (double) now_tm.tm_sec * (clock_pi / 30.0);

    vsl_color(handle, WHITE);
    vsf_color(handle, WHITE);
    (void) vsl_width(handle, 1);
    clock_fill_hand(handle, cx, cy, hour_angle, 56.0,
        3.0, 44.0, 7.0);
    clock_fill_hand(handle, cx, cy, minute_angle, 88.0,
        2.0, 70.0, 6.0);
    clock_draw_second_hand(handle, cx, cy, second_angle,
        (double) radius - 18.0, 18.0);
    clock_draw_hub(handle, cx, cy);
    vs_clip(handle, 0, clip);
}

/*
 * Redraws the visible work-area clips that intersect the requested
 * dirty rectangle.
 */
static void clock_draw(clock_state_t *state, const GRECT *dirty_rect)
{
    GRECT clip_rect;
    GRECT work_rect;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
        &work_rect.g_w, &work_rect.g_h) == 0) {
        return;
    }

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
        &clip_rect.g_w, &clip_rect.g_h);
    while (clip_rect.g_w > 0 && clip_rect.g_h > 0) {
        GRECT draw_rect;

        draw_rect = clip_rect;
        if (dirty_rect != NULL) {
            WORD dirty_x1 = (WORD) (dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_y1 = (WORD) (dirty_rect->g_y + dirty_rect->g_h - 1);
            WORD draw_x1 = (WORD) (draw_rect.g_x + draw_rect.g_w - 1);
            WORD draw_y1 = (WORD) (draw_rect.g_y + draw_rect.g_h - 1);

            if (draw_rect.g_x < dirty_rect->g_x) {
                draw_rect.g_x = dirty_rect->g_x;
            }
            if (draw_rect.g_y < dirty_rect->g_y) {
                draw_rect.g_y = dirty_rect->g_y;
            }
            if (draw_x1 > dirty_x1) {
                draw_x1 = dirty_x1;
            }
            if (draw_y1 > dirty_y1) {
                draw_y1 = dirty_y1;
            }
            draw_rect.g_w = (WORD) (draw_x1 - draw_rect.g_x + 1);
            draw_rect.g_h = (WORD) (draw_y1 - draw_rect.g_y + 1);
        }

        if (draw_rect.g_w > 0 && draw_rect.g_h > 0) {
            GRECT clipped_work = work_rect;
            WORD work_x1;
            WORD work_y1;
            WORD draw_x1;
            WORD draw_y1;

            if (clipped_work.g_x < draw_rect.g_x) {
                clipped_work.g_x = draw_rect.g_x;
            }
            if (clipped_work.g_y < draw_rect.g_y) {
                clipped_work.g_y = draw_rect.g_y;
            }

            work_x1 = (WORD) (work_rect.g_x + work_rect.g_w - 1);
            work_y1 = (WORD) (work_rect.g_y + work_rect.g_h - 1);
            draw_x1 = (WORD) (draw_rect.g_x + draw_rect.g_w - 1);
            draw_y1 = (WORD) (draw_rect.g_y + draw_rect.g_h - 1);
            if (work_x1 > draw_x1) {
                work_x1 = draw_x1;
            }
            if (work_y1 > draw_y1) {
                work_y1 = draw_y1;
            }
            clipped_work.g_w = (WORD) (work_x1 - clipped_work.g_x + 1);
            clipped_work.g_h = (WORD) (work_y1 - clipped_work.g_y + 1);

            if (clipped_work.g_w > 0 && clipped_work.g_h > 0) {
                clock_paint_face(state, &work_rect, &clipped_work);
            }
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }
    wind_update(END_UPDATE);
}

/*
 * Runs the hosted GEM analog clock application.
 */
int clock_main(void)
{
    const UWORD window_kind = (UWORD) (NAME | MOVER | CLOSER);
    WORD appl_id;
    WORD msg[8] = { 0 };
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;
    struct tm now_tm;
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

    memset(&state, 0, sizeof(state));
    state.last_second = -1;
    (void) clock_load_numbers(&state);

    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        if (state.numbers_loaded != 0) {
            rsrc_free();
        }
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.handle, WF_NAME, "Clock");
    (void) wind_calc(WC_BORDER, window_kind, 84, 36,
        clock_work_width, clock_work_height,
        &outer_x, &outer_y, &outer_w, &outer_h);
    (void) wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void) graf_mouse(M_ON, NULL);

    clock_local_time(&now_tm);
    state.last_second = now_tm.tm_sec;

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_KEYBD | MU_TIMER),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            200, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0 && msg[3] == state.handle) {
            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw_rect;

                redraw_rect.g_x = msg[4];
                redraw_rect.g_y = msg[5];
                redraw_rect.g_w = msg[6];
                redraw_rect.g_h = msg[7];
                clock_draw(&state, &redraw_rect);
            } else if (msg[0] == WM_MOVED) {
                (void) wind_set(state.handle, WF_WXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
            } else if (msg[0] == WM_TOPPED) {
                (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            }
        }

        if ((event_flags & MU_KEYBD) != 0 && (key_code & 0x00ff) == 27) {
            break;
        }

        if ((event_flags & MU_TIMER) != 0) {
            clock_local_time(&now_tm);
            if (now_tm.tm_sec != state.last_second) {
                state.last_second = now_tm.tm_sec;
                clock_draw(&state, NULL);
            }
        }
    }

    if (state.handle > 0) {
        (void) wind_close(state.handle);
        (void) wind_delete(state.handle);
    }
    clock_free_icons(&state);
    appl_exit();
    gem_os_shutdown();
    return 0;
}

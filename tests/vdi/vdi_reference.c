/*
 * Implements simple specification-oriented reference renderers for the
 * hosted VDI tests. These routines favor clarity over speed so the test
 * suite can compare VDI output against an independently written model.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TEST_VDI_PI 3.14159265358979323846
#define TEST_VDI_MAX_POINTS 256

static WORD test_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

static WORD test_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

static void test_plot_clipped(test_bitmap_t *bitmap,
                              const test_clip_rect_t *clip,
                              WORD x,
                              WORD y,
                              uint8_t value)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (clip != NULL && clip->enabled != 0) {
        x0 = test_min_word(clip->xy[0], clip->xy[2]);
        y0 = test_min_word(clip->xy[1], clip->xy[3]);
        x1 = test_max_word(clip->xy[0], clip->xy[2]);
        y1 = test_max_word(clip->xy[1], clip->xy[3]);
        if (x < x0 || x > x1 || y < y0 || y > y1) {
            return;
        }
    }

    test_bitmap_set_pixel(bitmap, x, y, value);
}

static void test_draw_line(test_bitmap_t *bitmap,
                           const test_clip_rect_t *clip,
                           WORD x0,
                           WORD y0,
                           WORD x1,
                           WORD y1,
                           uint8_t value)
{
    WORD dx = (WORD) abs(x1 - x0);
    WORD sx = (WORD) ((x0 < x1) ? 1 : -1);
    WORD dy = (WORD) -abs(y1 - y0);
    WORD sy = (WORD) ((y0 < y1) ? 1 : -1);
    WORD err = (WORD) (dx + dy);

    for (;;) {
        WORD e2 = (WORD) (2 * err);

        test_plot_clipped(bitmap, clip, x0, y0, value);
        if (x0 == x1 && y0 == y1) {
            return;
        }
        if (e2 >= dy) {
            err = (WORD) (err + dy);
            x0 = (WORD) (x0 + sx);
        }
        if (e2 <= dx) {
            err = (WORD) (err + dx);
            y0 = (WORD) (y0 + sy);
        }
    }
}

static void test_fill_polygon(test_bitmap_t *bitmap,
                              const test_clip_rect_t *clip,
                              WORD count,
                              const WORD *xy,
                              uint8_t value)
{
    WORD min_y;
    WORD max_y;
    WORD y;
    WORD i;

    if (count < 3 || xy == NULL) {
        return;
    }

    min_y = max_y = xy[1];
    for (i = 1; i < count; ++i) {
        WORD py = xy[i * 2 + 1];

        if (py < min_y) {
            min_y = py;
        }
        if (py > max_y) {
            max_y = py;
        }
    }

    for (y = min_y; y <= max_y; ++y) {
        double intersections[TEST_VDI_MAX_POINTS];
        WORD intersections_count = 0;

        for (i = 0; i < count; ++i) {
            WORD next = (WORD) ((i + 1) % count);
            WORD x0 = xy[i * 2];
            WORD y0 = xy[i * 2 + 1];
            WORD x1 = xy[next * 2];
            WORD y1 = xy[next * 2 + 1];

            if (y0 == y1) {
                continue;
            }
            if (y < test_min_word(y0, y1) || y >= test_max_word(y0, y1)) {
                continue;
            }

            intersections[intersections_count++] =
                (double) x0 + ((double) (y - y0) * (double) (x1 - x0)) /
                (double) (y1 - y0);
        }

        for (i = 0; i < (WORD) (intersections_count - 1); ++i) {
            WORD j;

            for (j = (WORD) (i + 1); j < intersections_count; ++j) {
                if (intersections[j] < intersections[i]) {
                    double temp = intersections[i];

                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        for (i = 0; i + 1 < intersections_count; i += 2) {
            WORD left = (WORD) ceil(intersections[i]);
            WORD right = (WORD) floor(intersections[i + 1]);
            WORD x;

            for (x = left; x <= right; ++x) {
                test_plot_clipped(bitmap, clip, x, y, value);
            }
        }
    }
}

static WORD test_normalize_angle(WORD angle)
{
    while (angle < 0) {
        angle = (WORD) (angle + 3600);
    }
    while (angle >= 3600) {
        angle = (WORD) (angle - 3600);
    }
    return angle;
}

static int test_angle_in_sweep(WORD angle, WORD start, WORD end)
{
    angle = test_normalize_angle(angle);
    start = test_normalize_angle(start);
    end = test_normalize_angle(end);

    if (start <= end) {
        return angle >= start && angle <= end;
    }
    return angle >= start || angle <= end;
}

static WORD test_arc_sample_count(WORD xrad, WORD yrad)
{
    WORD radius = test_max_word(xrad, yrad);
    WORD samples = (WORD) (radius * 8);

    if (samples < 32) {
        samples = 32;
    }
    if (samples > (WORD) (TEST_VDI_MAX_POINTS - 2)) {
        samples = (WORD) (TEST_VDI_MAX_POINTS - 2);
    }
    return samples;
}

static WORD test_build_arc_points(WORD cx,
                                  WORD cy,
                                  WORD xrad,
                                  WORD yrad,
                                  WORD begang,
                                  WORD endang,
                                  WORD *points,
                                  WORD include_center)
{
    WORD samples = test_arc_sample_count(xrad, yrad);
    WORD point_count = 0;
    WORD sample;

    if (include_center != 0) {
        points[point_count * 2] = cx;
        points[point_count * 2 + 1] = cy;
        ++point_count;
    }

    for (sample = 0; sample <= samples; ++sample) {
        WORD angle = (WORD) ((LONG) sample * 3600L / samples);

        if (test_angle_in_sweep(angle, begang, endang)) {
            double radians = ((double) angle / 10.0) * (TEST_VDI_PI / 180.0);
            WORD px = (WORD) lround((double) cx + cos(radians) * xrad);
            WORD py = (WORD) lround((double) cy - sin(radians) * yrad);

            if (point_count == 0 ||
                points[(point_count - 1) * 2] != px ||
                points[(point_count - 1) * 2 + 1] != py) {
                points[point_count * 2] = px;
                points[point_count * 2 + 1] = py;
                ++point_count;
            }
        }
    }

    return point_count;
}

void test_reference_pline(test_bitmap_t *bitmap,
                          const test_clip_rect_t *clip,
                          WORD count,
                          const WORD *pxy,
                          uint8_t color)
{
    WORD i;

    for (i = 0; i + 1 < count; ++i) {
        test_draw_line(bitmap, clip, pxy[i * 2], pxy[i * 2 + 1],
            pxy[i * 2 + 2], pxy[i * 2 + 3], color);
    }
}

void test_reference_pmarker(test_bitmap_t *bitmap,
                            const test_clip_rect_t *clip,
                            WORD count,
                            const WORD *xy,
                            uint8_t color)
{
    WORD i;

    for (i = 0; i < count; ++i) {
        WORD x = xy[i * 2];
        WORD y = xy[i * 2 + 1];

        test_draw_line(bitmap, clip, (WORD) (x - 2), y, (WORD) (x + 2), y,
            color);
        test_draw_line(bitmap, clip, x, (WORD) (y - 2), x, (WORD) (y + 2),
            color);
    }
}

void test_reference_bar(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        const WORD xy[4],
                        uint8_t color)
{
    WORD x0 = test_min_word(xy[0], xy[2]);
    WORD y0 = test_min_word(xy[1], xy[3]);
    WORD x1 = test_max_word(xy[0], xy[2]);
    WORD y1 = test_max_word(xy[1], xy[3]);
    WORD y;

    for (y = y0; y <= y1; ++y) {
        WORD x;

        for (x = x0; x <= x1; ++x) {
            test_plot_clipped(bitmap, clip, x, y, color);
        }
    }
}

void test_reference_fillarea(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD count,
                             const WORD *xy,
                             uint8_t fill_color,
                             uint8_t border_color,
                             WORD draw_perimeter)
{
    WORD i;

    test_fill_polygon(bitmap, clip, count, xy, fill_color);
    if (draw_perimeter == 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        WORD next = (WORD) ((i + 1) % count);

        test_draw_line(bitmap, clip, xy[i * 2], xy[i * 2 + 1],
            xy[next * 2], xy[next * 2 + 1], border_color);
    }
}

void test_reference_cellarray(test_bitmap_t *bitmap,
                              const test_clip_rect_t *clip,
                              const WORD xy[4],
                              WORD columns,
                              WORD rows,
                              const WORD *colors)
{
    WORD x0 = test_min_word(xy[0], xy[2]);
    WORD y0 = test_min_word(xy[1], xy[3]);
    WORD x1 = test_max_word(xy[0], xy[2]);
    WORD y1 = test_max_word(xy[1], xy[3]);
    WORD cell_width = (WORD) (((x1 - x0) + columns) / columns);
    WORD cell_height = (WORD) (((y1 - y0) + rows) / rows);
    WORD row;

    for (row = 0; row < rows; ++row) {
        WORD col;

        for (col = 0; col < columns; ++col) {
            WORD cell_xy[4];

            cell_xy[0] = (WORD) (x0 + col * cell_width);
            cell_xy[1] = (WORD) (y0 + row * cell_height);
            cell_xy[2] = (WORD) (cell_xy[0] + cell_width - 1);
            cell_xy[3] = (WORD) (cell_xy[1] + cell_height - 1);
            test_reference_bar(bitmap, clip, cell_xy,
                (uint8_t) ((colors[row * columns + col] != 0) ? 1 : 0));
        }
    }
}

void test_reference_circle(test_bitmap_t *bitmap,
                           const test_clip_rect_t *clip,
                           WORD x,
                           WORD y,
                           WORD radius,
                           uint8_t color)
{
    test_reference_arc(bitmap, clip, x, y, radius, radius, 0, 3599, color);
}

void test_reference_arc(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        WORD x,
                        WORD y,
                        WORD xrad,
                        WORD yrad,
                        WORD begang,
                        WORD endang,
                        uint8_t color)
{
    WORD points[TEST_VDI_MAX_POINTS * 2];
    WORD count = test_build_arc_points(x, y, xrad, yrad, begang, endang,
        points, 0);
    WORD i;

    for (i = 0; i + 1 < count; ++i) {
        test_draw_line(bitmap, clip, points[i * 2], points[i * 2 + 1],
            points[i * 2 + 2], points[i * 2 + 3], color);
    }
}

void test_reference_pieslice(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD x,
                             WORD y,
                             WORD xrad,
                             WORD yrad,
                             WORD begang,
                             WORD endang,
                             uint8_t fill_color,
                             uint8_t line_color)
{
    WORD points[TEST_VDI_MAX_POINTS * 2];
    WORD count = test_build_arc_points(x, y, xrad, yrad, begang, endang,
        points, 1);
    WORD i;

    test_fill_polygon(bitmap, clip, count, points, fill_color);
    for (i = 0; i + 1 < count; ++i) {
        test_draw_line(bitmap, clip, points[i * 2], points[i * 2 + 1],
            points[i * 2 + 2], points[i * 2 + 3], line_color);
    }
    if (count > 2) {
        test_draw_line(bitmap, clip, points[(count - 1) * 2],
            points[(count - 1) * 2 + 1], points[0], points[1], line_color);
    }
}

void test_reference_rbox(test_bitmap_t *bitmap,
                         const test_clip_rect_t *clip,
                         const WORD xy[4],
                         uint8_t line_color,
                         uint8_t fill_color,
                         WORD filled,
                         WORD draw_perimeter)
{
    WORD x0 = test_min_word(xy[0], xy[2]);
    WORD y0 = test_min_word(xy[1], xy[3]);
    WORD x1 = test_max_word(xy[0], xy[2]);
    WORD y1 = test_max_word(xy[1], xy[3]);

    if (filled != 0) {
        test_reference_bar(bitmap, clip, xy, fill_color);
    }
    if (draw_perimeter != 0) {
        test_draw_line(bitmap, clip, x0, y0, x1, y0, line_color);
        test_draw_line(bitmap, clip, x1, y0, x1, y1, line_color);
        test_draw_line(bitmap, clip, x1, y1, x0, y1, line_color);
        test_draw_line(bitmap, clip, x0, y1, x0, y0, line_color);
    }
}

void test_reference_contourfill(test_bitmap_t *bitmap,
                                const test_clip_rect_t *clip,
                                WORD x,
                                WORD y,
                                uint8_t color)
{
    size_t capacity = (size_t) bitmap->width * (size_t) bitmap->height;
    WORD *queue_x = calloc(capacity, sizeof(WORD));
    WORD *queue_y = calloc(capacity, sizeof(WORD));
    uint8_t target;
    size_t head = 0;
    size_t tail = 0;

    if (queue_x == NULL || queue_y == NULL) {
        free(queue_x);
        free(queue_y);
        return;
    }

    target = test_bitmap_get_pixel(bitmap, x, y);
    if (target == color) {
        free(queue_x);
        free(queue_y);
        return;
    }

    queue_x[tail] = x;
    queue_y[tail++] = y;
    while (head < tail) {
        WORD px = queue_x[head];
        WORD py = queue_y[head++];

        if (clip != NULL && clip->enabled != 0) {
            WORD x0 = test_min_word(clip->xy[0], clip->xy[2]);
            WORD y0 = test_min_word(clip->xy[1], clip->xy[3]);
            WORD x1 = test_max_word(clip->xy[0], clip->xy[2]);
            WORD y1 = test_max_word(clip->xy[1], clip->xy[3]);

            if (px < x0 || px > x1 || py < y0 || py > y1) {
                continue;
            }
        }

        if (test_bitmap_get_pixel(bitmap, px, py) != target) {
            continue;
        }

        test_bitmap_set_pixel(bitmap, px, py, color);
        if (tail + 4u <= capacity) {
            queue_x[tail] = (WORD) (px - 1);
            queue_y[tail++] = py;
            queue_x[tail] = (WORD) (px + 1);
            queue_y[tail++] = py;
            queue_x[tail] = px;
            queue_y[tail++] = (WORD) (py - 1);
            queue_x[tail] = px;
            queue_y[tail++] = (WORD) (py + 1);
        }
    }

    free(queue_x);
    free(queue_y);
}

void test_reference_vro_cpyfm(test_bitmap_t *dst,
                              test_bitmap_t *src,
                              const WORD pxy[8])
{
    test_reference_vro_cpyfm_clipped(dst, src, pxy, NULL);
}

void test_reference_vro_cpyfm_clipped(test_bitmap_t *dst,
                                      test_bitmap_t *src,
                                      const WORD pxy[8],
                                      const test_clip_rect_t *clip)
{
    WORD src_x0 = test_min_word(pxy[0], pxy[2]);
    WORD src_y0 = test_min_word(pxy[1], pxy[3]);
    WORD src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    WORD src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    WORD dst_x0 = test_min_word(pxy[4], pxy[6]);
    WORD dst_y0 = test_min_word(pxy[5], pxy[7]);
    WORD dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    WORD dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);
    WORD y;

    for (y = 0; y < dst_h; ++y) {
        WORD x;
        WORD sample_y = (WORD) (src_y0 + (LONG) y * src_h / dst_h);

        for (x = 0; x < dst_w; ++x) {
            WORD sample_x = (WORD) (src_x0 + (LONG) x * src_w / dst_w);

            test_plot_clipped(dst, clip, (WORD) (dst_x0 + x),
                (WORD) (dst_y0 + y),
                test_bitmap_get_pixel(src, sample_x, sample_y));
        }
    }
}

void test_reference_vrt_cpyfm(test_bitmap_t *dst,
                              test_bitmap_t *src,
                              const WORD pxy[8],
                              uint8_t foreground)
{
    test_reference_vrt_cpyfm_clipped(dst, src, pxy, foreground, NULL);
}

void test_reference_vrt_cpyfm_clipped(test_bitmap_t *dst,
                                      test_bitmap_t *src,
                                      const WORD pxy[8],
                                      uint8_t foreground,
                                      const test_clip_rect_t *clip)
{
    WORD src_x0 = test_min_word(pxy[0], pxy[2]);
    WORD src_y0 = test_min_word(pxy[1], pxy[3]);
    WORD src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    WORD src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    WORD dst_x0 = test_min_word(pxy[4], pxy[6]);
    WORD dst_y0 = test_min_word(pxy[5], pxy[7]);
    WORD dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    WORD dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);
    WORD y;

    for (y = 0; y < dst_h; ++y) {
        WORD x;
        WORD sample_y = (WORD) (src_y0 + (LONG) y * src_h / dst_h);

        for (x = 0; x < dst_w; ++x) {
            WORD sample_x = (WORD) (src_x0 + (LONG) x * src_w / dst_w);

            if (test_bitmap_get_pixel(src, sample_x, sample_y) != 0u) {
                test_plot_clipped(dst, clip, (WORD) (dst_x0 + x),
                    (WORD) (dst_y0 + y), foreground);
            }
        }
    }
}

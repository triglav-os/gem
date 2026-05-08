/*
 * Implements GEM VDI geometric primitive and filled-area entry points.
 * This file keeps polygon, arc, and contour operations together so the
 * drawing logic is no longer buried inside one monolithic source file.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include "platform/os.h"

#include <math.h>
#include <stdlib.h>

static double _vdi_deci_degrees_to_radians(WORD angle)
{
    return (double) angle * (VDI_PI / 1800.0);
}

static WORD _vdi_normalize_angle(WORD angle)
{
    int a = (int) angle % 3600;

    if (a < 0) {
        a += 3600;
    }
    return (WORD) a;
}

static int _vdi_angle_in_sweep(WORD angle, WORD start, WORD end)
{
    angle = _vdi_normalize_angle(angle);
    start = _vdi_normalize_angle(start);
    end = _vdi_normalize_angle(end);

    if (start <= end) {
        return angle >= start && angle <= end;
    }
    return angle >= start || angle <= end;
}

static WORD _vdi_point_count_from_radius(WORD xrad, WORD yrad)
{
    WORD radius = _vdi_max_word(xrad, yrad);
    WORD points = (WORD) (radius * 8);

    if (points < 32) {
        points = 32;
    }
    if (points > (WORD) (VDI_POLYGON_MAX_POINTS - 2)) {
        points = (WORD) (VDI_POLYGON_MAX_POINTS - 2);
    }
    return points;
}

void _vdi_fill_polygon_points(const WORD *points, WORD count, WORD color)
{
    vdi_rect_t clip;
    WORD min_y;
    WORD max_y;
    WORD y;

    if (points == NULL || count < 3) {
        return;
    }

    min_y = max_y = points[1];
    for (y = 1; y < count; ++y) {
        WORD py = points[y * 2 + 1];

        if (py < min_y) {
            min_y = py;
        }
        if (py > max_y) {
            max_y = py;
        }
    }

    _vdi_get_active_clip_rect(&clip);
    if (clip.x0 > clip.x1 || clip.y0 > clip.y1) {
        return;
    }

    if (min_y < clip.y0) {
        min_y = clip.y0;
    }
    if (max_y > clip.y1) {
        max_y = clip.y1;
    }

    for (y = min_y; y <= max_y; ++y) {
        double intersections[VDI_POLYGON_MAX_POINTS];
        WORD intersections_count = 0;
        WORD i;

        for (i = 0; i < count; ++i) {
            WORD next = (WORD) ((i + 1 < count) ? i + 1 : 0);
            WORD x0 = points[i * 2];
            WORD y0 = points[i * 2 + 1];
            WORD x1 = points[next * 2];
            WORD y1 = points[next * 2 + 1];

            if (y0 == y1) {
                continue;
            }
            if ((y < _vdi_min_word(y0, y1)) || (y >= _vdi_max_word(y0, y1))) {
                continue;
            }

            if (intersections_count < VDI_POLYGON_MAX_POINTS) {
                intersections[intersections_count++] =
                    (double) x0 + ((double) (y - y0) * (double) (x1 - x0)) /
                    (double) (y1 - y0);
            }
        }

        if (intersections_count < 2) {
            continue;
        }

        for (i = 1; i < intersections_count; ++i) {
            double key = intersections[i];
            int j = (int) i - 1;

            while (j >= 0 && intersections[j] > key) {
                intersections[j + 1] = intersections[j];
                --j;
            }
            intersections[j + 1] = key;
        }

        for (i = 0; i + 1 < intersections_count; i += 2) {
            WORD left = (WORD) ceil(intersections[i]);
            WORD right = (WORD) floor(intersections[i + 1]);

            if (left < clip.x0) {
                left = clip.x0;
            }
            if (right > clip.x1) {
                right = clip.x1;
            }
            if (left <= right) {
                _vdi_draw_screen_hline_direct(y, left, right, color);
            }
        }
    }
}

WORD _vdi_build_arc_points(WORD cx, WORD cy, WORD xrad, WORD yrad,
    WORD start, WORD end, WORD *points, WORD close_polygon)
{
    WORD samples;
    WORD point_count = 0;
    WORD sample;

    if (points == NULL || xrad < 0 || yrad < 0) {
        return 0;
    }

    if (close_polygon != 0) {
        points[point_count * 2] = cx;
        points[point_count * 2 + 1] = cy;
        ++point_count;
    }

    samples = _vdi_point_count_from_radius(xrad, yrad);
    for (sample = 0; sample <= samples; ++sample) {
        WORD angle = (WORD) ((LONG) sample * 3600L / samples);

        if (_vdi_angle_in_sweep(angle, start, end)) {
            double radians = _vdi_deci_degrees_to_radians(angle);
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

void _vdi_draw_polyline_points(const WORD *points, WORD count, WORD color,
    WORD closed)
{
    WORD i;

    if (points == NULL || count < 2) {
        return;
    }

    for (i = 0; i < (WORD) (count - 1); ++i) {
        _vdi_draw_line(points[i * 2], points[i * 2 + 1],
            points[i * 2 + 2], points[i * 2 + 3], color);
    }
    if (closed != 0) {
        _vdi_draw_line(points[(count - 1) * 2], points[(count - 1) * 2 + 1],
            points[0], points[1], color);
    }
}

VOID v_pmarker(WORD handle, WORD count, WORD xy[])
{
    WORD i;

    if (!_vdi_valid_handle(handle) || xy == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        WORD x = xy[i * 2];
        WORD y = xy[i * 2 + 1];

        _vdi_draw_line((WORD) (x - 2), y, (WORD) (x + 2), y,
            _vdi_compat.marker_color);
        _vdi_draw_line(x, (WORD) (y - 2), x, (WORD) (y + 2),
            _vdi_compat.marker_color);
    }
    _vdi_present_screen();
}

VOID v_fillarea(WORD handle, WORD count, WORD xy[])
{
    if (!_vdi_valid_handle(handle) || xy == NULL || count <= 0) {
        return;
    }

    _vdi_fill_polygon_points(xy, count, _vdi.fill_color);
    if (_vdi_compat.fill_perimeter != 0) {
        _vdi_draw_polyline_points(xy, count, _vdi.line_color, 1);
    }
    _vdi_present_screen();
}

VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, begang, endang,
        points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 0);
    _vdi_present_screen();
}

VOID v_pieslice(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
    WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, begang, endang,
        points, 1);
    _vdi_fill_polygon_points(points, count, _vdi.fill_color);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_pie(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang)
{
    v_pieslice(handle, x, y, radius, begang, endang);
}

VOID v_circle(WORD handle, WORD x, WORD y, WORD radius)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle) || radius < 0) {
        return;
    }

    count = _vdi_build_arc_points(x, y, radius, radius, 0, 3599, points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, 0, 3599, points, 0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_ellarc(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad, WORD begang,
    WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, begang, endang, points,
        0);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 0);
    _vdi_present_screen();
}

VOID v_ellpie(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad, WORD begang,
    WORD endang)
{
    WORD points[VDI_POLYGON_MAX_POINTS * 2];
    WORD count;

    if (!_vdi_valid_handle(handle)) {
        return;
    }

    count = _vdi_build_arc_points(x, y, xrad, yrad, begang, endang, points,
        1);
    _vdi_fill_polygon_points(points, count, _vdi.fill_color);
    _vdi_draw_polyline_points(points, count, _vdi.line_color, 1);
    _vdi_present_screen();
}

VOID v_rbox(WORD handle, WORD xy[4])
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    _vdi_rect_from_xy(xy, &x0, &y0, &x1, &y1);
    _vdi_outline_box(x0, y0, x1, y1, _vdi.line_color);
    _vdi_present_screen();
}

VOID v_rfbox(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    vr_recfl(handle, xy);
}

VOID v_contourfill(WORD handle, WORD x, WORD y, WORD index)
{
    typedef struct vdi_seed {
        WORD x;
        WORD y;
    } vdi_seed_t;

    vdi_rect_t clip;
    WORD target;
    WORD fill_color = _vdi_color_to_pixel(index);
    size_t capacity;
    vdi_seed_t *stack;
    size_t stack_size = 0;

    if (!_vdi_valid_handle(handle) || !_vdi_point_visible(x, y)) {
        return;
    }

    _vdi_get_active_clip_rect(&clip);
    target = _vdi_get_screen_pixel(x, y);
    if (target == fill_color) {
        return;
    }

    capacity = (size_t) _vdi.width * (size_t) _vdi.height;
    stack = gem_os_alloc(sizeof(vdi_seed_t) * capacity);
    if (stack == NULL) {
        return;
    }

    {
        size_t pitch = _vdi.surface->pitch;
        const uint8_t *pixels = (const uint8_t *) _vdi.surface->pixels;

#define _CF_ROW(yy) (pixels + (size_t) (yy) * pitch)
#define _CF_PIX(row, xx) \
    (((row)[(size_t) (xx) / 8u] & \
    (uint8_t) (0x80u >> ((unsigned int) (xx) & 7u))) != 0u)

        stack[stack_size].x = x;
        stack[stack_size].y = y;
        ++stack_size;
        while (stack_size > 0u) {
            WORD left;
            WORD right;
            WORD scan_y;
            vdi_seed_t seed = stack[--stack_size];
            const uint8_t *seed_row;

            if (seed.x < clip.x0 || seed.x > clip.x1 ||
                seed.y < clip.y0 || seed.y > clip.y1) {
                continue;
            }
            seed_row = _CF_ROW(seed.y);
            if ((WORD) _CF_PIX(seed_row, seed.x) != target) {
                continue;
            }

            left = seed.x;
            right = seed.x;
            while (left > clip.x0 &&
                (WORD) _CF_PIX(seed_row, left - 1) == target) {
                --left;
            }
            while (right < clip.x1 &&
                (WORD) _CF_PIX(seed_row, right + 1) == target) {
                ++right;
            }

            _vdi_draw_screen_hline_direct(seed.y, left, right, fill_color);

            for (scan_y = (WORD) (seed.y - 1); scan_y <= (WORD) (seed.y + 1);
                scan_y += 2) {
                const uint8_t *scan_row;
                WORD scan_x = left;

                if (scan_y < clip.y0 || scan_y > clip.y1) {
                    continue;
                }
                scan_row = _CF_ROW(scan_y);

                while (scan_x <= right) {
                    while (scan_x <= right &&
                        (WORD) _CF_PIX(scan_row, scan_x) != target) {
                        ++scan_x;
                    }
                    if (scan_x > right) {
                        break;
                    }
                    if (stack_size < capacity) {
                        stack[stack_size].x = scan_x;
                        stack[stack_size].y = scan_y;
                        ++stack_size;
                    }
                    while (scan_x <= right &&
                        (WORD) _CF_PIX(scan_row, scan_x) == target) {
                        ++scan_x;
                    }
                }
            }
        }

#undef _CF_ROW
#undef _CF_PIX
    }

    gem_os_free(stack);
    _vdi_present_screen();
}

VOID vr_recfl(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return;
    }

    v_bar(handle, xy);
}

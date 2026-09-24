/*
 * Gemread: a read-only GEM document viewer with color rendering, native
 * scrollbars, shared toolbar/status controls, and page thumbnails.
 *
 * AGPL-3.0-or-later License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/gemctl.h>
#include <gem/os.h>
#include <mupdf/fitz.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    gemread_root,
    gemread_bar,
    gemread_titles,
    gemread_file_title,
    gemread_popups,
    gemread_file_box,
    gemread_file_open,
    gemread_menu_count
};
enum {
    gemread_zoom_min = 50,
    gemread_zoom_max = 400,
    gemread_zoom_step = 25,
    gemread_page_margin = 8,
    gemread_thumb_slot_height = 112,
    gemread_thumb_padding = 6
};

typedef struct gemread_layout {
    gemctl_chrome_t chrome;
    GRECT sidebar;
    GRECT viewport;
} gemread_layout_t;

typedef struct gemread_state {
    WORD handle, vdi_handle, char_w, char_h, box_w, box_h;
    OBJECT menu[gemread_menu_count];
    fz_context *context;
    fz_document *document;
    fz_page *page;
    uint8_t *page_bgra;
    int page_width, page_height;
    uint8_t *body_bgra;
    int body_width, body_height;
    uint8_t *mono;
    int mono_stride;
    int page_count, page_number, thumbnail_top;
    int zoom_percent, scroll_x, scroll_y;
    GRECT normal_rect;
    int full_open;
    char path[PATH_MAX];
#if defined(GEMREAD_OVERLAY) && GEMREAD_OVERLAY
    gem_overlay_id_t overlay;
#endif
} gemread_state_t;

static void gemread_object(OBJECT *o, WORD next, WORD head, WORD tail,
                           UWORD type, UWORD flags, UWORD state, LONG spec,
                           WORD x, WORD y, WORD width, WORD height)
{
    o->ob_next = next;
    o->ob_head = head;
    o->ob_tail = tail;
    o->ob_type = type;
    o->ob_flags = flags;
    o->ob_state = state;
    o->ob_spec = spec;
    o->ob_x = x;
    o->ob_y = y;
    o->ob_width = width;
    o->ob_height = height;
}

static void gemread_init_menu(gemread_state_t *state)
{
    OBJECT *t = state->menu;
    gemread_object(&t[gemread_root], NIL, gemread_bar, gemread_popups, G_IBOX,
                   NONE, NORMAL, 0, 0, 0, 0, 0);
    gemread_object(&t[gemread_bar], gemread_popups, gemread_titles,
                   gemread_titles, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1);
    gemread_object(&t[gemread_titles], gemread_bar, gemread_file_title,
                   gemread_file_title, G_IBOX, NONE, NORMAL, 0, 0, 0, 80, 1);
    gemread_object(&t[gemread_popups], gemread_root, gemread_file_box,
                   gemread_file_box, G_IBOX, NONE, NORMAL, 0, 0, 0, 0, 0);
    gemread_object(&t[gemread_file_title], gemread_titles, NIL, NIL, G_TITLE,
                   NONE, NORMAL, (LONG)(intptr_t)" File ", 0, 0, 6, 1);
    gemread_object(&t[gemread_file_box], gemread_popups, gemread_file_open,
                   gemread_file_open, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 16,
                   1);
    gemread_object(&t[gemread_file_open], gemread_file_box, NIL, NIL, G_STRING,
                   LASTOB, NORMAL, (LONG)(intptr_t)"  Open", 0, 0, 16, 1);
    (void)menu_bar(t, 1);
}

static void gemread_layout(const gemread_state_t *state, const GRECT *work,
                           gemread_layout_t *layout)
{
    WORD width = 116;
    memset(layout, 0, sizeof(*layout));
    gemctl_chrome_layout(work, state->char_h, state->box_h, &layout->chrome);
    if (layout->chrome.content.g_w < 360)
        width = (WORD)(layout->chrome.content.g_w / 4);
    if (width < 72)
        width = 72;
    if (width >= layout->chrome.content.g_w)
        width = (WORD)(layout->chrome.content.g_w / 3);
    layout->sidebar = layout->chrome.content;
    layout->sidebar.g_w = width;
    layout->viewport = layout->chrome.content;
    layout->viewport.g_x = (WORD)(layout->sidebar.g_x + width + 1);
    layout->viewport.g_w = (WORD)(layout->chrome.content.g_w - width - 1);
    if (layout->viewport.g_w < 0)
        layout->viewport.g_w = 0;
}

static int gemread_current_layout(const gemread_state_t *state, GRECT *work,
                                  gemread_layout_t *layout)
{
    if (!wind_get(state->handle, WF_WORKXYWH, &work->g_x, &work->g_y,
                  &work->g_w, &work->g_h))
        return 0;
    gemread_layout(state, work, layout);
    return 1;
}

static void gemread_free_pixels(gemread_state_t *state)
{
    free(state->page_bgra);
    state->page_bgra = NULL;
    state->page_width = state->page_height = 0;
    free(state->body_bgra);
    state->body_bgra = NULL;
    state->body_width = state->body_height = 0;
    free(state->mono);
    state->mono = NULL;
    state->mono_stride = 0;
}

static void gemread_close_document(gemread_state_t *state)
{
    gemread_free_pixels(state);
    if (state->page != NULL) {
        fz_drop_page(state->context, state->page);
        state->page = NULL;
    }
    if (state->document != NULL) {
        fz_drop_document(state->context, state->document);
        state->document = NULL;
    }
    state->page_count = state->page_number = state->thumbnail_top = 0;
}

static uint8_t *gemread_pixmap_bgra(const fz_pixmap *pixmap)
{
    uint8_t *pixels;
    int x, y;
    if (pixmap == NULL || pixmap->w <= 0 || pixmap->h <= 0 || pixmap->n < 3)
        return NULL;
    pixels = malloc((size_t)pixmap->w * pixmap->h * 4u);
    if (pixels == NULL)
        return NULL;
    for (y = 0; y < pixmap->h; ++y) {
        const uint8_t *src = pixmap->samples + (size_t)y * pixmap->stride;
        uint8_t *dst = pixels + (size_t)y * pixmap->w * 4u;
        for (x = 0; x < pixmap->w; ++x) {
            dst[x * 4] = src[x * pixmap->n + 2];
            dst[x * 4 + 1] = src[x * pixmap->n + 1];
            dst[x * 4 + 2] = src[x * pixmap->n];
            dst[x * 4 + 3] = 255u;
        }
    }
    return pixels;
}

static float gemread_page_scale(gemread_state_t *state, fz_page *page,
                                int target_width, int target_height,
                                int zoom_percent)
{
    fz_rect b = fz_bound_page(state->context, page);
    float width = b.x1 - b.x0, height = b.y1 - b.y0;
    float sx, sy, scale;
    if (width <= 0.0f || height <= 0.0f)
        return 1.0f;
    sx = (float)target_width / width;
    sy = (float)target_height / height;
    scale = sx < sy ? sx : sy;
    if (scale <= 0.0f)
        scale = 1.0f;
    return scale * (float)zoom_percent / 100.0f;
}

static int gemread_render_current(gemread_state_t *state, const GRECT *viewport)
{
    fz_pixmap *pixmap = NULL;
    uint8_t *pixels = NULL;
    int tw = viewport->g_w - gemread_page_margin * 2;
    int th = viewport->g_h - gemread_page_margin * 2;
    if (state->document == NULL || state->page_count <= 0 ||
        viewport->g_w <= 0 || viewport->g_h <= 0)
        return 0;
    if (tw < 1)
        tw = 1;
    if (th < 1)
        th = 1;
    if (state->page != NULL) {
        fz_drop_page(state->context, state->page);
        state->page = NULL;
    }
    free(state->page_bgra);
    state->page_bgra = NULL;
    state->page_width = state->page_height = 0;
    fz_var(pixmap);
    fz_var(pixels);
    fz_try(state->context)
    {
        float scale;
        state->page =
            fz_load_page(state->context, state->document, state->page_number);
        scale =
            gemread_page_scale(state, state->page, tw, th, state->zoom_percent);
        pixmap = fz_new_pixmap_from_page(state->context, state->page,
                                         fz_scale(scale, scale),
                                         fz_device_rgb(state->context), 0);
        pixels = gemread_pixmap_bgra(pixmap);
        if (pixels == NULL)
            fz_throw(state->context, FZ_ERROR_GENERIC,
                     "document page pixel allocation");
        state->page_bgra = pixels;
        pixels = NULL;
        state->page_width = pixmap->w;
        state->page_height = pixmap->h;
    }
    fz_always(state->context)
    {
        free(pixels);
        fz_drop_pixmap(state->context, pixmap);
    }
    fz_catch(state->context)
    {
        fprintf(stderr, "gemread: cannot render page %d: %s\n",
                state->page_number + 1, fz_caught_message(state->context));
        return 0;
    }
    state->scroll_x = state->scroll_y = 0;
    return 1;
}

static uint8_t *gemread_render_thumbnail(gemread_state_t *state, int number,
                                         int max_width, int max_height,
                                         int *width, int *height)
{
    fz_page *page = NULL;
    fz_pixmap *pixmap = NULL;
    uint8_t *pixels = NULL;
    *width = *height = 0;
    fz_var(page);
    fz_var(pixmap);
    fz_var(pixels);
    fz_try(state->context)
    {
        float scale;
        page = fz_load_page(state->context, state->document, number);
        scale = gemread_page_scale(state, page, max_width, max_height, 100);
        pixmap = fz_new_pixmap_from_page(state->context, page,
                                         fz_scale(scale, scale),
                                         fz_device_rgb(state->context), 0);
        pixels = gemread_pixmap_bgra(pixmap);
        if (pixels == NULL)
            fz_throw(state->context, FZ_ERROR_GENERIC,
                     "thumbnail pixel allocation");
        *width = pixmap->w;
        *height = pixmap->h;
    }
    fz_always(state->context)
    {
        fz_drop_pixmap(state->context, pixmap);
        fz_drop_page(state->context, page);
    }
    fz_catch(state->context)
    {
        free(pixels);
        pixels = NULL;
    }
    return pixels;
}

static void gemread_bgra_fill(uint8_t *pixels, int width, int height,
                              uint8_t blue, uint8_t green, uint8_t red)
{
    size_t i, count = (size_t)width * height;
    for (i = 0; i < count; ++i) {
        pixels[i * 4] = blue;
        pixels[i * 4 + 1] = green;
        pixels[i * 4 + 2] = red;
        pixels[i * 4 + 3] = 255u;
    }
}

static void gemread_bgra_rect(uint8_t *pixels, int width, int height, int x,
                              int y, int rw, int rh, uint8_t blue,
                              uint8_t green, uint8_t red)
{
    int row, col;
    for (row = y; row < y + rh; ++row) {
        if (row < 0 || row >= height)
            continue;
        for (col = x; col < x + rw; ++col) {
            uint8_t *p;
            if (col < 0 || col >= width)
                continue;
            p = pixels + ((size_t)row * width + col) * 4u;
            p[0] = blue;
            p[1] = green;
            p[2] = red;
            p[3] = 255u;
        }
    }
}

static void gemread_bgra_blit(uint8_t *target, int tw, int th, int tx, int ty,
                              const uint8_t *source, int sw, int sh)
{
    int row;
    for (row = 0; row < sh; ++row) {
        int sx = 0, dx = tx, count = sw, dy = ty + row;
        if (dy < 0 || dy >= th)
            continue;
        if (dx < 0) {
            sx = -dx;
            count -= sx;
            dx = 0;
        }
        if (dx + count > tw)
            count = tw - dx;
        if (count > 0)
            memcpy(target + ((size_t)dy * tw + dx) * 4u,
                   source + ((size_t)row * sw + sx) * 4u, (size_t)count * 4u);
    }
}

static int gemread_visible_thumbnails(const gemread_layout_t *layout)
{
    int count = layout->sidebar.g_h / gemread_thumb_slot_height;
    return count > 0 ? count : 1;
}

static void gemread_adjust_thumbnail_top(gemread_state_t *state,
                                         const gemread_layout_t *layout)
{
    int visible = gemread_visible_thumbnails(layout);
    int maximum = state->page_count - visible;
    if (maximum < 0)
        maximum = 0;
    if (state->page_number < state->thumbnail_top)
        state->thumbnail_top = state->page_number;
    if (state->page_number >= state->thumbnail_top + visible)
        state->thumbnail_top = state->page_number - visible + 1;
    if (state->thumbnail_top > maximum)
        state->thumbnail_top = maximum;
    if (state->thumbnail_top < 0)
        state->thumbnail_top = 0;
}

static int gemread_rebuild_body(gemread_state_t *state,
                                const gemread_layout_t *layout)
{
    int bw = layout->chrome.content.g_w, bh = layout->chrome.content.g_h;
    int side = layout->sidebar.g_w;
    int vx = layout->viewport.g_x - layout->chrome.content.g_x;
    int vw = layout->viewport.g_w, vh = layout->viewport.g_h;
    int page_x, page_y, visible, slot;
    uint8_t *pixels;
    if (bw <= 0 || bh <= 0)
        return 0;
    pixels = malloc((size_t)bw * bh * 4u);
    if (pixels == NULL)
        return 0;
    gemread_bgra_fill(pixels, bw, bh, 244u, 244u, 244u);
    gemread_bgra_rect(pixels, bw, bh, 0, 0, side, bh, 220u, 220u, 220u);
    gemread_bgra_rect(pixels, bw, bh, side, 0, 1, bh, 64u, 64u, 64u);
    if (state->page_bgra != NULL) {
        page_x = vx - state->scroll_x;
        page_y = -state->scroll_y;
        if (state->page_width < vw)
            page_x = vx + (vw - state->page_width) / 2;
        if (state->page_height < vh)
            page_y = (vh - state->page_height) / 2;
        gemread_bgra_rect(pixels, bw, bh, page_x - 1, page_y - 1,
                          state->page_width + 2, state->page_height + 2, 72u,
                          72u, 72u);
        gemread_bgra_blit(pixels, bw, bh, page_x, page_y, state->page_bgra,
                          state->page_width, state->page_height);
    }
    gemread_adjust_thumbnail_top(state, layout);
    visible = gemread_visible_thumbnails(layout);
    for (slot = 0; slot < visible; ++slot) {
        int number = state->thumbnail_top + slot;
        int width, height, x,
            y = slot * gemread_thumb_slot_height + gemread_thumb_padding;
        uint8_t *thumbnail;
        if (number >= state->page_count)
            break;
        thumbnail = gemread_render_thumbnail(
            state, number, side - gemread_thumb_padding * 2,
            gemread_thumb_slot_height - gemread_thumb_padding * 2 - 10, &width,
            &height);
        if (thumbnail == NULL)
            continue;
        x = (side - width) / 2;
        if (number == state->page_number)
            gemread_bgra_rect(pixels, bw, bh, x - 3, y - 3, width + 6,
                              height + 6, 40u, 88u, 220u);
        else
            gemread_bgra_rect(pixels, bw, bh, x - 1, y - 1, width + 2,
                              height + 2, 80u, 80u, 80u);
        gemread_bgra_blit(pixels, bw, bh, x, y, thumbnail, width, height);
        free(thumbnail);
    }
    free(state->body_bgra);
    state->body_bgra = pixels;
    state->body_width = bw;
    state->body_height = bh;
    free(state->mono);
    state->mono = NULL;
    state->mono_stride = 0;
    return 1;
}

static int gemread_supported_name(const char *name)
{
    const char *extension;
    static const char *const formats[] = {".pdf",  ".xps", ".oxps", ".epub",
                                          ".mobi", ".fb2", ".cbz",  ".cbt"};
    size_t i;
    if (name == NULL || (extension = strrchr(name, '.')) == NULL ||
        extension == name)
        return 0;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        const char *left = extension, *right = formats[i];
        while (*left != '\0' && *right != '\0') {
            char a =
                (*left >= 'A' && *left <= 'Z') ? (char)(*left + 32) : *left;
            char b =
                (*right >= 'A' && *right <= 'Z') ? (char)(*right + 32) : *right;
            if (a != b)
                break;
            ++left;
            ++right;
        }
        if (*left == '\0' && *right == '\0')
            return 1;
    }
    return 0;
}

static int gemread_open_document(gemread_state_t *state, const char *path)
{
    fz_document *document = NULL;
    GRECT work;
    gemread_layout_t layout;
    if (path == NULL || !gemread_current_layout(state, &work, &layout))
        return 0;
    fz_var(document);
    fz_try(state->context)
    {
        document = fz_open_document(state->context, path);
        if (fz_is_document_reflowable(state->context, document))
            fz_layout_document(state->context, document,
                               (float)layout.viewport.g_w,
                               (float)layout.viewport.g_h, 12.0f);
        if (fz_count_pages(state->context, document) <= 0)
            fz_throw(state->context, FZ_ERROR_GENERIC,
                     "document contains no pages");
    }
    fz_catch(state->context)
    {
        fprintf(stderr, "gemread: cannot open %s: %s\n", path,
                fz_caught_message(state->context));
        fz_drop_document(state->context, document);
        return 0;
    }
    gemread_close_document(state);
    state->document = document;
    state->page_count = fz_count_pages(state->context, document);
    state->zoom_percent = 100;
    (void)snprintf(state->path, sizeof(state->path), "%s", path);
    if (!gemread_render_current(state, &layout.viewport)) {
        gemread_close_document(state);
        return 0;
    }
    return gemread_rebuild_body(state, &layout);
}

static void gemread_clamp_scroll(gemread_state_t *state, const GRECT *viewport)
{
    int horizontal = state->page_width - viewport->g_w;
    int vertical = state->page_height - viewport->g_h;
    if (horizontal < 0)
        horizontal = 0;
    if (vertical < 0)
        vertical = 0;
    if (state->scroll_x < 0)
        state->scroll_x = 0;
    if (state->scroll_y < 0)
        state->scroll_y = 0;
    if (state->scroll_x > horizontal)
        state->scroll_x = horizontal;
    if (state->scroll_y > vertical)
        state->scroll_y = vertical;
}

static WORD gemread_slider_size(int viewport, int content)
{
    LONG value;
    if (viewport <= 0 || content <= viewport)
        return 1000;
    value = (LONG)viewport * 1000L / content;
    return (WORD)(value < 1 ? 1 : value);
}

static WORD gemread_slider_position(int offset, int viewport, int content)
{
    int limit = content - viewport;
    if (limit <= 0 || offset <= 0)
        return 0;
    return (WORD)((LONG)offset * 1000L / limit);
}

static void gemread_update_sliders(gemread_state_t *state,
                                   const GRECT *viewport)
{
    gemread_clamp_scroll(state, viewport);
    (void)wind_set(state->handle, WF_VSLSIZ,
                   gemread_slider_size(viewport->g_h, state->page_height), 0, 0,
                   0);
    (void)wind_set(state->handle, WF_HSLSIZ,
                   gemread_slider_size(viewport->g_w, state->page_width), 0, 0,
                   0);
    (void)wind_set(state->handle, WF_VSLIDE,
                   gemread_slider_position(state->scroll_y, viewport->g_h,
                                           state->page_height),
                   0, 0, 0);
    (void)wind_set(state->handle, WF_HSLIDE,
                   gemread_slider_position(state->scroll_x, viewport->g_w,
                                           state->page_width),
                   0, 0, 0);
}

#if !defined(GEMREAD_OVERLAY) || !GEMREAD_OVERLAY
static void gemread_ensure_mono(gemread_state_t *state)
{
    int x, y;
    if (state->body_bgra == NULL || state->mono != NULL)
        return;
    state->mono_stride = ((state->body_width + 15) / 16) * 2;
    state->mono = calloc((size_t)state->mono_stride, state->body_height);
    if (state->mono == NULL)
        return;
    for (y = 0; y < state->body_height; ++y) {
        UWORD *row =
            (UWORD *)(void *)(state->mono + (size_t)y * state->mono_stride);
        for (x = 0; x < state->body_width; ++x) {
            const uint8_t *p =
                state->body_bgra + ((size_t)y * state->body_width + x) * 4u;
            unsigned gray = (unsigned)p[2] * 30u + (unsigned)p[1] * 59u +
                            (unsigned)p[0] * 11u;
            if (gray < 12800u)
                row[x / 16] |= (UWORD)(0x8000u >> (x & 15));
        }
    }
}

static void gemread_draw_body_mono(gemread_state_t *state,
                                   const gemread_layout_t *layout)
{
    MFDB source;
    WORD pxy[8];
    gemread_ensure_mono(state);
    if (state->mono == NULL)
        return;
    memset(&source, 0, sizeof(source));
    source.fd_addr = state->mono;
    source.fd_w = (WORD)state->body_width;
    source.fd_h = (WORD)state->body_height;
    source.fd_wdwidth = (WORD)(state->mono_stride / 2);
    source.fd_stand = 1;
    source.fd_nplanes = 1;
    pxy[0] = pxy[1] = 0;
    pxy[2] = (WORD)(state->body_width - 1);
    pxy[3] = (WORD)(state->body_height - 1);
    pxy[4] = layout->chrome.content.g_x;
    pxy[5] = layout->chrome.content.g_y;
    pxy[6] = (WORD)(pxy[4] + state->body_width - 1);
    pxy[7] = (WORD)(pxy[5] + state->body_height - 1);
    (void)vswr_mode(state->vdi_handle, MD_REPLACE);
    vro_cpyfm(state->vdi_handle, MD_REPLACE, pxy, &source, NULL);
}
#endif

static void gemread_draw(gemread_state_t *state, const GRECT *dirty)
{
    GRECT work, visible;
    gemread_layout_t layout;
    char status[160];
    if (!gemread_current_layout(state, &work, &layout))
        return;
    if (state->body_bgra == NULL ||
        state->body_width != layout.chrome.content.g_w ||
        state->body_height != layout.chrome.content.g_h)
        (void)gemread_rebuild_body(state, &layout);
    if (state->document != NULL)
        (void)snprintf(status, sizeof(status), "Page %d of %d   Zoom %d%%",
                       state->page_number + 1, state->page_count,
                       state->zoom_percent);
    else
        (void)snprintf(status, sizeof(status), "Ready. Open a document.");
    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
             &visible.g_w, &visible.g_h);
    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD clip[4], left = visible.g_x, top = visible.g_y;
        WORD right = (WORD)(visible.g_x + visible.g_w - 1);
        WORD bottom = (WORD)(visible.g_y + visible.g_h - 1);
        if (dirty != NULL) {
            WORD dr = (WORD)(dirty->g_x + dirty->g_w - 1);
            WORD db = (WORD)(dirty->g_y + dirty->g_h - 1);
            if (dirty->g_x > left)
                left = dirty->g_x;
            if (dirty->g_y > top)
                top = dirty->g_y;
            if (dr < right)
                right = dr;
            if (db < bottom)
                bottom = db;
        }
        if (right >= left && bottom >= top) {
            GRECT slot;
            clip[0] = left;
            clip[1] = top;
            clip[2] = right;
            clip[3] = bottom;
            vs_clip(state->vdi_handle, 1, clip);
            gemctl_toolbar_draw(state->vdi_handle, &layout.chrome.toolbar);
            slot = layout.chrome.toolbar;
            slot.g_w = slot.g_h;
            gemctl_toolbar_zoom_icon(state->vdi_handle, &slot, 0);
            gemctl_toolbar_separator(state->vdi_handle,
                                     (WORD)(slot.g_x + slot.g_w - 1), slot.g_y,
                                     (WORD)(slot.g_y + slot.g_h - 1));
            slot.g_x = (WORD)(slot.g_x + slot.g_w);
            gemctl_toolbar_zoom_icon(state->vdi_handle, &slot, 1);
            gemctl_toolbar_separator(state->vdi_handle,
                                     (WORD)(slot.g_x + slot.g_w - 1), slot.g_y,
                                     (WORD)(slot.g_y + slot.g_h - 1));
            gemctl_status_draw(state->vdi_handle, &layout.chrome.status,
                               status);
#if !defined(GEMREAD_OVERLAY) || !GEMREAD_OVERLAY
            gemread_draw_body_mono(state, &layout);
#endif
            vs_clip(state->vdi_handle, 0, clip);
        }
        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                 &visible.g_w, &visible.g_h);
    }
    wind_update(END_UPDATE);
#if defined(GEMREAD_OVERLAY) && GEMREAD_OVERLAY
    if (state->overlay != 0 && state->body_bgra != NULL)
        (void)gem_overlay_update(
            state->overlay, (uint16_t)(layout.chrome.content.g_x - work.g_x),
            (uint16_t)(layout.chrome.content.g_y - work.g_y),
            (uint16_t)state->body_width, (uint16_t)state->body_height,
            state->body_bgra, (uint16_t)(state->body_width * 4));
#endif
    gemread_update_sliders(state, &layout.viewport);
}

static void gemread_open(gemread_state_t *state)
{
    char directory[PATH_MAX], filename[256] = "", path[PATH_MAX];
    char *slash, *backslash;
    WORD button = 0;
    size_t length;
    if (!gem_os_getcwd(directory, sizeof(directory)) ||
        strlen(directory) + 4u >= sizeof(directory))
        strcpy(directory, "/*.*");
    else {
        length = strlen(directory);
        strcpy(directory + length,
               length > 0u && directory[length - 1u] == '/' ? "*.*" : "/*.*");
    }
    if (!fsel_exinput(directory, filename, &button, "Open Document") ||
        button == 0 || filename[0] == '\0' || !gemread_supported_name(filename))
        return;
    slash = strrchr(directory, '/');
    backslash = strrchr(directory, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash))
        slash = backslash;
    length = slash != NULL ? (size_t)(slash - directory + 1) : 0u;
    if (length + strlen(filename) >= sizeof(path))
        return;
    memcpy(path, directory, length);
    memcpy(path + length, filename, strlen(filename) + 1u);
    if (gemread_open_document(state, path)) {
        (void)wind_set_str(state->handle, WF_NAME, filename);
        gemread_draw(state, NULL);
    }
}

static void gemread_rerender(gemread_state_t *state)
{
    GRECT work;
    gemread_layout_t layout;
    if (!gemread_current_layout(state, &work, &layout))
        return;
    if (state->document != NULL)
        (void)gemread_render_current(state, &layout.viewport);
    (void)gemread_rebuild_body(state, &layout);
    gemread_draw(state, NULL);
}

static void gemread_select_page(gemread_state_t *state, int page)
{
    GRECT work;
    gemread_layout_t layout;
    int visible;

    if (state->document == NULL || page < 0 || page >= state->page_count ||
        page == state->page_number)
        return;
    if (gemread_current_layout(state, &work, &layout)) {
        visible = gemread_visible_thumbnails(&layout);
        if (page == state->thumbnail_top + visible - 1 &&
            page + 1 < state->page_count)
            ++state->thumbnail_top;
        else if (page == state->thumbnail_top && page > 0)
            --state->thumbnail_top;
    }
    state->page_number = page;
    gemread_rerender(state);
}

static void gemread_zoom(gemread_state_t *state, int direction)
{
    int zoom;
    if (state->document == NULL)
        return;
    zoom = state->zoom_percent + direction * gemread_zoom_step;
    if (zoom < gemread_zoom_min)
        zoom = gemread_zoom_min;
    if (zoom > gemread_zoom_max)
        zoom = gemread_zoom_max;
    if (zoom == state->zoom_percent)
        return;
    state->zoom_percent = zoom;
    gemread_rerender(state);
}

static void gemread_click(gemread_state_t *state, WORD x, WORD y)
{
    GRECT work;
    gemread_layout_t layout;
    if (!gemread_current_layout(state, &work, &layout))
        return;
    if (y >= layout.chrome.toolbar.g_y &&
        y < layout.chrome.toolbar.g_y + layout.chrome.toolbar.g_h) {
        int slot = (x - layout.chrome.toolbar.g_x) / layout.chrome.toolbar.g_h;
        if (slot == 0)
            gemread_zoom(state, -1);
        else if (slot == 1)
            gemread_zoom(state, 1);
    } else if (x >= layout.sidebar.g_x &&
               x < layout.sidebar.g_x + layout.sidebar.g_w &&
               y >= layout.sidebar.g_y &&
               y < layout.sidebar.g_y + layout.sidebar.g_h) {
        int slot = (y - layout.sidebar.g_y) / gemread_thumb_slot_height;
        gemread_select_page(state, state->thumbnail_top + slot);
    }
}

static void gemread_scroll(gemread_state_t *state, int vertical, WORD value)
{
    GRECT work;
    gemread_layout_t layout;
    int limit;
    if (!gemread_current_layout(state, &work, &layout))
        return;
    limit = vertical ? state->page_height - layout.viewport.g_h
                     : state->page_width - layout.viewport.g_w;
    if (limit < 0)
        limit = 0;
    if (vertical)
        state->scroll_y = (int)((LONG)value * limit / 1000L);
    else
        state->scroll_x = (int)((LONG)value * limit / 1000L);
    (void)gemread_rebuild_body(state, &layout);
    gemread_draw(state, NULL);
}

static void gemread_arrow(gemread_state_t *state, WORD arrow)
{
    GRECT work;
    gemread_layout_t layout;
    int *position, limit, step;
    if (!gemread_current_layout(state, &work, &layout))
        return;
    if (arrow == WA_UPLINE || arrow == WA_DNLINE || arrow == WA_UPPAGE ||
        arrow == WA_DNPAGE) {
        position = &state->scroll_y;
        limit = state->page_height - layout.viewport.g_h;
        step = (arrow == WA_UPPAGE || arrow == WA_DNPAGE) ? layout.viewport.g_h
                                                          : 24;
        if (arrow == WA_UPLINE || arrow == WA_UPPAGE)
            step = -step;
    } else {
        position = &state->scroll_x;
        limit = state->page_width - layout.viewport.g_w;
        step = (arrow == WA_LFPAGE || arrow == WA_RTPAGE) ? layout.viewport.g_w
                                                          : 24;
        if (arrow == WA_LFLINE || arrow == WA_LFPAGE)
            step = -step;
    }
    if (limit < 0)
        limit = 0;
    *position += step;
    if (*position < 0)
        *position = 0;
    if (*position > limit)
        *position = limit;
    (void)gemread_rebuild_body(state, &layout);
    gemread_draw(state, NULL);
}

static void gemread_toggle_full(gemread_state_t *state)
{
    GRECT full, current;
    if (!wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h) ||
        !wind_get(state->handle, WF_WXYWH, &current.g_x, &current.g_y,
                  &current.g_w, &current.g_h))
        return;
    if (!state->full_open) {
        state->normal_rect = current;
        (void)wind_set(state->handle, WF_WXYWH, full.g_x, full.g_y, full.g_w,
                       full.g_h);
        state->full_open = 1;
    } else {
        (void)wind_set(state->handle, WF_WXYWH, state->normal_rect.g_x,
                       state->normal_rect.g_y, state->normal_rect.g_w,
                       state->normal_rect.g_h);
        state->full_open = 0;
    }
    gemread_rerender(state);
}

static int gemread_main(int argc, char **argv)
{
    gemread_state_t state;
    WORD msg[8] = {0}, mouse_x, mouse_y, buttons, key_state, key_code;
    WORD button_return, event_flags, outer_x, outer_y, outer_w, outer_h;
    const UWORD kind = NAME | MOVER | CLOSER | FULLER | SIZER | UPARROW |
                       DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE;
    memset(&state, 0, sizeof(state));
    state.zoom_percent = 100;
    if (!gem_os_init() || appl_init() < 0)
        return 1;
    state.vdi_handle =
        graf_handle(&state.char_w, &state.char_h, &state.box_w, &state.box_h);
    state.context = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (state.context == NULL) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }
    fz_register_document_handlers(state.context);
    state.handle = wind_create(kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        fz_drop_context(state.context);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }
    (void)wind_set_str(state.handle, WF_NAME, "Gemread");
    gemread_init_menu(&state);
    if (!wind_get(0, WF_WORKXYWH, &outer_x, &outer_y, &outer_w, &outer_h)) {
        outer_x = 0;
        outer_y = 20;
        outer_w = 640;
        outer_h = 400;
    }
    if (outer_w > 40)
        outer_w = (WORD)(outer_w - 40);
    if (outer_h > 20)
        outer_h = (WORD)(outer_h - 20);
    if (outer_w > 680)
        outer_w = 680;
    if (outer_h > 560)
        outer_h = 560;
    state.normal_rect.g_x = (WORD)(outer_x + 20);
    state.normal_rect.g_y = (WORD)(outer_y + 10);
    state.normal_rect.g_w = outer_w;
    state.normal_rect.g_h = outer_h;
    (void)wind_open(state.handle, state.normal_rect.g_x, state.normal_rect.g_y,
                    state.normal_rect.g_w, state.normal_rect.g_h);
#if defined(GEMREAD_OVERLAY) && GEMREAD_OVERLAY
    {
        GRECT work;
        gemread_layout_t layout;
        if (gemread_current_layout(&state, &work, &layout))
            state.overlay = gem_overlay_create(state.handle, (uint16_t)work.g_w,
                                               (uint16_t)work.g_h);
    }
#endif
    if (argc > 1 && gemread_supported_name(argv[1])) {
        if (gemread_open_document(&state, argv[1]))
            (void)wind_set_str(state.handle, WF_NAME, argv[1]);
    } else {
        GRECT work;
        gemread_layout_t layout;
        if (gemread_current_layout(&state, &work, &layout))
            (void)gemread_rebuild_body(&state, &layout);
    }
    (void)graf_mouse(M_ON, NULL);
    gemread_draw(&state, NULL);
    for (;;) {
        event_flags =
            evnt_multi((UWORD)(MU_MESAG | MU_KEYBD | MU_BUTTON), 1, 1, 1, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, msg, 100, 0, &mouse_x, &mouse_y,
                       &buttons, &key_state, &key_code, &button_return);
        if ((event_flags & MU_MESAG) != 0) {
            if (msg[0] == MN_SELECTED && msg[4] == gemread_file_open) {
                gemread_open(&state);
                menu_tnormal(state.menu, msg[3], 1);
            } else if (msg[0] == WM_CLOSED || msg[0] == AP_TERM)
                break;
            else if (msg[0] == WM_REDRAW) {
                GRECT dirty = {msg[4], msg[5], msg[6], msg[7]};
                gemread_draw(&state, &dirty);
            } else if (msg[0] == WM_VSLID)
                gemread_scroll(&state, 1, msg[4]);
            else if (msg[0] == WM_HSLID)
                gemread_scroll(&state, 0, msg[4]);
            else if (msg[0] == WM_ARROWED)
                gemread_arrow(&state, msg[4]);
            else if (msg[0] == WM_SIZED || msg[0] == WM_MOVED) {
                (void)wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                               msg[7]);
                if (!state.full_open) {
                    state.normal_rect.g_x = msg[4];
                    state.normal_rect.g_y = msg[5];
                    state.normal_rect.g_w = msg[6];
                    state.normal_rect.g_h = msg[7];
                }
#if defined(GEMREAD_OVERLAY) && GEMREAD_OVERLAY
                if (state.overlay != 0) {
                    GRECT work;
                    gemread_layout_t layout;
                    if (gemread_current_layout(&state, &work, &layout)) {
                        if (msg[0] == WM_MOVED)
                            (void)gem_overlay_map(state.overlay, state.handle);
                        else
                            (void)gem_overlay_resize(state.overlay,
                                                     (uint16_t)work.g_w,
                                                     (uint16_t)work.g_h);
                    }
                }
#endif
                if (msg[0] == WM_SIZED)
                    gemread_rerender(&state);
                else
                    gemread_draw(&state, NULL);
            } else if (msg[0] == WM_FULLED)
                gemread_toggle_full(&state);
            else if (msg[0] == WM_TOPPED)
                (void)wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
        }
        if ((event_flags & MU_BUTTON) != 0)
            gemread_click(&state, mouse_x, mouse_y);
        if ((event_flags & MU_KEYBD) != 0) {
            unsigned scan = ((unsigned)key_code >> 8) & 0xffu;
            unsigned ascii = (unsigned)key_code & 0xffu;
            if (ascii == 27u)
                break;
            if (scan == 75u || scan == 82u)
                gemread_select_page(&state, state.page_number - 1);
            else if (scan == 78u || scan == 81u)
                gemread_select_page(&state, state.page_number + 1);
        }
    }
    menu_bar(state.menu, 0);
#if defined(GEMREAD_OVERLAY) && GEMREAD_OVERLAY
    if (state.overlay != 0)
        (void)gem_overlay_destroy(state.overlay);
#endif
    wind_close(state.handle);
    wind_delete(state.handle);
    gemread_close_document(&state);
    fz_drop_context(state.context);
    appl_exit();
    gem_os_shutdown();
    return 0;
}

int main(int argc, char **argv)
{
    return gemread_main(argc, argv);
}

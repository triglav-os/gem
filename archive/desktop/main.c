/*
 * Launches the hosted GEM desktop session used by the current Linux
 * port. The launcher now describes its user interface through AES
 * object trees and window state so the session exercises GEM-style
 * structures instead of painting the full scene directly with ad hoc
 * VDI calls.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/portab.h>
#include <gem/vdi.h>

#include "platform/hid.h"
#include "platform/os.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    desktop_menu_root = 0,
    desktop_menu_desktop = 1,
    desktop_menu_file = 2,
    desktop_menu_view = 3,
    desktop_menu_help = 4
};

enum {
    desktop_root = 0,
    desktop_icon_trash = 1,
    desktop_icon_files = 2,
    desktop_icon_tools = 3,
    desktop_status_box = 4,
    desktop_status_text = 5
};

enum {
    panel_root = 0,
    panel_text_line1 = 1,
    panel_text_line2 = 2,
    panel_text_line3 = 3
};

enum {
    desktop_menu_height = 18,
    desktop_status_height = 18,
    desktop_icon_width = 56,
    desktop_icon_height = 48,
    panel_width = 240,
    panel_height = 110
};

static OBJECT g_menu_tree[5];
static OBJECT g_desktop_tree[6];
static OBJECT g_panel_tree[4];
static char g_status_text[64];
static char g_status_message[64];
static char g_panel_title[32];
static char g_panel_line1[64];
static char g_panel_line2[64];
static char g_panel_line3[64];
static WORD g_panel_handle;
static WORD g_panel_open;
static WORD g_menu_popup_open;
static WORD g_menu_popup_title;
static WORD g_dragging_panel;
static WORD g_drag_offset_x;
static WORD g_drag_offset_y;

static const char *g_desktop_menu_items[] = {
    "ABOUT DESKTOP",
    "RESET DESKTOP",
    "CLOSE WINDOW"
};

static const char *g_file_menu_items[] = {
    "OPEN FILES",
    "OPEN TOOLS",
    "OPEN TRASH"
};

static const char *g_view_menu_items[] = {
    "REFRESH",
    "CLEAR STATUS",
    "CLOSE MENU"
};

static const char *g_help_menu_items[] = {
    "ABOUT GEM",
    "KEYS",
    "ABOUT RASTA"
};

static int desktop_rects_intersect(WORD ax,
                                   WORD ay,
                                   WORD aw,
                                   WORD ah,
                                   WORD bx,
                                   WORD by,
                                   WORD bw,
                                   WORD bh);
static int desktop_point_in_panel_close(WORD x, WORD y);

static WORD desktop_key_to_ascii(uint16_t key)
{
    return (WORD) (key & 0xffu);
}

static void desktop_init_object(OBJECT *object,
                                WORD next,
                                WORD head,
                                WORD tail,
                                UWORD type,
                                UWORD flags,
                                UWORD state,
                                LONG spec,
                                WORD x,
                                WORD y,
                                WORD w,
                                WORD h)
{
    object->ob_next = next;
    object->ob_head = head;
    object->ob_tail = tail;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = w;
    object->ob_height = h;
}

static void desktop_init_menu_tree(WORD screen_width)
{
    desktop_init_object(&g_menu_tree[desktop_menu_root],
        NIL, desktop_menu_desktop, desktop_menu_help,
        G_BOX, SELECTED, SELECTED, 0L,
        0, 0, screen_width, desktop_menu_height);
    desktop_init_object(&g_menu_tree[desktop_menu_desktop],
        desktop_menu_file, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "DESKTOP",
        10, 2, 64, 12);
    desktop_init_object(&g_menu_tree[desktop_menu_file],
        desktop_menu_view, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "FILE",
        86, 2, 36, 12);
    desktop_init_object(&g_menu_tree[desktop_menu_view],
        desktop_menu_help, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "VIEW",
        126, 2, 36, 12);
    desktop_init_object(&g_menu_tree[desktop_menu_help],
        desktop_menu_root, NIL, NIL,
        G_TITLE, LASTOB, NORMAL, (LONG) (intptr_t) "HELP",
        168, 2, 36, 12);
}

static void desktop_init_desktop_tree(WORD x, WORD y, WORD w, WORD h)
{
    WORD status_y = (WORD) (h - desktop_status_height);

    desktop_init_object(&g_desktop_tree[desktop_root],
        NIL, desktop_icon_trash, desktop_status_text,
        G_BOX, NONE, NORMAL, 0L,
        x, y, w, h);
    desktop_init_object(&g_desktop_tree[desktop_icon_trash],
        desktop_icon_files, NIL, NIL,
        G_BUTTON, SELECTABLE, NORMAL, (LONG) (intptr_t) "TRASH",
        28, 22, desktop_icon_width, desktop_icon_height);
    desktop_init_object(&g_desktop_tree[desktop_icon_files],
        desktop_icon_tools, NIL, NIL,
        G_BUTTON, SELECTABLE, NORMAL, (LONG) (intptr_t) "FILES",
        104, 22, desktop_icon_width, desktop_icon_height);
    desktop_init_object(&g_desktop_tree[desktop_icon_tools],
        desktop_status_box, NIL, NIL,
        G_BUTTON, SELECTABLE, NORMAL, (LONG) (intptr_t) "TOOLS",
        180, 22, desktop_icon_width, desktop_icon_height);
    desktop_init_object(&g_desktop_tree[desktop_status_box],
        desktop_status_text, NIL, NIL,
        G_BOX, NONE, NORMAL, 0L,
        0, status_y, w, desktop_status_height);
    desktop_init_object(&g_desktop_tree[desktop_status_text],
        desktop_root, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) g_status_text,
        10, (WORD) (status_y + 3), w - 20, 12);
}

static void desktop_set_status_message(const char *message)
{
    if (message != NULL) {
        strncpy(g_status_message, message, sizeof(g_status_message) - 1u);
        g_status_message[sizeof(g_status_message) - 1u] = '\0';
    } else {
        g_status_message[0] = '\0';
    }
}

static void desktop_update_status_text(WORD mouse_x, WORD mouse_y)
{
    if (g_status_message[0] != '\0') {
        int prefix_max = (int) sizeof(g_status_text) - 24;

        (void) snprintf(g_status_text, sizeof(g_status_text),
            "%.*s  Mouse %d,%d", prefix_max, g_status_message,
            (int) mouse_x, (int) mouse_y);
    } else {
        (void) snprintf(g_status_text, sizeof(g_status_text),
            "Esc/q quits  Mouse %d,%d", (int) mouse_x, (int) mouse_y);
    }
}

static void desktop_select_icon(WORD object)
{
    g_desktop_tree[desktop_icon_trash].ob_state = NORMAL;
    g_desktop_tree[desktop_icon_files].ob_state = NORMAL;
    g_desktop_tree[desktop_icon_tools].ob_state = NORMAL;

    if (object == desktop_icon_trash || object == desktop_icon_files ||
        object == desktop_icon_tools) {
        g_desktop_tree[object].ob_state = SELECTED;
    }
}

static const char *desktop_icon_message(WORD object)
{
    switch (object) {
    case desktop_icon_trash:
        return "Selected Trash";
    case desktop_icon_files:
        return "Selected Files";
    case desktop_icon_tools:
        return "Selected Tools";
    default:
        return "";
    }
}

static void desktop_init_panel_tree(WORD work_x, WORD work_y, WORD work_w, WORD work_h)
{
    desktop_init_object(&g_panel_tree[panel_root],
        NIL, panel_text_line1, panel_text_line3,
        G_BOX, NONE, NORMAL, 0L,
        work_x, work_y, work_w, work_h);
    desktop_init_object(&g_panel_tree[panel_text_line1],
        panel_text_line2, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) g_panel_line1,
        12, 12, (WORD) (work_w - 24), 12);
    desktop_init_object(&g_panel_tree[panel_text_line2],
        panel_text_line3, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) g_panel_line2,
        12, 30, (WORD) (work_w - 24), 12);
    desktop_init_object(&g_panel_tree[panel_text_line3],
        panel_root, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) g_panel_line3,
        12, 48, (WORD) (work_w - 24), 12);
}

static void desktop_get_panel_outer_rect(WORD *x, WORD *y, WORD *w, WORD *h)
{
    WORD outer_x = 0;
    WORD outer_y = 0;
    WORD outer_w = 0;
    WORD outer_h = 0;

    if (g_panel_open != 0) {
        (void) wind_get(g_panel_handle, WF_WXYWH,
            &outer_x, &outer_y, &outer_w, &outer_h);
    }
    if (x != NULL) {
        *x = outer_x;
    }
    if (y != NULL) {
        *y = outer_y;
    }
    if (w != NULL) {
        *w = outer_w;
    }
    if (h != NULL) {
        *h = outer_h;
    }
}

static void desktop_get_menu_popup_items(WORD title,
                                         const char ***items,
                                         WORD *count)
{
    switch (title) {
    case desktop_menu_desktop:
        *items = g_desktop_menu_items;
        *count = 3;
        break;
    case desktop_menu_file:
        *items = g_file_menu_items;
        *count = 3;
        break;
    case desktop_menu_view:
        *items = g_view_menu_items;
        *count = 3;
        break;
    case desktop_menu_help:
        *items = g_help_menu_items;
        *count = 3;
        break;
    default:
        *items = NULL;
        *count = 0;
        break;
    }
}

static void desktop_get_menu_popup_rect(WORD *x, WORD *y, WORD *w, WORD *h)
{
    WORD title_x = 0;
    WORD title_y = 0;
    WORD title_h = 0;
    const char **items = NULL;
    WORD count = 0;
    WORD popup_w = 112;

    if (g_menu_popup_open == 0) {
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        if (w != NULL) {
            *w = 0;
        }
        if (h != NULL) {
            *h = 0;
        }
        return;
    }

    desktop_get_menu_popup_items(g_menu_popup_title, &items, &count);
    objc_offset(g_menu_tree, g_menu_popup_title, &title_x, &title_y);
    title_h = g_menu_tree[g_menu_popup_title].ob_height;
    if (g_menu_popup_title == desktop_menu_file) {
        popup_w = 104;
    } else if (g_menu_popup_title == desktop_menu_view) {
        popup_w = 100;
    }

    if (x != NULL) {
        *x = title_x;
    }
    if (y != NULL) {
        *y = (WORD) (title_y + title_h + 2);
    }
    if (w != NULL) {
        *w = popup_w;
    }
    if (h != NULL) {
        *h = (WORD) (count * 16 + 4);
    }
}

static void desktop_close_menu_popup(void)
{
    if (g_menu_popup_open == 0) {
        return;
    }

    g_menu_tree[g_menu_popup_title].ob_state = NORMAL;
    g_menu_popup_open = 0;
    g_menu_popup_title = NIL;
}

static void desktop_open_menu_popup(WORD title)
{
    desktop_close_menu_popup();
    g_menu_popup_open = 1;
    g_menu_popup_title = title;
    g_menu_tree[title].ob_state = SELECTED;
}

static WORD desktop_menu_popup_hit(WORD x, WORD y)
{
    WORD popup_x;
    WORD popup_y;
    WORD popup_w;
    WORD popup_h;
    const char **items = NULL;
    WORD count = 0;
    WORD index;

    if (g_menu_popup_open == 0) {
        return NIL;
    }

    desktop_get_menu_popup_items(g_menu_popup_title, &items, &count);
    desktop_get_menu_popup_rect(&popup_x, &popup_y, &popup_w, &popup_h);
    if (!desktop_rects_intersect(x, y, 1, 1, popup_x, popup_y, popup_w, popup_h)) {
        return NIL;
    }

    index = (WORD) ((y - popup_y - 2) / 16);
    if (index < 0 || index >= count) {
        return NIL;
    }
    return index;
}

static int desktop_point_in_panel_title(WORD x, WORD y)
{
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;

    if (g_panel_open == 0) {
        return 0;
    }

    desktop_get_panel_outer_rect(&outer_x, &outer_y, &outer_w, &outer_h);
    (void) outer_h;
    return x >= outer_x && x < outer_x + outer_w &&
        y >= outer_y && y <= outer_y + 17 &&
        !desktop_point_in_panel_close(x, y);
}

static int desktop_rects_intersect(WORD ax,
                                   WORD ay,
                                   WORD aw,
                                   WORD ah,
                                   WORD bx,
                                   WORD by,
                                   WORD bw,
                                   WORD bh)
{
    return ax < bx + bw && bx < ax + aw &&
        ay < by + bh && by < ay + ah;
}

static void desktop_draw_panel(WORD clip_x,
                               WORD clip_y,
                               WORD clip_w,
                               WORD clip_h)
{
    VDI_HANDLE handle;
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;
    WORD work_x;
    WORD work_y;
    WORD work_w;
    WORD work_h;
    WORD title_bar[4];
    WORD close_box[4];
    WORD clip[4];

    if (g_panel_open == 0) {
        return;
    }

    desktop_get_panel_outer_rect(&outer_x, &outer_y, &outer_w, &outer_h);
    if (!desktop_rects_intersect(outer_x, outer_y, outer_w, outer_h,
            clip_x, clip_y, clip_w, clip_h)) {
        return;
    }

    (void) wind_get(g_panel_handle, WF_CXYWH,
        &work_x, &work_y, &work_w, &work_h);

    title_bar[0] = outer_x;
    title_bar[1] = outer_y;
    title_bar[2] = (WORD) (outer_x + outer_w - 1);
    title_bar[3] = (WORD) (outer_y + 17);
    close_box[0] = (WORD) (outer_x + 4);
    close_box[1] = (WORD) (outer_y + 3);
    close_box[2] = (WORD) (outer_x + 16);
    close_box[3] = (WORD) (outer_y + 15);
    clip[0] = clip_x;
    clip[1] = clip_y;
    clip[2] = (WORD) (clip_x + clip_w - 1);
    clip[3] = (WORD) (clip_y + clip_h - 1);

    desktop_init_panel_tree(work_x, work_y, work_w, work_h);
    handle = graf_handle(NULL, NULL, NULL, NULL);
    vs_clip(handle, 1, clip);
    objc_draw(g_panel_tree, ROOT, MAX_DEPTH,
        clip_x, clip_y, clip_w, clip_h);
    vsf_color(handle, 1);
    v_bar(handle, title_bar);
    vsl_color(handle, 1);
    v_rbox(handle, title_bar);
    vsf_color(handle, 0);
    v_bar(handle, close_box);
    vsl_color(handle, 1);
    v_rbox(handle, close_box);
    vst_color(handle, 1);
    v_gtext(handle,
        (WORD) (outer_x + 24), (WORD) (outer_y + 12),
        (CONST BYTE *) g_panel_title);
    v_gtext(handle,
        (WORD) (outer_x + 8), (WORD) (outer_y + 12),
        (CONST BYTE *) "X");
    vs_clip(handle, 0, clip);
}

static void desktop_draw_menu_popup(WORD clip_x,
                                    WORD clip_y,
                                    WORD clip_w,
                                    WORD clip_h)
{
    VDI_HANDLE handle;
    WORD popup_x;
    WORD popup_y;
    WORD popup_w;
    WORD popup_h;
    WORD rect[4];
    WORD clip[4];
    const char **items = NULL;
    WORD count = 0;
    WORD i;

    if (g_menu_popup_open == 0) {
        return;
    }

    desktop_get_menu_popup_items(g_menu_popup_title, &items, &count);
    desktop_get_menu_popup_rect(&popup_x, &popup_y, &popup_w, &popup_h);
    if (!desktop_rects_intersect(popup_x, popup_y, popup_w, popup_h,
            clip_x, clip_y, clip_w, clip_h)) {
        return;
    }

    handle = graf_handle(NULL, NULL, NULL, NULL);
    clip[0] = clip_x;
    clip[1] = clip_y;
    clip[2] = (WORD) (clip_x + clip_w - 1);
    clip[3] = (WORD) (clip_y + clip_h - 1);
    rect[0] = popup_x;
    rect[1] = popup_y;
    rect[2] = (WORD) (popup_x + popup_w - 1);
    rect[3] = (WORD) (popup_y + popup_h - 1);

    vs_clip(handle, 1, clip);
    vsf_color(handle, 1);
    v_bar(handle, rect);
    vsl_color(handle, 1);
    v_rbox(handle, rect);
    vst_color(handle, 0);
    for (i = 0; i < count; ++i) {
        v_gtext(handle, (WORD) (popup_x + 8), (WORD) (popup_y + 12 + i * 16),
            (CONST BYTE *) items[i]);
    }
    vs_clip(handle, 0, clip);
}

static void desktop_open_panel(WORD screen_width,
                               WORD screen_height,
                               WORD x,
                               WORD y,
                               const char *title,
                               const char *line1,
                               const char *line2,
                               const char *line3)
{
    if (x < 12) {
        x = 12;
    }
    if (y < desktop_menu_height + 8) {
        y = (WORD) (desktop_menu_height + 8);
    }
    if (x + panel_width > screen_width - 12) {
        x = (WORD) (screen_width - panel_width - 12);
    }
    if (y + panel_height > screen_height - desktop_status_height - 8) {
        y = (WORD) (screen_height - desktop_status_height - panel_height - 8);
    }

    strncpy(g_panel_title, title, sizeof(g_panel_title) - 1u);
    g_panel_title[sizeof(g_panel_title) - 1u] = '\0';
    strncpy(g_panel_line1, line1, sizeof(g_panel_line1) - 1u);
    g_panel_line1[sizeof(g_panel_line1) - 1u] = '\0';
    strncpy(g_panel_line2, line2, sizeof(g_panel_line2) - 1u);
    g_panel_line2[sizeof(g_panel_line2) - 1u] = '\0';
    strncpy(g_panel_line3, line3, sizeof(g_panel_line3) - 1u);
    g_panel_line3[sizeof(g_panel_line3) - 1u] = '\0';

    if (g_panel_open == 0) {
        g_panel_handle = wind_create(NAME | CLOSER, x, y, panel_width, panel_height);
        g_panel_open = (g_panel_handle != 0);
    }
    if (g_panel_open != 0) {
        (void) wind_open(g_panel_handle, x, y, panel_width, panel_height);
    }
}

static void desktop_close_panel(void)
{
    if (g_panel_open != 0) {
        (void) wind_close(g_panel_handle);
        (void) wind_delete(g_panel_handle);
        g_panel_open = 0;
        g_panel_handle = 0;
    }
}

static int desktop_point_in_panel_close(WORD x, WORD y)
{
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;

    if (g_panel_open == 0) {
        return 0;
    }

    (void) wind_get(g_panel_handle, WF_WXYWH,
        &outer_x, &outer_y, &outer_w, &outer_h);
    (void) outer_w;
    (void) outer_h;
    return x >= outer_x + 4 && y >= outer_y + 3 &&
        x <= outer_x + 16 && y <= outer_y + 15;
}

static void desktop_handle_menu(WORD object, WORD screen_width, WORD screen_height)
{
    WORD title_x;
    WORD title_y;

    switch (object) {
    case desktop_menu_desktop:
    case desktop_menu_file:
    case desktop_menu_view:
    case desktop_menu_help:
        objc_offset(g_menu_tree, object, &title_x, &title_y);
        (void) screen_width;
        (void) screen_height;
        desktop_open_menu_popup(object);
        break;
    default:
        break;
    }
}

static void desktop_handle_menu_item(WORD screen_width,
                                     WORD screen_height,
                                     WORD item)
{
    switch (g_menu_popup_title) {
    case desktop_menu_desktop:
        if (item == 0) {
            desktop_open_panel(screen_width, screen_height, 48, 72,
                "Desktop", "Hosted GEM desktop session", "Using AES trees and VDI",
                "Real historical port comes next.");
            desktop_set_status_message("Opened Desktop info");
        } else if (item == 1) {
            desktop_select_icon(NIL);
            desktop_close_panel();
            desktop_set_status_message("Desktop reset");
        } else if (item == 2) {
            desktop_close_panel();
            desktop_set_status_message("Closed window");
        }
        break;
    case desktop_menu_file:
        if (item == 0) {
            desktop_open_panel(screen_width, screen_height, 88, 86,
                "Files", "File views are still hosted.", "No folder browser yet.",
                "RCS and apps can be wired next.");
            desktop_set_status_message("Opened Files");
        } else if (item == 1) {
            desktop_open_panel(screen_width, screen_height, 210, 96,
                "Tools", "Tool launching placeholder.", "Menus are now pop-downs.",
                "App launch hooks come next.");
            desktop_set_status_message("Opened Tools");
        } else if (item == 2) {
            desktop_open_panel(screen_width, screen_height, 332, 86,
                "Trash", "Trash is empty.", "Delete/restore not wired yet.",
                "Window is draggable now.");
            desktop_set_status_message("Opened Trash");
        }
        break;
    case desktop_menu_view:
        if (item == 0) {
            desktop_set_status_message("Desktop refreshed");
        } else if (item == 1) {
            desktop_set_status_message("");
        } else if (item == 2) {
            desktop_set_status_message("Closed menu");
        }
        break;
    case desktop_menu_help:
        if (item == 0) {
            desktop_open_panel(screen_width, screen_height, 160, 60,
                "About GEM", "Hosted GEM desktop launcher", "AES trees and VDI renderer",
                "Press Esc or q to quit.");
            desktop_set_status_message("Opened Help");
        } else if (item == 1) {
            desktop_open_panel(screen_width, screen_height, 190, 94,
                "Keys", "Esc or q quits.", "Click menu titles for pop-downs.",
                "Drag window by its title bar.");
            desktop_set_status_message("Opened Keys");
        } else if (item == 2) {
            desktop_open_panel(screen_width, screen_height, 224, 128,
                "Rasta", "Framebuffer: /tmp/rasta.fb", "Input: UDP 127.0.0.1:5000",
                "Current host display backend.");
            desktop_set_status_message("Opened Rasta info");
        }
        break;
    default:
        break;
    }
}

static void desktop_handle_icon(WORD object, WORD screen_width, WORD screen_height)
{
    desktop_select_icon(object);
    switch (object) {
    case desktop_icon_trash:
        desktop_open_panel(screen_width, screen_height, 56, 76,
            "Trash", "Trash is empty.", "No delete or restore support yet.",
            "Historical desktop behavior comes later.");
        desktop_set_status_message(desktop_icon_message(object));
        break;
    case desktop_icon_files:
        desktop_open_panel(screen_width, screen_height, 160, 92,
            "Files", "File manager is not ported yet.", "Desktop launcher is using AES trees.",
            "Historical file views will come next.");
        desktop_set_status_message(desktop_icon_message(object));
        break;
    case desktop_icon_tools:
        desktop_open_panel(screen_width, screen_height, 264, 108,
            "Tools", "Tools area placeholder.", "Next step is real app launching.",
            "RCS and calclock targets already build.");
            desktop_set_status_message(desktop_icon_message(object));
        break;
    default:
        desktop_set_status_message("");
        break;
    }
}

static void desktop_redraw_region(WORD screen_width,
                                  WORD screen_height,
                                  WORD mouse_x,
                                  WORD mouse_y,
                                  WORD clip_x,
                                  WORD clip_y,
                                  WORD clip_w,
                                  WORD clip_h)
{
    (void) screen_width;
    (void) screen_height;

    wind_update(BEG_UPDATE);
    menu_bar(g_menu_tree, 1);
    desktop_update_status_text(mouse_x, mouse_y);
    objc_draw(g_menu_tree, ROOT, MAX_DEPTH,
        clip_x, clip_y, clip_w, clip_h);
    objc_draw(g_desktop_tree, ROOT, MAX_DEPTH,
        clip_x, clip_y, clip_w, clip_h);
    desktop_draw_panel(clip_x, clip_y, clip_w, clip_h);
    desktop_draw_menu_popup(clip_x, clip_y, clip_w, clip_h);
    wind_update(END_UPDATE);
}

static void desktop_draw_scene(WORD screen_width,
                               WORD screen_height,
                               WORD mouse_x,
                               WORD mouse_y)
{
    desktop_redraw_region(screen_width, screen_height, mouse_x, mouse_y,
        0, 0, screen_width, screen_height);
}

static void desktop_draw_status(WORD screen_width,
                                WORD screen_height,
                                WORD mouse_x,
                                WORD mouse_y)
{
    WORD clip_y = (WORD) (screen_height - desktop_status_height);

    desktop_redraw_region(screen_width, screen_height, mouse_x, mouse_y,
        0, clip_y, screen_width, desktop_status_height);
}

static void desktop_draw_icons(WORD screen_width,
                               WORD screen_height,
                               WORD mouse_x,
                               WORD mouse_y)
{
    desktop_redraw_region(screen_width, screen_height, mouse_x, mouse_y,
        0, desktop_menu_height, screen_width, 96);
}

static void desktop_redraw_saved_panel(WORD screen_width,
                                       WORD screen_height,
                                       WORD mouse_x,
                                       WORD mouse_y,
                                       WORD x,
                                       WORD y,
                                       WORD w,
                                       WORD h)
{
    if (w <= 0 || h <= 0) {
        return;
    }

    desktop_redraw_region(screen_width, screen_height, mouse_x, mouse_y,
        x, y, w, h);
}

static void desktop_refresh_after_panel_change(WORD screen_width,
                                               WORD screen_height,
                                               WORD mouse_x,
                                               WORD mouse_y,
                                               WORD old_x,
                                               WORD old_y,
                                               WORD old_w,
                                               WORD old_h,
                                               WORD redraw_icons)
{
    desktop_redraw_saved_panel(screen_width, screen_height, mouse_x, mouse_y,
        old_x, old_y, old_w, old_h);
    if (g_panel_open != 0) {
        WORD new_x;
        WORD new_y;
        WORD new_w;
        WORD new_h;

        desktop_get_panel_outer_rect(&new_x, &new_y, &new_w, &new_h);
        desktop_redraw_saved_panel(screen_width, screen_height, mouse_x, mouse_y,
            new_x, new_y, new_w, new_h);
    }
    if (redraw_icons != 0) {
        desktop_draw_icons(screen_width, screen_height, mouse_x, mouse_y);
    }
    desktop_draw_status(screen_width, screen_height, mouse_x, mouse_y);
}

int main(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD screen_width;
    WORD screen_height;
    WORD desk_x;
    WORD desk_y;
    WORD desk_w;
    WORD desk_h;
    WORD window_handle;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    int running = 1;

    (void) appl_init();
    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }

    screen_width = (WORD) (work_out[0] + 1);
    screen_height = (WORD) (work_out[1] + 1);

    desktop_init_menu_tree(screen_width);
    window_handle = wind_create(0, 0, desktop_menu_height,
        screen_width, (WORD) (screen_height - desktop_menu_height));
    (void) wind_open(window_handle, 0, desktop_menu_height,
        screen_width, (WORD) (screen_height - desktop_menu_height));
    (void) wind_get(window_handle, WF_CXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
    desktop_init_desktop_tree(desk_x, desk_y, desk_w, desk_h);
    desktop_set_status_message("");
    desktop_draw_scene(screen_width, screen_height, mouse_x, mouse_y);

    while (running != 0) {
        gem_hid_event_t evt;

        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(8u);
            continue;
        }

        switch (evt.type) {
        case GEM_HID_MOUSE_MOVE:
            mouse_x = evt.x;
            mouse_y = evt.y;
            if (g_dragging_panel != 0 && (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
                WORD old_panel_x = 0;
                WORD old_panel_y = 0;
                WORD old_panel_w = 0;
                WORD old_panel_h = 0;
                WORD new_x;
                WORD new_y;

                desktop_get_panel_outer_rect(&old_panel_x, &old_panel_y,
                    &old_panel_w, &old_panel_h);
                new_x = (WORD) (mouse_x - g_drag_offset_x);
                new_y = (WORD) (mouse_y - g_drag_offset_y);
                (void) wind_set(g_panel_handle, WF_WXYWH, new_x, new_y,
                    panel_width, panel_height);
                desktop_refresh_after_panel_change(screen_width, screen_height,
                    mouse_x, mouse_y, old_panel_x, old_panel_y, old_panel_w,
                    old_panel_h, 0);
            } else {
                desktop_draw_status(screen_width, screen_height, mouse_x, mouse_y);
            }
            break;
        case GEM_HID_MOUSE_BUTTON:
            mouse_x = evt.x;
            mouse_y = evt.y;
            if ((evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
                WORD old_panel_x = 0;
                WORD old_panel_y = 0;
                WORD old_panel_w = 0;
                WORD old_panel_h = 0;

                desktop_get_panel_outer_rect(&old_panel_x, &old_panel_y,
                    &old_panel_w, &old_panel_h);
                if (desktop_point_in_panel_close(mouse_x, mouse_y)) {
                    desktop_close_menu_popup();
                    desktop_close_panel();
                    desktop_set_status_message("Closed window");
                    desktop_refresh_after_panel_change(screen_width,
                        screen_height, mouse_x, mouse_y,
                        old_panel_x, old_panel_y, old_panel_w, old_panel_h, 0);
                } else if (desktop_point_in_panel_title(mouse_x, mouse_y)) {
                    g_dragging_panel = 1;
                    g_drag_offset_x = (WORD) (mouse_x - old_panel_x);
                    g_drag_offset_y = (WORD) (mouse_y - old_panel_y);
                    desktop_set_status_message("Dragging window");
                    desktop_draw_status(screen_width, screen_height,
                        mouse_x, mouse_y);
                } else if (g_menu_popup_open != 0) {
                    static WORD desktop_menu_popup_hit(WORD x, WORD y);

                    if (item != NIL) {
                        desktop_handle_menu_item(screen_width, screen_height, item);
                    } else {
                        desktop_set_status_message("Closed menu");
                    }
                    desktop_close_menu_popup();
                    desktop_refresh_after_panel_change(screen_width,
                        screen_height, mouse_x, mouse_y,
                        old_panel_x, old_panel_y, old_panel_w, old_panel_h, 0);
                } else {
                    WORD object = objc_find(g_menu_tree, ROOT, MAX_DEPTH,
                        mouse_x, mouse_y);

                    if (object != NIL && object != ROOT) {
                        desktop_handle_menu(object, screen_width, screen_height);
                        if (g_menu_popup_open != 0) {
                            WORD popup_x;
                            WORD popup_y;
                            WORD popup_w;
                            WORD popup_h;

                            desktop_get_menu_popup_rect(&popup_x, &popup_y,
                                &popup_w, &popup_h);
                            desktop_redraw_region(screen_width, screen_height,
                                mouse_x, mouse_y, popup_x, popup_y,
                                popup_w, popup_h);
                            desktop_draw_status(screen_width, screen_height,
                                mouse_x, mouse_y);
                        } else {
                            desktop_refresh_after_panel_change(screen_width,
                                screen_height, mouse_x, mouse_y,
                                old_panel_x, old_panel_y, old_panel_w,
                                old_panel_h, object == desktop_menu_desktop);
                        }
                    } else {
                        object = objc_find(g_desktop_tree, ROOT, MAX_DEPTH,
                            mouse_x, mouse_y);
                        if (object == desktop_icon_trash ||
                            object == desktop_icon_files ||
                            object == desktop_icon_tools) {
                            desktop_handle_icon(object, screen_width,
                                screen_height);
                            desktop_refresh_after_panel_change(screen_width,
                                screen_height, mouse_x, mouse_y,
                                old_panel_x, old_panel_y, old_panel_w,
                                old_panel_h, 1);
                        } else {
                            desktop_draw_status(screen_width, screen_height,
                                mouse_x, mouse_y);
                        }
                    }
                }
            } else if (evt.flags == 0u) {
                if (g_dragging_panel != 0) {
                    g_dragging_panel = 0;
                    desktop_set_status_message("Moved window");
                }
                desktop_draw_status(screen_width, screen_height, mouse_x, mouse_y);
            }
            break;
        case GEM_HID_KEY: {
            WORD ch;

            if ((evt.flags & 1u) == 0u) {
                break;
            }
            ch = desktop_key_to_ascii(evt.key);
            if (ch == 27 || ch == 'q') {
                running = 0;
            }
            break;
        }
        case GEM_HID_QUIT:
            running = 0;
            break;
        default:
            break;
        }
    }

    desktop_close_panel();
    (void) wind_close(window_handle);
    (void) wind_delete(window_handle);
    v_clsvwk(handle);
    (void) appl_exit();
    return 0;
}

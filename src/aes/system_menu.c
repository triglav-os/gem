/*
 * Implements the always-present hosted GEM system menu. A fixed three-bar
 * menu button sits at the left of the menu bar and drops a popup containing
 * registered GEM services, a separator and Shutdown. It is gemd's own menu.
 * Service selections are delivered to their owner as AC_OPEN messages.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "system_menu.h"
#include "window_internal.h"

#include "../vdi/vdi_internal.h"

#include "platform/os.h"

#include <string.h>

/* 16x16 menu button: three horizontal bars, 2 pixels thick, cols 0-12.
 * Most-significant bit leftmost, set bit = ink. */
static const UWORD menu_bits[16] = {
    0x0000, 0x0000, 0x0000, 0xfff8, 0xfff8, 0x0000, 0x0000, 0xfff8,
    0xfff8, 0x0000, 0x0000, 0xfff8, 0xfff8, 0x0000, 0x0000, 0x0000,
};

enum {
    SYSTEM_GLYPH = 16,
    SYSTEM_PAD_LEFT = 6,
    SYSTEM_PAD_RIGHT = 1,
    SYSTEM_SEPARATOR_HEIGHT = 7
};

int (*aes_system_shutdown_hook)(void);
int (*aes_system_delete_hook)(void);

static const char system_shutdown_text[] = "Shutdown";

WORD aes_system_menu_register(WORD app_id, const char *text)
{
    WORD i;
    const unsigned char *cursor = (const unsigned char *)text;

    if (app_id <= 0 || text == NULL || text[0] == '\0' ||
        strlen(text) >= AES_SYSTEM_ITEM_TEXT) {
        return -1;
    }
    while (*cursor != '\0') {
        if (*cursor < 32u) {
            return -1;
        }
        ++cursor;
    }
    for (i = 0; i < AES_MAX_SYSTEM_ITEMS; ++i) {
        aes_system_item_t *item = &aes_state.system_items[i];

        if (item->used == 0) {
            item->used = 1;
            item->owner = app_id;
            strcpy(item->text, text);
            aes_trace("system_menu register app=%d id=%d text=%s", app_id, i,
                      text);
            return i;
        }
    }
    return -1;
}

WORD aes_system_menu_unregister(WORD app_id, WORD menu_id)
{
    aes_system_item_t *item;

    if (menu_id < 0 || menu_id >= AES_MAX_SYSTEM_ITEMS) {
        return 0;
    }
    item = &aes_state.system_items[menu_id];
    if (item->used == 0 || item->owner != app_id) {
        return 0;
    }
    memset(item, 0, sizeof(*item));
    aes_trace("system_menu unregister app=%d id=%d", app_id, menu_id);
    return 1;
}

void aes_system_menu_unregister_owner(WORD app_id)
{
    WORD i;

    for (i = 0; i < AES_MAX_SYSTEM_ITEMS; ++i) {
        if (aes_state.system_items[i].used != 0 &&
            aes_state.system_items[i].owner == app_id) {
            aes_trace("system_menu unregister_owner app=%d id=%d", app_id, i);
            memset(&aes_state.system_items[i], 0,
                   sizeof(aes_state.system_items[i]));
        }
    }
}

static WORD system_registration_count(void)
{
    WORD i;
    WORD count = 0;

    for (i = 0; i < AES_MAX_SYSTEM_ITEMS; ++i) {
        if (aes_state.system_items[i].used != 0) {
            ++count;
        }
    }
    return count;
}

WORD aes_system_menu_width(void)
{
    return (WORD)(SYSTEM_PAD_LEFT + SYSTEM_GLYPH + SYSTEM_PAD_RIGHT);
}

static WORD system_glyph_top(void)
{
    WORD height = aes_menu_bar_height();
    WORD top = (WORD)((height - SYSTEM_GLYPH) / 2);

    return (top > 0) ? top : 0;
}

void aes_system_menu_draw(void)
{
    WORD height = aes_menu_bar_height();
    WORD top;
    WORD row;
    WORD fill[4];

    if (aes_ensure_vdi() == 0 || height <= 0) {
        return;
    }

    top = system_glyph_top();
    fill[0] = 0;
    fill[1] = 0;
    fill[2] = (WORD)(aes_system_menu_width() - 1);
    fill[3] = (WORD)(height - 2);
    vsf_color(aes_state.vdi_handle, aes_light_color());
    v_bar(aes_state.vdi_handle, fill);

    vsf_color(aes_state.vdi_handle, aes_dark_color());
    for (row = 0; row < SYSTEM_GLYPH; ++row) {
        UWORD bits = menu_bits[row];
        WORD col = 0;

        while (col < SYSTEM_GLYPH) {
            WORD start;

            while (col < SYSTEM_GLYPH &&
                   (bits & (UWORD)(0x8000u >> col)) == 0u) {
                ++col;
            }
            if (col >= SYSTEM_GLYPH) {
                break;
            }
            start = col;
            while (col < SYSTEM_GLYPH &&
                   (bits & (UWORD)(0x8000u >> col)) != 0u) {
                ++col;
            }
            fill[0] = (WORD)(SYSTEM_PAD_LEFT + start);
            fill[1] = (WORD)(top + row);
            fill[2] = (WORD)(SYSTEM_PAD_LEFT + col - 1);
            fill[3] = fill[1];
            v_bar(aes_state.vdi_handle, fill);
        }
    }
    /* Leave the shared fill color at paper: applications that clear their
     * work area without setting a fill color rely on that default, and in
     * direct mode there is no per-session VDI state to isolate them. */
    vsf_color(aes_state.vdi_handle, aes_light_color());
}

int aes_system_menu_hit(WORD x, WORD y)
{
    return x >= 0 && x < aes_system_menu_width() && y >= 0 &&
           y < aes_menu_bar_height();
}

static void system_popup_rect(GRECT *rect)
{
    WORD width = vdi_string_width(system_shutdown_text);
    WORD item;
    WORD registrations = system_registration_count();

    for (item = 0; item < AES_MAX_SYSTEM_ITEMS; ++item) {
        WORD w;

        if (aes_state.system_items[item].used == 0) {
            continue;
        }
        w = vdi_string_width(aes_state.system_items[item].text);
        if (w > width) {
            width = w;
        }
    }
    aes_set_rect(rect, 0, aes_menu_bar_height(), (WORD)(width + 8),
                 (WORD)((registrations + 1) * aes_menu_chrome_height() +
                        (registrations != 0 ? SYSTEM_SEPARATOR_HEIGHT : 0) +
                        2));
}

static WORD system_item_at(const GRECT *popup, WORD x, WORD y)
{
    WORD item_height = aes_menu_chrome_height();
    WORD cursor;
    WORD item;

    if (x < popup->g_x || x >= popup->g_x + popup->g_w || y < popup->g_y + 1 ||
        y >= popup->g_y + popup->g_h - 1) {
        return -1;
    }
    cursor = (WORD)(popup->g_y + 1);
    for (item = 0; item < AES_MAX_SYSTEM_ITEMS; ++item) {
        if (aes_state.system_items[item].used == 0) {
            continue;
        }
        if (y >= cursor && y < cursor + item_height) {
            return (WORD)(AES_SYSTEM_MENU_REGISTERED + item);
        }
        cursor = (WORD)(cursor + item_height);
    }
    if (system_registration_count() != 0) {
        if (y < cursor + SYSTEM_SEPARATOR_HEIGHT) {
            return AES_SYSTEM_MENU_NONE;
        }
        cursor = (WORD)(cursor + SYSTEM_SEPARATOR_HEIGHT);
    }
    return (y >= cursor && y < cursor + item_height) ? AES_SYSTEM_MENU_SHUTDOWN
                                                     : AES_SYSTEM_MENU_NONE;
}

static void system_draw_popup(const GRECT *popup, WORD highlight)
{
    WORD item_height = aes_menu_chrome_height();
    WORD frame[4];
    WORD item;
    WORD top;

    frame[0] = popup->g_x;
    frame[1] = popup->g_y;
    frame[2] = (WORD)(popup->g_x + popup->g_w - 1);
    frame[3] = (WORD)(popup->g_y + popup->g_h - 1);
    vsf_color(aes_state.vdi_handle, aes_light_color());
    v_bar(aes_state.vdi_handle, frame);
    vsl_color(aes_state.vdi_handle, aes_dark_color());
    v_rbox(aes_state.vdi_handle, frame);

    top = (WORD)(popup->g_y + 1);
    for (item = 0; item < AES_MAX_SYSTEM_ITEMS; ++item) {
        WORD text_y = (WORD)(top + vdi_font_ascent());
        WORD ink = aes_dark_color();

        if (aes_state.system_items[item].used == 0) {
            continue;
        }
        if ((WORD)(AES_SYSTEM_MENU_REGISTERED + item) == highlight) {
            WORD bar[4] = {(WORD)(popup->g_x + 1), top,
                           (WORD)(popup->g_x + popup->g_w - 2),
                           (WORD)(top + item_height - 1)};
            vsf_color(aes_state.vdi_handle, aes_dark_color());
            v_bar(aes_state.vdi_handle, bar);
            ink = aes_light_color();
        }
        aes_draw_text((WORD)(popup->g_x + 4), text_y, ink,
                      aes_state.system_items[item].text);
        top = (WORD)(top + item_height);
    }
    if (system_registration_count() != 0) {
        WORD line[4] = {(WORD)(popup->g_x + 4),
                        (WORD)(top + SYSTEM_SEPARATOR_HEIGHT / 2),
                        (WORD)(popup->g_x + popup->g_w - 5),
                        (WORD)(top + SYSTEM_SEPARATOR_HEIGHT / 2)};

        vsl_color(aes_state.vdi_handle, aes_dark_color());
        v_pline(aes_state.vdi_handle, 2, line);
        top = (WORD)(top + SYSTEM_SEPARATOR_HEIGHT);
    }
    {
        WORD text_y = (WORD)(top + vdi_font_ascent());
        WORD ink = aes_dark_color();

        if (highlight == AES_SYSTEM_MENU_SHUTDOWN) {
            WORD bar[4] = {(WORD)(popup->g_x + 1), top,
                           (WORD)(popup->g_x + popup->g_w - 2),
                           (WORD)(top + item_height - 1)};

            vsf_color(aes_state.vdi_handle, aes_dark_color());
            v_bar(aes_state.vdi_handle, bar);
            ink = aes_light_color();
        }
        aes_draw_text((WORD)(popup->g_x + 4), text_y, ink,
                      system_shutdown_text);
    }
}

WORD aes_system_menu_track(const gem_hid_event_t *first_evt)
{
    GRECT popup;
    GRECT title = {0, 0, 0, 0};
    uint8_t *saved = NULL;
    uint8_t *title_saved = NULL;
    WORD highlight = -1;
    WORD chosen = AES_SYSTEM_MENU_NONE;
    gem_hid_event_t evt;

    if (first_evt == NULL || aes_ensure_vdi() == 0) {
        return AES_SYSTEM_MENU_NONE;
    }

    system_popup_rect(&popup);
    aes_set_rect(&title, 0, 0, aes_system_menu_width(), aes_menu_bar_height());

    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    (void)aes_save_region_pixels(&title, &title_saved);
    (void)aes_save_region_pixels(&popup, &saved);
    /* Selected title: invert the menu-button strip while the popup is open. */
    {
        WORD box[4] = {title.g_x, title.g_y, (WORD)(title.g_x + title.g_w - 1),
                       (WORD)(title.g_y + title.g_h - 2)};
        (void)vswr_mode(aes_state.vdi_handle, MD_XOR);
        vsf_color(aes_state.vdi_handle, aes_dark_color());
        v_bar(aes_state.vdi_handle, box);
        (void)vswr_mode(aes_state.vdi_handle, MD_REPLACE);
    }
    system_draw_popup(&popup, highlight);
    v_show_c(aes_state.vdi_handle, 1);
    vdi_flush_rect(title.g_x, title.g_y, title.g_w,
                   (WORD)(title.g_h + popup.g_h));
    vdi_end_update();

    for (;;) {
        WORD hovered;

        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (evt.type != GEM_HID_MOUSE_MOVE &&
            evt.type != GEM_HID_MOUSE_BUTTON) {
            continue;
        }
        aes_store_mouse_state(&evt);
        hovered = system_item_at(&popup, (WORD)evt.x, (WORD)evt.y);
        if (hovered != highlight) {
            highlight = hovered;
            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            system_draw_popup(&popup, highlight);
            v_show_c(aes_state.vdi_handle, 1);
            vdi_flush_rect(popup.g_x, popup.g_y, popup.g_w, popup.g_h);
            vdi_end_update();
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            chosen = highlight;
            break;
        }
    }

    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    if (saved != NULL) {
        aes_restore_region_pixels(&popup, saved);
    }
    if (title_saved != NULL) {
        aes_restore_region_pixels(&title, title_saved);
    }
    v_show_c(aes_state.vdi_handle, 1);
    vdi_flush_rect(title.g_x, title.g_y, title.g_w,
                   (WORD)(title.g_h + popup.g_h));
    vdi_end_update();

    gem_os_free(saved);
    gem_os_free(title_saved);

    /* Restore shared drawing state the popup changed. */
    (void)vswr_mode(aes_state.vdi_handle, MD_REPLACE);
    vsf_color(aes_state.vdi_handle, aes_light_color());
    vsl_color(aes_state.vdi_handle, aes_dark_color());

    if (chosen >= AES_SYSTEM_MENU_REGISTERED &&
        chosen < AES_SYSTEM_MENU_REGISTERED + AES_MAX_SYSTEM_ITEMS) {
        WORD menu_id = (WORD)(chosen - AES_SYSTEM_MENU_REGISTERED);
        aes_system_item_t *item = &aes_state.system_items[menu_id];

        if (item->used != 0 && aes_find_app_by_id(item->owner) != NULL) {
            WORD message[8] = {AC_OPEN, 0, 0, menu_id, 0, 0, 0, 0};

            (void)appl_write(item->owner, (WORD)sizeof(message), message);
        } else {
            chosen = AES_SYSTEM_MENU_NONE;
        }
    } else if (chosen == AES_SYSTEM_MENU_SHUTDOWN) {
        if (form_alert(2, "[2][ Shut down GEM and | close all applications? ]"
                          "[ Shutdown | Cancel ]") == 1 &&
            aes_system_shutdown_hook != NULL) {
            (void)aes_system_shutdown_hook();
        } else {
            chosen = AES_SYSTEM_MENU_NONE;
        }
    }
    return chosen;
}

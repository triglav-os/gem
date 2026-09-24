/*
 * Declares the private DRI DESKTOP.INF compatibility model and the
 * bidirectional adapter between that legacy byte stream and desktop.cfg.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_DESKTOP_INF_H
#define GEM_DESKTOP_INF_H

#include <stddef.h>

enum {
    DESKTOP_INF_MAX_WINDOWS = 16,
    DESKTOP_INF_MAX_ITEMS = 128,
    DESKTOP_INF_MAX_ACCESSORIES = 16,
    DESKTOP_INF_MAX_UNKNOWN = 32,
    DESKTOP_INF_TEXT_MAX = 512,
    DESKTOP_INF_DATA_MAX = 32767
};

typedef struct desktop_inf_preferences {
    int present;
    int icon_view;
    int sort;
    int confirm_delete;
    int confirm_copy;
    int double_click;
    int confirm_overwrite;
    int menu_click;
    int month_day_date;
    int twelve_hour_time;
    int sound;
    unsigned char reserved_second;
    char extra_hex[32];
} desktop_inf_preferences_t;

typedef struct desktop_inf_window {
    int horizontal_scroll;
    int vertical_scroll;
    int x;
    int y;
    int width;
    int height;
    int object;
    char path[DESKTOP_INF_TEXT_MAX];
} desktop_inf_window_t;

typedef struct desktop_inf_item {
    char code;
    int x;
    int y;
    int application_icon;
    int document_icon;
    char drive;
    char name[DESKTOP_INF_TEXT_MAX];
    char patterns[DESKTOP_INF_TEXT_MAX];
    int memory_kib;
} desktop_inf_item_t;

typedef struct desktop_inf_accessory {
    int swap;
    char name[DESKTOP_INF_TEXT_MAX];
} desktop_inf_accessory_t;

typedef struct desktop_inf_model {
    desktop_inf_preferences_t preferences;
    desktop_inf_window_t windows[DESKTOP_INF_MAX_WINDOWS];
    size_t window_count;
    desktop_inf_item_t items[DESKTOP_INF_MAX_ITEMS];
    size_t item_count;
    desktop_inf_accessory_t accessories[DESKTOP_INF_MAX_ACCESSORIES];
    size_t accessory_count;
    char unknown[DESKTOP_INF_MAX_UNKNOWN][DESKTOP_INF_TEXT_MAX];
    size_t unknown_count;
} desktop_inf_model_t;

/* Parse one bounded DRI/FreeGEM DESKTOP.INF image into a validated model. */
int desktop_inf_parse(const unsigned char *data, size_t size,
                      desktop_inf_model_t *model);

/* Serialize a model as canonical CRLF records followed by Ctrl-Z and NUL. */
int desktop_inf_format(const desktop_inf_model_t *model, unsigned char *data,
                       size_t capacity, size_t *size_out);

/* Load desktop.cfg and return its generated DESKTOP.INF compatibility view. */
int desktop_inf_config_get(unsigned char *data, size_t capacity,
                           size_t *size_out);

/* Parse a complete DESKTOP.INF image and atomically persist desktop.cfg. */
int desktop_inf_config_put(const unsigned char *data, size_t size);

#endif

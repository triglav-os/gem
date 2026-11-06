/*
 * Embeds CEF as Gemscape's off-screen browser, forwards browser input and
 * navigation, reports page scroll metrics and manages CEF process lifetime.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "cef_browser.h"

#include <gem/os.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_load_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/capi/cef_devtools_message_observer_capi.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_values_capi.h"
#include "include/capi/cef_jsdialog_handler_capi.h"
#include "include/capi/cef_request_context_capi.h"
#include "include/cef_api_hash.h"
#include "include/internal/cef_string_types.h"
#include "include/internal/cef_types.h"

/* Include AES after CEF: both APIs define a CT_MOVE identifier. */
#include <gem/aes.h>
#include "browser_settings.h"
#include <math.h>
#include <stdatomic.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct gem_cef_renderer {
    cef_render_handler_t api;
    atomic_int refs;
    uint16_t width;
    uint16_t height;
    int ready;
    uint8_t *pixels;
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t *popup;
    cef_rect_t popup_rect;
    int popup_width;
    int popup_height;
    int popup_visible;
    pthread_mutex_t mutex;
} gem_cef_renderer_t;

typedef struct gem_cef_load_handler {
    cef_load_handler_t api;
    atomic_int refs;
} gem_cef_load_handler_t;

typedef struct gem_cef_display_handler {
    cef_display_handler_t api;
    atomic_int refs;
} gem_cef_display_handler_t;

typedef struct gem_cef_life_span_handler {
    cef_life_span_handler_t api;
    atomic_int refs;
} gem_cef_life_span_handler_t;

typedef struct gem_cef_client {
    cef_client_t api;
    atomic_int refs;
    gem_cef_renderer_t *renderer;
    gem_cef_load_handler_t *load_handler;
    gem_cef_display_handler_t *display_handler;
    gem_cef_life_span_handler_t *life_span_handler;
} gem_cef_client_t;

typedef struct gem_cef_dialog {
    cef_jsdialog_handler_t api;
    atomic_int refs;
} gem_cef_dialog_t;

typedef struct gem_cef_observer {
    cef_dev_tools_message_observer_t api;
    atomic_int refs;
} gem_cef_observer_t;

static cef_app_t g_app;
static atomic_int g_app_refs;

static struct {
    cef_browser_t *browser;
    gem_cef_client_t *client;
    gem_cef_renderer_t *renderer;
    int initialized;
    uint8_t *frame;
    size_t frame_size;
    int scroll_x;
    int scroll_y;
    int viewport_width;
    int viewport_height;
    int content_width;
    int content_height;
    int scroll_ready;
    int browser_closed;
    gemscape_browser_info_t info;
    gemscape_preferences_t preferences;
    gem_cef_observer_t *observer;
    cef_registration_t *registration;
    int next_id;
    int metrics_id;
    int clear_id;
    int profile_lock;
    uint32_t metrics_tick;
    int mouse_buttons;
    gem_cef_dialog_t *dialog;
    char selection[4096];
    char clipboard[4096];
} g_state;

static void gem_string(cef_string_t *value, const char *text);

static void
gem_base_init(cef_base_ref_counted_t *base, size_t size,
              void(CEF_CALLBACK *add_ref)(cef_base_ref_counted_t *),
              int(CEF_CALLBACK *release)(cef_base_ref_counted_t *),
              int(CEF_CALLBACK *has_one_ref)(cef_base_ref_counted_t *),
              int(CEF_CALLBACK *has_ref)(cef_base_ref_counted_t *))
{
    base->size = size;
    base->add_ref = add_ref;
    base->release = release;
    base->has_one_ref = has_one_ref;
    base->has_at_least_one_ref = has_ref;
}

#define GEM_REF_FUNCS(prefix, type)                                            \
    static void CEF_CALLBACK prefix##_add_ref(cef_base_ref_counted_t *base)    \
    {                                                                          \
        type *object = (type *)((char *)base - offsetof(type, api.base));      \
        ++object->refs;                                                        \
    }                                                                          \
    static int CEF_CALLBACK prefix##_release(cef_base_ref_counted_t *base)     \
    {                                                                          \
        type *object = (type *)((char *)base - offsetof(type, api.base));      \
        if (--object->refs != 0)                                               \
            return 0;                                                          \
        free(object);                                                          \
        return 1;                                                              \
    }                                                                          \
    static int CEF_CALLBACK prefix##_has_one_ref(cef_base_ref_counted_t *base) \
    {                                                                          \
        type *object = (type *)((char *)base - offsetof(type, api.base));      \
        return object->refs == 1;                                              \
    }                                                                          \
    static int CEF_CALLBACK prefix##_has_ref(cef_base_ref_counted_t *base)     \
    {                                                                          \
        type *object = (type *)((char *)base - offsetof(type, api.base));      \
        return object->refs >= 1;                                              \
    }

GEM_REF_FUNCS(gem_renderer, gem_cef_renderer_t)
GEM_REF_FUNCS(gem_client, gem_cef_client_t)
GEM_REF_FUNCS(gem_load, gem_cef_load_handler_t)
GEM_REF_FUNCS(gem_display, gem_cef_display_handler_t)
GEM_REF_FUNCS(gem_life_span, gem_cef_life_span_handler_t)
GEM_REF_FUNCS(gem_observer, gem_cef_observer_t)
GEM_REF_FUNCS(gem_dialog, gem_cef_dialog_t)

static void CEF_CALLBACK gem_app_add_ref(cef_base_ref_counted_t *base)
{
    (void)base;
    ++g_app_refs;
}

static int CEF_CALLBACK gem_app_release(cef_base_ref_counted_t *base)
{
    (void)base;
    int refs = atomic_load(&g_app_refs);
    while (refs > 1 &&
           !atomic_compare_exchange_weak(&g_app_refs, &refs, refs - 1)) {
    }
    return 0;
}

static int CEF_CALLBACK gem_app_has_one_ref(cef_base_ref_counted_t *base)
{
    (void)base;
    return g_app_refs == 1;
}

static int CEF_CALLBACK gem_app_has_ref(cef_base_ref_counted_t *base)
{
    (void)base;
    return g_app_refs > 0;
}

static void gem_command_switch(cef_command_line_t *command_line,
                               const char *name)
{
    cef_string_t value = {0};

    gem_string(&value, name);
    command_line->append_switch(command_line, &value);
    cef_string_clear(&value);
}

static void CEF_CALLBACK
gem_app_before_command_line(cef_app_t *self, const cef_string_t *process_type,
                            cef_command_line_t *command_line)
{
    cef_string_t name = {0};
    cef_string_t value = {0};

    (void)self;
    (void)process_type;
    gem_command_switch(command_line, "no-zygote");
    gem_command_switch(command_line, "disable-gpu");
    /* CEF paints into GEM's overlay, including on hosts without X/Wayland. */
    gem_string(&name, "ozone-platform");
    gem_string(&value, "headless");
    command_line->append_switch_with_value(command_line, &name, &value);
    cef_string_clear(&name);
    cef_string_clear(&value);
    command_line->base.release(&command_line->base);
}

static void gem_app_init(void)
{
    memset(&g_app, 0, sizeof(g_app));
    gem_base_init(&g_app.base, sizeof(g_app), gem_app_add_ref, gem_app_release,
                  gem_app_has_one_ref, gem_app_has_ref);
    g_app.on_before_command_line_processing = gem_app_before_command_line;
    g_app_refs = 1;
}

/* CEF callback object arguments transfer one reference to this C adapter.
 * Consume unused arguments immediately; release used arguments after use. */
static void CEF_CALLBACK gem_renderer_get_view_rect(cef_render_handler_t *self,
                                                    cef_browser_t *browser,
                                                    cef_rect_t *rect)
{
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    rect->x = 0;
    rect->y = 0;
    rect->width = renderer->width;
    rect->height = renderer->height;
}

static void CEF_CALLBACK gem_renderer_on_paint(
    cef_render_handler_t *self, cef_browser_t *browser,
    cef_paint_element_type_t type, size_t dirty_count,
    const cef_rect_t *dirty_rects, const void *buffer, int width, int height)
{
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    size_t size = (size_t)width * (size_t)height * 4u;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)dirty_count;
    (void)dirty_rects;
    if (width <= 0 || height <= 0 || width > UINT16_MAX || height > UINT16_MAX)
        return;
    pthread_mutex_lock(&renderer->mutex);
    if (size > (size_t)UINT16_MAX * (size_t)UINT16_MAX * 4u) {
        pthread_mutex_unlock(&renderer->mutex);
        return;
    }
    if (type == PET_POPUP) {
        uint8_t *popup = realloc(renderer->popup, size);
        if (popup != NULL) {
            renderer->popup = popup;
            memcpy(popup, buffer, size);
            renderer->popup_width = width;
            renderer->popup_height = height;
            renderer->ready = renderer->pixels != NULL;
        }
        pthread_mutex_unlock(&renderer->mutex);
        return;
    }
    uint8_t *pixels = realloc(renderer->pixels, size);
    if (pixels != NULL) {
        renderer->pixels = pixels;
        memcpy(renderer->pixels, buffer, size);
        renderer->frame_width = (uint16_t)width;
        renderer->frame_height = (uint16_t)height;
        renderer->ready = 1;
    }
    pthread_mutex_unlock(&renderer->mutex);
}

static void CEF_CALLBACK gem_popup_show(cef_render_handler_t *self,
                                        cef_browser_t *browser, int show)
{
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    pthread_mutex_lock(&renderer->mutex);
    renderer->popup_visible = show;
    renderer->ready = renderer->pixels != NULL;
    pthread_mutex_unlock(&renderer->mutex);
}

static void CEF_CALLBACK gem_popup_size(cef_render_handler_t *self,
                                        cef_browser_t *browser,
                                        const cef_rect_t *rect)
{
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    pthread_mutex_lock(&renderer->mutex);
    renderer->popup_rect = *rect;
    pthread_mutex_unlock(&renderer->mutex);
}

static void CEF_CALLBACK gem_selection(cef_render_handler_t *self,
                                       cef_browser_t *browser,
                                       const cef_string_t *text,
                                       const cef_range_t *range)
{
    cef_string_utf8_t value = {0};
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)range;
    g_state.selection[0] = '\0';
    if (text != NULL && cef_string_to_utf8(text->str, text->length, &value)) {
        snprintf(g_state.selection, sizeof(g_state.selection), "%s", value.str);
        cef_string_utf8_clear(&value);
    }
}

static cef_render_handler_t *CEF_CALLBACK
gem_client_get_render_handler(cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;
    client->renderer->api.base.add_ref(&client->renderer->api.base);
    return &client->renderer->api;
}

static void gem_text(char *output, size_t capacity, const cef_string_t *value)
{
    cef_string_utf8_t text = {0};
    output[0] = '\0';
    if (value != NULL &&
        cef_string_utf16_to_utf8(value->str, value->length, &text)) {
        snprintf(output, capacity, "%s", text.str != NULL ? text.str : "");
        cef_string_utf8_clear(&text);
    }
}

static void gem_status(const char *text)
{
    snprintf(g_state.info.status, sizeof(g_state.info.status), "%s", text);
    ++g_state.info.revision;
}

static int CEF_CALLBACK
gem_script_dialog(cef_jsdialog_handler_t *self, cef_browser_t *browser,
                  const cef_string_t *origin, cef_jsdialog_type_t kind,
                  const cef_string_t *message, const cef_string_t *default_text,
                  cef_jsdialog_callback_t *callback, int *suppress)
{
    char source[256], description[256], text[256];
    cef_string_t answer = {0};
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)suppress;
    gem_text(source, sizeof(source), origin);
    gem_text(description, sizeof(description), message);
    gem_text(text, sizeof(text), default_text);
    int accepted = gemscape_script_dialog((int)kind, source, description, text,
                                          sizeof(text));
    gem_string(&answer, text);
    callback->cont(callback, accepted, &answer);
    callback->base.release(&callback->base);
    cef_string_clear(&answer);
    return 1;
}

static int CEF_CALLBACK gem_before_unload(cef_jsdialog_handler_t *self,
                                          cef_browser_t *browser,
                                          const cef_string_t *message,
                                          int reload,
                                          cef_jsdialog_callback_t *callback)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)message;
    (void)reload;
    int accepted =
        gemscape_script_dialog(1, "Leave this website?",
                               "Changes you made may not be saved.", NULL, 0);
    callback->cont(callback, accepted, NULL);
    callback->base.release(&callback->base);
    return 1;
}

static cef_jsdialog_handler_t *CEF_CALLBACK gem_get_dialog(cef_client_t *self)
{
    (void)self;
    g_state.dialog->api.base.add_ref(&g_state.dialog->api.base);
    return &g_state.dialog->api;
}

static int gem_devtools(const char *method, cef_dictionary_value_t *parameters)
{
    cef_browser_host_t *host;
    cef_string_t name = {0};
    int id;
    if (g_state.browser == NULL)
        return 0;
    host = g_state.browser->get_host(g_state.browser);
    gem_string(&name, method);
    /* DLL-owned CEF objects transfer a reference when passed back as an
     * argument (unlike the method's self pointer). Retain our caller's copy. */
    if (parameters != NULL)
        parameters->base.add_ref(&parameters->base);
    id = host->execute_dev_tools_method(host, ++g_state.next_id, &name,
                                        parameters);
    cef_string_clear(&name);
    host->base.release(&host->base);
    return id;
}

static void gem_parameter(cef_dictionary_value_t *parameters, const char *key,
                          const char *value)
{
    cef_string_t name = {0}, text = {0};
    gem_string(&name, key);
    gem_string(&text, value);
    parameters->set_string(parameters, &name, &text);
    cef_string_clear(&name);
    cef_string_clear(&text);
}

static cef_dictionary_value_t *gem_dictionary(cef_dictionary_value_t *parent,
                                              const char *key)
{
    cef_string_t name = {0};
    cef_dictionary_value_t *result;
    gem_string(&name, key);
    result = parent->get_dictionary(parent, &name);
    cef_string_clear(&name);
    return result;
}

static int gem_number(cef_dictionary_value_t *parent, const char *key)
{
    cef_string_t name = {0};
    double result;
    gem_string(&name, key);
    result = parent->get_type(parent, &name) == VTYPE_INT
                 ? parent->get_int(parent, &name)
                 : parent->get_double(parent, &name);
    cef_string_clear(&name);
    return isfinite(result) && result > 0
               ? (int)(result > 16000000 ? 16000000 : result)
               : 0;
}

static void CEF_CALLBACK gem_devtools_result(
    cef_dev_tools_message_observer_t *self, cef_browser_t *browser, int id,
    int success, const void *result, size_t size)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    if (id == g_state.clear_id) {
        g_state.clear_id = 0;
        g_state.info.clearing = 0;
        gem_status(success ? "Browsing data cleared."
                           : "Could not clear browsing data.");
    }
    if (id == g_state.metrics_id) {
        cef_value_t *value;
        cef_dictionary_value_t *dictionary, *viewport, *content;
        g_state.metrics_id = 0;
        if (!success || result == NULL || size > 65536)
            return;
        value = cef_parse_json_buffer(result, size, JSON_PARSER_RFC);
        if (value == NULL)
            return;
        dictionary = value->get_dictionary(value);
        if (dictionary != NULL) {
            viewport = gem_dictionary(dictionary, "cssLayoutViewport");
            content = gem_dictionary(dictionary, "cssContentSize");
            if (viewport != NULL && content != NULL) {
                g_state.scroll_x = gem_number(viewport, "pageX");
                g_state.scroll_y = gem_number(viewport, "pageY");
                g_state.viewport_width = gem_number(viewport, "clientWidth");
                g_state.viewport_height = gem_number(viewport, "clientHeight");
                g_state.content_width = gem_number(content, "width");
                g_state.content_height = gem_number(content, "height");
                g_state.scroll_ready =
                    g_state.viewport_width > 0 && g_state.viewport_height > 0;
            }
            if (viewport != NULL)
                viewport->base.release(&viewport->base);
            if (content != NULL)
                content->base.release(&content->base);
            dictionary->base.release(&dictionary->base);
        }
        value->base.release(&value->base);
    }
}

static void CEF_CALLBACK
gem_load_on_load_start(cef_load_handler_t *self, cef_browser_t *browser,
                       cef_frame_t *frame, cef_transition_type_t transition)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)transition;
    if (frame != NULL && frame->is_main(frame))
        g_state.scroll_ready = 0;
    if (frame != NULL)
        frame->base.release(&frame->base);
}

static void CEF_CALLBACK gem_load_on_load_end(cef_load_handler_t *self,
                                              cef_browser_t *browser,
                                              cef_frame_t *frame, int status)
{
    cef_string_t code = {0}, source = {0};
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)status;
    if (frame == NULL)
        return;
    if (!frame->is_main(frame)) {
        frame->base.release(&frame->base);
        return;
    }
    /* Keep nested scrolling controls usable; GEM replaces only root scrollbars.
     */
    gem_string(&code,
               "(()=>{let "
               "s=document.createElement('style');s.textContent='html::-webkit-"
               "scrollbar,body::-webkit-scrollbar{width:0!important;height:0!"
               "important}html,body{scrollbar-width:none!important}';(document."
               "head||document.documentElement).appendChild(s)})()");
    gem_string(&source, "gemscape://scrollbars");
    frame->execute_java_script(frame, &code, &source, 1);
    frame->base.release(&frame->base);
    cef_string_clear(&code);
    cef_string_clear(&source);
    gemscape_cef_zoom(g_state.preferences.zoom);
}

static void CEF_CALLBACK gem_loading_state(cef_load_handler_t *self,
                                           cef_browser_t *browser, int loading,
                                           int back, int forward)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    g_state.info.loading = loading;
    g_state.info.can_back = back;
    g_state.info.can_forward = forward;
    if (loading)
        gem_status("Loading...");
    else if (strcmp(g_state.info.status, "Loading...") == 0)
        gem_status("Ready.");
    ++g_state.info.revision;
}

static void CEF_CALLBACK gem_load_error(
    cef_load_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    cef_errorcode_t code, const cef_string_t *text, const cef_string_t *url)
{
    char error[160], status[256];
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    (void)url;
    int main_frame = frame != NULL && frame->is_main(frame);
    if (frame != NULL)
        frame->base.release(&frame->base);
    if (!main_frame || code == ERR_ABORTED)
        return;
    gem_text(error, sizeof(error), text);
    snprintf(status, sizeof(status), "Load failed (%d): %s", (int)code, error);
    gem_status(status);
}

static void CEF_CALLBACK gem_address_changed(cef_display_handler_t *self,
                                             cef_browser_t *browser,
                                             cef_frame_t *frame,
                                             const cef_string_t *url)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    int main_frame = frame != NULL && frame->is_main(frame);
    if (frame != NULL)
        frame->base.release(&frame->base);
    if (!main_frame)
        return;
    gem_text(g_state.info.url, sizeof(g_state.info.url), url);
    ++g_state.info.revision;
}

static void CEF_CALLBACK gem_title_changed(cef_display_handler_t *self,
                                           cef_browser_t *browser,
                                           const cef_string_t *title)
{
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    gem_text(g_state.info.title, sizeof(g_state.info.title), title);
    ++g_state.info.revision;
}

static int CEF_CALLBACK gem_before_popup(
    cef_life_span_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    int popup_id, const cef_string_t *url, const cef_string_t *name,
    cef_window_open_disposition_t disposition, int gesture,
    const cef_popup_features_t *features, cef_window_info_t *window,
    cef_client_t **client, cef_browser_settings_t *settings,
    cef_dictionary_value_t **extra, int *no_access)
{
    char target[2048];
    (void)self;
    if (browser != NULL)
        browser->base.release(&browser->base);
    if (frame != NULL)
        frame->base.release(&frame->base);
    (void)popup_id;
    (void)name;
    (void)disposition;
    (void)features;
    (void)window;
    if (client != NULL && *client != NULL) {
        (*client)->base.release(&(*client)->base);
        *client = NULL;
    }
    (void)settings;
    if (extra != NULL && *extra != NULL) {
        (*extra)->base.release(&(*extra)->base);
        *extra = NULL;
    }
    (void)no_access;
    gem_text(target, sizeof(target), url);
    if (gesture && target[0])
        gemscape_cef_load_url(target);
    else
        gem_status("Popup blocked.");
    return 1;
}

static cef_display_handler_t *CEF_CALLBACK
gem_client_get_display_handler(cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;

    client->display_handler->api.base.add_ref(
        &client->display_handler->api.base);
    return &client->display_handler->api;
}

static cef_load_handler_t *CEF_CALLBACK
gem_client_get_load_handler(cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;
    client->load_handler->api.base.add_ref(&client->load_handler->api.base);
    return &client->load_handler->api;
}

static void CEF_CALLBACK gem_life_span_on_before_close(
    cef_life_span_handler_t *self, cef_browser_t *browser)
{
    (void)self;
    if (g_state.browser == browser) {
        g_state.browser->base.release(&g_state.browser->base);
        g_state.browser = NULL;
    }
    g_state.browser_closed = 1;
    if (browser != NULL)
        browser->base.release(&browser->base);
}

static cef_life_span_handler_t *CEF_CALLBACK
gem_client_get_life_span_handler(cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;

    client->life_span_handler->api.base.add_ref(
        &client->life_span_handler->api.base);
    return &client->life_span_handler->api;
}

static gem_cef_renderer_t *gem_renderer_create(uint16_t width, uint16_t height)
{
    gem_cef_renderer_t *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
        return NULL;
    gem_base_init(&renderer->api.base, sizeof(renderer->api),
                  gem_renderer_add_ref, gem_renderer_release,
                  gem_renderer_has_one_ref, gem_renderer_has_ref);
    renderer->refs = 1;
    renderer->width = width;
    renderer->height = height;
    renderer->api.get_view_rect = gem_renderer_get_view_rect;
    renderer->api.on_paint = gem_renderer_on_paint;
    renderer->api.on_popup_show = gem_popup_show;
    renderer->api.on_popup_size = gem_popup_size;
    pthread_mutex_init(&renderer->mutex, NULL);
    return renderer;
}

static gem_cef_client_t *gem_client_create(gem_cef_renderer_t *renderer)
{
    gem_cef_client_t *client = calloc(1, sizeof(*client));
    if (client == NULL)
        return NULL;
    gem_base_init(&client->api.base, sizeof(client->api), gem_client_add_ref,
                  gem_client_release, gem_client_has_one_ref,
                  gem_client_has_ref);
    client->refs = 1;
    client->renderer = renderer;
    client->load_handler = calloc(1, sizeof(*client->load_handler));
    client->display_handler = calloc(1, sizeof(*client->display_handler));
    client->life_span_handler = calloc(1, sizeof(*client->life_span_handler));
    if (client->load_handler == NULL || client->display_handler == NULL ||
        client->life_span_handler == NULL) {
        free(client->life_span_handler);
        free(client->display_handler);
        free(client->load_handler);
        free(client);
        return NULL;
    }
    gem_base_init(&client->load_handler->api.base,
                  sizeof(client->load_handler->api), gem_load_add_ref,
                  gem_load_release, gem_load_has_one_ref, gem_load_has_ref);
    client->load_handler->refs = 1;
    client->load_handler->api.on_loading_state_change = gem_loading_state;
    client->load_handler->api.on_load_error = gem_load_error;
    client->load_handler->api.on_load_start = gem_load_on_load_start;
    client->load_handler->api.on_load_end = gem_load_on_load_end;
    gem_base_init(&client->display_handler->api.base,
                  sizeof(client->display_handler->api), gem_display_add_ref,
                  gem_display_release, gem_display_has_one_ref,
                  gem_display_has_ref);
    client->display_handler->refs = 1;
    client->display_handler->api.on_address_change = gem_address_changed;
    client->display_handler->api.on_title_change = gem_title_changed;
    gem_base_init(&client->life_span_handler->api.base,
                  sizeof(client->life_span_handler->api), gem_life_span_add_ref,
                  gem_life_span_release, gem_life_span_has_one_ref,
                  gem_life_span_has_ref);
    client->life_span_handler->refs = 1;
    client->life_span_handler->api.on_before_popup = gem_before_popup;
    client->life_span_handler->api.on_before_close =
        gem_life_span_on_before_close;
    client->api.get_render_handler = gem_client_get_render_handler;
    client->api.get_load_handler = gem_client_get_load_handler;
    client->api.get_display_handler = gem_client_get_display_handler;
    client->api.get_life_span_handler = gem_client_get_life_span_handler;
    return client;
}

static void gem_string(cef_string_t *value, const char *text)
{
    if (text != NULL)
        cef_string_utf8_to_utf16(text, strlen(text), value);
}

static void gem_settings_path(cef_string_t *value, const char *path)
{
    gem_string(value, path);
}

extern int gemscape_cef_start(int argc, char **argv, uint16_t width,
                              uint16_t height, const char *url,
                              const struct gemscape_preferences *preferences)
{
    cef_main_args_t args = {argc, argv};
    cef_settings_t settings;
    cef_window_info_t window;
    cef_browser_settings_t browser_settings;
    cef_request_context_t *context = NULL;
    cef_string_t initial_url = {0};
    char cache_root[2048];
    char cache_path[2200];
    char lock_path[2200];
    char resources[512];
    char locales[1024];

    memset(&g_state, 0, sizeof(g_state));
    memset(&settings, 0, sizeof(settings));
    settings.size = sizeof(settings);
    settings.no_sandbox = 1;
    settings.windowless_rendering_enabled = 1;
    settings.log_severity = LOGSEVERITY_INFO;
    g_state.preferences = *preferences;
    g_state.profile_lock = -1;
    if (!gemscape_profile_path(cache_root, sizeof(cache_root))) {
        fprintf(stderr, "Gemscape: cannot create the browser profile\n");
        return 0;
    }
    snprintf(lock_path, sizeof(lock_path), "%s/gemscape.lock", cache_root);
    g_state.profile_lock =
        open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (g_state.profile_lock < 0 ||
        flock(g_state.profile_lock, LOCK_EX | LOCK_NB) != 0) {
        if (g_state.profile_lock >= 0)
            close(g_state.profile_lock);
        g_state.profile_lock = -1;
        fprintf(stderr, "Gemscape: profile is already in use or unavailable\n");
        return 0;
    }
    /* The cross-container flock is authoritative for this dedicated profile.
     * Chromium's hostname/PID symlinks can outlive a killed Docker namespace.
     */
    const char *singletons[] = {"SingletonLock", "SingletonCookie",
                                "SingletonSocket"};
    for (size_t i = 0; i < sizeof(singletons) / sizeof(singletons[0]); ++i) {
        struct stat info;
        snprintf(lock_path, sizeof(lock_path), "%s/%s", cache_root,
                 singletons[i]);
        if (lstat(lock_path, &info) == 0 && S_ISLNK(info.st_mode) &&
            info.st_uid == geteuid())
            unlink(lock_path);
    }
    snprintf(cache_path, sizeof(cache_path), "%s/cef.log", cache_root);
    gem_string(&settings.log_file, cache_path);
    gem_settings_path(&settings.root_cache_path, cache_root);
    snprintf(cache_path, sizeof(cache_path), "%s/profile", cache_root);
    gem_settings_path(&settings.cache_path, cache_path);
    settings.persist_session_cookies = preferences->session_cookies;
    settings.cookieable_schemes_exclude_defaults = !preferences->cookies;
    snprintf(resources, sizeof(resources), "%s/Resources", GEM_CEF_ROOT_PATH);
    snprintf(locales, sizeof(locales), "%s/locales", resources);
    gem_string(&settings.resources_dir_path, resources);
    gem_string(&settings.locales_dir_path, locales);

    gem_app_init();
    (void)cef_api_hash(CEF_API_VERSION, 0);
    int initialized = cef_initialize(&args, &settings, &g_app, NULL);
    cef_string_clear(&settings.log_file);
    cef_string_clear(&settings.root_cache_path);
    cef_string_clear(&settings.cache_path);
    cef_string_clear(&settings.resources_dir_path);
    cef_string_clear(&settings.locales_dir_path);
    if (!initialized) {
        close(g_state.profile_lock);
        g_state.profile_lock = -1;
        return 0;
    }
    g_state.initialized = 1;
    g_state.renderer = gem_renderer_create(width, height);
    if (g_state.renderer != NULL)
        g_state.renderer->api.on_text_selection_changed = gem_selection;
    g_state.dialog = calloc(1, sizeof(*g_state.dialog));
    if (g_state.dialog == NULL) {
        gemscape_cef_stop();
        return 0;
    }
    gem_base_init(&g_state.dialog->api.base, sizeof(g_state.dialog->api),
                  gem_dialog_add_ref, gem_dialog_release,
                  gem_dialog_has_one_ref, gem_dialog_has_ref);
    g_state.dialog->refs = 1;
    g_state.dialog->api.on_jsdialog = gem_script_dialog;
    g_state.dialog->api.on_before_unload_dialog = gem_before_unload;
    g_state.client =
        g_state.renderer != NULL ? gem_client_create(g_state.renderer) : NULL;
    if (g_state.renderer == NULL || g_state.client == NULL) {
        gemscape_cef_stop();
        return 0;
    }
    g_state.client->api.get_jsdialog_handler = gem_get_dialog;

    memset(&window, 0, sizeof(window));
    window.size = sizeof(window);
    window.windowless_rendering_enabled = 1;
    memset(&browser_settings, 0, sizeof(browser_settings));
    browser_settings.size = sizeof(browser_settings);
    browser_settings.windowless_frame_rate = preferences->frame_rate;
    browser_settings.javascript =
        preferences->javascript ? STATE_ENABLED : STATE_DISABLED;
    browser_settings.image_loading =
        preferences->images ? STATE_ENABLED : STATE_DISABLED;
    browser_settings.javascript_close_windows = STATE_DISABLED;
    if (!preferences->persistent) {
        cef_request_context_settings_t request = {0};
        request.size = sizeof(request);
        request.cookieable_schemes_exclude_defaults = !preferences->cookies;
        context = cef_request_context_create_context(&request, NULL);
        if (context == NULL) {
            gemscape_cef_stop();
            return 0;
        }
    }
    gem_string(&initial_url, url);
    g_state.client->api.base.add_ref(&g_state.client->api.base);
    g_state.browser = cef_browser_host_create_browser_sync(
        &window, &g_state.client->api, &initial_url, &browser_settings, NULL,
        context);
    cef_string_clear(&initial_url);
    if (g_state.browser == NULL) {
        gemscape_cef_stop();
        return 0;
    }
    g_state.browser_closed = 0;
    g_state.observer = calloc(1, sizeof(*g_state.observer));
    if (g_state.observer == NULL) {
        gemscape_cef_stop();
        return 0;
    }
    gem_base_init(&g_state.observer->api.base, sizeof(g_state.observer->api),
                  gem_observer_add_ref, gem_observer_release,
                  gem_observer_has_one_ref, gem_observer_has_ref);
    g_state.observer->refs = 1;
    g_state.observer->api.on_dev_tools_method_result = gem_devtools_result;
    cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
    g_state.observer->api.base.add_ref(&g_state.observer->api.base);
    g_state.registration =
        host->add_dev_tools_message_observer(host, &g_state.observer->api);
    host->base.release(&host->base);
    gemscape_cef_zoom(preferences->zoom);
    return 1;
}

extern int gemscape_cef_execute_process(int argc, char **argv)
{
    cef_main_args_t args = {argc, argv};
    gem_app_init();
    (void)cef_api_hash(CEF_API_VERSION, 0);
    return cef_execute_process(&args, &g_app, NULL);
}

extern void gemscape_cef_pump(void)
{
    if (g_state.initialized) {
        cef_do_message_loop_work();
        if (g_state.browser != NULL && g_state.metrics_id == 0 &&
            gem_os_ticks_ms() - g_state.metrics_tick >= 150u) {
            g_state.metrics_tick = gem_os_ticks_ms();
            g_state.metrics_id = gem_devtools("Page.getLayoutMetrics", NULL);
        }
    }
}

extern void gemscape_cef_resize(uint16_t width, uint16_t height)
{
    gem_cef_renderer_t *renderer = g_state.renderer;
    if (!g_state.initialized || renderer == NULL || width == 0u || height == 0u)
        return;
    pthread_mutex_lock(&renderer->mutex);
    renderer->width = width;
    renderer->height = height;
    renderer->ready = 0;
    pthread_mutex_unlock(&renderer->mutex);
    if (g_state.browser != NULL) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->was_resized(host);
        host->base.release(&host->base);
    }
}

extern int gemscape_cef_take_frame(uint16_t *width, uint16_t *height,
                                   const uint8_t **pixels, uint32_t *size)
{
    gem_cef_renderer_t *renderer = g_state.renderer;
    size_t frame_size;
    if (!g_state.initialized || renderer == NULL)
        return 0;
    pthread_mutex_lock(&renderer->mutex);
    if (!renderer->ready) {
        pthread_mutex_unlock(&renderer->mutex);
        return 0;
    }
    frame_size = (size_t)renderer->frame_width * renderer->frame_height * 4u;
    uint8_t *frame = realloc(g_state.frame, frame_size);
    if (frame == NULL) {
        pthread_mutex_unlock(&renderer->mutex);
        return 0;
    }
    g_state.frame = frame;
    memcpy(g_state.frame, renderer->pixels, frame_size);
    if (renderer->popup_visible && renderer->popup != NULL) {
        for (int row = 0; row < renderer->popup_height; ++row) {
            int y = renderer->popup_rect.y + row;
            if (y < 0 || y >= renderer->frame_height)
                continue;
            int left = renderer->popup_rect.x < 0 ? -renderer->popup_rect.x : 0;
            int right = renderer->popup_width;
            if (renderer->popup_rect.x + right > renderer->frame_width)
                right = renderer->frame_width - renderer->popup_rect.x;
            if (right > left)
                memcpy(g_state.frame + ((size_t)y * renderer->frame_width +
                                        renderer->popup_rect.x + left) *
                                           4,
                       renderer->popup +
                           ((size_t)row * renderer->popup_width + left) * 4,
                       (size_t)(right - left) * 4);
        }
    }
    *width = renderer->frame_width;
    *height = renderer->frame_height;
    *pixels = g_state.frame;
    *size = (uint32_t)frame_size;
    g_state.frame_size = frame_size;
    renderer->ready = 0;
    pthread_mutex_unlock(&renderer->mutex);
    return 1;
}

extern void gemscape_cef_load_url(const char *url)
{
    if (g_state.browser != NULL && url != NULL) {
        cef_string_t target = {0};
        gem_string(&target, url);
        cef_frame_t *frame = g_state.browser->get_main_frame(g_state.browser);
        frame->load_url(frame, &target);
        frame->base.release(&frame->base);
        cef_string_clear(&target);
    }
}

static uint32_t gem_mouse_flags(uint16_t modifiers)
{
    uint32_t flags = 0;
    if (g_state.mouse_buttons & 1)
        flags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (g_state.mouse_buttons & 2)
        flags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (modifiers & (K_LSHIFT | K_RSHIFT))
        flags |= EVENTFLAG_SHIFT_DOWN;
    if (modifiers & K_CTRL)
        flags |= EVENTFLAG_CONTROL_DOWN;
    if (modifiers & K_ALT)
        flags |= EVENTFLAG_ALT_DOWN;
    return flags;
}

extern void gemscape_cef_mouse_move(int x, int y, int leave, uint16_t modifiers)
{
    if (g_state.browser != NULL) {
        cef_mouse_event_t event = {x, y, gem_mouse_flags(modifiers)};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->send_mouse_move_event(host, &event, leave);
        host->base.release(&host->base);
    }
}

extern void gemscape_cef_mouse_click(int x, int y, int button, int mouse_up,
                                     int clicks, uint16_t modifiers)
{
    if (g_state.browser != NULL) {
        if (mouse_up)
            g_state.mouse_buttons &= ~button;
        else
            g_state.mouse_buttons |= button;
        cef_mouse_event_t event = {x, y, gem_mouse_flags(modifiers)};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->set_focus(host, 1);
        host->send_mouse_click_event(
            host, &event, button == 2 ? MBT_RIGHT : MBT_LEFT, mouse_up, clicks);
        host->base.release(&host->base);
    }
}

extern void gemscape_cef_focus(int focused)
{
    if (g_state.browser != NULL) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->set_focus(host, focused != 0);
        host->base.release(&host->base);
    }
}

static int gemscape_windows_key_code(uint16_t key)
{
    unsigned int ascii = key & 0xffu;
    unsigned int scan = (key >> 8) & 0xffu;

    if (scan >= 4u && scan <= 29u)
        return 'A' + (int)(scan - 4u);
    if (scan >= 30u && scan <= 38u)
        return '1' + (int)(scan - 30u);
    if (scan == 39u)
        return '0';
    switch (scan) {
        case 40u:
            return 13;
        case 41u:
            return 27;
        case 42u:
            return 8;
        case 43u:
            return 9;
        case 44u:
            return 32;
        case 45u:
            return 0xbd; /* VK_OEM_MINUS */
        case 46u:
            return 0xbb; /* VK_OEM_PLUS */
        case 47u:
            return 0xdb; /* VK_OEM_4: [ */
        case 48u:
            return 0xdd; /* VK_OEM_6: ] */
        case 49u:
            return 0xdc; /* VK_OEM_5: backslash */
        case 50u:
            return 0xe2; /* VK_OEM_102 */
        case 51u:
            return 0xba; /* VK_OEM_1: ; */
        case 52u:
            return 0xde; /* VK_OEM_7: quote */
        case 53u:
            return 0xc0; /* VK_OEM_3: grave */
        case 54u:
            return 0xbc; /* VK_OEM_COMMA */
        case 55u:
            return 0xbe; /* VK_OEM_PERIOD */
        case 56u:
            return 0xbf; /* VK_OEM_2: slash */
        case 74u:
            return 36;
        case 75u:
            return 33;
        case 76u:
            return 46;
        case 77u:
            return 35;
        case 78u:
            return 34;
        case 79u:
            return 39;
        case 80u:
            return 37;
        case 81u:
            return 40;
        case 82u:
            return 38;
        case 84u:
            return 0x6f; /* VK_DIVIDE */
        case 85u:
            return 0x6a; /* VK_MULTIPLY */
        case 86u:
            return 0x6d; /* VK_SUBTRACT */
        case 87u:
            return 0x6b; /* VK_ADD */
        case 88u:
            return 13;
        case 89u:
        case 90u:
        case 91u:
        case 92u:
        case 93u:
        case 94u:
        case 95u:
        case 96u:
        case 97u:
            return 0x61 + (int)(scan - 89u); /* VK_NUMPAD1..9 */
        case 98u:
            return 0x60; /* VK_NUMPAD0 */
        case 99u:
            return 0x6e; /* VK_DECIMAL */
        default:
            return ascii != 0u ? (int)ascii : (int)scan;
    }
}

extern void gemscape_cef_key(uint16_t key, uint16_t modifiers)
{
    cef_key_event_t event;
    cef_browser_host_t *host;
    unsigned int ascii = key & 0xffu;

    if (g_state.browser == NULL)
        return;
    if ((key >> 8) == 40u || (key >> 8) == 88u)
        ascii = 13u;
    if (modifiers & K_CTRL) {
        if ((key >> 8) == 6u || (key >> 8) == 27u)
            gemscape_cef_set_clipboard(g_state.selection);
        if ((key >> 8) == 25u && g_state.clipboard[0]) {
            cef_dictionary_value_t *parameters = cef_dictionary_value_create();
            gem_parameter(parameters, "text", g_state.clipboard);
            gem_devtools("Input.insertText", parameters);
            parameters->base.release(&parameters->base);
            return;
        }
    }
    host = g_state.browser->get_host(g_state.browser);
    memset(&event, 0, sizeof(event));
    event.size = sizeof(event);
    if ((modifiers & (K_LSHIFT | K_RSHIFT)) != 0u)
        event.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if ((modifiers & K_CTRL) != 0u)
        event.modifiers |= EVENTFLAG_CONTROL_DOWN;
    if ((modifiers & K_ALT) != 0u)
        event.modifiers |= EVENTFLAG_ALT_DOWN;
    event.windows_key_code = gemscape_windows_key_code(key);
    event.native_key_code = (int)((key >> 8) & 0xffu);
    event.unmodified_character = (char16_t)ascii;
    event.character = (char16_t)ascii;

    host->set_focus(host, 1);
    event.type = KEYEVENT_RAWKEYDOWN;
    host->send_key_event(host, &event);
    if (((ascii >= 32u && ascii < 127u) || ascii == 13u) &&
        (modifiers & (K_CTRL | K_ALT)) == 0u) {
        event.type = KEYEVENT_CHAR;
        host->send_key_event(host, &event);
    }
    event.type = KEYEVENT_KEYUP;
    host->send_key_event(host, &event);
    host->base.release(&host->base);
}

extern void gemscape_cef_mouse_wheel(int x, int y, int delta_y)
{
    if (g_state.browser != NULL) {
        cef_mouse_event_t event = {x, y, 0};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->send_mouse_wheel_event(host, &event, 0, delta_y);
        host->base.release(&host->base);
    }
}

extern int gemscape_cef_get_scroll(int *x, int *y, int *viewport_width,
                                   int *viewport_height, int *content_width,
                                   int *content_height)
{
    if (!g_state.initialized || !g_state.scroll_ready)
        return 0;
    if (x != NULL)
        *x = g_state.scroll_x;
    if (y != NULL)
        *y = g_state.scroll_y;
    if (viewport_width != NULL)
        *viewport_width = g_state.viewport_width;
    if (viewport_height != NULL)
        *viewport_height = g_state.viewport_height;
    if (content_width != NULL)
        *content_width = g_state.content_width;
    if (content_height != NULL)
        *content_height = g_state.content_height;
    return 1;
}

extern void gemscape_cef_scroll_to(int x, int y)
{
    cef_dictionary_value_t *parameters = cef_dictionary_value_create();
    char expression[128];
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    snprintf(expression, sizeof(expression),
             "window.scrollTo({left:%d,top:%d,behavior:'instant'})", x, y);
    gem_parameter(parameters, "expression", expression);
    gem_devtools("Runtime.evaluate", parameters);
    parameters->base.release(&parameters->base);
}

extern const gemscape_browser_info_t *gemscape_cef_info(void)
{
    return &g_state.info;
}

const char *gemscape_cef_clipboard(void)
{
    return g_state.clipboard;
}

void gemscape_cef_set_clipboard(const char *text)
{
    snprintf(g_state.clipboard, sizeof(g_state.clipboard), "%s", text);
}

extern void gemscape_cef_zoom(int percent)
{
    if (g_state.browser != NULL && percent >= 50 && percent <= 200) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->set_zoom_level(host, log(percent / 100.0) / log(1.2));
        host->base.release(&host->base);
        g_state.preferences.zoom = percent;
    }
}

extern void gemscape_cef_cancel_load(void)
{
    if (g_state.browser != NULL)
        g_state.browser->stop_load(g_state.browser);
}

extern int gemscape_cef_clear_data(int kind)
{
    cef_dictionary_value_t *parameters = NULL;
    const char *method;
    if (g_state.browser == NULL || g_state.clear_id != 0)
        return 0;
    if (kind == 0)
        method = "Network.clearBrowserCookies";
    else if (kind == 1)
        method = "Network.clearBrowserCache";
    else {
        cef_urlparts_t parts = {0};
        cef_string_t url = {0};
        char origin[2048];
        parts.size = sizeof(parts);
        gem_string(&url, g_state.info.url);
        int valid = cef_parse_url(&url, &parts);
        gem_text(origin, sizeof(origin), &parts.origin);
        cef_string_clear(&url);
        cef_string_clear(&parts.spec);
        cef_string_clear(&parts.scheme);
        cef_string_clear(&parts.username);
        cef_string_clear(&parts.password);
        cef_string_clear(&parts.host);
        cef_string_clear(&parts.port);
        cef_string_clear(&parts.origin);
        cef_string_clear(&parts.path);
        cef_string_clear(&parts.query);
        cef_string_clear(&parts.fragment);
        if (!valid || (strncmp(origin, "http://", 7) != 0 &&
                       strncmp(origin, "https://", 8) != 0)) {
            gem_status("Open an HTTP or HTTPS page to clear its site data.");
            return 0;
        }
        parameters = cef_dictionary_value_create();
        gem_parameter(parameters, "origin", origin);
        gem_parameter(parameters, "storageTypes", "all");
        method = "Storage.clearDataForOrigin";
    }
    g_state.clear_id = gem_devtools(method, parameters);
    if (parameters != NULL)
        parameters->base.release(&parameters->base);
    g_state.info.clearing = g_state.clear_id != 0;
    gem_status(g_state.clear_id ? "Clearing browsing data..."
                                : "Could not clear browsing data.");
    return g_state.clear_id != 0;
}

extern void gemscape_cef_back(void)
{
    if (g_state.browser != NULL &&
        g_state.browser->can_go_back(g_state.browser))
        g_state.browser->go_back(g_state.browser);
}

extern void gemscape_cef_forward(void)
{
    if (g_state.browser != NULL &&
        g_state.browser->can_go_forward(g_state.browser))
        g_state.browser->go_forward(g_state.browser);
}

extern void gemscape_cef_reload(void)
{
    if (g_state.browser != NULL)
        g_state.browser->reload(g_state.browser);
}

extern void gemscape_cef_stop(void)
{
    unsigned int attempts;

    if (!g_state.initialized)
        return;
    if (g_state.registration != NULL) {
        g_state.registration->base.release(&g_state.registration->base);
        g_state.registration = NULL;
    }
    if (g_state.browser != NULL) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->close_browser(host, 1);
        host->base.release(&host->base);
        for (attempts = 0u; attempts < 5000u && !g_state.browser_closed;
             ++attempts) {
            cef_do_message_loop_work();
            gem_os_sleep_ms(1u);
        }
        if (g_state.browser != NULL) {
            g_state.browser->base.release(&g_state.browser->base);
            g_state.browser = NULL;
        }
    }
    cef_shutdown();
    if (g_state.dialog != NULL) {
        g_state.dialog->api.base.release(&g_state.dialog->api.base);
        g_state.dialog = NULL;
    }
    if (g_state.observer != NULL) {
        g_state.observer->api.base.release(&g_state.observer->api.base);
        g_state.observer = NULL;
    }
    if (g_state.client != NULL) {
        g_state.client->load_handler->api.base.release(
            &g_state.client->load_handler->api.base);
        g_state.client->display_handler->api.base.release(
            &g_state.client->display_handler->api.base);
        g_state.client->life_span_handler->api.base.release(
            &g_state.client->life_span_handler->api.base);
        g_state.client->api.base.release(&g_state.client->api.base);
        g_state.client = NULL;
    }
    if (g_state.renderer != NULL) {
        free(g_state.renderer->pixels);
        free(g_state.renderer->popup);
        pthread_mutex_destroy(&g_state.renderer->mutex);
        g_state.renderer->api.base.release(&g_state.renderer->api.base);
        g_state.renderer = NULL;
    }
    if (g_state.profile_lock >= 0) {
        close(g_state.profile_lock);
        g_state.profile_lock = -1;
    }
    free(g_state.frame);
    g_state.frame = NULL;
    g_state.frame_size = 0;
    g_state.scroll_ready = 0;
    g_state.initialized = 0;
}

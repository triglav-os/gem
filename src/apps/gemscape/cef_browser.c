/*
 * Embeds CEF as Gemscape's off-screen browser, forwards browser input and
 * navigation, reports page scroll metrics and manages CEF process lifetime.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "cef_browser.h"

#include <gem/os.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_display_handler_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_load_handler_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/cef_api_hash.h"
#include "include/internal/cef_string_types.h"
#include "include/internal/cef_types.h"

/* Include AES after CEF: both APIs define a CT_MOVE identifier. */
#include <gem/aes.h>

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct gem_cef_renderer {
    cef_render_handler_t api;
    int refs;
    uint16_t width;
    uint16_t height;
    int ready;
    uint8_t *pixels;
    pthread_mutex_t mutex;
} gem_cef_renderer_t;

typedef struct gem_cef_load_handler {
    cef_load_handler_t api;
    int refs;
} gem_cef_load_handler_t;

typedef struct gem_cef_display_handler {
    cef_display_handler_t api;
    int refs;
} gem_cef_display_handler_t;

typedef struct gem_cef_life_span_handler {
    cef_life_span_handler_t api;
    int refs;
} gem_cef_life_span_handler_t;

typedef struct gem_cef_client {
    cef_client_t api;
    int refs;
    gem_cef_renderer_t *renderer;
    gem_cef_load_handler_t *load_handler;
    gem_cef_display_handler_t *display_handler;
    gem_cef_life_span_handler_t *life_span_handler;
} gem_cef_client_t;

static cef_app_t g_app;
static int g_app_refs;

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
} g_state;

static void gem_string(cef_string_t *value, const char *text);

static void gem_base_init(cef_base_ref_counted_t *base, size_t size,
                          void (CEF_CALLBACK *add_ref)(cef_base_ref_counted_t *),
                          int (CEF_CALLBACK *release)(cef_base_ref_counted_t *),
                          int (CEF_CALLBACK *has_one_ref)(cef_base_ref_counted_t *),
                          int (CEF_CALLBACK *has_ref)(cef_base_ref_counted_t *)) {
    base->size = size;
    base->add_ref = add_ref;
    base->release = release;
    base->has_one_ref = has_one_ref;
    base->has_at_least_one_ref = has_ref;
}

#define GEM_REF_FUNCS(prefix, type) \
    static void CEF_CALLBACK prefix##_add_ref(cef_base_ref_counted_t *base) { \
        type *object = (type *)((char *)base - offsetof(type, api.base)); \
        ++object->refs; \
    } \
    static int CEF_CALLBACK prefix##_release(cef_base_ref_counted_t *base) { \
        type *object = (type *)((char *)base - offsetof(type, api.base)); \
        if (--object->refs != 0) return 0; \
        free(object); \
        return 1; \
    } \
    static int CEF_CALLBACK prefix##_has_one_ref(cef_base_ref_counted_t *base) { \
        type *object = (type *)((char *)base - offsetof(type, api.base)); \
        return object->refs == 1; \
    } \
    static int CEF_CALLBACK prefix##_has_ref(cef_base_ref_counted_t *base) { \
        type *object = (type *)((char *)base - offsetof(type, api.base)); \
        return object->refs >= 1; \
    }

GEM_REF_FUNCS(gem_renderer, gem_cef_renderer_t)
GEM_REF_FUNCS(gem_client, gem_cef_client_t)
GEM_REF_FUNCS(gem_load, gem_cef_load_handler_t)
GEM_REF_FUNCS(gem_display, gem_cef_display_handler_t)
GEM_REF_FUNCS(gem_life_span, gem_cef_life_span_handler_t)

static void CEF_CALLBACK gem_app_add_ref(cef_base_ref_counted_t *base)
{
    (void)base;
    ++g_app_refs;
}

static int CEF_CALLBACK gem_app_release(cef_base_ref_counted_t *base)
{
    (void)base;
    if (g_app_refs > 1)
        --g_app_refs;
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

static void CEF_CALLBACK gem_app_before_command_line(
    cef_app_t *self, const cef_string_t *process_type,
    cef_command_line_t *command_line)
{
    (void)self;
    (void)process_type;
    gem_command_switch(command_line, "no-zygote");
}

static void gem_app_init(void)
{
    memset(&g_app, 0, sizeof(g_app));
    gem_base_init(&g_app.base, sizeof(g_app), gem_app_add_ref, gem_app_release,
                  gem_app_has_one_ref, gem_app_has_ref);
    g_app.on_before_command_line_processing = gem_app_before_command_line;
    g_app_refs = 1;
}

static void CEF_CALLBACK gem_renderer_get_view_rect(
    cef_render_handler_t *self, cef_browser_t *browser, cef_rect_t *rect) {
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    (void)browser;
    rect->x = 0;
    rect->y = 0;
    rect->width = renderer->width;
    rect->height = renderer->height;
}

static void CEF_CALLBACK gem_renderer_on_paint(
    cef_render_handler_t *self, cef_browser_t *browser,
    cef_paint_element_type_t type, size_t dirty_count,
    const cef_rect_t *dirty_rects, const void *buffer, int width, int height) {
    gem_cef_renderer_t *renderer = (gem_cef_renderer_t *)self;
    size_t size = (size_t)width * (size_t)height * 4u;
    (void)browser;
    (void)dirty_count;
    (void)dirty_rects;
    if (type != PET_VIEW || width <= 0 || height <= 0)
        return;
    pthread_mutex_lock(&renderer->mutex);
    if (size > (size_t)UINT16_MAX * (size_t)UINT16_MAX * 4u) {
        pthread_mutex_unlock(&renderer->mutex);
        return;
    }
    uint8_t *pixels = realloc(renderer->pixels, size);
    if (pixels != NULL) {
        renderer->pixels = pixels;
        memcpy(renderer->pixels, buffer, size);
        renderer->width = (uint16_t)width;
        renderer->height = (uint16_t)height;
        renderer->ready = 1;
    }
    pthread_mutex_unlock(&renderer->mutex);
}

static cef_render_handler_t *CEF_CALLBACK gem_client_get_render_handler(
    cef_client_t *self) {
    gem_cef_client_t *client = (gem_cef_client_t *)self;
    client->renderer->api.base.add_ref(&client->renderer->api.base);
    return &client->renderer->api;
}

static void gem_load_install_page_bridge(cef_frame_t *frame)
{
    static const char script[] =
        "(function(){"
        "if(window.__gemscapeBridge)return;window.__gemscapeBridge=1;"
        "var s=document.createElement('style');"
        "s.textContent='*::-webkit-scrollbar{width:0!important;height:0!important}'"
        "+'html,body{scrollbar-width:none!important}';"
        "(document.head||document.documentElement).appendChild(s);"
        "var report=function(){var d=document.documentElement,b=document.body;"
        "var cw=Math.max(d?d.scrollWidth:0,b?b.scrollWidth:0,innerWidth);"
        "var ch=Math.max(d?d.scrollHeight:0,b?b.scrollHeight:0,innerHeight);"
        "console.info('__GEMSCAPE_SCROLL__ '+Math.round(scrollX)+' '+"
        "Math.round(scrollY)+' '+innerWidth+' '+innerHeight+' '+cw+' '+ch);};"
        "addEventListener('scroll',report,{passive:true});"
        "addEventListener('resize',report);"
        "if(window.ResizeObserver){var ro=new ResizeObserver(report);"
        "ro.observe(document.documentElement);}"
        "report();})()";
    cef_string_t code = {0};
    cef_string_t source = {0};
    if (frame == NULL || frame->is_main(frame) == 0)
        return;
    gem_string(&code, script);
    gem_string(&source, "https://gemscape.local/style.js");
    frame->execute_java_script(frame, &code, &source, 1);
    cef_string_clear(&code);
    cef_string_clear(&source);
}

static void CEF_CALLBACK gem_load_on_load_start(
    cef_load_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    cef_transition_type_t transition_type) {
    (void)self;
    (void)browser;
    (void)transition_type;
    if (frame != NULL && frame->is_main(frame) != 0)
        g_state.scroll_ready = 0;
}

static void CEF_CALLBACK gem_load_on_load_end(
    cef_load_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    int http_status_code) {
    (void)self;
    (void)browser;
    (void)http_status_code;
    gem_load_install_page_bridge(frame);
}

static int CEF_CALLBACK gem_display_on_console_message(
    cef_display_handler_t *self, cef_browser_t *browser,
    cef_log_severity_t level, const cef_string_t *message,
    const cef_string_t *source, int line)
{
    cef_string_utf8_t text = {0};
    int x;
    int y;
    int viewport_width;
    int viewport_height;
    int content_width;
    int content_height;
    int matched = 0;

    (void)self;
    (void)browser;
    (void)level;
    (void)source;
    (void)line;
    if (message == NULL || message->str == NULL ||
        !cef_string_utf16_to_utf8(message->str, message->length, &text))
        return 0;
    if (text.str != NULL &&
        sscanf(text.str, "__GEMSCAPE_SCROLL__ %d %d %d %d %d %d", &x, &y,
               &viewport_width, &viewport_height, &content_width,
               &content_height) == 6) {
        g_state.scroll_x = x;
        g_state.scroll_y = y;
        g_state.viewport_width = viewport_width;
        g_state.viewport_height = viewport_height;
        g_state.content_width = content_width;
        g_state.content_height = content_height;
        g_state.scroll_ready = 1;
        matched = 1;
    }
    cef_string_utf8_clear(&text);
    return matched;
}

static cef_display_handler_t *CEF_CALLBACK gem_client_get_display_handler(
    cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;

    client->display_handler->api.base.add_ref(
        &client->display_handler->api.base);
    return &client->display_handler->api;
}

static cef_load_handler_t *CEF_CALLBACK gem_client_get_load_handler(
    cef_client_t *self) {
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
}

static cef_life_span_handler_t *CEF_CALLBACK
gem_client_get_life_span_handler(cef_client_t *self)
{
    gem_cef_client_t *client = (gem_cef_client_t *)self;

    client->life_span_handler->api.base.add_ref(
        &client->life_span_handler->api.base);
    return &client->life_span_handler->api;
}

static gem_cef_renderer_t *gem_renderer_create(uint16_t width, uint16_t height) {
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
    pthread_mutex_init(&renderer->mutex, NULL);
    return renderer;
}

static gem_cef_client_t *gem_client_create(gem_cef_renderer_t *renderer) {
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
    client->load_handler->api.on_load_start = gem_load_on_load_start;
    client->load_handler->api.on_load_end = gem_load_on_load_end;
    gem_base_init(&client->display_handler->api.base,
                  sizeof(client->display_handler->api), gem_display_add_ref,
                  gem_display_release, gem_display_has_one_ref,
                  gem_display_has_ref);
    client->display_handler->refs = 1;
    client->display_handler->api.on_console_message =
        gem_display_on_console_message;
    gem_base_init(&client->life_span_handler->api.base,
                  sizeof(client->life_span_handler->api),
                  gem_life_span_add_ref, gem_life_span_release,
                  gem_life_span_has_one_ref, gem_life_span_has_ref);
    client->life_span_handler->refs = 1;
    client->life_span_handler->api.on_before_close =
        gem_life_span_on_before_close;
    client->api.get_render_handler = gem_client_get_render_handler;
    client->api.get_load_handler = gem_client_get_load_handler;
    client->api.get_display_handler = gem_client_get_display_handler;
    client->api.get_life_span_handler = gem_client_get_life_span_handler;
    return client;
}

static void gem_string(cef_string_t *value, const char *text) {
    if (text != NULL)
        cef_string_utf8_to_utf16(text, strlen(text), value);
}

static void gem_settings_path(cef_string_t *value, const char *path) {
    gem_string(value, path);
}

extern int gemscape_cef_start(int argc, char **argv, uint16_t width,
                              uint16_t height, const char *url) {
    cef_main_args_t args = {argc, argv};
    cef_settings_t settings;
    cef_window_info_t window;
    cef_browser_settings_t browser_settings;
    cef_string_t initial_url = {0};
    char cache_root[128];
    char cache_path[160];
    char resources[512];
    char locales[1024];

    memset(&settings, 0, sizeof(settings));
    settings.size = sizeof(settings);
    settings.no_sandbox = 1;
    settings.windowless_rendering_enabled = 1;
    settings.log_severity = LOGSEVERITY_INFO;
    gem_string(&settings.log_file, "/tmp/gemscape-cef.log");
    snprintf(cache_root, sizeof(cache_root), "/tmp/gemscape-cef-%ld",
             (long)getpid());
    mkdir(cache_root, 0700);
    snprintf(cache_path, sizeof(cache_path), "%s/cache", cache_root);
    mkdir(cache_path, 0700);
    gem_settings_path(&settings.root_cache_path, cache_root);
    gem_settings_path(&settings.cache_path, cache_path);
    snprintf(resources, sizeof(resources), "%s/Resources",
             GEM_CEF_ROOT_PATH);
    snprintf(locales, sizeof(locales), "%s/locales", resources);
    gem_string(&settings.resources_dir_path, resources);
    gem_string(&settings.locales_dir_path, locales);

    gem_app_init();
    (void)cef_api_hash(CEF_API_VERSION, 0);
    if (!cef_initialize(&args, &settings, &g_app, NULL))
        return 0;
    g_state.renderer = gem_renderer_create(width, height);
    g_state.client = gem_client_create(g_state.renderer);
    if (g_state.renderer == NULL || g_state.client == NULL)
        return 0;

    memset(&window, 0, sizeof(window));
    window.size = sizeof(window);
    window.windowless_rendering_enabled = 1;
    memset(&browser_settings, 0, sizeof(browser_settings));
    browser_settings.size = sizeof(browser_settings);
    browser_settings.windowless_frame_rate = 30;
    gem_string(&initial_url, url);
    g_state.browser = cef_browser_host_create_browser_sync(
        &window, &g_state.client->api, &initial_url, &browser_settings, NULL,
        NULL);
    cef_string_clear(&initial_url);
    if (g_state.browser == NULL)
        return 0;
    g_state.browser_closed = 0;
    g_state.initialized = 1;
    return 1;
}

extern int gemscape_cef_execute_process(int argc, char **argv) {
    cef_main_args_t args = {argc, argv};
    gem_app_init();
    (void)cef_api_hash(CEF_API_VERSION, 0);
    return cef_execute_process(&args, &g_app, NULL);
}

extern void gemscape_cef_pump(void) {
    if (g_state.initialized)
        cef_do_message_loop_work();
}

extern void gemscape_cef_resize(uint16_t width, uint16_t height) {
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
    }
}

extern int gemscape_cef_take_frame(uint16_t *width, uint16_t *height,
                                   const uint8_t **pixels, uint32_t *size) {
    gem_cef_renderer_t *renderer = g_state.renderer;
    size_t frame_size;
    if (!g_state.initialized || renderer == NULL)
        return 0;
    pthread_mutex_lock(&renderer->mutex);
    if (!renderer->ready) {
        pthread_mutex_unlock(&renderer->mutex);
        return 0;
    }
    frame_size = (size_t)renderer->width * renderer->height * 4u;
    g_state.frame = realloc(g_state.frame, frame_size);
    if (g_state.frame == NULL) {
        pthread_mutex_unlock(&renderer->mutex);
        return 0;
    }
    memcpy(g_state.frame, renderer->pixels, frame_size);
    *width = renderer->width;
    *height = renderer->height;
    *pixels = g_state.frame;
    *size = (uint32_t)frame_size;
    g_state.frame_size = frame_size;
    renderer->ready = 0;
    pthread_mutex_unlock(&renderer->mutex);
    return 1;
}

extern void gemscape_cef_load_url(const char *url) {
    if (g_state.browser != NULL && url != NULL) {
        cef_string_t target = {0};
        gem_string(&target, url);
        g_state.browser->get_main_frame(g_state.browser)->load_url(
            g_state.browser->get_main_frame(g_state.browser), &target);
        cef_string_clear(&target);
    }
}

extern void gemscape_cef_mouse_move(int x, int y, int leave) {
    if (g_state.browser != NULL) {
        cef_mouse_event_t event = {x, y, 0};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->send_mouse_move_event(host, &event, leave);
    }
}

extern void gemscape_cef_mouse_click(int x, int y, int button, int mouse_up) {
    if (g_state.browser != NULL) {
        cef_mouse_event_t event = {
            x,
            y,
            mouse_up ? EVENTFLAG_NONE
                     : (button == 2 ? EVENTFLAG_RIGHT_MOUSE_BUTTON
                                    : EVENTFLAG_LEFT_MOUSE_BUTTON)};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        if (getenv("GEMSCAPE_TRACE") != NULL)
            fprintf(stderr, "gemscape: CEF click %d,%d button=%d up=%d\n", x,
                    y, button, mouse_up);
        host->set_focus(host, 1);
        host->send_mouse_click_event(host, &event,
                                     button == 2 ? MBT_RIGHT : MBT_LEFT,
                                     mouse_up, 1);
    }
}

extern void gemscape_cef_focus(int focused) {
    if (g_state.browser != NULL) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->set_focus(host, focused != 0);
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

extern void gemscape_cef_key(uint16_t key, uint16_t modifiers) {
    cef_key_event_t event;
    cef_browser_host_t *host;
    unsigned int ascii = key & 0xffu;

    if (g_state.browser == NULL)
        return;
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
    if (ascii >= 32u && ascii < 127u &&
        (modifiers & (K_CTRL | K_ALT)) == 0u) {
        event.type = KEYEVENT_CHAR;
        host->send_key_event(host, &event);
    }
    event.type = KEYEVENT_KEYUP;
    host->send_key_event(host, &event);
}

extern void gemscape_cef_mouse_wheel(int x, int y, int delta_y) {
    if (g_state.browser != NULL) {
        cef_mouse_event_t event = {x, y, 0};
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->send_mouse_wheel_event(host, &event, 0, delta_y);
    }
}

extern int gemscape_cef_get_scroll(int *x, int *y, int *viewport_width,
                                   int *viewport_height, int *content_width,
                                   int *content_height) {
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

extern void gemscape_cef_scroll_to(int x, int y) {
    if (g_state.browser != NULL) {
        cef_frame_t *frame = g_state.browser->get_main_frame(g_state.browser);
        cef_string_t code = {0};
        cef_string_t source = {0};
        char script[96];

        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        snprintf(script, sizeof(script), "window.scrollTo(%d,%d)", x, y);
        gem_string(&code, script);
        gem_string(&source, "https://gemscape.local/scroll.js");
        frame->execute_java_script(frame, &code, &source, 1);
        cef_string_clear(&code);
        cef_string_clear(&source);
    }
}

extern void gemscape_cef_back(void) {
    if (g_state.browser != NULL && g_state.browser->can_go_back(g_state.browser))
        g_state.browser->go_back(g_state.browser);
}

extern void gemscape_cef_forward(void) {
    if (g_state.browser != NULL &&
        g_state.browser->can_go_forward(g_state.browser))
        g_state.browser->go_forward(g_state.browser);
}

extern void gemscape_cef_reload(void) {
    if (g_state.browser != NULL)
        g_state.browser->reload(g_state.browser);
}

extern void gemscape_cef_stop(void) {
    unsigned int attempts;

    if (!g_state.initialized)
        return;
    if (g_state.browser != NULL) {
        cef_browser_host_t *host = g_state.browser->get_host(g_state.browser);
        host->close_browser(host, 1);
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
    free(g_state.frame);
    g_state.frame = NULL;
    g_state.frame_size = 0;
    g_state.scroll_ready = 0;
    g_state.initialized = 0;
}

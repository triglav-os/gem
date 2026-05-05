/*
 * Implements the GEM input abstraction on top of the rasta UDP event
 * stream. The backend subscribes to the emulator, polls non-blocking
 * datagrams, and translates rasta keyboard and mouse messages into GEM
 * input events.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "platform/hid.h"
#include "platform/raster.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

enum rasta_msgid {
    rasta_msg_key_down = 1,
    rasta_msg_key_up = 2,
    rasta_msg_mouse_move = 3,
    rasta_msg_mouse_left_down = 10,
    rasta_msg_mouse_left_up = 11,
    rasta_msg_mouse_middle_down = 20,
    rasta_msg_mouse_middle_up = 21,
    rasta_msg_mouse_right_down = 30,
    rasta_msg_mouse_right_up = 31
};

struct rasta_input_message {
    uint16_t msgid;
    int16_t par1;
    int16_t par2;
};

static int g_socket_fd = -1;
static int16_t g_mouse_x;
static int16_t g_mouse_y;
static uint16_t g_button_flags;

static const char *rasta_framebuffer_path(void)
{
    const char *value = getenv("GEM_RASTA_FRAMEBUFFER");

    if (value != NULL && value[0] != '\0') {
        return value;
    }

    value = getenv("RASTA_FRAMEBUFFER");
    if (value != NULL && value[0] != '\0') {
        return value;
    }

    return "/tmp/rasta.fb";
}

static const char *rasta_host(void)
{
    const char *value = getenv("GEM_RASTA_HOST");

    if (value != NULL && value[0] != '\0') {
        return value;
    }

    value = getenv("RASTA_HOST");
    if (value != NULL && value[0] != '\0') {
        return value;
    }

    return "127.0.0.1";
}

static uint16_t rasta_port(void)
{
    const char *value = getenv("GEM_RASTA_PORT");
    char *end = NULL;
    unsigned long port;

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_PORT");
    }
    if (value == NULL || value[0] == '\0') {
        return 5000u;
    }

    port = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || port == 0ul || port > 65535ul) {
        return 5000u;
    }

    return (uint16_t) port;
}

static uint16_t rasta_scale(void)
{
    const char *value = getenv("GEM_RASTA_SCALE");
    char *end = NULL;
    unsigned long scale;

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_SCALE");
    }
    if (value == NULL || value[0] == '\0') {
        return 1u;
    }

    scale = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || scale == 0ul || scale > 65535ul) {
        return 1u;
    }

    return (uint16_t) scale;
}

static const char *rasta_cursor_mode(void)
{
    const char *value = getenv("GEM_RASTA_CURSOR");

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_CURSOR");
    }
    if (value == NULL || value[0] == '\0') {
        return "off";
    }

    if (strcmp(value, "on") == 0 || strcmp(value, "true") == 0 ||
        strcmp(value, "1") == 0) {
        return "on";
    }

    return "off";
}

static const char *rasta_inverse_mode(void)
{
    const char *value = getenv("GEM_RASTA_INVERSE");

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_INVERSE");
    }
    if (value == NULL || value[0] == '\0') {
        return "on";
    }

    if (strcmp(value, "off") == 0 || strcmp(value, "false") == 0 ||
        strcmp(value, "0") == 0) {
        return "off";
    }

    return "on";
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0) {
        return 0;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return 0;
    }

    return 1;
}

static uint16_t ntoh_i16(int16_t value)
{
    return ntohs((uint16_t) value);
}

static int receive_message(struct rasta_input_message *message)
{
    ssize_t received;

    received = recvfrom(g_socket_fd, message, sizeof(*message), 0, NULL,
        NULL);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }

    if (received != (ssize_t) sizeof(*message)) {
        return 0;
    }

    message->msgid = ntohs(message->msgid);
    message->par1 = (int16_t) ntoh_i16(message->par1);
    message->par2 = (int16_t) ntoh_i16(message->par2);
    return 1;
}

/*
 * Builds a subscriber datagram that asks rasta to reconfigure itself to
 * match GEM's active raster geometry and framebuffer path.
 */
static char *build_subscription_payload(void)
{
    const gem_raster_surface_t *surface = gem_raster_surface();
    const char *path = rasta_framebuffer_path();
    uint16_t scale = rasta_scale();
    const char *cursor_mode = rasta_cursor_mode();
    const char *inverse_mode = rasta_inverse_mode();
    const char *scan;
    size_t escaped_length;
    size_t total_length;
    char *payload;
    int prefix_length;
    char *cursor;

    if (surface == NULL || path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    escaped_length = strlen(path);
    for (scan = path; *scan != '\0'; ++scan) {
        if (*scan == '"' || *scan == '\\') {
            ++escaped_length;
        }
    }

    prefix_length = snprintf(NULL, 0,
        "--width %u --height %u --bpp 1 --scale %u --cursor %s --inverse %s "
        "--framebuffer \"",
        (unsigned) surface->width, (unsigned) surface->height,
        (unsigned) scale, cursor_mode, inverse_mode);
    if (prefix_length < 0) {
        errno = EOVERFLOW;
        return NULL;
    }

    total_length = (size_t) prefix_length + escaped_length + 2u;
    payload = calloc(1u, total_length);
    if (payload == NULL) {
        return NULL;
    }

    prefix_length = snprintf(payload, total_length,
        "--width %u --height %u --bpp 1 --scale %u --cursor %s --inverse %s "
        "--framebuffer \"",
        (unsigned) surface->width, (unsigned) surface->height,
        (unsigned) scale, cursor_mode, inverse_mode);
    if (prefix_length < 0 || (size_t) prefix_length >= total_length) {
        free(payload);
        errno = EOVERFLOW;
        return NULL;
    }

    cursor = payload + (size_t) prefix_length;
    while (*path != '\0') {
        if (*path == '"' || *path == '\\') {
            *cursor++ = '\\';
        }
        *cursor++ = *path++;
    }
    *cursor++ = '"';
    *cursor = '\0';
    return payload;
}

static void fill_mouse_move(gem_hid_event_t *evt,
    const struct rasta_input_message *message)
{
    evt->type = GEM_HID_MOUSE_MOVE;
    evt->flags = g_button_flags;
    evt->x = message->par1;
    evt->y = message->par2;
    evt->dx = (int16_t) (message->par1 - g_mouse_x);
    evt->dy = (int16_t) (message->par2 - g_mouse_y);
    evt->button = 0u;
    evt->key = 0u;
    evt->mod = 0u;

    g_mouse_x = message->par1;
    g_mouse_y = message->par2;
}

static void fill_button_event(gem_hid_event_t *evt, uint16_t button,
    int pressed, const struct rasta_input_message *message)
{
    if (pressed != 0) {
        g_button_flags = (uint16_t) (g_button_flags | button);
    } else {
        g_button_flags = (uint16_t) (g_button_flags & (uint16_t) ~button);
    }

    evt->type = GEM_HID_MOUSE_BUTTON;
    evt->flags = g_button_flags;
    evt->x = message->par1;
    evt->y = message->par2;
    evt->dx = (int16_t) (message->par1 - g_mouse_x);
    evt->dy = (int16_t) (message->par2 - g_mouse_y);
    evt->button = button;
    evt->key = 0u;
    evt->mod = 0u;

    g_mouse_x = message->par1;
    g_mouse_y = message->par2;
}

static void fill_key_event(gem_hid_event_t *evt,
    const struct rasta_input_message *message, int pressed)
{
    evt->type = GEM_HID_KEY;
    evt->flags = (uint16_t) ((pressed != 0) ? 1u : 0u);
    evt->x = g_mouse_x;
    evt->y = g_mouse_y;
    evt->dx = 0;
    evt->dy = 0;
    evt->button = 0u;
    evt->key = (uint16_t) message->par1;
    evt->mod = 0u;
}

static int translate_message(gem_hid_event_t *evt,
    const struct rasta_input_message *message)
{
    switch (message->msgid) {
    case rasta_msg_key_down:
        fill_key_event(evt, message, 1);
        return 1;
    case rasta_msg_key_up:
        fill_key_event(evt, message, 0);
        return 1;
    case rasta_msg_mouse_move:
        fill_mouse_move(evt, message);
        return 1;
    case rasta_msg_mouse_left_down:
        fill_button_event(evt, GEM_HID_BUTTON_LEFT, 1, message);
        return 1;
    case rasta_msg_mouse_left_up:
        fill_button_event(evt, GEM_HID_BUTTON_LEFT, 0, message);
        return 1;
    case rasta_msg_mouse_middle_down:
        fill_button_event(evt, GEM_HID_BUTTON_MIDDLE, 1, message);
        return 1;
    case rasta_msg_mouse_middle_up:
        fill_button_event(evt, GEM_HID_BUTTON_MIDDLE, 0, message);
        return 1;
    case rasta_msg_mouse_right_down:
        fill_button_event(evt, GEM_HID_BUTTON_RIGHT, 1, message);
        return 1;
    case rasta_msg_mouse_right_up:
        fill_button_event(evt, GEM_HID_BUTTON_RIGHT, 0, message);
        return 1;
    default:
        return 0;
    }
}

int gem_hid_init(void)
{
    struct sockaddr_in addr;
    const char *host = rasta_host();
    uint16_t port = rasta_port();
    char *subscription;
    size_t subscription_length;

    g_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket_fd < 0) {
        return 0;
    }

    if (!set_nonblocking(g_socket_fd)) {
        close(g_socket_fd);
        g_socket_fd = -1;
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(g_socket_fd);
        g_socket_fd = -1;
        errno = EINVAL;
        return 0;
    }

    subscription = build_subscription_payload();
    if (subscription == NULL) {
        close(g_socket_fd);
        g_socket_fd = -1;
        return 0;
    }

    subscription_length = strlen(subscription);
    if (sendto(g_socket_fd, subscription, subscription_length, 0,
        (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        free(subscription);
        close(g_socket_fd);
        g_socket_fd = -1;
        return 0;
    }
    free(subscription);

    g_mouse_x = 0;
    g_mouse_y = 0;
    g_button_flags = 0u;
    return 1;
}

void gem_hid_shutdown(void)
{
    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }

    g_mouse_x = 0;
    g_mouse_y = 0;
    g_button_flags = 0u;
}

int gem_hid_poll(gem_hid_event_t *evt)
{
    struct rasta_input_message message;
    int rc;

    if (evt == NULL || g_socket_fd < 0) {
        return 0;
    }

    for (;;) {
        rc = receive_message(&message);
        if (rc <= 0) {
            return 0;
        }
        if (translate_message(evt, &message) != 0) {
            return 1;
        }
    }
}

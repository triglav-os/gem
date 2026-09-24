/*
 * Resolves the Rasta viewer connection settings from the environment for
 * both the raster and input backends: framebuffer path, host, port,
 * window scale, cursor and inverse modes, each with a GEM_RASTA_* name
 * and the older RASTA_* fallback.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "rasta_internal.h"

#include <stdlib.h>
#include <string.h>

const char *rasta_framebuffer_path(void)
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

const char *rasta_host(void)
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

uint16_t rasta_port(void)
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

    return (uint16_t)port;
}

uint16_t rasta_scale(void)
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

    return (uint16_t)scale;
}

const char *rasta_cursor_mode(void)
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

const char *rasta_inverse_mode(void)
{
    const char *value = getenv("GEM_RASTA_INVERSE");

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_INVERSE");
    }
    if (value == NULL || value[0] == '\0') {
        return "off";
    }

    if (strcmp(value, "off") == 0 || strcmp(value, "false") == 0 ||
        strcmp(value, "0") == 0) {
        return "off";
    }

    return "on";
}

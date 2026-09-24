/*
 * Declares the small GEM configuration library. It loads component-specific
 * INI-style files from the XDG configuration hierarchy and can atomically
 * save a user override without exposing parser or filesystem details.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_GEMCONF_H
#define GEM_GEMCONF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gemconf {
    void *entries;
    size_t count;
    size_t capacity;
    char component[64];
} gemconf_t;

/* Initialize an empty configuration object. */
void gemconf_init(gemconf_t *config);

/* Release all entries and return config to its empty initialized state. */
void gemconf_clear(gemconf_t *config);

/*
 * Load `<component>.cfg` from XDG_CONFIG_DIRS and then XDG_CONFIG_HOME.
 * Later files override matching section/key pairs. Returns nonzero when the
 * component name is valid; missing files are a valid empty configuration.
 */
int gemconf_load(gemconf_t *config, const char *component);

/* Return a stored value, or fallback when section/key is absent. */
const char *gemconf_get(const gemconf_t *config, const char *section,
                        const char *key, const char *fallback);

/* Return a boolean value accepting true/false, yes/no, on/off and 1/0. */
int gemconf_get_bool(const gemconf_t *config, const char *section,
                     const char *key, int fallback);

/* Return a decimal integer value, or fallback when it is absent/invalid. */
long gemconf_get_long(const gemconf_t *config, const char *section,
                      const char *key, long fallback);

/* Set or replace one value. Returns nonzero on success. */
int gemconf_set(gemconf_t *config, const char *section, const char *key,
                const char *value);

/* Remove one value. Returns nonzero when an entry was removed. */
int gemconf_remove(gemconf_t *config, const char *section, const char *key);

/* Return the number of loaded entries. */
size_t gemconf_count(const gemconf_t *config);

/* Return an entry's section, or NULL when index is outside the configuration.
 */
const char *gemconf_entry_section(const gemconf_t *config, size_t index);

/* Return an entry's key, or NULL when index is outside the configuration. */
const char *gemconf_entry_key(const gemconf_t *config, size_t index);

/* Return an entry's value, or NULL when index is outside the configuration. */
const char *gemconf_entry_value(const gemconf_t *config, size_t index);

/*
 * Resolve the writable XDG user path for config's component. The directory
 * is not created by this query. Returns nonzero when the path fits.
 */
int gemconf_user_path(const gemconf_t *config, char *path, size_t path_size);

/* Atomically save the merged configuration as the component's user file. */
int gemconf_save(const gemconf_t *config);

#ifdef __cplusplus
}
#endif

#endif

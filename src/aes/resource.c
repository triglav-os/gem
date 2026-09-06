/*
 * Implements hosted AES resource loading and pointer fixups
 * for external or built-in resource images.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"

#include "platform/os.h"

#include <stdint.h>
static int aes_resource_contains_ptr(const void *ptr);
static LONG aes_resource_fix_offset(LONG value);
static void aes_resource_fix_tedinfo(TEDINFO *ted);
static void aes_resource_fix_iconblk(ICONBLK *icon);
static void aes_resource_fix_bitblk(BITBLK *bitblk);
WORD rsrc_load(char *filename);
WORD rsrc_free(void);
WORD rsrc_gaddr(WORD type, WORD index, void **addr);
WORD rsrc_saddr(WORD type, WORD index, void *addr);
WORD rsrc_obfix(OBJECT *tree, WORD object);

static int aes_resource_contains_ptr(const void *ptr)
{
    uintptr_t base;
    uintptr_t end;
    uintptr_t value;

    if (aes_state.resource_data == NULL || ptr == NULL) {
        return 0;
    }

    base = (uintptr_t)aes_state.resource_data;
    end = base + aes_state.resource_size;
    value = (uintptr_t)ptr;
    return value >= base && value < end;
}

static LONG aes_resource_fix_offset(LONG value)
{
    uintptr_t offset;

    if (aes_state.resource_data == NULL || value <= 0) {
        return value;
    }

    offset = (uintptr_t)value;
    if (offset >= aes_state.resource_size) {
        return value;
    }

    return (LONG)((uintptr_t)aes_state.resource_data + offset);
}

static void aes_resource_fix_tedinfo(TEDINFO *ted)
{
    if (ted == NULL || !aes_resource_contains_ptr(ted)) {
        return;
    }

    ted->te_ptext = aes_resource_fix_offset(ted->te_ptext);
    ted->te_ptmplt = aes_resource_fix_offset(ted->te_ptmplt);
    ted->te_pvalid = aes_resource_fix_offset(ted->te_pvalid);
}

static void aes_resource_fix_iconblk(ICONBLK *icon)
{
    if (icon == NULL || !aes_resource_contains_ptr(icon)) {
        return;
    }

    icon->ib_pmask = aes_resource_fix_offset(icon->ib_pmask);
    icon->ib_pdata = aes_resource_fix_offset(icon->ib_pdata);
    icon->ib_ptext = aes_resource_fix_offset(icon->ib_ptext);
}

static void aes_resource_fix_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL || !aes_resource_contains_ptr(bitblk)) {
        return;
    }

    bitblk->bi_pdata = aes_resource_fix_offset(bitblk->bi_pdata);
}

WORD rsrc_load(char *filename)
{
    char resolved[260];

    if (filename == NULL) {
        return 0;
    }
    if (aes_state.resource_data != NULL) {
        gem_os_free(aes_state.resource_data);
        aes_state.resource_data = NULL;
        aes_state.resource_size = 0;
    }
    aes_state.resource_is_builtin = 0;
    if (gem_builtin_rsrc_load(filename) != 0) {
        aes_state.resource_is_builtin = 1;
        return 1;
    }
    if (aes_try_resolve_path(filename, resolved, sizeof(resolved))) {
        return aes_load_file(resolved, &aes_state.resource_data,
                             &aes_state.resource_size);
    }
    if (aes_load_file(filename, &aes_state.resource_data,
                      &aes_state.resource_size) != 0) {
        return 1;
    }
    return 0;
}

WORD rsrc_free(void)
{
    if (aes_state.resource_data != NULL) {
        gem_os_free(aes_state.resource_data);
        aes_state.resource_data = NULL;
        aes_state.resource_size = 0;
    }
    if (aes_state.resource_is_builtin != 0) {
        gem_builtin_rsrc_free();
        aes_state.resource_is_builtin = 0;
    }
    return 1;
}

WORD rsrc_gaddr(WORD type, WORD index, void **addr)
{
    RSHDR *header;
    uint8_t *base;

    if (addr == NULL) {
        return 0;
    }
    if (aes_state.resource_is_builtin != 0) {
        return gem_builtin_rsrc_gaddr(type, index, addr);
    }
    if (aes_state.resource_data == NULL) {
        return 0;
    }

    base = (uint8_t *)aes_state.resource_data;
    header = (RSHDR *)aes_state.resource_data;
    switch (type) {
        case R_TREE: {
            WORD *trindex = (WORD *)(base + header->rsh_trindex);
            *addr = base + trindex[index];
            return 1;
        }
        case R_OBJECT:
            *addr = base + header->rsh_object + index * sizeof(OBJECT);
            return 1;
        case R_TEDINFO:
            *addr = base + header->rsh_tedinfo + index * sizeof(TEDINFO);
            aes_resource_fix_tedinfo((TEDINFO *)*addr);
            return 1;
        case R_ICONBLK:
            *addr = base + header->rsh_iconblk + index * sizeof(ICONBLK);
            aes_resource_fix_iconblk((ICONBLK *)*addr);
            return 1;
        case R_BITBLK:
            *addr = base + header->rsh_bitblk + index * sizeof(BITBLK);
            aes_resource_fix_bitblk((BITBLK *)*addr);
            return 1;
        case R_STRING: {
            LONG *strings = (LONG *)(base + header->rsh_string);
            *addr = base + strings[index];
            return 1;
        }
        default:
            return 0;
    }
}

WORD rsrc_saddr(WORD type, WORD index, void *addr)
{
    (void)type;
    (void)index;
    (void)addr;
    return (aes_state.resource_data != NULL ||
            aes_state.resource_is_builtin != 0)
               ? 1
               : 0;
}

WORD rsrc_obfix(OBJECT *tree, WORD object)
{
    LONG spec;

    if (tree == NULL || object < 0) {
        return 0;
    }
    if (!aes_resource_contains_ptr(tree)) {
        return 1;
    }

    spec = tree[object].ob_spec;
    switch (tree[object].ob_type) {
        case G_STRING:
        case G_TITLE:
        case G_BUTTON:
            tree[object].ob_spec = aes_resource_fix_offset(spec);
            break;
        case G_TEXT:
        case G_BOXTEXT:
        case G_FTEXT:
        case G_FBOXTEXT:
            spec = aes_resource_fix_offset(spec);
            tree[object].ob_spec = spec;
            aes_resource_fix_tedinfo((TEDINFO *)(intptr_t)spec);
            break;
        case G_ICON:
            spec = aes_resource_fix_offset(spec);
            tree[object].ob_spec = spec;
            aes_resource_fix_iconblk((ICONBLK *)(intptr_t)spec);
            break;
        default:
            break;
    }
    return 1;
}

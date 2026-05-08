/*
 * Implements hosted AES resource loading and pointer fixups
 * for external or built-in resource images.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "platform/os.h"

#include <stdint.h>
static int _aes_resource_contains_ptr(const void *ptr);
static LONG _aes_resource_fix_offset(LONG value);
static void _aes_resource_fix_tedinfo(TEDINFO *ted);
static void _aes_resource_fix_iconblk(ICONBLK *icon);
static void _aes_resource_fix_bitblk(BITBLK *bitblk);
WORD rsrc_load(char *filename);
WORD rsrc_free(void);
WORD rsrc_gaddr(WORD type, WORD index, void **addr);
WORD rsrc_saddr(WORD type, WORD index, void *addr);
WORD rsrc_obfix(OBJECT *tree, WORD object);

static int _aes_resource_contains_ptr(const void *ptr)
{
    uintptr_t base;
    uintptr_t end;
    uintptr_t value;

    if (_aes.resource_data == NULL || ptr == NULL) {
        return 0;
    }

    base = (uintptr_t) _aes.resource_data;
    end = base + _aes.resource_size;
    value = (uintptr_t) ptr;
    return value >= base && value < end;
}

static LONG _aes_resource_fix_offset(LONG value)
{
    uintptr_t offset;

    if (_aes.resource_data == NULL || value <= 0) {
        return value;
    }

    offset = (uintptr_t) value;
    if (offset >= _aes.resource_size) {
        return value;
    }

    return (LONG) ((uintptr_t) _aes.resource_data + offset);
}

static void _aes_resource_fix_tedinfo(TEDINFO *ted)
{
    if (ted == NULL || !_aes_resource_contains_ptr(ted)) {
        return;
    }

    ted->te_ptext = _aes_resource_fix_offset(ted->te_ptext);
    ted->te_ptmplt = _aes_resource_fix_offset(ted->te_ptmplt);
    ted->te_pvalid = _aes_resource_fix_offset(ted->te_pvalid);
}

static void _aes_resource_fix_iconblk(ICONBLK *icon)
{
    if (icon == NULL || !_aes_resource_contains_ptr(icon)) {
        return;
    }

    icon->ib_pmask = _aes_resource_fix_offset(icon->ib_pmask);
    icon->ib_pdata = _aes_resource_fix_offset(icon->ib_pdata);
    icon->ib_ptext = _aes_resource_fix_offset(icon->ib_ptext);
}

static void _aes_resource_fix_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL || !_aes_resource_contains_ptr(bitblk)) {
        return;
    }

    bitblk->bi_pdata = _aes_resource_fix_offset(bitblk->bi_pdata);
}

WORD rsrc_load(char *filename)
{
    char resolved[260];

    if (filename == NULL) {
        return 0;
    }
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    _aes.resource_is_builtin = 0;
    if (gem_builtin_rsrc_load(filename) != 0) {
        _aes.resource_is_builtin = 1;
        return 1;
    }
    if (_aes_try_resolve_path(filename, resolved, sizeof(resolved))) {
        return _aes_load_file(resolved, &_aes.resource_data,
            &_aes.resource_size);
    }
    if (_aes_load_file(filename, &_aes.resource_data,
        &_aes.resource_size) != 0) {
        return 1;
    }
    return 0;
}

WORD rsrc_free(void)
{
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    if (_aes.resource_is_builtin != 0) {
        gem_builtin_rsrc_free();
        _aes.resource_is_builtin = 0;
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
    if (_aes.resource_is_builtin != 0) {
        return gem_builtin_rsrc_gaddr(type, index, addr);
    }
    if (_aes.resource_data == NULL) {
        return 0;
    }

    base = (uint8_t *) _aes.resource_data;
    header = (RSHDR *) _aes.resource_data;
    switch (type) {
    case R_TREE: {
        WORD *trindex = (WORD *) (base + header->rsh_trindex);
        *addr = base + trindex[index];
        return 1;
    }
    case R_OBJECT:
        *addr = base + header->rsh_object + index * sizeof(OBJECT);
        return 1;
    case R_TEDINFO:
        *addr = base + header->rsh_tedinfo + index * sizeof(TEDINFO);
        _aes_resource_fix_tedinfo((TEDINFO *) *addr);
        return 1;
    case R_ICONBLK:
        *addr = base + header->rsh_iconblk + index * sizeof(ICONBLK);
        _aes_resource_fix_iconblk((ICONBLK *) *addr);
        return 1;
    case R_BITBLK:
        *addr = base + header->rsh_bitblk + index * sizeof(BITBLK);
        _aes_resource_fix_bitblk((BITBLK *) *addr);
        return 1;
    case R_STRING: {
        LONG *strings = (LONG *) (base + header->rsh_string);
        *addr = base + strings[index];
        return 1;
    }
    default:
        return 0;
    }
}

WORD rsrc_saddr(WORD type, WORD index, void *addr)
{
    (void) type;
    (void) index;
    (void) addr;
    return (_aes.resource_data != NULL || _aes.resource_is_builtin != 0) ?
        1 : 0;
}

WORD rsrc_obfix(OBJECT *tree, WORD object)
{
    LONG spec;

    if (tree == NULL || object < 0) {
        return 0;
    }
    if (!_aes_resource_contains_ptr(tree)) {
        return 1;
    }

    spec = tree[object].ob_spec;
    switch (tree[object].ob_type) {
    case G_STRING:
    case G_TITLE:
    case G_BUTTON:
        tree[object].ob_spec = _aes_resource_fix_offset(spec);
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        spec = _aes_resource_fix_offset(spec);
        tree[object].ob_spec = spec;
        _aes_resource_fix_tedinfo((TEDINFO *) (intptr_t) spec);
        break;
    case G_ICON:
        spec = _aes_resource_fix_offset(spec);
        tree[object].ob_spec = spec;
        _aes_resource_fix_iconblk((ICONBLK *) (intptr_t) spec);
        break;
    default:
        break;
    }
    return 1;
}

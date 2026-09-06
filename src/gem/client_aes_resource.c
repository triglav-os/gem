/*
 * Loads host-native AES resources into client memory and relocates descriptors
 * there. Resource addresses returned to applications never refer to gemd.
 * Bounds checks reject truncated tables before any pointer is constructed.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *resource;
static size_t resource_size;
static void *span(size_t offset, size_t length)
{
    if (!resource || offset > resource_size || length > resource_size - offset)
        return NULL;
    return resource + offset;
}
static LONG fix(LONG value, size_t length)
{
    uintptr_t address = (uintptr_t)value, base = (uintptr_t)resource;
    if (address >= base && address - base <= resource_size &&
        length <= resource_size - (address - base))
        return value;
    void *pointer = value >= 0 ? span((size_t)value, length) : NULL;
    return (LONG)(intptr_t)pointer;
}
static LONG fix_string(LONG value)
{
    LONG address = fix(value, 1);
    if (!address)
        return 0;
    size_t offset = (size_t)((uintptr_t)address - (uintptr_t)resource);
    if (!memchr(resource + offset, 0, resource_size - offset))
        return 0;
    return address;
}
static int fix_ted(TEDINFO *ted)
{
    if ((uintptr_t)ted % _Alignof(TEDINFO))
        return 0;
    if (ted->te_txtlen <= 0)
        return 0;
    ted->te_ptext = fix(ted->te_ptext, (size_t)ted->te_txtlen);
    ted->te_ptmplt = fix_string(ted->te_ptmplt);
    ted->te_pvalid = fix_string(ted->te_pvalid);
    return ted->te_ptext && ted->te_ptmplt && ted->te_pvalid &&
           memchr((void *)(intptr_t)ted->te_ptext, 0, (size_t)ted->te_txtlen);
}
static int fix_icon(ICONBLK *icon)
{
    if ((uintptr_t)icon % _Alignof(ICONBLK))
        return 0;
    if (icon->ib_wicon <= 0 || icon->ib_hicon <= 0)
        return 0;
    size_t size =
        ((size_t)icon->ib_wicon + 15) / 16 * 2 * (size_t)icon->ib_hicon;
    icon->ib_pmask = fix(icon->ib_pmask, size);
    icon->ib_pdata = fix(icon->ib_pdata, size);
    icon->ib_ptext = fix_string(icon->ib_ptext);
    return icon->ib_pmask && icon->ib_pdata && icon->ib_ptext;
}
static int fix_bit(BITBLK *bit)
{
    if ((uintptr_t)bit % _Alignof(BITBLK))
        return 0;
    if (bit->bi_wb <= 0 || bit->bi_hl <= 0)
        return 0;
    bit->bi_pdata = fix(bit->bi_pdata, (size_t)bit->bi_wb * (size_t)bit->bi_hl);
    return bit->bi_pdata != 0;
}
WORD rsrc_free(void)
{
    free(resource);
    resource = NULL;
    resource_size = 0;
    return 1;
}
WORD rsrc_load(char *filename)
{
    char path[260];
    if (!filename || strlen(filename) >= sizeof(path))
        return 0;
    strcpy(path, filename);
    if (!shel_find(path))
        return 0;
    FILE *stream = fopen(path, "rb");
    if (!stream)
        return 0;
    if (fseek(stream, 0, SEEK_END)) {
        fclose(stream);
        return 0;
    }
    long size = ftell(stream);
    if (size < (long)sizeof(RSHDR) || size > 8 * 1024 * 1024 ||
        fseek(stream, 0, SEEK_SET)) {
        fclose(stream);
        return 0;
    }
    unsigned char *bytes = malloc((size_t)size);
    if (!bytes) {
        fclose(stream);
        return 0;
    }
    size_t count = fread(bytes, 1, (size_t)size, stream);
    fclose(stream);
    if (count != (size_t)size) {
        free(bytes);
        return 0;
    }
    rsrc_free();
    resource = bytes;
    resource_size = count;
    return 1;
}
WORD rsrc_gaddr(WORD type, WORD index, void **address)
{
    if (!resource || !address || index < 0)
        return 0;
    const RSHDR *header = (const RSHDR *)resource;
    void *value = NULL;
#define TABLE(field, count, type)                                              \
    if (index < header->count && header->field >= 0 &&                         \
        (size_t)header->field % _Alignof(type) == 0)                           \
    value = span((size_t)header->field + (size_t)index * sizeof(type),         \
                 sizeof(type))
    switch (type) {
        case R_TREE: {
            WORD offset;
            if (index >= header->rsh_ntree || header->rsh_trindex < 0)
                return 0;
            void *entry =
                span((size_t)header->rsh_trindex + (size_t)index * sizeof(WORD),
                     sizeof(WORD));
            if (!entry)
                return 0;
            memcpy(&offset, entry, sizeof(offset));
            if (offset < 0 || (size_t)offset % _Alignof(OBJECT))
                return 0;
            value = span((size_t)offset, sizeof(OBJECT));
            break;
        }
        case R_OBJECT:
            TABLE(rsh_object, rsh_nobs, OBJECT);
            break;
        case R_TEDINFO:
            TABLE(rsh_tedinfo, rsh_nted, TEDINFO);
            if (value && !fix_ted(value))
                value = NULL;
            break;
        case R_ICONBLK:
            TABLE(rsh_iconblk, rsh_nib, ICONBLK);
            if (value && !fix_icon(value))
                value = NULL;
            break;
        case R_BITBLK:
            TABLE(rsh_bitblk, rsh_nbb, BITBLK);
            if (value && !fix_bit(value))
                value = NULL;
            break;
        case R_STRING: {
            LONG offset;
            if (index >= header->rsh_nstring || header->rsh_string < 0)
                return 0;
            void *entry =
                span((size_t)header->rsh_string + (size_t)index * sizeof(LONG),
                     sizeof(LONG));
            if (!entry)
                return 0;
            memcpy(&offset, entry, sizeof(offset));
            value = (void *)(intptr_t)fix_string(offset);
            break;
        }
        default:
            return 0;
    }
#undef TABLE
    if (!value)
        return 0;
    *address = value;
    return 1;
}
WORD rsrc_saddr(WORD type, WORD index, void *address)
{
    (void)type;
    (void)index;
    (void)address;
    return resource != NULL;
}
WORD rsrc_obfix(OBJECT *tree, WORD object)
{
    if (!tree || object < 0)
        return 0;
    uintptr_t base = (uintptr_t)resource, address = (uintptr_t)tree;
    if (!resource || address < base || address - base >= resource_size)
        return 1;
    size_t offset = address - base + (size_t)object * sizeof(OBJECT);
    OBJECT *obj = span(offset, sizeof(OBJECT));
    if (!obj || (uintptr_t)obj % _Alignof(OBJECT))
        return 0;
    switch (obj->ob_type & 0xff) {
        case G_STRING:
        case G_TITLE:
        case G_BUTTON:
            obj->ob_spec = fix_string(obj->ob_spec);
            return obj->ob_spec != 0;
        case G_TEXT:
        case G_BOXTEXT:
        case G_FTEXT:
        case G_FBOXTEXT:
            obj->ob_spec = fix(obj->ob_spec, sizeof(TEDINFO));
            return obj->ob_spec && fix_ted((TEDINFO *)(intptr_t)obj->ob_spec);
        case G_ICON:
            obj->ob_spec = fix(obj->ob_spec, sizeof(ICONBLK));
            return obj->ob_spec && fix_icon((ICONBLK *)(intptr_t)obj->ob_spec);
        case G_IMAGE:
            obj->ob_spec = fix(obj->ob_spec, sizeof(BITBLK));
            return obj->ob_spec && fix_bit((BITBLK *)(intptr_t)obj->ob_spec);
        default:
            return 1;
    }
}

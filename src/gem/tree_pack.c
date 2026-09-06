/*
 * Serializes AES objects, TEDINFO, icons and bitmap data without process
 * pointers. The bounded arena rejects oversized or unsupported trees.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/gemd.h"
#include <string.h>

static LONG append(gem_tree_packet_t *p, const void *data, size_t size)
{
    size_t offset = (p->used + 7u) & ~(size_t)7u;
    if (!data || !size || offset > GEM_TREE_BYTES ||
        size > GEM_TREE_BYTES - offset)
        return 0;
    memcpy(p->data + offset, data, size);
    p->used = (uint32_t)(offset + size);
    return (LONG)offset;
}
static LONG string(gem_tree_packet_t *p, LONG address, size_t capacity)
{
    const char *text = (const char *)(intptr_t)address;
    size_t length = 0;
    if (!text)
        text = "";
    while (length < GEM_TREE_BYTES && text[length])
        ++length;
    if (length == GEM_TREE_BYTES)
        return 0;
    if (!capacity)
        capacity = length + 1;
    if (capacity > GEM_TREE_BYTES || length >= capacity)
        return 0;
    size_t offset = (p->used + 7u) & ~(size_t)7u;
    if (offset > GEM_TREE_BYTES || capacity > GEM_TREE_BYTES - offset)
        return 0;
    memset(p->data + offset, 0, capacity);
    memcpy(p->data + offset, text, length + 1);
    p->used = (uint32_t)(offset + capacity);
    return (LONG)offset;
}
int gem_tree_pack(gem_tree_packet_t *p, const OBJECT *tree)
{
    if (!tree)
        return 0;
    p->used = 8;
    p->count = 0;
    do {
        if (p->count == GEM_TREE_OBJECTS)
            return 0;
        p->objects[p->count] = tree[p->count];
        ++p->count;
    } while (!(tree[p->count - 1].ob_flags & LASTOB));
    for (unsigned i = 0; i < p->count; ++i) {
        OBJECT *obj = &p->objects[i];
        LONG spec = obj->ob_spec;
        if (obj->ob_flags & INDIRECT) {
            if (!spec)
                return 0;
            spec = *(LONG *)(intptr_t)spec;
            obj->ob_flags &= ~INDIRECT;
        }
        switch (obj->ob_type & 0xff) {
            case G_BOX:
            case G_IBOX:
            case G_BOXCHAR:
                break;
            case G_STRING:
            case G_TITLE:
            case G_BUTTON:
                obj->ob_spec = string(p, spec, 0);
                if (!obj->ob_spec)
                    return 0;
                break;
            case G_TEXT:
            case G_BOXTEXT:
            case G_FTEXT:
            case G_FBOXTEXT: {
                TEDINFO ted;
                if (!spec)
                    return 0;
                memcpy(&ted, (void *)(intptr_t)spec, sizeof(ted));
                if (ted.te_txtlen <= 0)
                    return 0;
                ted.te_ptext = string(p, ted.te_ptext, (size_t)ted.te_txtlen);
                ted.te_ptmplt = string(p, ted.te_ptmplt, 0);
                ted.te_pvalid = string(p, ted.te_pvalid, 0);
                if (!ted.te_ptext || !ted.te_ptmplt || !ted.te_pvalid)
                    return 0;
                obj->ob_spec = append(p, &ted, sizeof(ted));
                if (!obj->ob_spec)
                    return 0;
                break;
            }
            case G_USERDEF: {
                USERBLK user = {0};
                obj->ob_spec = append(p, &user, sizeof(user));
                if (!obj->ob_spec)
                    return 0;
                break;
            }
            case G_IMAGE: {
                BITBLK bit;
                if (!spec)
                    return 0;
                memcpy(&bit, (void *)(intptr_t)spec, sizeof(bit));
                if (bit.bi_wb <= 0 || bit.bi_hl <= 0)
                    return 0;
                bit.bi_pdata = append(p, (void *)(intptr_t)bit.bi_pdata,
                                      (size_t)bit.bi_wb * (size_t)bit.bi_hl);
                if (!bit.bi_pdata)
                    return 0;
                obj->ob_spec = append(p, &bit, sizeof(bit));
                if (!obj->ob_spec)
                    return 0;
                break;
            }
            case G_ICON: {
                ICONBLK icon;
                if (!spec)
                    return 0;
                memcpy(&icon, (void *)(intptr_t)spec, sizeof(icon));
                if (icon.ib_wicon <= 0 || icon.ib_hicon <= 0)
                    return 0;
                size_t size = ((size_t)icon.ib_wicon + 15) / 16 * 2 *
                              (size_t)icon.ib_hicon;
                icon.ib_pmask =
                    append(p, (void *)(intptr_t)icon.ib_pmask, size);
                icon.ib_pdata =
                    append(p, (void *)(intptr_t)icon.ib_pdata, size);
                icon.ib_ptext = string(p, icon.ib_ptext, 0);
                if (!icon.ib_pmask || !icon.ib_pdata || !icon.ib_ptext)
                    return 0;
                obj->ob_spec = append(p, &icon, sizeof(icon));
                if (!obj->ob_spec)
                    return 0;
                break;
            }
            default:
                return 0;
        }
    }
    return gem_tree_decode(p, 0);
}

void gem_tree_copy_back(OBJECT *tree, const gem_tree_packet_t *p)
{
    for (unsigned i = 0; i < p->count; ++i) {
        LONG spec = tree[i].ob_spec;
        UWORD indirect = tree[i].ob_flags & INDIRECT;
        LONG resolved = indirect ? *(LONG *)(intptr_t)spec : spec;
        tree[i] = p->objects[i];
        tree[i].ob_spec = spec;
        tree[i].ob_flags |= indirect;
        switch (tree[i].ob_type & 0xff) {
            case G_TEXT:
            case G_BOXTEXT:
            case G_FTEXT:
            case G_FBOXTEXT: {
                TEDINFO *target = (TEDINFO *)(intptr_t)resolved;
                const TEDINFO *source =
                    (const TEDINFO *)(p->data + p->objects[i].ob_spec);
                size_t size = (size_t)target->te_txtlen;
                if (size > (size_t)source->te_txtlen)
                    size = (size_t)source->te_txtlen;
                if (target->te_ptext && size) {
                    const char *text =
                        (const char *)(p->data + source->te_ptext);
                    char *destination = (char *)(intptr_t)target->te_ptext;
                    if (strcmp(destination, text)) {
                        size_t length = strlen(text) + 1;
                        if (length <= size)
                            memcpy(destination, text, length);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

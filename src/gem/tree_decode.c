/*
 * Validates AES tree graphs and nonoverlapping arena regions before relocation.
 * This keeps nested text and bitmap descriptors inside connection-owned data.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/gemd.h"
#include <string.h>
static int region(gem_tree_packet_t *p, LONG offset, size_t size,
                  unsigned char *claimed)
{
    if (offset < 8 || (size_t)offset > p->used ||
        size > p->used - (size_t)offset)
        return 0;
    for (size_t i = (size_t)offset; i < (size_t)offset + size; ++i) {
        if (claimed[i])
            return 0;
        claimed[i] = 1;
    }
    return 1;
}
static int text(gem_tree_packet_t *p, LONG offset, size_t capacity,
                unsigned char *claimed)
{
    if (offset < 8 || (size_t)offset >= p->used)
        return 0;
    size_t limit = p->used - (size_t)offset;
    if (capacity && capacity > limit)
        return 0;
    const unsigned char *end =
        memchr(p->data + offset, 0, capacity ? capacity : limit);
    if (!end)
        return 0;
    return region(p, offset,
                  capacity ? capacity : (size_t)(end - (p->data + offset)) + 1,
                  claimed);
}
static int graph(const gem_tree_packet_t *p)
{
    int parents[GEM_TREE_OBJECTS];
    for (unsigned i = 0; i < p->count; ++i)
        parents[i] = -1;
    for (unsigned i = 0; i < p->count; ++i) {
        const OBJECT *o = &p->objects[i];
        if (o->ob_flags & INDIRECT || o->ob_next < NIL ||
            o->ob_next >= p->count || o->ob_head < NIL ||
            o->ob_head >= p->count || o->ob_tail < NIL ||
            o->ob_tail >= p->count ||
            (o->ob_head == NIL) != (o->ob_tail == NIL) ||
            ((o->ob_flags & LASTOB) != 0) != (i + 1 == p->count))
            return 0;
        int child = o->ob_head;
        while (child != NIL) {
            if (child == (int)i || parents[child] != -1)
                return 0;
            parents[child] = (int)i;
            if (child == o->ob_tail) {
                if (p->objects[child].ob_next != (int)i)
                    return 0;
                break;
            }
            child = p->objects[child].ob_next;
            if (child < 0 || child >= p->count)
                return 0;
        }
    }
    for (unsigned i = 0; i < p->count; ++i) {
        int parent = (int)i;
        unsigned depth = 0;
        while (parent != -1) {
            if (++depth > p->count)
                return 0;
            parent = parents[parent];
        }
    }
    return 1;
}
static void relocate(gem_tree_packet_t *p, int encode)
{
    LONG base = (LONG)(intptr_t)p->data;
    for (unsigned i = 0; i < p->count; ++i) {
        OBJECT *obj = &p->objects[i];
        unsigned type = obj->ob_type & 0xff;
        if (type == G_BOX || type == G_IBOX || type == G_BOXCHAR)
            continue;
        if (!encode)
            obj->ob_spec += base;
#define FIX(field) field += encode ? -base : base
        switch (type) {
            case G_TEXT:
            case G_BOXTEXT:
            case G_FTEXT:
            case G_FBOXTEXT: {
                TEDINFO *t = (TEDINFO *)(intptr_t)obj->ob_spec;
                FIX(t->te_ptext);
                FIX(t->te_ptmplt);
                FIX(t->te_pvalid);
                break;
            }
            case G_USERDEF: {
                USERBLK *u = (USERBLK *)(intptr_t)obj->ob_spec;
                if (encode)
                    u->ab_code = u->ab_parm = 0;
                break;
            }
            case G_IMAGE: {
                BITBLK *b = (BITBLK *)(intptr_t)obj->ob_spec;
                FIX(b->bi_pdata);
                break;
            }
            case G_ICON: {
                ICONBLK *b = (ICONBLK *)(intptr_t)obj->ob_spec;
                FIX(b->ib_pmask);
                FIX(b->ib_pdata);
                FIX(b->ib_ptext);
                break;
            }
            default:
                break;
        }
#undef FIX
        if (encode)
            obj->ob_spec -= base;
    }
}
int gem_tree_decode(gem_tree_packet_t *p, int fix)
{
    unsigned char claimed[GEM_TREE_BYTES] = {0};
    if (!p->count || p->count > GEM_TREE_OBJECTS || p->used > GEM_TREE_BYTES ||
        !graph(p))
        return 0;
    for (unsigned i = 0; i < p->count; ++i) {
        OBJECT *obj = &p->objects[i];
        LONG offset = obj->ob_spec;
        switch (obj->ob_type & 0xff) {
            case G_BOX:
            case G_IBOX:
            case G_BOXCHAR:
                break;
            case G_STRING:
            case G_TITLE:
            case G_BUTTON:
                if (!text(p, offset, 0, claimed))
                    return 0;
                break;
            case G_TEXT:
            case G_BOXTEXT:
            case G_FTEXT:
            case G_FBOXTEXT: {
                if ((offset & 7) ||
                    !region(p, offset, sizeof(TEDINFO), claimed))
                    return 0;
                TEDINFO *t = (TEDINFO *)(p->data + offset);
                if (t->te_txtlen <= 0 ||
                    !text(p, t->te_ptext, (size_t)t->te_txtlen, claimed) ||
                    !text(p, t->te_ptmplt, 0, claimed) ||
                    !text(p, t->te_pvalid, 0, claimed))
                    return 0;
                break;
            }
            case G_USERDEF: {
                if ((offset & 7) ||
                    !region(p, offset, sizeof(USERBLK), claimed))
                    return 0;
                USERBLK *u = (USERBLK *)(p->data + offset);
                if (u->ab_code || u->ab_parm)
                    return 0;
                break;
            }
            case G_IMAGE: {
                if ((offset & 7) || !region(p, offset, sizeof(BITBLK), claimed))
                    return 0;
                BITBLK *b = (BITBLK *)(p->data + offset);
                if (b->bi_wb <= 0 || b->bi_hl <= 0 || (b->bi_wb & 1) ||
                    (b->bi_pdata & 1) ||
                    !region(p, b->bi_pdata, (size_t)b->bi_wb * (size_t)b->bi_hl,
                            claimed))
                    return 0;
                break;
            }
            case G_ICON: {
                if ((offset & 7) ||
                    !region(p, offset, sizeof(ICONBLK), claimed))
                    return 0;
                ICONBLK *b = (ICONBLK *)(p->data + offset);
                if (b->ib_wicon <= 0 || b->ib_hicon <= 0)
                    return 0;
                size_t size =
                    ((size_t)b->ib_wicon + 15) / 16 * 2 * (size_t)b->ib_hicon;
                if ((b->ib_pmask & 1) || (b->ib_pdata & 1) ||
                    !region(p, b->ib_pmask, size, claimed) ||
                    !region(p, b->ib_pdata, size, claimed) ||
                    !text(p, b->ib_ptext, 0, claimed))
                    return 0;
                break;
            }
            default:
                return 0;
        }
    }
    if (fix)
        relocate(p, 0);
    return 1;
}
void gem_tree_encode(gem_tree_packet_t *p)
{
    relocate(p, 1);
}

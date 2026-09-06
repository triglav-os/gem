/*
 * Rejects malformed extended RPC arrays, tree graphs and bitmap geometry.
 * Runs without a display so transport validation failures stay deterministic.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/gemd.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    gem_vdi_packet_t v = {.handle = 1, .function = rpc_v_cellarray};
    v.args[0] = 1;
    v.args[2] = 2;
    v.args[3] = 2;
    v.args[4] = 2;
    v.counts[1] = 4;
    v.counts[6] = 4;
    assert(gem_vdi_validate(&v));
    v.args[4] = -1;
    assert(!gem_vdi_validate(&v));
    v.args[4] = 2;
    v.counts[6] = 2048;
    assert(!gem_vdi_validate(&v));
    memset(&v, 0, sizeof(v));
    v.handle = 1;
    v.args[0] = 1;
    v.function = rpc_v_curtext;
    v.counts[1] = 1;
    v.data[0] = 0x4141;
    assert(!gem_vdi_validate(&v));
    v.data[0] = 0;
    assert(gem_vdi_validate(&v));
    v.function = 65535;
    assert(!gem_vdi_validate(&v));
    gem_aes_packet_t a = {.function = rpc_shel_put};
    a.args[1] = 3;
    a.counts[0] = 2;
    assert(gem_aes_validate(&a));
    a.args[1] = -3;
    assert(!gem_aes_validate(&a));
    gem_bitmap_form_t f = {640, 400, 40, 0, 1, 1};
    assert(gem_bitmap_size(&f) == 32000);
    f.stride = 39;
    assert(!gem_bitmap_size(&f));
    f.stride = 40;
    f.height = -1;
    assert(!gem_bitmap_size(&f));
    gem_tree_packet_t *p = calloc(1, sizeof(*p));
    assert(p);
    p->count = 2;
    p->used = 16;
    p->objects[0] = (OBJECT){-1, 1, 1, G_IBOX, 0, 0, 0, 0, 0, 100, 100};
    p->objects[1] = (OBJECT){0, -1, -1, G_STRING, LASTOB, 0, 8, 0, 0, 10, 10};
    strcpy((char *)p->data + 8, "test");
    assert(gem_tree_decode(p, 0));
    assert(gem_tree_decode(p, 1));
    assert(!strcmp((char *)(intptr_t)p->objects[1].ob_spec, "test"));
    gem_tree_encode(p);
    assert(p->objects[1].ob_spec == 8);
    p->objects[1].ob_next = 1;
    assert(!gem_tree_decode(p, 0));
    p->objects[1].ob_next = 0;
    p->objects[1].ob_spec = GEM_TREE_BYTES;
    assert(!gem_tree_decode(p, 0));
    p->objects[1].ob_spec = 8;
    memset(p->data + 8, 'x', 8);
    assert(!gem_tree_decode(p, 0));
    p->count = GEM_TREE_OBJECTS + 1;
    assert(!gem_tree_decode(p, 0));
    free(p);
    return 0;
}

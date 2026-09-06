/*
 * Forwards AES form and object operations with copied object trees and text.
 * Replies update caller-owned fields while preserving original pointer
 * identity.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include "gem/gemd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static OBJECT *callback_tree;
static unsigned callback_count;
static const OBJECT *edit_tree;
static uint64_t edit_identity;
static uint64_t next_identity;

LONG gem_client_user_callback(PARMBLK *parm)
{
    if (!callback_tree || parm->pb_obj < 0 ||
        (unsigned)parm->pb_obj >= callback_count)
        return 0;
    OBJECT *obj = &callback_tree[parm->pb_obj];
    if ((obj->ob_type & 0xff) != G_USERDEF)
        return 0;
    LONG spec = obj->ob_spec;
    if (obj->ob_flags & INDIRECT)
        spec = *(LONG *)(intptr_t)spec;
    USERBLK *user = (USERBLK *)(intptr_t)spec;
    if (!user || !user->ab_code)
        return 0;
    parm->pb_tree = (LONG)(intptr_t)callback_tree;
    parm->pb_parm = user->ab_parm;
    WORD (*draw)(LONG) = (WORD(*)(LONG))(intptr_t)user->ab_code;
    return draw((LONG)(intptr_t)parm);
}
WORD gem_client_tree(OBJECT *tree, uint16_t operation, WORD args[12])
{
    gem_tree_packet_t *packet = calloc(1, sizeof(*packet));
    int32_t result = 0;
    if (!packet)
        return 0;
    packet->operation = operation;
    if (operation == tree_edit && (args[3] == EDINIT || args[3] == EDSTART)) {
        if (edit_tree != tree) {
            edit_tree = tree;
            edit_identity = ++next_identity;
        }
    }
    if (tree == edit_tree)
        packet->identity = edit_identity;
    memcpy(packet->args, args, sizeof(packet->args));
    OBJECT *previous = callback_tree;
    unsigned previous_count = callback_count;
    callback_tree = tree;
    int packed = gem_tree_pack(packet, tree);
    callback_count = packet->count;
    if (!packed && getenv("GEM_TRACE_RPC"))
        fprintf(stderr, "AES tree packing failed: operation=%u count=%u\n",
                operation, packet->count);
    if (!packed ||
        !gem_rpc_call(GEM_RPC_AES_TREE, packet, sizeof(*packet), &result,
                      packet, sizeof(*packet)) ||
        !gem_tree_decode(packet, 0)) {
        callback_tree = previous;
        callback_count = previous_count;
        free(packet);
        return 0;
    }
    callback_tree = previous;
    callback_count = previous_count;
    gem_tree_copy_back(tree, packet);
    memcpy(args, packet->args, sizeof(packet->args));
    free(packet);
    return (WORD)result;
}
WORD objc_edit(OBJECT *tree, WORD object, WORD key, WORD *index, WORD kind)
{
    WORD args[12] = {object, key, index ? *index : 0, kind};
    WORD result = gem_client_tree(tree, tree_edit, args);
    if (index)
        *index = args[2];
    return result;
}
WORD form_do(OBJECT *tree, WORD start)
{
    WORD args[12] = {start};
    return gem_client_tree(tree, tree_form_do, args);
}
WORD form_center(OBJECT *tree, WORD *x, WORD *y, WORD *w, WORD *h)
{
    WORD args[12] = {0};
    WORD result = gem_client_tree(tree, tree_form_center, args);
    if (x)
        *x = args[0];
    if (y)
        *y = args[1];
    if (w)
        *w = args[2];
    if (h)
        *h = args[3];
    return result;
}
WORD form_keybd(OBJECT *tree, WORD object, WORD next, WORD key, WORD *newobj,
                WORD *newchar)
{
    WORD args[12] = {object, next, key};
    WORD result = gem_client_tree(tree, tree_form_keybd, args);
    if (newobj)
        *newobj = args[3];
    if (newchar)
        *newchar = args[4];
    return result;
}
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *next)
{
    WORD args[12] = {object, clicks};
    WORD result = gem_client_tree(tree, tree_form_button, args);
    if (next)
        *next = args[2];
    return result;
}
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in, UWORD out)
{
    WORD args[12] = {object, (WORD)in, (WORD)out};
    return gem_client_tree(tree, tree_watchbox, args);
}
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object, WORD orientation)
{
    WORD args[12] = {parent, object, orientation};
    return gem_client_tree(tree, tree_slidebox, args);
}

/*
 * Marshals the AES menu bar to gemd as a copied object array with its
 * title and entry strings, and republishes mutations made to the active
 * tree. The reachable extent of the tree bounds what is serialized.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_protocol.h"
#include "gem/gemd.h"

#include <stdint.h>
#include <string.h>

static OBJECT *client_menu;

WORD gem_client_menu_changed(OBJECT *tree)
{
    return tree == client_menu ? menu_bar(tree, 1) : 1;
}

static void
gem_menu_tree_extent_visit(const OBJECT *tree, WORD object,
                           uint8_t visited[GEM_RPC_MENU_MAX_OBJECTS],
                           WORD *extent)
{
    if (tree == NULL || visited == NULL || extent == NULL || object < ROOT ||
        object >= (WORD)GEM_RPC_MENU_MAX_OBJECTS) {
        return;
    }
    if (visited[object] != 0u) {
        return;
    }

    visited[object] = 1u;
    if (object > *extent) {
        *extent = object;
    }

    gem_menu_tree_extent_visit(tree, tree[object].ob_head, visited, extent);
    gem_menu_tree_extent_visit(tree, tree[object].ob_tail, visited, extent);
    gem_menu_tree_extent_visit(tree, tree[object].ob_next, visited, extent);
}

/*
 * Menu trees are small graphs, not flat arrays with a reliable global
 * terminator. Some trees place LASTOB on several sibling chains, while
 * others are short static arrays that would be overrun by a blind scan.
 * Walk the references reachable from ROOT and use the highest visited
 * slot as the serialization extent.
 */
static WORD gem_menu_tree_extent(const OBJECT *tree)
{
    uint8_t visited[GEM_RPC_MENU_MAX_OBJECTS] = {0};
    WORD extent = ROOT;

    gem_menu_tree_extent_visit(tree, ROOT, visited, &extent);
    return extent;
}

WORD menu_bar(OBJECT *tree, WORD show)
{
    client_menu = show ? tree : NULL;
    int32_t status = 0;
    gem_rpc_menu_bar_req_t req;
    WORD extent;
    WORD i;

    if (tree == NULL && show != 0) {
        return 0;
    }

    memset(&req, 0, sizeof(req));
    req.show = show;
    if (show == 0) {
        if (!gem_rpc_call(GEM_RPC_MENU_BAR, &req, sizeof(req), &status, NULL,
                          0))
            return 0;
        return (WORD)status;
    }
    extent = gem_menu_tree_extent(tree);
    req.object_count = (WORD)(extent + 1);
    if (req.object_count > (WORD)GEM_RPC_MENU_MAX_OBJECTS) {
        req.object_count = (WORD)GEM_RPC_MENU_MAX_OBJECTS;
    }
    memcpy(req.objects, tree, (size_t)req.object_count * sizeof(OBJECT));

    for (i = 0; i < req.object_count &&
                req.string_count < (WORD)GEM_RPC_MENU_MAX_STRINGS;
         ++i) {
        const char *text;
        UWORD type = (UWORD)(tree[i].ob_type & 0xffu);

        /* gemd keys its string requirement on the low type byte; an
         * extended type in the high byte must not skip the string here. */
        if (type != G_TITLE && type != G_STRING) {
            continue;
        }
        if ((tree[i].ob_flags & INDIRECT) != 0 || tree[i].ob_spec == 0) {
            continue;
        }
        text = (const char *)(intptr_t)tree[i].ob_spec;
        req.strings[req.string_count].object = i;
        strncpy(req.strings[req.string_count].text, text,
                sizeof(req.strings[req.string_count].text) - 1u);
        ++req.string_count;
    }

    if (!gem_rpc_call(GEM_RPC_MENU_BAR, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    int32_t status = 0;
    gem_rpc_menu_tnormal_req_t req;

    (void)tree;
    req.title = title;
    req.normal = normal;
    if (!gem_rpc_call(GEM_RPC_MENU_TNORMAL, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD menu_click(WORD click, WORD setit)
{
    int32_t status = 0;
    gem_rpc_menu_click_req_t req;

    req.click = click;
    req.setit = setit;
    if (!gem_rpc_call(GEM_RPC_MENU_CLICK, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

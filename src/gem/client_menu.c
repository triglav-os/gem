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

/* Walk reachable links without recursion. Reject links outside the bounded
 * transport instead of silently omitting objects. The public API has no
 * allocation length; callers must still supply valid in-process pointers. */
static WORD gem_menu_tree_extent(const OBJECT *tree)
{
    uint8_t visited[GEM_RPC_MENU_MAX_OBJECTS] = {1};
    WORD queue[GEM_RPC_MENU_MAX_OBJECTS] = {ROOT};
    WORD first = 0, used = 1, extent = ROOT;

    while (first < used) {
        WORD object = queue[first++];
        WORD links[] = {tree[object].ob_head, tree[object].ob_tail,
                        tree[object].ob_next};
        for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); ++i) {
            WORD link = links[i];
            if (link == NIL)
                continue;
            if (link < ROOT || link >= (WORD)GEM_RPC_MENU_MAX_OBJECTS)
                return NIL;
            if (visited[link])
                continue;
            visited[link] = 1;
            queue[used++] = link;
            if (link > extent)
                extent = link;
        }
    }
    return extent;
}

WORD menu_bar(OBJECT *tree, WORD show)
{
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
        if (status > 0)
            client_menu = NULL;
        return (WORD)status;
    }
    extent = gem_menu_tree_extent(tree);
    if (extent == NIL)
        return -1;
    req.object_count = (WORD)(extent + 1);
    memcpy(req.objects, tree, (size_t)req.object_count * sizeof(OBJECT));

    for (i = 0; i < req.object_count; ++i) {
        const char *text;
        UWORD type = (UWORD)(tree[i].ob_type & 0xffu);

        /* gemd keys its string requirement on the low type byte; an
         * extended type in the high byte must not skip the string here. */
        if (type != G_TITLE && type != G_STRING) {
            continue;
        }
        if ((tree[i].ob_flags & INDIRECT) != 0 || tree[i].ob_spec == 0) {
            return -1;
        }
        text = (const char *)(intptr_t)tree[i].ob_spec;
        size_t length = 0;
        while (length < GEM_RPC_MENU_STRING_MAX && text[length] != '\0')
            ++length;
        if (length == GEM_RPC_MENU_STRING_MAX ||
            req.string_count >= (WORD)GEM_RPC_MENU_MAX_STRINGS)
            return -1;
        req.strings[req.string_count].object = i;
        memcpy(req.strings[req.string_count].text, text, length + 1u);
        ++req.string_count;
    }

    if (!gem_rpc_valid_request(GEM_RPC_MENU_BAR, &req, sizeof(req)))
        return -1;
    if (!gem_rpc_call(GEM_RPC_MENU_BAR, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    if (status > 0)
        client_menu = tree;
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

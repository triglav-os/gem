/*
 * Exercises complete menu serialization at old and current bounds, including
 * rejected replacements and continued mutation of the installed tree.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* One title and a linked popup keep the maximum-sized fixture compact. */
static void make_menu(OBJECT *tree, WORD count)
{
    for (WORD i = 0; i < count; ++i)
        tree[i] = (OBJECT){NIL, NIL, NIL, G_IBOX, 0, 0, 0, 0, 0, 80, 16};
    tree[0].ob_head = 1;
    tree[0].ob_tail = 4;
    tree[1].ob_next = 4;
    tree[1].ob_head = tree[1].ob_tail = 2;
    tree[1].ob_type = G_BOX;
    tree[2].ob_next = 1;
    tree[2].ob_head = tree[2].ob_tail = 3;
    tree[3].ob_next = 2;
    tree[3].ob_type = G_TITLE;
    tree[3].ob_spec = (LONG)(intptr_t) " File ";
    tree[4].ob_next = 0;
    tree[4].ob_head = tree[4].ob_tail = 5;
    tree[5].ob_next = 4;
    tree[5].ob_head = 6;
    tree[5].ob_tail = count - 1;
    tree[5].ob_type = G_BOX;
    for (WORD i = 6; i < count; ++i) {
        tree[i].ob_type = G_STRING;
        tree[i].ob_spec = (LONG)(intptr_t) " Entry ";
        tree[i].ob_next = i + 1 < count ? i + 1 : 5;
    }
    tree[count - 1].ob_flags = LASTOB;
}

int main(void)
{
    OBJECT tree[GEM_RPC_MENU_MAX_OBJECTS + 1];
    OBJECT rejected[GEM_RPC_MENU_MAX_OBJECTS + 1];
    const WORD counts[] = {32 + 5, 33 + 5, 49 + 5,
                           64,     65,     GEM_RPC_MENU_MAX_OBJECTS};
    char long_text[GEM_RPC_MENU_STRING_MAX + 1];
    assert(appl_init() > 0);
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        make_menu(tree, counts[i]);
        assert(menu_bar(tree, 1) == 1);
        assert(menu_icheck(tree, counts[i] - 1, 1) == 1);
        assert(menu_ienable(tree, counts[i] - 1, 0) == 1);
        assert(menu_bar(tree, 0) == 1);
    }
    make_menu(tree, 7);
    assert(menu_bar(tree, 1) == 1);
    make_menu(rejected, GEM_RPC_MENU_MAX_OBJECTS + 1);
    assert(menu_bar(rejected, 1) == -1);
    make_menu(rejected, 7);
    memset(long_text, 'x', sizeof(long_text));
    long_text[sizeof(long_text) - 1] = '\0';
    rejected[6].ob_spec = (LONG)(intptr_t)long_text;
    assert(menu_bar(rejected, 1) == -1);
    long_text[GEM_RPC_MENU_STRING_MAX - 1] = '\0';
    assert(menu_bar(rejected, 1) == 1);
    assert(menu_bar(tree, 1) == 1);
    rejected[6].ob_next = 6;
    assert(menu_bar(rejected, 1) == -1);
    assert(gem_client_menu_changed(tree) == 1);
    /* If failure incorrectly selected rejected, this would attempt its
     * invalid graph again rather than doing nothing. */
    assert(gem_client_menu_changed(rejected) == 1);
    assert(menu_bar(tree, 0) == 1);
    assert(appl_exit() == 1);
    puts("32/33/49 strings, 64/65/256 objects and rejected replacements pass");
    return 0;
}

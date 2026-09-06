/*
 * Implements pointer-local AES utilities and cooperative event convenience
 * wrappers. Menu mutations republish the active tree to its owning server.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include "platform/os.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
WORD evnt_keybd(void)
{
    WORD key = 0;
    (void)evnt_multi(MU_KEYBD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0,
                     0, NULL, NULL, NULL, NULL, &key, NULL);
    return key;
}
WORD evnt_button(WORD clicks, UWORD mask, UWORD state, WORD *x, WORD *y,
                 WORD *buttons, WORD *keys)
{
    WORD count = 0;
    (void)evnt_multi(MU_BUTTON, clicks, mask, state, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, NULL, 0, 0, x, y, buttons, keys, NULL, &count);
    return count;
}
WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h, WORD *mx, WORD *my,
                WORD *buttons, WORD *keys)
{
    return evnt_multi(MU_M1, 0, 0, 0, flags, x, y, w, h, 0, 0, 0, 0, 0, NULL, 0,
                      0, mx, my, buttons, keys, NULL, NULL) != 0;
}
WORD evnt_timer(WORD low, WORD high)
{
    gem_os_sleep_ms((uint32_t)(UWORD)low | ((uint32_t)(UWORD)high << 16));
    return 1;
}
WORD menu_icheck(OBJECT *tree, WORD item, WORD check)
{
    if (!tree || item < 0)
        return 0;
    if (check)
        tree[item].ob_state |= CHECKED;
    else
        tree[item].ob_state &= ~CHECKED;
    return gem_client_menu_changed(tree);
}
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable)
{
    if (!tree || item < 0)
        return 0;
    if (enable)
        tree[item].ob_state &= ~DISABLED;
    else
        tree[item].ob_state |= DISABLED;
    return gem_client_menu_changed(tree);
}
WORD menu_text(OBJECT *tree, WORD item, char *text)
{
    if (!tree || item < 0 || !text)
        return 0;
    tree[item].ob_spec = (LONG)(intptr_t)text;
    return gem_client_menu_changed(tree);
}
static WORD parent_of(OBJECT *tree, WORD object)
{
    for (WORD i = 0; i < 128; ++i) {
        WORD child = tree[i].ob_head;
        for (unsigned n = 0; child >= 0 && child < 128 && n < 128; ++n) {
            if (child == object)
                return i;
            if (child == tree[i].ob_tail)
                break;
            child = tree[child].ob_next;
        }
        if (tree[i].ob_flags & LASTOB)
            break;
    }
    return NIL;
}
WORD objc_delete(OBJECT *tree, WORD object)
{
    if (!tree || object <= ROOT)
        return 0;
    WORD parent = parent_of(tree, object);
    if (parent < 0)
        return 0;
    WORD child = tree[parent].ob_head, previous = NIL;
    while (child != object) {
        previous = child;
        child = tree[child].ob_next;
    }
    if (previous == NIL)
        tree[parent].ob_head =
            object == tree[parent].ob_tail ? NIL : tree[object].ob_next;
    else
        tree[previous].ob_next = tree[object].ob_next;
    if (tree[parent].ob_tail == object)
        tree[parent].ob_tail = previous;
    tree[object].ob_next = NIL;
    return 1;
}
WORD objc_order(OBJECT *tree, WORD object, WORD position)
{
    if (!tree || object <= ROOT)
        return 0;
    WORD parent = parent_of(tree, object);
    if (parent < 0 || !objc_delete(tree, object))
        return 0;
    if (position < 0)
        return objc_add(tree, parent, object);
    WORD child = tree[parent].ob_head, previous = NIL;
    while (position-- > 0 && child != NIL && child != parent) {
        previous = child;
        child = tree[child].ob_next;
    }
    if (child == NIL || child == parent)
        return objc_add(tree, parent, object);
    tree[object].ob_next = child;
    if (previous == NIL)
        tree[parent].ob_head = object;
    else
        tree[previous].ob_next = object;
    return 1;
}
WORD shel_envrn(char **env, char *var)
{
    if (!env || !var)
        return 0;
    char *value = getenv(var);
    if (!value)
        return 0;
    *env = value;
    return 1;
}
WORD shel_find(char *path)
{
    char resolved[260];
    const char *resources = getenv("GEM_RESOURCE_DIR");
    if (!path)
        return 0;
    if (access(path, R_OK) == 0)
        return 1;
    if (!resources)
        resources = "bin/resources";
    int length = snprintf(resolved, sizeof(resolved), "%s/%s", resources, path);
    if (length < 0 || length >= (int)sizeof(resolved) || access(resolved, R_OK))
        return 0;
    strcpy(path, resolved);
    return 1;
}

/*
 * Verifies safe file/directory copies plus home Trash moves, metadata,
 * restore collisions, symlink-safe purge and emptying in isolated storage.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "gem_trash.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { TEST_ITEMS_MAX = 16 };

typedef struct test_items {
    gem_trash_item_t items[TEST_ITEMS_MAX];
    size_t count;
} test_items_t;

static void test_join(const char *left, const char *right, char *out,
                      size_t out_size)
{
    int length = snprintf(out, out_size, "%s/%s", left, right);

    assert(length > 0 && (size_t)length < out_size);
}

static void test_write(const char *path, const char *text)
{
    assert(gem_os_write_file_atomic(path, text, strlen(text)) != 0);
}

static void test_read(const char *path, const char *expected)
{
    char text[128];

    assert(gem_os_read_file(path, text, sizeof(text), NULL) != 0);
    assert(strcmp(text, expected) == 0);
}

static int test_collect(const gem_trash_item_t *item, void *opaque)
{
    test_items_t *items = (test_items_t *)opaque;

    assert(items->count < TEST_ITEMS_MAX);
    items->items[items->count++] = *item;
    return 1;
}

static void test_refresh(test_items_t *items)
{
    memset(items, 0, sizeof(*items));
    assert(gem_trash_list(test_collect, items, NULL) == GEM_TRASH_OK);
}

static int test_items_are_isolated(const test_items_t *items,
                                   const char *root)
{
    size_t index;
    size_t root_length = strlen(root);

    for (index = 0u; index < items->count; ++index) {
        if (strncmp(items->items[index].original_path, root, root_length) != 0 ||
            items->items[index].original_path[root_length] != '/') {
            return 0;
        }
    }
    return 1;
}

static void test_other_mount(const char *data)
{
    char directory[GEM_OS_PATH_MAX];
    char source[GEM_OS_PATH_MAX];
    char store[GEM_OS_PATH_MAX];
    char files[GEM_OS_PATH_MAX];
    char info[GEM_OS_PATH_MAX];
    gem_os_file_info_t mount_info;
    gem_trash_item_t item;
    test_items_t items;
    size_t index;
    int found = 0;
    int store_existed;

    if (gem_os_stat_path("/dev/shm", &mount_info) == 0 ||
        mount_info.is_directory == 0 ||
        gem_os_same_device("/dev/shm", data) != 0) {
        return;
    }
    (void)snprintf(directory, sizeof(directory), "/dev/shm/gem-trash-test-%ld",
                   (long)getpid());
    (void)snprintf(store, sizeof(store), "/dev/shm/.Trash-%lu",
                   (unsigned long)gem_os_getuid());
    store_existed = gem_os_path_exists(store);
    assert(gem_os_mkdir_p(directory) != 0);
    test_join(directory, "other.txt", source, sizeof(source));
    test_write(source, "other");
    assert(gem_trash_put(source, &item) == GEM_TRASH_OK);
    assert(strcmp(item.store_root, store) == 0);
    test_refresh(&items);
    for (index = 0u; index < items.count; ++index) {
        found |= strcmp(items.items[index].original_path, source) == 0;
    }
    assert(found != 0);
    assert(gem_trash_restore(&item) == GEM_TRASH_OK);
    test_read(source, "other");
    assert(gem_os_unlink(source) != 0);
    assert(gem_os_rmdir(directory) != 0);
    if (store_existed == 0) {
        test_join(store, "files", files, sizeof(files));
        test_join(store, "info", info, sizeof(info));
        (void)gem_os_rmdir(files);
        (void)gem_os_rmdir(info);
        (void)gem_os_rmdir(store);
    }
}

static void test_cross_device_directory_move(const char *work)
{
    char source[GEM_OS_PATH_MAX];
    char nested[GEM_OS_PATH_MAX];
    char source_file[GEM_OS_PATH_MAX];
    char destination[GEM_OS_PATH_MAX];
    char destination_file[GEM_OS_PATH_MAX];
    gem_os_file_info_t shared_memory;

    if (gem_os_stat_path("/dev/shm", &shared_memory) == 0 ||
        shared_memory.is_directory == 0 ||
        gem_os_same_device("/dev/shm", work) != 0) {
        return;
    }
    test_join(work, "tools", source, sizeof(source));
    test_join(source, "scripts", nested, sizeof(nested));
    assert(gem_os_mkdir_p(nested) != 0);
    test_join(nested, "tool.sh", source_file, sizeof(source_file));
    test_write(source_file, "#!/bin/sh\n");
    (void)snprintf(destination, sizeof(destination),
                   "/dev/shm/gem-move-tools-%ld", (long)getpid());
    assert(gem_os_path_exists(destination) == 0);
    assert(gem_os_move_path(source, destination) != 0);
    assert(gem_os_path_exists(source) == 0);
    test_join(destination, "scripts/tool.sh", destination_file,
              sizeof(destination_file));
    test_read(destination_file, "#!/bin/sh\n");
    assert(gem_os_remove_tree_under(destination, "/dev/shm") != 0);
}

static void test_copy_path(const char *work)
{
    char source[GEM_OS_PATH_MAX];
    char destination[GEM_OS_PATH_MAX];
    char nested[GEM_OS_PATH_MAX];
    char copied[GEM_OS_PATH_MAX];

    test_join(work, "copy-source.txt", source, sizeof(source));
    test_join(work, "copy-destination.txt", destination,
              sizeof(destination));
    test_write(source, "copy-file");
    assert(gem_os_copy_path(source, destination) != 0);
    test_read(source, "copy-file");
    test_read(destination, "copy-file");
    assert(gem_os_copy_path(source, destination) == 0);
    test_read(destination, "copy-file");

    test_join(work, "copy-tree", source, sizeof(source));
    test_join(source, "nested", nested, sizeof(nested));
    assert(gem_os_mkdir_p(nested) != 0);
    test_join(nested, "entry.txt", copied, sizeof(copied));
    test_write(copied, "copy-tree");
    test_join(work, "copied-tree", destination, sizeof(destination));
    assert(gem_os_copy_path(source, destination) != 0);
    test_join(destination, "nested/entry.txt", copied, sizeof(copied));
    test_read(copied, "copy-tree");
    test_join(source, "inside", destination, sizeof(destination));
    assert(gem_os_copy_path(source, destination) == 0);
    assert(gem_os_path_exists(destination) == 0);

    test_join(work, "move-source.txt", source, sizeof(source));
    test_join(work, "move-destination.txt", destination,
              sizeof(destination));
    test_write(source, "move-source");
    test_write(destination, "move-destination");
    assert(gem_os_move_path(source, destination) == 0);
    test_read(source, "move-source");
    test_read(destination, "move-destination");
}

int main(void)
{
    char root[] = "/tmp/gem-trash-test-XXXXXX";
    char data[GEM_OS_PATH_MAX];
    char work[GEM_OS_PATH_MAX];
    char path[GEM_OS_PATH_MAX];
    char child[GEM_OS_PATH_MAX];
    char nested[GEM_OS_PATH_MAX];
    char encoded[GEM_OS_PATH_MAX * 3];
    char decoded[GEM_OS_PATH_MAX];
    gem_trash_item_t first;
    gem_trash_item_t second;
    test_items_t items;
    size_t removed = 0u;

    assert(mkdtemp(root) != NULL);
    test_join(root, "data", data, sizeof(data));
    test_join(root, "work", work, sizeof(work));
    assert(gem_os_mkdir_p(data) != 0);
    assert(gem_os_mkdir_p(work) != 0);
    assert(setenv("XDG_DATA_HOME", data, 1) == 0);
    test_copy_path(work);
    test_cross_device_directory_move(work);
    test_other_mount(data);

    assert(gem_trash_encode_path("/tmp/a b/%", encoded, sizeof(encoded)) ==
           GEM_TRASH_OK);
    assert(strcmp(encoded, "%2Ftmp%2Fa%20b%2F%25") == 0);
    assert(gem_trash_decode_path(encoded, decoded, sizeof(decoded)) ==
           GEM_TRASH_OK);
    assert(strcmp(decoded, "/tmp/a b/%") == 0);
    assert(gem_trash_decode_path("%", decoded, sizeof(decoded)) ==
           GEM_TRASH_INVALID);

    test_join(work, "note one.txt", path, sizeof(path));
    test_write(path, "hello");
    assert(gem_trash_put(path, &first) == GEM_TRASH_OK);
    assert(gem_os_path_exists(path) == 0);
    assert(gem_os_path_exists(first.payload_path) != 0);
    assert(gem_os_path_exists(first.info_path) != 0);
    test_refresh(&items);
    assert(items.count == 1u);
    assert(strcmp(items.items[0].original_path, path) == 0);
    assert(gem_trash_restore(&items.items[0]) == GEM_TRASH_OK);
    test_read(path, "hello");

    test_join(work, "folder", path, sizeof(path));
    test_join(path, "sub", child, sizeof(child));
    assert(gem_os_mkdir_p(child) != 0);
    test_join(child, "nested.txt", nested, sizeof(nested));
    test_write(nested, "nested");
    assert(gem_trash_put(path, &first) == GEM_TRASH_OK);
    assert(first.is_directory != 0);
    assert(gem_trash_purge(&first) == GEM_TRASH_OK);
    assert(gem_os_path_exists(path) == 0);

    test_join(work, "same.txt", path, sizeof(path));
    test_write(path, "one");
    assert(gem_trash_put(path, &first) == GEM_TRASH_OK);
    test_write(path, "two");
    assert(gem_trash_put(path, &second) == GEM_TRASH_OK);
    assert(strcmp(first.name, second.name) != 0);
    assert(gem_trash_restore(&first) == GEM_TRASH_OK);
    assert(gem_trash_restore(&second) == GEM_TRASH_EXISTS);
    assert(gem_trash_restore_renamed(&second) == GEM_TRASH_OK);
    test_read(path, "one");
    test_join(work, "same.txt.1", path, sizeof(path));
    test_read(path, "two");

    test_join(work, "target.txt", child, sizeof(child));
    test_write(child, "target");
    test_join(work, "link.txt", path, sizeof(path));
    assert(symlink(child, path) == 0);
    assert(gem_trash_put(path, &first) == GEM_TRASH_OK);
    assert(gem_trash_purge(&first) == GEM_TRASH_OK);
    test_read(child, "target");

    test_join(work, "empty-a", path, sizeof(path));
    test_write(path, "a");
    assert(gem_trash_put(path, NULL) == GEM_TRASH_OK);
    test_join(work, "empty-b", path, sizeof(path));
    test_write(path, "b");
    assert(gem_trash_put(path, NULL) == GEM_TRASH_OK);
    test_refresh(&items);
    assert(items.count >= 2u);
    if (test_items_are_isolated(&items, root) != 0) {
        assert(gem_trash_empty(&removed) == GEM_TRASH_OK);
        assert(removed == items.count);
        test_refresh(&items);
        assert(items.count == 0u);
    }

    assert(gem_os_remove_tree_under(root, "/tmp") != 0);
    return 0;
}

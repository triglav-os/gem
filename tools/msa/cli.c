#define _POSIX_C_SOURCE 200809L

/*
 * Implements a small command-line front-end for the reusable MSA and
 * FAT12 libraries. The tool manages whole floppy images and can also
 * inspect DOS/FAT12 directory contents when the decoded image contains
 * a compatible filesystem.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fat12/fat12.h"
#include "msa/msa.h"

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct msa_cli_options {
    msa_geometry_t geometry;
    int geometry_given;
} msa_cli_options_t;

typedef struct msa_ls_context {
    int printed_anything;
} msa_ls_context_t;

typedef struct msa_extract_context {
    const fat12_image_t *image;
    const char *dest_dir;
    char error_text[MSA_ERROR_LENGTH];
} msa_extract_context_t;

static void msa_print_usage(FILE *stream, const char *program_name);
static int msa_has_extension(const char *path, const char *extension);
static int msa_parse_u16(const char *text, uint16_t *value_out);
static int msa_parse_options(msa_cli_options_t *options,
    int *arg_index, int argc, char **argv);
static int msa_parse_command_io_args(msa_cli_options_t *options,
    int argc, char **argv, int start_index,
    const char **source_path, const char **dest_path);
static int msa_read_binary_file(const char *path, uint8_t **data_out,
    size_t *size_out, char *error_text, size_t error_size);
static int msa_write_binary_file(const char *path, const uint8_t *data,
    size_t size, char *error_text, size_t error_size);
static int msa_ls_print_entry(const fat12_dirent_t *entry,
    const char *path, void *context);
static int msa_command_ls(const char *path, int summary_only);
static int msa_command_cat(const char *path);
static int msa_command_unpack(const char *source_path, const char *dest_path);
static int msa_command_pack(const char *source_path, const char *dest_path,
    const msa_cli_options_t *options);
static int msa_command_cp(const char *source_path, const char *dest_path,
    const msa_cli_options_t *options);
static int msa_memory_read_sector(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size);
static int msa_mkdir_p(const char *path);
static void msa_set_extract_error(msa_extract_context_t *extract,
    const char *message, const char *path);
static int msa_extract_entry(const fat12_dirent_t *entry,
    const char *path, void *context);
static int msa_command_extract(const char *path, const char *dest_dir);

int main(int argc, char **argv)
{
    msa_cli_options_t options;
    int arg_index;
    const char *source_path = NULL;
    const char *dest_path = NULL;

    memset(&options, 0, sizeof(options));
    arg_index = 1;
    if (!msa_parse_options(&options, &arg_index, argc, argv)) {
        msa_print_usage(stderr, argv[0]);
        return 1;
    }
    if (arg_index >= argc) {
        msa_print_usage(stderr, argv[0]);
        return 1;
    }

    if (strcmp(argv[arg_index], "ls") == 0) {
        if (arg_index + 1 >= argc) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_ls(argv[arg_index + 1], 0);
    }
    if (strcmp(argv[arg_index], "info") == 0) {
        if (arg_index + 1 >= argc) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_ls(argv[arg_index + 1], 1);
    }
    if (strcmp(argv[arg_index], "cat") == 0) {
        if (arg_index + 1 >= argc) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_cat(argv[arg_index + 1]);
    }
    if (strcmp(argv[arg_index], "unpack") == 0) {
        if (!msa_parse_command_io_args(&options, argc, argv, arg_index + 1,
                &source_path, &dest_path)) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_unpack(source_path, dest_path);
    }
    if (strcmp(argv[arg_index], "pack") == 0) {
        if (!msa_parse_command_io_args(&options, argc, argv, arg_index + 1,
                &source_path, &dest_path)) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_pack(source_path, dest_path, &options);
    }
    if (strcmp(argv[arg_index], "cp") == 0) {
        if (!msa_parse_command_io_args(&options, argc, argv, arg_index + 1,
                &source_path, &dest_path)) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_cp(source_path, dest_path, &options);
    }
    if (strcmp(argv[arg_index], "extract") == 0 ||
        strcmp(argv[arg_index], "x") == 0) {
        if (!msa_parse_command_io_args(&options, argc, argv, arg_index + 1,
                &source_path, &dest_path)) {
            msa_print_usage(stderr, argv[0]);
            return 1;
        }
        return msa_command_extract(source_path, dest_path);
    }

    msa_print_usage(stderr, argv[0]);
    return 1;
}

static void msa_print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream,
        "Usage:\n"
        "  %s ls IMAGE.MSA\n"
        "  %s info IMAGE.MSA\n"
        "  %s cat IMAGE.MSA > IMAGE.ST\n"
        "  %s unpack IMAGE.MSA IMAGE.ST\n"
        "  %s pack IMAGE.ST IMAGE.MSA [--sectors-per-track N --sides N]\n"
        "  %s cp SRC DST [--sectors-per-track N --sides N]\n"
        "  %s extract IMAGE.MSA DIR\n"
        "\n"
        "Notes:\n"
        "  `cp` decodes when SRC ends in .MSA and encodes when DST ends in .MSA.\n"
        "  `ls` shows FAT12 directory contents when the decoded image has one.\n"
        "  Raw geometry is guessed from the boot sector when possible.\n",
        program_name, program_name, program_name, program_name, program_name,
        program_name, program_name);
}

static int msa_has_extension(const char *path, const char *extension)
{
    size_t path_length;
    size_t extension_length;
    size_t i;

    if (path == NULL || extension == NULL) {
        return 0;
    }
    path_length = strlen(path);
    extension_length = strlen(extension);
    if (path_length < extension_length) {
        return 0;
    }
    path += path_length - extension_length;
    for (i = 0; i < extension_length; ++i) {
        if (tolower((unsigned char) path[i]) !=
            tolower((unsigned char) extension[i])) {
            return 0;
        }
    }
    return 1;
}

static int msa_parse_u16(const char *text, uint16_t *value_out)
{
    char *end = NULL;
    long value;

    if (text == NULL || value_out == NULL) {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (end == text || end == NULL || *end != '\0' ||
        value < 0 || value > 65535L) {
        return 0;
    }
    *value_out = (uint16_t) value;
    return 1;
}

static int msa_parse_options(msa_cli_options_t *options,
    int *arg_index, int argc, char **argv)
{
    int index;

    if (options == NULL || arg_index == NULL) {
        return 0;
    }

    index = *arg_index;
    while (index < argc && strncmp(argv[index], "--", 2) == 0) {
        if (strcmp(argv[index], "--sectors-per-track") == 0) {
            if (index + 1 >= argc ||
                !msa_parse_u16(argv[index + 1],
                    &options->geometry.sectors_per_track)) {
                return 0;
            }
            options->geometry_given = 1;
            index += 2;
            continue;
        }
        if (strcmp(argv[index], "--sides") == 0) {
            if (index + 1 >= argc ||
                !msa_parse_u16(argv[index + 1], &options->geometry.sides)) {
                return 0;
            }
            options->geometry_given = 1;
            index += 2;
            continue;
        }
        if (strcmp(argv[index], "--start-track") == 0) {
            if (index + 1 >= argc ||
                !msa_parse_u16(argv[index + 1], &options->geometry.start_track)) {
                return 0;
            }
            options->geometry_given = 1;
            index += 2;
            continue;
        }
        if (strcmp(argv[index], "--end-track") == 0) {
            if (index + 1 >= argc ||
                !msa_parse_u16(argv[index + 1], &options->geometry.end_track)) {
                return 0;
            }
            options->geometry_given = 1;
            index += 2;
            continue;
        }
        return 0;
    }

    *arg_index = index;
    return 1;
}

static int msa_parse_command_io_args(msa_cli_options_t *options,
    int argc, char **argv, int start_index,
    const char **source_path, const char **dest_path)
{
    int index;
    int positional_count = 0;

    if (options == NULL || argv == NULL || source_path == NULL ||
        dest_path == NULL) {
        return 0;
    }

    *source_path = NULL;
    *dest_path = NULL;

    for (index = start_index; index < argc; ++index) {
        if (strncmp(argv[index], "--", 2) == 0) {
            int option_index = index;

            if (!msa_parse_options(options, &option_index, argc, argv)) {
                return 0;
            }
            index = option_index - 1;
            continue;
        }

        if (positional_count == 0) {
            *source_path = argv[index];
        } else if (positional_count == 1) {
            *dest_path = argv[index];
        } else {
            return 0;
        }
        ++positional_count;
    }

    return positional_count == 2;
}

static int msa_read_binary_file(const char *path, uint8_t **data_out,
    size_t *size_out, char *error_text, size_t error_size)
{
    FILE *stream;
    long file_size;
    uint8_t *data;

    if (path == NULL || data_out == NULL || size_out == NULL) {
        snprintf(error_text, error_size, "invalid input path");
        return 0;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        snprintf(error_text, error_size, "%s", strerror(errno));
        return 0;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to seek input file");
        return 0;
    }
    file_size = ftell(stream);
    if (file_size < 0 || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to size input file");
        return 0;
    }

    data = malloc((size_t) file_size);
    if (data == NULL) {
        fclose(stream);
        snprintf(error_text, error_size, "out of memory");
        return 0;
    }
    if (file_size > 0 &&
        fread(data, 1u, (size_t) file_size, stream) != (size_t) file_size) {
        free(data);
        fclose(stream);
        snprintf(error_text, error_size, "failed to read input file");
        return 0;
    }
    fclose(stream);

    *data_out = data;
    *size_out = (size_t) file_size;
    return 1;
}

static int msa_write_binary_file(const char *path, const uint8_t *data,
    size_t size, char *error_text, size_t error_size)
{
    FILE *stream;

    if (path == NULL || (size > 0 && data == NULL)) {
        snprintf(error_text, error_size, "invalid output path");
        return 0;
    }

    stream = fopen(path, "wb");
    if (stream == NULL) {
        snprintf(error_text, error_size, "%s", strerror(errno));
        return 0;
    }
    if (size > 0 && fwrite(data, 1u, size, stream) != size) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to write output file");
        return 0;
    }
    fclose(stream);
    return 1;
}

static int msa_ls_print_entry(const fat12_dirent_t *entry,
    const char *path, void *context)
{
    char attr[8];
    msa_ls_context_t *ls_context = context;

    fat12_format_attr(entry->attr, attr, sizeof(attr));
    printf("%-12s %4s %8u  %s\n", entry->name, attr, entry->size, path);
    if (ls_context != NULL) {
        ls_context->printed_anything = 1;
    }
    return 1;
}

static int msa_memory_read_sector(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size)
{
    fat12_memory_disk_t *memory_disk = context;
    return fat12_memory_disk_read_sector(memory_disk, sector_index,
        buffer, buffer_size);
}

static int msa_mkdir_p(const char *path)
{
    char *copy;
    char *scan;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    copy = malloc(strlen(path) + 1u);
    if (copy == NULL) {
        return 0;
    }
    strcpy(copy, path);

    for (scan = copy + 1; *scan != '\0'; ++scan) {
        if (*scan != '/') {
            continue;
        }
        *scan = '\0';
        if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
            free(copy);
            return 0;
        }
        *scan = '/';
    }

    if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
        free(copy);
        return 0;
    }
    free(copy);
    return 1;
}

static void msa_set_extract_error(msa_extract_context_t *extract,
    const char *message, const char *path)
{
    if (extract == NULL) {
        return;
    }
    if (path == NULL) {
        snprintf(extract->error_text, sizeof(extract->error_text),
            "%s", message);
        return;
    }
    snprintf(extract->error_text, sizeof(extract->error_text),
        "%s: %.180s", message, path);
}

static int msa_extract_entry(const fat12_dirent_t *entry,
    const char *path, void *context)
{
    msa_extract_context_t *extract = context;
    char output_path[512];
    char parent_path[512];
    char *slash;
    FILE *stream;
    uint8_t *data = NULL;
    size_t size = 0;

    if (extract == NULL || entry == NULL || path == NULL) {
        return 0;
    }

    snprintf(output_path, sizeof(output_path), "%s/%s", extract->dest_dir, path);
    if ((entry->attr & FAT12_ATTR_VOLUME) != 0u) {
        return 1;
    }
    if ((entry->attr & FAT12_ATTR_DIRECTORY) != 0u) {
        if (!msa_mkdir_p(output_path)) {
            msa_set_extract_error(extract, "failed to create directory",
                output_path);
            return 0;
        }
        return 1;
    }

    snprintf(parent_path, sizeof(parent_path), "%s", output_path);
    slash = strrchr(parent_path, '/');
    if (slash != NULL) {
        *slash = '\0';
        if (!msa_mkdir_p(parent_path)) {
            msa_set_extract_error(extract, "failed to create directory",
                parent_path);
            return 0;
        }
    }

    if (!fat12_read_file_alloc(extract->image, entry, &data, &size,
            extract->error_text, sizeof(extract->error_text))) {
        return 0;
    }

    stream = fopen(output_path, "wb");
    if (stream == NULL) {
        msa_set_extract_error(extract, "failed to open for writing",
            output_path);
        free(data);
        return 0;
    }
    if (size > 0 && fwrite(data, 1u, size, stream) != size) {
        msa_set_extract_error(extract, "failed to write", output_path);
        fclose(stream);
        free(data);
        return 0;
    }
    fclose(stream);
    free(data);
    return 1;
}

static int msa_command_ls(const char *path, int summary_only)
{
    char error_text[MSA_ERROR_LENGTH];
    msa_image_t image;
    fat12_image_t fat_image;
    fat12_disk_t fat_disk;
    fat12_memory_disk_t memory_disk;
    msa_ls_context_t ls_context;

    msa_image_init(&image);
    if (!msa_read_file(path, &image, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", path, error_text);
        return 1;
    }

    printf("%s\n", path);
    printf("  sectors/track: %u\n", image.geometry.sectors_per_track);
    printf("  sides: %u\n", image.geometry.sides);
    printf("  track range: %u-%u\n",
        image.geometry.start_track, image.geometry.end_track);
    printf("  decoded size: %zu bytes\n", image.data_size);
    printf("  track size: %zu bytes\n", msa_track_size(&image));
    if (summary_only) {
        msa_image_free(&image);
        return 0;
    }

    fat12_memory_disk_init(&memory_disk, image.data, image.data_size);
    fat_disk.context = &memory_disk;
    fat_disk.read_sector = msa_memory_read_sector;

    if (!fat12_image_open(&fat_image, &fat_disk,
            error_text, sizeof(error_text))) {
        printf("  filesystem: unrecognized (%s)\n", error_text);
        msa_image_free(&image);
        return 0;
    }

    printf("  filesystem: FAT12\n");
    printf("\n");
    printf("Name         Attr     Size  Path\n");
    printf("------------ ---- --------  ----\n");
    memset(&ls_context, 0, sizeof(ls_context));
    if (!fat12_walk_root(&fat_image, msa_ls_print_entry, &ls_context,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", path, error_text);
        msa_image_free(&image);
        return 1;
    }
    if (!ls_context.printed_anything) {
        printf("(no directory entries)\n");
    }

    msa_image_free(&image);
    return 0;
}

static int msa_command_cat(const char *path)
{
    char error_text[MSA_ERROR_LENGTH];
    msa_image_t image;

    msa_image_init(&image);
    if (!msa_read_file(path, &image, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", path, error_text);
        return 1;
    }
    if (image.data_size > 0 &&
        fwrite(image.data, 1u, image.data_size, stdout) != image.data_size) {
        fprintf(stderr, "msa: stdout: failed to write decoded image\n");
        msa_image_free(&image);
        return 1;
    }

    msa_image_free(&image);
    return 0;
}

static int msa_command_unpack(const char *source_path, const char *dest_path)
{
    char error_text[MSA_ERROR_LENGTH];
    msa_image_t image;

    msa_image_init(&image);
    if (!msa_read_file(source_path, &image, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", source_path, error_text);
        return 1;
    }
    if (!msa_write_binary_file(dest_path, image.data, image.data_size,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", dest_path, error_text);
        msa_image_free(&image);
        return 1;
    }

    msa_image_free(&image);
    return 0;
}

static int msa_command_pack(const char *source_path, const char *dest_path,
    const msa_cli_options_t *options)
{
    char error_text[MSA_ERROR_LENGTH];
    uint8_t *raw_data = NULL;
    size_t raw_size = 0;
    msa_geometry_t geometry;
    msa_image_t image;
    size_t track_count;

    msa_image_init(&image);
    memset(&geometry, 0, sizeof(geometry));

    if (!msa_read_binary_file(source_path, &raw_data, &raw_size,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", source_path, error_text);
        return 1;
    }

    if (options != NULL && options->geometry_given) {
        geometry = options->geometry;
        if (geometry.sectors_per_track == 0 || geometry.sides == 0) {
            fprintf(stderr,
                "msa: explicit geometry needs at least sectors-per-track and sides\n");
            free(raw_data);
            return 1;
        }
        if (raw_size % ((size_t) geometry.sectors_per_track *
                (size_t) geometry.sides * 512u) != 0u) {
            fprintf(stderr,
                "msa: raw image size is not divisible by the explicit geometry\n");
            free(raw_data);
            return 1;
        }
        track_count = raw_size / ((size_t) geometry.sectors_per_track *
            (size_t) geometry.sides * 512u);
        if (geometry.end_track == 0) {
            geometry.end_track = (uint16_t) (geometry.start_track +
                track_count - 1u);
        }
    } else if (!msa_guess_geometry(raw_data, raw_size, &geometry,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", source_path, error_text);
        free(raw_data);
        return 1;
    }

    if (!msa_build_image_from_raw(&image, raw_data, raw_size, &geometry,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", source_path, error_text);
        free(raw_data);
        return 1;
    }
    if (!msa_write_file(dest_path, &image, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", dest_path, error_text);
        free(raw_data);
        msa_image_free(&image);
        return 1;
    }

    free(raw_data);
    msa_image_free(&image);
    return 0;
}

static int msa_command_cp(const char *source_path, const char *dest_path,
    const msa_cli_options_t *options)
{
    int source_is_msa = msa_has_extension(source_path, ".msa");
    int dest_is_msa = msa_has_extension(dest_path, ".msa");

    if (source_is_msa == dest_is_msa) {
        fprintf(stderr,
            "msa: cp needs exactly one .MSA path; use pack or unpack explicitly\n");
        return 1;
    }
    if (source_is_msa) {
        return msa_command_unpack(source_path, dest_path);
    }
    return msa_command_pack(source_path, dest_path, options);
}

static int msa_command_extract(const char *path, const char *dest_dir)
{
    char error_text[MSA_ERROR_LENGTH];
    msa_image_t image;
    fat12_image_t fat_image;
    fat12_disk_t fat_disk;
    fat12_memory_disk_t memory_disk;
    msa_extract_context_t extract;

    msa_image_init(&image);
    if (!msa_read_file(path, &image, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", path, error_text);
        return 1;
    }

    fat12_memory_disk_init(&memory_disk, image.data, image.data_size);
    fat_disk.context = &memory_disk;
    fat_disk.read_sector = msa_memory_read_sector;
    if (!fat12_image_open(&fat_image, &fat_disk, error_text, sizeof(error_text))) {
        fprintf(stderr, "msa: %s: %s\n", path, error_text);
        msa_image_free(&image);
        return 1;
    }

    if (!msa_mkdir_p(dest_dir)) {
        fprintf(stderr, "msa: %s: failed to create destination directory\n", dest_dir);
        msa_image_free(&image);
        return 1;
    }

    memset(&extract, 0, sizeof(extract));
    extract.image = &fat_image;
    extract.dest_dir = dest_dir;
    if (!fat12_walk_root(&fat_image, msa_extract_entry, &extract,
            error_text, sizeof(error_text))) {
        if (extract.error_text[0] != '\0') {
            fprintf(stderr, "msa: %s\n", extract.error_text);
        } else {
            fprintf(stderr, "msa: %s: %s\n", path, error_text);
        }
        msa_image_free(&image);
        return 1;
    }

    msa_image_free(&image);
    return 0;
}

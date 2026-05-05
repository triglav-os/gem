/*
 * Implements a hosted replacement for the desktop DOS bridge. The module
 * translates GEM desktop file, directory, and drive calls onto the public
 * host OS abstraction so the imported shell can use real Linux services
 * without 16-bit DOS trap glue.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define GEM_USE_LIBC_STRINGS 1

#include <portab.h>
#include <machine.h>
#include <dos.h>
#include <infodef.h>
#include <platform/os.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

GLOBAL UWORD DOS_AX;
GLOBAL UWORD DOS_BX;
GLOBAL UWORD DOS_CX;
GLOBAL UWORD DOS_DX;
GLOBAL UWORD DOS_DS;
GLOBAL UWORD DOS_ES;
GLOBAL UWORD DOS_SI;
GLOBAL UWORD DOS_DI;
GLOBAL UWORD DOS_ERR;

static BYTE g_default_dta[128];
static BYTE *g_current_dta = &g_default_dta[0];
static int g_host_state_ready;
static int g_current_drive;
static char g_drive_root[26][GEM_OS_PATH_MAX];
static char g_drive_cwd[26][GEM_OS_PATH_MAX];

typedef struct host_search_state {
    int active;
    WORD attr;
    gem_os_dir_t dir;
    char pattern[32];
} host_search_state_t;

static host_search_state_t g_search;

static void host_copy_string(char *dst, size_t dst_size, const char *src)
{
    size_t i;

    if (dst == NULL || dst_size == 0u) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    i = 0u;
    while (src[i] != '\0' && i + 1u < dst_size) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static size_t host_string_length(const char *text)
{
    size_t length;

    if (text == NULL) {
        return 0u;
    }

    length = 0u;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static int host_strings_equal(const char *lhs, const char *rhs)
{
    size_t i;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    i = 0u;
    while (lhs[i] != '\0' && rhs[i] != '\0') {
        if (lhs[i] != rhs[i]) {
            return 0;
        }
        ++i;
    }

    return lhs[i] == rhs[i];
}

static int host_path_is_sep(char ch)
{
    return ch == '\\' || ch == '/';
}

static WORD host_errno_to_dos(int errnum)
{
    switch (errnum) {
    case 0:
        return E_NOFILES;
    case ENOENT:
        return E_FILENOTFND;
    case ENOTDIR:
        return E_PATHNOTFND;
    case EACCES:
    case EPERM:
        return E_NOACCESS;
    case EMFILE:
    case ENFILE:
        return E_NOHANDLES;
    case ENOMEM:
        return E_NOMEMORY;
    case EEXIST:
        return E_NOACCESS;
    case ENOTEMPTY:
        return E_NODELDIR;
    case ENOSPC:
        return E_NOMEMORY;
    default:
        return E_BADACCESS;
    }
}

static void host_set_error(WORD errnum)
{
    DOS_ERR = TRUE;
    DOS_AX = errnum;
}

static void host_set_errno_error(void)
{
    host_set_error(host_errno_to_dos(errno));
}

static void host_set_success(WORD value)
{
    DOS_ERR = FALSE;
    DOS_AX = value;
}

static void host_close_search(void)
{
    if (g_search.active) {
        gem_os_dir_close(&g_search.dir);
    }

    g_search.active = 0;
    g_search.pattern[0] = '\0';
}

static void host_ensure_state(void)
{
    char cwd[GEM_OS_PATH_MAX];
    int drive;

    if (g_host_state_ready) {
        return;
    }

    if (!gem_os_getcwd(cwd, sizeof(cwd))) {
        host_copy_string(cwd, sizeof(cwd), ".");
    }

    for (drive = 0; drive < 26; ++drive) {
        host_copy_string(g_drive_root[drive], sizeof(g_drive_root[drive]),
            cwd);
        host_copy_string(g_drive_cwd[drive], sizeof(g_drive_cwd[drive]), cwd);
    }

    g_current_drive = 0;
    g_host_state_ready = 1;
}

static int host_drive_from_spec(const char *path, const char **rest)
{
    int drive;

    host_ensure_state();

    drive = g_current_drive;
    if (rest != NULL) {
        *rest = path;
    }

    if (path != NULL &&
        isalpha((unsigned char) path[0]) &&
        path[1] == ':') {
        drive = toupper((unsigned char) path[0]) - 'A';
        if (rest != NULL) {
            *rest = path + 2;
        }
    }

    if (drive < 0 || drive >= 26) {
        drive = g_current_drive;
    }

    return drive;
}

static int host_translate_path(const char *dos_path, char *host_path,
                               size_t host_size)
{
    const char *rest;
    const char *base;
    size_t out;
    int drive;
    int absolute;

    host_ensure_state();

    if (host_path == NULL || host_size == 0u) {
        errno = EINVAL;
        return 0;
    }

    drive = host_drive_from_spec(dos_path, &rest);
    absolute = (rest != NULL && host_path_is_sep(rest[0])) ? 1 : 0;
    base = absolute ? g_drive_root[drive] : g_drive_cwd[drive];

    host_copy_string(host_path, host_size, base);
    out = host_string_length(host_path);

    while (rest != NULL && host_path_is_sep(*rest)) {
        ++rest;
    }

    if (rest == NULL || *rest == '\0') {
        return 1;
    }

    if (out > 0u && host_path[out - 1u] != '/') {
        if (out + 1u >= host_size) {
            errno = ENAMETOOLONG;
            return 0;
        }
        host_path[out++] = '/';
        host_path[out] = '\0';
    }

    while (*rest != '\0') {
        if (host_path_is_sep(*rest)) {
            if (out == 0u || host_path[out - 1u] != '/') {
                if (out + 1u >= host_size) {
                    errno = ENAMETOOLONG;
                    return 0;
                }
                host_path[out++] = '/';
                host_path[out] = '\0';
            }
            ++rest;
            continue;
        }

        if (out + 1u >= host_size) {
            errno = ENAMETOOLONG;
            return 0;
        }
        host_path[out++] = *rest++;
        host_path[out] = '\0';
    }

    return 1;
}

static int host_dos_attr_from_info(const gem_os_file_info_t *info)
{
    int attr;

    attr = 0;
    if (info == NULL) {
        return attr;
    }

    if (info->is_directory) {
        attr |= F_SUBDIR;
    } else {
        attr |= F_ARCHIVE;
    }
    if (info->is_hidden) {
        attr |= F_HIDDEN;
    }
    if (info->is_read_only) {
        attr |= F_RDONLY;
    }
    return attr;
}

static void host_pack_dos_datetime(uint64_t ms, UWORD *time_word,
                                   UWORD *date_word)
{
    time_t seconds;
    struct tm tm_value;
    struct tm *tm_ptr;
    int year;

    seconds = (time_t) (ms / 1000u);
    tm_ptr = localtime(&seconds);
    if (tm_ptr == NULL) {
        if (time_word != NULL) {
            *time_word = 0;
        }
        if (date_word != NULL) {
            *date_word = 0;
        }
        return;
    }

    tm_value = *tm_ptr;
    year = tm_value.tm_year + 1900;
    if (year < 1980) {
        year = 1980;
    }

    if (time_word != NULL) {
        *time_word = (UWORD) (((tm_value.tm_hour & 0x1f) << 11) |
            ((tm_value.tm_min & 0x3f) << 5) |
            ((tm_value.tm_sec / 2) & 0x1f));
    }

    if (date_word != NULL) {
        *date_word = (UWORD) ((((year - 1980) & 0x7f) << 9) |
            (((tm_value.tm_mon + 1) & 0x0f) << 5) |
            (tm_value.tm_mday & 0x1f));
    }
}

static void host_make_dos_name(const char *name, char *out_name,
                               size_t out_size)
{
    size_t i;

    if (out_name == NULL || out_size == 0u) {
        return;
    }

    if (name == NULL) {
        out_name[0] = '\0';
        return;
    }

    i = 0u;
    while (name[i] != '\0' && i + 1u < out_size) {
        out_name[i] = (char) toupper((unsigned char) name[i]);
        ++i;
    }
    out_name[i] = '\0';
}

static void host_fill_dta(const char *name, const gem_os_file_info_t *info)
{
    BYTE *dta;
    FCB *fcb;
    UWORD time_word;
    UWORD date_word;
    uint32_t size32;
    char dos_name[13];
    int attr;

    dta = (g_current_dta != NULL) ? g_current_dta : &g_default_dta[0];
    fcb = (FCB *) dta;

    memset(dta, 0, 43u);
    attr = host_dos_attr_from_info(info);
    host_pack_dos_datetime(info->mtime_ms, &time_word, &date_word);
    size32 = (uint32_t) (info->size_bytes & 0xffffffffu);
    host_make_dos_name(name, dos_name, sizeof(dos_name));

    dta[21] = (BYTE) attr;
    dta[22] = (BYTE) (time_word & 0x00ffu);
    dta[23] = (BYTE) ((time_word >> 8) & 0x00ffu);
    dta[24] = (BYTE) (date_word & 0x00ffu);
    dta[25] = (BYTE) ((date_word >> 8) & 0x00ffu);
    dta[26] = (BYTE) (size32 & 0x000000ffu);
    dta[27] = (BYTE) ((size32 >> 8) & 0x000000ffu);
    dta[28] = (BYTE) ((size32 >> 16) & 0x000000ffu);
    dta[29] = (BYTE) ((size32 >> 24) & 0x000000ffu);
    host_copy_string((char *) &dta[30], 13u, dos_name);

    memset(fcb->fcb_reserved, 0, sizeof(fcb->fcb_reserved));
    fcb->fcb_attr = (BYTE) attr;
    fcb->fcb_time = time_word;
    fcb->fcb_date = date_word;
    fcb->fcb_size = (LONG) info->size_bytes;
    host_copy_string((char *) fcb->fcb_name, sizeof(fcb->fcb_name), dos_name);
}

static int host_pattern_match(const char *pattern, const char *name)
{
    unsigned char pc;
    unsigned char nc;

    if (pattern == NULL || name == NULL) {
        return 0;
    }
    if (host_strings_equal(pattern, "*") ||
        host_strings_equal(pattern, "*.*")) {
        return 1;
    }

    pc = (unsigned char) *pattern;
    nc = (unsigned char) *name;

    if (pc == '\0') {
        return nc == '\0';
    }

    if (pc == '*') {
        do {
            if (host_pattern_match(pattern + 1, name)) {
                return 1;
            }
        } while (*name++ != '\0');
        return 0;
    }

    if (nc == '\0') {
        return 0;
    }

    if (pc == '?' ||
        toupper(pc) == toupper(nc)) {
        return host_pattern_match(pattern + 1, name + 1);
    }

    return 0;
}

static int host_search_accept(const gem_os_dirent_t *entry, WORD attr_mask)
{
    if (entry == NULL) {
        return 0;
    }

    if (host_strings_equal(entry->name, ".") ||
        host_strings_equal(entry->name, "..")) {
        return 0;
    }

    if (entry->info.is_directory && !(attr_mask & F_SUBDIR)) {
        return 0;
    }

    if (entry->info.is_hidden && !(attr_mask & F_HIDDEN)) {
        return 0;
    }

    return 1;
}

static WORD host_continue_search(void)
{
    gem_os_dirent_t entry;

    if (!g_search.active) {
        host_set_error(E_NOFILES);
        return FALSE;
    }

    while (gem_os_dir_read(&g_search.dir, &entry)) {
        if (!host_search_accept(&entry, g_search.attr)) {
            continue;
        }
        if (!host_pattern_match(g_search.pattern, entry.name)) {
            continue;
        }

        host_fill_dta(entry.name, &entry.info);
        host_set_success(0);
        return TRUE;
    }

    host_close_search();
    host_set_error(E_NOFILES);
    return FALSE;
}

VOID dos_chdir(LONG pdrvpath)
{
    char host_path[GEM_OS_PATH_MAX];
    gem_os_file_info_t info;
    const char *source;
    const char *rest;
    int drive;

    source = (const char *) (uintptr_t) pdrvpath;
    drive = host_drive_from_spec(source, &rest);

    if (!host_translate_path(source, host_path, sizeof(host_path)) ||
        !gem_os_stat_path(host_path, &info) ||
        !info.is_directory) {
        host_set_errno_error();
        return;
    }

    host_copy_string(g_drive_cwd[drive], sizeof(g_drive_cwd[drive]), host_path);
    g_current_drive = drive;
    host_set_success(0);
}

WORD dos_gdir(WORD drive, LONG pdrvpath)
{
    char *target;
    char *base;
    char *cwd;
    size_t base_length;

    host_ensure_state();

    if (drive <= 0 || drive > 26 || pdrvpath == 0) {
        host_set_error(E_BADDRIVE);
        return FALSE;
    }

    target = (char *) (uintptr_t) pdrvpath;
    base = g_drive_root[drive - 1];
    cwd = g_drive_cwd[drive - 1];
    base_length = host_string_length(base);

    if (strncmp(cwd, base, base_length) == 0) {
        cwd += base_length;
    }
    while (*cwd == '/') {
        ++cwd;
    }

    host_copy_string(target, GEM_OS_PATH_MAX, cwd);
    host_set_success(0);
    return TRUE;
}

WORD dos_gdrv(void)
{
    host_ensure_state();
    host_set_success((WORD) g_current_drive);
    return (WORD) g_current_drive;
}

WORD dos_sdrv(WORD newdrv)
{
    host_ensure_state();

    if (newdrv < 0 || newdrv >= 26) {
        host_set_error(E_BADDRIVE);
        return FALSE;
    }

    g_current_drive = newdrv;
    host_set_success((WORD) g_current_drive);
    return (WORD) g_current_drive;
}

VOID dos_sdta(LONG ldta)
{
    g_current_dta = (BYTE *) (uintptr_t) ldta;
    host_set_success(0);
}

LONG dos_gdta(void)
{
    host_set_success(0);
    return (LONG) (uintptr_t) g_current_dta;
}

WORD dos_gpsp(void)
{
    host_set_success(0);
    return 0;
}

WORD dos_spsp(WORD newpsp)
{
    host_set_success(newpsp);
    return newpsp;
}

WORD dos_sfirst(LONG pspec, WORD attr)
{
    char host_spec[GEM_OS_PATH_MAX];
    char directory[GEM_OS_PATH_MAX];
    char *slash;
    const char *pattern;

    if (pspec == 0) {
        host_set_error(E_PATHNOTFND);
        return FALSE;
    }

    if (!host_translate_path((const char *) (uintptr_t) pspec,
            host_spec, sizeof(host_spec))) {
        host_set_errno_error();
        return FALSE;
    }

    host_close_search();
    slash = strrchr(host_spec, '/');
    if (slash != NULL) {
        *slash = '\0';
        pattern = slash + 1;
        host_copy_string(directory, sizeof(directory),
            host_spec[0] != '\0' ? host_spec : "/");
    } else {
        pattern = host_spec;
        host_copy_string(directory, sizeof(directory), ".");
    }

    if (pattern == NULL || *pattern == '\0') {
        pattern = "*";
    }

    if (!gem_os_dir_open(directory, &g_search.dir)) {
        host_set_errno_error();
        return FALSE;
    }

    g_search.active = 1;
    g_search.attr = attr;
    host_copy_string(g_search.pattern, sizeof(g_search.pattern), pattern);
    return host_continue_search();
}

WORD dos_snext(void)
{
    return host_continue_search();
}

WORD dos_open(LONG pname, WORD access)
{
    int fd;
    char host_path[GEM_OS_PATH_MAX];

    (void) access;

    if (!host_translate_path((const char *) (uintptr_t) pname,
            host_path, sizeof(host_path))) {
        host_set_errno_error();
        return 0;
    }

    fd = gem_os_open_read(host_path);
    if (fd < 0) {
        host_set_errno_error();
        return 0;
    }

    host_set_success((WORD) fd);
    return (WORD) fd;
}

WORD dos_close(WORD handle)
{
    if (gem_os_close(handle) != 0) {
        host_set_errno_error();
        return FALSE;
    }

    host_set_success(0);
    return TRUE;
}

WORD dos_read(WORD handle, WORD cnt, LONG pbuffer)
{
    int32_t read_count;

    read_count = gem_os_read(handle, (void *) (uintptr_t) pbuffer,
        (uint32_t) cnt);
    if (read_count < 0) {
        host_set_errno_error();
        return 0;
    }

    host_set_success((WORD) read_count);
    return (WORD) read_count;
}

LONG dos_lseek(WORD handle, WORD smode, LONG sofst)
{
    int64_t offset;

    offset = gem_os_seek(handle, (int64_t) sofst, smode);
    if (offset < 0) {
        host_set_errno_error();
        return 0;
    }

    host_set_success((WORD) (offset & 0xffff));
    return (LONG) offset;
}

VOID dos_exec(LONG pcspec, WORD segenv, LONG pcmdln, LONG pfcb1, LONG pfcb2)
{
    (void) pcspec;
    (void) segenv;
    (void) pcmdln;
    (void) pfcb1;
    (void) pfcb2;

    host_set_error(E_BADFUNC);
}

WORD dos_wait(void)
{
    host_set_success(0);
    return 0;
}

LONG dos_alloc(LONG nbytes)
{
    void *ptr;

    if (nbytes <= 0) {
        host_set_error(E_NOMEMORY);
        return 0;
    }

    ptr = gem_os_alloc((size_t) nbytes);
    if (ptr == NULL) {
        host_set_error(E_NOMEMORY);
        return 0;
    }

    host_set_success(0);
    return (LONG) (uintptr_t) ptr;
}

LONG dos_avail(void)
{
    uint64_t total_bytes;
    uint64_t avail_bytes;

    host_ensure_state();

    total_bytes = 0u;
    if (!gem_os_space(g_drive_cwd[g_current_drive], &total_bytes,
            &avail_bytes)) {
        host_set_errno_error();
        return 0;
    }

    host_set_success(0);
    return (LONG) avail_bytes;
}

WORD dos_free(LONG maddr)
{
    gem_os_free((void *) (uintptr_t) maddr);
    host_set_success(0);
    return TRUE;
}

WORD dos_stblk(WORD blockseg, WORD newsize)
{
    (void) blockseg;
    (void) newsize;
    host_set_success(0);
    return 0;
}

VOID dos_label(WORD drive, BYTE *plabel)
{
    char label[12];

    snprintf(label, sizeof(label), "DRIVE_%c",
        (char) ('A' + ((drive > 0) ? (drive - 1) : g_current_drive)));
    host_copy_string((char *) plabel, 12u, label);
    host_set_success(0);
}

VOID dos_space(WORD drv, LONG *ptotal, LONG *pavail)
{
    uint64_t total_bytes;
    uint64_t avail_bytes;
    int drive;

    host_ensure_state();

    drive = (drv > 0) ? (drv - 1) : g_current_drive;
    if (drive < 0 || drive >= 26) {
        host_set_error(E_BADDRIVE);
        if (ptotal != NULL) {
            *ptotal = 0;
        }
        if (pavail != NULL) {
            *pavail = 0;
        }
        return;
    }

    if (!gem_os_space(g_drive_cwd[drive], &total_bytes, &avail_bytes)) {
        host_set_errno_error();
        if (ptotal != NULL) {
            *ptotal = 0;
        }
        if (pavail != NULL) {
            *pavail = 0;
        }
        return;
    }

    if (ptotal != NULL) {
        *ptotal = (LONG) total_bytes;
    }
    if (pavail != NULL) {
        *pavail = (LONG) avail_bytes;
    }
    host_set_success(0);
}

WORD dos_rmdir(LONG ppath)
{
    char host_path[GEM_OS_PATH_MAX];

    if (!host_translate_path((const char *) (uintptr_t) ppath,
            host_path, sizeof(host_path)) ||
        !gem_os_rmdir(host_path)) {
        host_set_errno_error();
        return FALSE;
    }

    host_set_success(0);
    return TRUE;
}

WORD dos_create(LONG pname, WORD attr)
{
    int fd;
    char host_path[GEM_OS_PATH_MAX];

    (void) attr;

    if (!host_translate_path((const char *) (uintptr_t) pname,
            host_path, sizeof(host_path))) {
        host_set_errno_error();
        return 0;
    }

    fd = gem_os_open_write(host_path);
    if (fd < 0) {
        host_set_errno_error();
        return 0;
    }

    host_set_success((WORD) fd);
    return (WORD) fd;
}

WORD dos_mkdir(LONG ppath)
{
    char host_path[GEM_OS_PATH_MAX];

    if (!host_translate_path((const char *) (uintptr_t) ppath,
            host_path, sizeof(host_path)) ||
        !gem_os_mkdir(host_path)) {
        host_set_errno_error();
        return FALSE;
    }

    host_set_success(0);
    return TRUE;
}

WORD dos_delete(LONG pname)
{
    char host_path[GEM_OS_PATH_MAX];

    if (!host_translate_path((const char *) (uintptr_t) pname,
            host_path, sizeof(host_path)) ||
        !gem_os_unlink(host_path)) {
        host_set_errno_error();
        return DOS_AX;
    }

    host_set_success(0);
    return DOS_AX;
}

WORD dos_rename(LONG poname, LONG pnname)
{
    char old_path[GEM_OS_PATH_MAX];
    char new_path[GEM_OS_PATH_MAX];

    if (!host_translate_path((const char *) (uintptr_t) poname,
            old_path, sizeof(old_path)) ||
        !host_translate_path((const char *) (uintptr_t) pnname,
            new_path, sizeof(new_path)) ||
        !gem_os_rename(old_path, new_path)) {
        host_set_errno_error();
        return DOS_AX;
    }

    host_set_success(0);
    return DOS_AX;
}

WORD dos_write(WORD handle, WORD cnt, LONG pbuffer)
{
    int32_t write_count;

    write_count = gem_os_write(handle, (const void *) (uintptr_t) pbuffer,
        (uint32_t) cnt);
    if (write_count < 0) {
        host_set_errno_error();
        return 0;
    }

    host_set_success((WORD) write_count);
    return (WORD) write_count;
}

WORD dos_chmod(LONG pname, WORD func, WORD attr)
{
    char host_path[GEM_OS_PATH_MAX];
    gem_os_file_info_t info;
    WORD dos_attr;

    if (!host_translate_path((const char *) (uintptr_t) pname,
            host_path, sizeof(host_path)) ||
        !gem_os_stat_path(host_path, &info)) {
        host_set_errno_error();
        return 0;
    }

    dos_attr = (WORD) host_dos_attr_from_info(&info);
    if (func == F_SETMOD) {
        if (!gem_os_set_read_only(host_path, (attr & F_RDONLY) != 0)) {
            host_set_errno_error();
            return 0;
        }
        dos_attr = attr;
    }

    DOS_CX = dos_attr;
    host_set_success(dos_attr);
    return DOS_CX;
}

VOID dos_setdt(WORD handle, WORD time, WORD date)
{
    (void) handle;
    (void) time;
    (void) date;
    host_set_success(0);
}

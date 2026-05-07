/*
 * Provides hosted compatibility helpers for the imported desktop shell.
 * This module replaces a small part of the original DOS/compiler glue so
 * the historical desktop sources can build against the current hosted
 * AES/VDI runtime on Linux.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define GEM_USE_LIBC_STRINGS 1

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <gsxdefs.h>
#include <funcdef.h>
#include <gembind.h>

#include "resource/desktop.rsh"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern WORD contrl[];
extern WORD intin[];
extern WORD ptsin[];
extern WORD intout[];
extern WORD ptsout[];
extern WORD gl_handle;
extern WS gl_ws;
extern LONG ad_intin;
extern WORD gl_wchar;
extern WORD gl_hchar;

extern WORD global[];

extern WORD appl_init(VOID);
extern WORD appl_exit(VOID);
extern WORD appl_read(WORD rwid, WORD length, LONG pbuff);
extern WORD appl_write(WORD rwid, WORD length, LONG pbuff);
extern WORD appl_find(LONG pname);
extern WORD appl_tplay(LONG tbuffer, WORD tlength, WORD tscale);
extern WORD appl_trecord(LONG tbuffer, WORD tlength);
extern WORD appl_bvset(UWORD bvdisk, UWORD bvhard);
extern UWORD evnt_keybd(VOID);
extern WORD evnt_button(WORD clicks, UWORD mask, UWORD state,
    WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
extern WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h,
    WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
extern WORD evnt_mesag(LONG pbuff);
extern WORD evnt_timer(UWORD locnt, UWORD hicnt);
extern WORD evnt_multi(UWORD flags, UWORD bclk, UWORD bmsk, UWORD bst,
    UWORD m1flags, WORD m1x, WORD m1y, WORD m1w, WORD m1h,
    UWORD m2flags, WORD m2x, WORD m2y, WORD m2w, WORD m2h,
    WORD mepbuff[8], UWORD tlc, UWORD thc, WORD *pmx, WORD *pmy,
    WORD *pmb, WORD *pks, WORD *pkr, WORD *pbr);
extern WORD evnt_dclick(WORD rate, WORD setit);
extern WORD menu_bar(LONG tree, WORD showit);
extern WORD menu_icheck(LONG tree, WORD itemnum, WORD checkit);
extern WORD menu_ienable(LONG tree, WORD itemnum, WORD enableit);
extern WORD menu_tnormal(LONG tree, WORD titlenum, WORD normalit);
extern WORD menu_text(LONG tree, WORD inum, LONG ptext);
extern WORD menu_register(WORD pid, LONG pstr);
extern WORD menu_unregister(WORD mid);
extern WORD menu_click(WORD click, WORD setit);
extern WORD objc_add(LONG tree, WORD parent, WORD child);
extern WORD objc_delete(LONG tree, WORD delob);
extern WORD objc_draw(OBJECT *tree, WORD drawob, WORD depth,
    WORD xc, WORD yc, WORD wc, WORD hc);
extern WORD objc_find(OBJECT *tree, WORD startob, WORD depth,
    WORD mx, WORD my);
extern WORD objc_offset(LONG tree, WORD obj, WORD *poffx, WORD *poffy);
extern WORD objc_order(LONG tree, WORD mov_obj, WORD newpos);
extern WORD objc_edit(OBJECT *tree, WORD object, WORD charidx,
    WORD *idx, WORD kind);
extern WORD objc_change(OBJECT *tree, WORD object, WORD depth,
    WORD xc, WORD yc, WORD wc, WORD hc, WORD newstate, WORD redraw);
extern WORD form_do(LONG form, WORD start);
extern WORD form_dial(WORD flag, WORD x1, WORD y1, WORD w1, WORD h1,
    WORD x2, WORD y2, WORD w2, WORD h2);
extern WORD form_alert(WORD defbut, LONG astring);
extern WORD form_error(WORD errnum);
extern WORD form_center(LONG tree, WORD *pcx, WORD *pcy, WORD *pcw, WORD *pch);
extern WORD form_keybd(OBJECT *tree, WORD object, WORD next,
    WORD kchar, WORD *next_out, WORD *char_out);
extern WORD form_button(OBJECT *tree, WORD object, WORD clicks,
    WORD *next_out);
extern WORD graf_rubbox(WORD xorigin, WORD yorigin, WORD minwidth,
    WORD minheight, WORD *pwend, WORD *phend);
extern WORD graf_dragbox(WORD w, WORD h, WORD sx, WORD sy,
    WORD xc, WORD yc, WORD wc, WORD hc, WORD *pdx, WORD *pdy);
extern WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy,
    WORD dstx, WORD dsty);
extern WORD graf_growbox(WORD x1, WORD y1, WORD w1, WORD h1,
    WORD x2, WORD y2, WORD w2, WORD h2);
extern WORD graf_shrinkbox(WORD x1, WORD y1, WORD w1, WORD h1,
    WORD x2, WORD y2, WORD w2, WORD h2);
extern WORD graf_watchbox(OBJECT *tree, WORD object,
    UWORD in_state, UWORD out_state);
extern WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object,
    WORD orientation);
extern WORD graf_handle(WORD *pwchar, WORD *phchar, WORD *pwbox, WORD *phbox);
extern WORD graf_mouse(WORD m_number, LONG m_addr);
extern VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmstate, WORD *pkstate);
extern WORD scrp_read(LONG pscrap);
extern WORD scrp_write(LONG pscrap);
extern WORD fsel_input(LONG pipath, LONG pisel, WORD *pbutton);
extern WORD wind_create(UWORD kind, WORD wx, WORD wy, WORD ww, WORD wh);
extern WORD wind_open(WORD handle, WORD wx, WORD wy, WORD ww, WORD wh);
extern WORD wind_close(WORD handle);
extern WORD wind_delete(WORD handle);
extern WORD wind_get(WORD handle, WORD field,
    WORD *w1, WORD *w2, WORD *w3, WORD *w4);
extern WORD wind_set(WORD handle, WORD field,
    WORD w1, WORD w2, WORD w3, WORD w4);
extern WORD wind_find(WORD mx, WORD my);
extern WORD wind_update(WORD beg_update);
extern WORD wind_calc(WORD type, UWORD kind, WORD inx, WORD iny,
    WORD inw, WORD inh, WORD *outx, WORD *outy, WORD *outw, WORD *outh);
extern WORD _aes_wind_set_text(WORD handle, WORD field, const char *text);
extern WORD rsrc_load(LONG rsname);
extern WORD rsrc_free(VOID);
extern WORD rsrc_gaddr(WORD rstype, WORD rsid, LONG *paddr);
extern WORD rsrc_saddr(WORD rstype, WORD rsid, LONG lngval);
extern WORD rsrc_obfix(LONG tree, WORD obj);
extern WORD shel_read(LONG pcmd, LONG ptail);
extern WORD shel_write(WORD doex, WORD isgr, WORD iscr, LONG pcmd, LONG ptail);
extern WORD shel_get(LONG pbuffer, WORD len);
extern WORD shel_put(LONG pdata, WORD len);
extern WORD shel_find(LONG ppath);
extern WORD shel_envrn(LONG ppath, LONG psrch);
extern WORD shel_rdef(LONG lpcmd, LONG lpdir);
extern WORD shel_wdef(LONG lpcmd, LONG lpdir);

extern VOID v_opnvwk(WORD *pwork_in, WORD *phandle, WORD *pwork_out);
extern VOID v_clsvwk(WORD handle);
extern VOID v_pline(WORD handle, WORD count, WORD *pxyarray);
extern WORD vst_height(WORD handle, WORD height, WORD *charw, WORD *charh,
    WORD *cellw, WORD *cellh);
extern WORD vsl_width(WORD handle, WORD width);
extern WORD vsl_type(WORD handle, WORD style);
extern WORD vsl_udsty(WORD handle, WORD pattern);
extern WORD vsf_interior(WORD handle, WORD style);
extern WORD vsf_style(WORD handle, WORD style);
extern VOID vsf_color(WORD handle, WORD color_index);
extern VOID vsl_color(WORD handle, WORD color_index);
extern VOID vst_color(WORD handle, WORD color_index);
extern WORD vswr_mode(WORD handle, WORD mode);
extern VOID vro_cpyfm(WORD handle, WORD mode, const WORD xy[8],
    FDB *src, FDB *dst);
extern VOID vrt_cpyfm(WORD handle, WORD mode, const WORD xy[8],
    FDB *src, FDB *dst, const WORD colors[2]);
extern VOID vr_recfl(WORD handle, WORD xy[4]);
extern WORD vr_trnfm(WORD handle, FDB *src, FDB *dst);
extern VOID v_gtext(WORD handle, WORD x, WORD y, const BYTE *text);
extern VOID v_hide_c(WORD handle);
extern VOID v_show_c(WORD handle, WORD reset);
extern VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4]);
extern WORD dr_code(LONG pparms);

static WORD *g_gsx_ptsin = ptsin;
static WORD *g_gsx_intin = intin;
static WORD *g_gsx_ptsout = ptsout;
static WORD *g_gsx_intout = intout;
static void *g_gsx_ptr1;
static void *g_gsx_ptr2;
static WORD g_builtin_rsrc_fixed;

LONG drawaddr;

static void hostshim_trace(const char *text)
{
    FILE *fp = fopen("/tmp/gem_hostshim_trace.log", "a");

    if (fp == NULL) {
        return;
    }
    fputs(text, fp);
    fputc('\n', fp);
    fclose(fp);
}

static WORD hosted_fix_coord(WORD value, WORD scale)
{
    UWORD raw = (UWORD) value;

    return (WORD) (((raw & 0x00ffu) * (UWORD) scale) + ((raw >> 8) & 0x00ffu));
}

static void hosted_fix_tedinfo(void)
{
    size_t i;

    for (i = 0; i < NUM_TI; ++i) {
        rs_tedinfo[i].te_ptext =
            (LONG) (intptr_t) rs_strings[(size_t) rs_tedinfo[i].te_ptext];
        rs_tedinfo[i].te_ptmplt =
            (LONG) (intptr_t) rs_strings[(size_t) rs_tedinfo[i].te_ptmplt];
        rs_tedinfo[i].te_pvalid =
            (LONG) (intptr_t) rs_strings[(size_t) rs_tedinfo[i].te_pvalid];
    }
}

static void hosted_fix_bitblk(void)
{
    size_t i;

    for (i = 0; i < NUM_BB; ++i) {
        rs_bitblk[i].bi_pdata =
            (LONG) (intptr_t) rs_imdope[(size_t) rs_bitblk[i].bi_pdata].image;
    }
}

static void hosted_fix_objects(void)
{
    size_t i;

    for (i = 0; i < NUM_OBS; ++i) {
        OBJECT *obj = &rs_object[i];

        obj->ob_x = hosted_fix_coord(obj->ob_x, gl_wchar);
        obj->ob_y = hosted_fix_coord(obj->ob_y, gl_hchar);
        obj->ob_width = hosted_fix_coord(obj->ob_width, gl_wchar);
        obj->ob_height = hosted_fix_coord(obj->ob_height, gl_hchar);

        switch (obj->ob_type) {
        case G_TITLE:
        case G_STRING:
        case G_BUTTON:
            obj->ob_spec =
                (LONG) (intptr_t) rs_strings[(size_t) obj->ob_spec];
            break;
        case G_TEXT:
        case G_BOXTEXT:
        case G_FTEXT:
        case G_FBOXTEXT:
            obj->ob_spec =
                (LONG) (intptr_t) &rs_tedinfo[(size_t) obj->ob_spec];
            break;
        case G_IMAGE:
            obj->ob_spec =
                (LONG) (intptr_t) &rs_bitblk[(size_t) obj->ob_spec];
            break;
        default:
            break;
        }
    }
}

static void hosted_fix_builtin_rsrc(void)
{
    if (g_builtin_rsrc_fixed != 0) {
        return;
    }

    hosted_fix_tedinfo();
    hosted_fix_bitblk();
    hosted_fix_objects();
    g_builtin_rsrc_fixed = 1;
}

int gem_builtin_rsrc_load(const char *filename)
{
    if (filename == NULL) {
        return 0;
    }

    if (strcmp(filename, "desktop.rsc") == 0 ||
        strcmp(filename, "DESKTOP.RSC") == 0) {
        hosted_fix_builtin_rsrc();
        return 1;
    }

    return 0;
}

int gem_builtin_rsrc_gaddr(WORD type, WORD index, void **addr)
{
    if (addr == NULL) {
        return 0;
    }

    switch (type) {
    case R_TREE:
        *addr = &rs_object[rs_trindex[index]];
        return 1;
    case R_OBJECT:
        *addr = &rs_object[index];
        return 1;
    case R_TEDINFO:
        *addr = &rs_tedinfo[index];
        return 1;
    case R_ICONBLK:
        *addr = &rs_iconblk[index];
        return 1;
    case R_BITBLK:
        *addr = &rs_bitblk[index];
        return 1;
    case R_STRING:
        *addr = rs_strings[rs_frstr[index]];
        return 1;
    default:
        return 0;
    }
}

void gem_builtin_rsrc_free(void)
{
}

WORD min(WORD a, WORD b)
{
    return (a < b) ? a : b;
}

WORD max(WORD a, WORD b)
{
    return (a > b) ? a : b;
}

WORD mul_div(WORD a, WORD b, WORD c)
{
    if (c == 0) {
        return 0;
    }
    return (WORD) (((LONG) a * (LONG) b) / c);
}

VOID r_set(GRECT *rect, WORD x, WORD y, WORD w, WORD h)
{
    if (rect == NULL) {
        return;
    }
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = w;
    rect->g_h = h;
}

VOID r_get(GRECT *rect, WORD *x, WORD *y, WORD *w, WORD *h)
{
    if (rect == NULL) {
        return;
    }
    if (x != NULL) {
        *x = rect->g_x;
    }
    if (y != NULL) {
        *y = rect->g_y;
    }
    if (w != NULL) {
        *w = rect->g_w;
    }
    if (h != NULL) {
        *h = rect->g_h;
    }
}

WORD rc_copy(GRECT *src, GRECT *dst)
{
    if (src == NULL || dst == NULL) {
        return 0;
    }
    memcpy(dst, src, sizeof(GRECT));
    return 1;
}

WORD rc_equal(GRECT *a, GRECT *b)
{
    if (a == NULL || b == NULL) {
        return FALSE;
    }
    return a->g_x == b->g_x && a->g_y == b->g_y &&
        a->g_w == b->g_w && a->g_h == b->g_h;
}

VOID movs(WORD num, VOID *ps, VOID *pd)
{
    memcpy(pd, ps, (size_t) num);
}

VOID bfill(WORD num, WORD value, BYTE *dst)
{
    memset(dst, value, (size_t) num);
}

BYTE *scasb(BYTE *text, WORD ch)
{
    BYTE *found;

    if (text == NULL) {
        return NULLPTR;
    }
    found = strchr(text, ch);
    return (found != NULL) ? found : text + strlen(text);
}

WORD strchk(BYTE *left, BYTE *right)
{
    if (left == NULLPTR && right == NULLPTR) {
        return 0;
    }
    if (left == NULLPTR) {
        return -1;
    }
    if (right == NULLPTR) {
        return 1;
    }
    return (WORD) strcmp(left, right);
}

WORD LSTRLEN(BYTE *text)
{
    return (WORD) ((text != NULLPTR) ? strlen(text) : 0u);
}

WORD LWCOPY(LONG dst, LONG src, WORD count)
{
    memcpy((void *) (uintptr_t) dst, (const void *) (uintptr_t) src,
        (size_t) count * sizeof(WORD));
    return count;
}

BYTE LBCOPY(LONG dst, LONG src, WORD count)
{
    memcpy((void *) (uintptr_t) dst, (const void *) (uintptr_t) src,
        (size_t) count);
    return 0;
}

WORD LBWMOV(LONG dst, LONG src)
{
    const char *source = (const char *) (uintptr_t) src;
    char *target = (char *) (uintptr_t) dst;
    size_t length = (source != NULL) ? strlen(source) : 0u;

    if (target != NULL && source != NULL) {
        memcpy(target, source, length + 1u);
    }
    return (WORD) length;
}

WORD LSTCPY(LONG dst, LONG src)
{
    const char *source = (const char *) (uintptr_t) src;
    char *target = (char *) (uintptr_t) dst;

    if (target == NULL || source == NULL) {
        return 0;
    }
    strcpy(target, source);
    return (WORD) strlen(target);
}

LONG LLDS(void)
{
    return 0;
}

LONG LLCS(void)
{
    return 0;
}

LONG HW(UWORD x)
{
    return ((LONG) x) << 16;
}

WORD LHIWD(LONG x)
{
    return (WORD) ((ULONG) x >> 16);
}

VOID i_ptsin(WORD *ptr)
{
    g_gsx_ptsin = (ptr != NULL) ? ptr : ptsin;
}

VOID i_intin(WORD *ptr)
{
    g_gsx_intin = (ptr != NULL) ? ptr : intin;
}

VOID i_ptsout(WORD *ptr)
{
    g_gsx_ptsout = (ptr != NULL) ? ptr : ptsout;
}

VOID i_intout(WORD *ptr)
{
    g_gsx_intout = (ptr != NULL) ? ptr : intout;
}

VOID i_ptr(VOID *ptr)
{
    g_gsx_ptr1 = ptr;
}

VOID i_ptr2(VOID *ptr)
{
    g_gsx_ptr2 = ptr;
}

VOID gsx2(void)
{
    switch (contrl[0]) {
    case OPEN_VWORKSTATION:
        v_opnvwk(g_gsx_intin, &contrl[6], g_gsx_intout);
        memcpy(&gl_ws, g_gsx_intout, sizeof(gl_ws));
        break;
    case CLOSE_VWORKSTATION:
        v_clsvwk(contrl[6]);
        break;
    case POLYLINE:
        v_pline(contrl[6], contrl[1], g_gsx_ptsin);
        break;
    case CHAR_HEIGHT:
        (void) vst_height(contrl[6], g_gsx_ptsin[1],
            &g_gsx_ptsout[0], &g_gsx_ptsout[1],
            &g_gsx_ptsout[2], &g_gsx_ptsout[3]);
        break;
    case S_LINE_TYPE:
        intout[0] = vsl_type(contrl[6], g_gsx_intin[0]);
        break;
    case S_LINE_WIDTH:
        intout[0] = vsl_width(contrl[6], g_gsx_ptsin[0]);
        break;
    case S_LINE_COLOR:
        vsl_color(contrl[6], g_gsx_intin[0]);
        intout[0] = g_gsx_intin[0];
        break;
    case S_TEXT_COLOR:
        vst_color(contrl[6], g_gsx_intin[0]);
        intout[0] = g_gsx_intin[0];
        break;
    case S_FILL_STYLE:
        intout[0] = vsf_interior(contrl[6], g_gsx_intin[0]);
        break;
    case S_FILL_INDEX:
        intout[0] = vsf_style(contrl[6], g_gsx_intin[0]);
        break;
    case S_FILL_COLOR:
        vsf_color(contrl[6], g_gsx_intin[0]);
        intout[0] = g_gsx_intin[0];
        break;
    case SET_WRITING_MODE:
        intout[0] = vswr_mode(contrl[6], g_gsx_intin[0]);
        break;
    case ST_UD_LINE_STYLE:
        intout[0] = vsl_udsty(contrl[6], (UWORD) g_gsx_intin[0]);
        break;
    case COPY_RASTER_FORM:
        vro_cpyfm(contrl[6], g_gsx_intin[0], g_gsx_ptsin,
            (FDB *) g_gsx_ptr1, (FDB *) g_gsx_ptr2);
        break;
    case TRAN_RASTER_FORM: {
        WORD colors[2] = { g_gsx_intin[2], g_gsx_intin[1] };

        vrt_cpyfm(contrl[6], g_gsx_intin[0], g_gsx_ptsin,
            (FDB *) g_gsx_ptr1, (FDB *) g_gsx_ptr2, colors);
        break;
    }
    case TRANSFORM_FORM:
        (void) vr_trnfm(contrl[6], (FDB *) g_gsx_ptr1, (FDB *) g_gsx_ptr2);
        break;
    case FILL_RECTANGLE:
        vr_recfl(contrl[6], g_gsx_ptsin);
        break;
    case TEXT_CLIP:
        (void) vs_clip(contrl[6], g_gsx_intin[0], g_gsx_ptsin);
        break;
    case SHOW_CUR:
        v_show_c(contrl[6], 1);
        break;
    case HIDE_CUR:
        v_hide_c(contrl[6]);
        break;
    case TEXT:
        v_gtext(contrl[6], g_gsx_ptsin[0], g_gsx_ptsin[1],
            (const BYTE *) (uintptr_t) ad_intin);
        break;
    default:
        break;
    }
}

VOID gsx_ncode(WORD code, WORD n, WORD m)
{
    contrl[0] = code;
    contrl[1] = n;
    contrl[3] = m;
    contrl[6] = gl_handle;
    gsx2();
}

VOID gsx_1code(WORD code, WORD value)
{
    intin[0] = value;
    gsx_ncode(code, 0, 1);
}

VOID gsx_vopen(void)
{
    WORD i;
    char buffer[160];

    hostshim_trace("gsx_vopen:enter");
    if (drawaddr == 0) {
        drawaddr = (LONG) (intptr_t) dr_code;
    }

    for (i = 0; i < 10; ++i) {
        intin[i] = 1;
    }
    intin[10] = 2;
    v_opnvwk(intin, &gl_handle, (WORD *) &gl_ws);
    hostshim_trace("gsx_vopen:after v_opnvwk");
    snprintf(buffer, sizeof(buffer),
        "gsx_vopen:ws xres=%d yres=%d wpixel=%d hpixel=%d ncolors=%d chmin=%d chmax=%d handle=%d",
        gl_ws.ws_xres, gl_ws.ws_yres, gl_ws.ws_wpixel, gl_ws.ws_hpixel,
        gl_ws.ws_ncolors, gl_ws.ws_chminh, gl_ws.ws_chmaxh, gl_handle);
    hostshim_trace(buffer);
}

VOID gsx_vclose(void)
{
    v_clsvwk(gl_handle);
}

WORD vst_clip(WORD clip_flag, WORD *pxyarray)
{
    vs_clip(gl_handle, clip_flag, pxyarray);
    return TRUE;
}

VOID desk_v_pline(WORD count, WORD *pxyarray)
{
    v_pline(gl_handle, count, pxyarray);
}

VOID desk_vst_height(WORD height,
                     WORD *pchr_width,
                     WORD *pchr_height,
                     WORD *pcell_width,
                     WORD *pcell_height)
{
    (void) vst_height(gl_handle, height, pchr_width, pchr_height,
        pcell_width, pcell_height);
}

VOID desk_vsl_width(WORD width)
{
    (void) vsl_width(gl_handle, width);
}

VOID desk_vro_cpyfm(WORD wr_mode,
                    WORD *pxyarray,
                    WORD *psrc_mfdb,
                    WORD *pdes_mfdb)
{
    vro_cpyfm(gl_handle, wr_mode, pxyarray, (FDB *) psrc_mfdb,
        (FDB *) pdes_mfdb);
}

VOID desk_vrt_cpyfm(WORD wr_mode,
                    WORD *pxyarray,
                    WORD *psrc_mfdb,
                    WORD *pdes_mfdb,
                    WORD fgcolor,
                    WORD bgcolor)
{
    WORD colors[2];

    colors[0] = bgcolor;
    colors[1] = fgcolor;
    vrt_cpyfm(gl_handle, wr_mode, pxyarray, (FDB *) psrc_mfdb,
        (FDB *) pdes_mfdb, colors);
}

VOID desk_vr_recfl(WORD *pxyarray, WORD *pdes_mfdb)
{
    (void) pdes_mfdb;
    vr_recfl(gl_handle, pxyarray);
}

VOID vrn_trnfm(WORD *psrc_mfdb, WORD *pdst_mfdb)
{
    (void) vr_trnfm(gl_handle, (FDB *) psrc_mfdb, (FDB *) pdst_mfdb);
}

UWORD inside(WORD x, WORD y, GRECT *rect)
{
    if (rect == NULL) {
        return FALSE;
    }

    return (x >= rect->g_x &&
        y >= rect->g_y &&
        x < rect->g_x + rect->g_w &&
        y < rect->g_y + rect->g_h) ? TRUE : FALSE;
}

WORD desk_wind_setl(WORD handle, WORD field, LONG value)
{
    return _aes_wind_set_text(handle, field,
        (const char *)(uintptr_t) value);
}

VOID takedos(void)
{
}

VOID takekey(void)
{
}

VOID takevid(void)
{
}

VOID givevid(void)
{
}

VOID givekey(void)
{
}

VOID givedos(void)
{
}

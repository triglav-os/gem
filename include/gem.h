/*
 * Provides a compatibility umbrella include for code that expects the
 * traditional top-level `gem.h` header name. It forwards to the public
 * hosted GEM umbrella inside the `gem/` namespace.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_COMPAT_H
#define GEM_COMPAT_H

#include <gem/gem.h>

#include <stdint.h>

/*
 * Classic name for the window work-area rectangle selector.
 * The hosted AES names the same selector `WF_CXYWH`.
 */
#ifndef WF_WORKXYWH
#define WF_WORKXYWH WF_CXYWH
#endif

/*
 * Classic name for the current outer window rectangle selector.
 * The hosted AES names the same selector `WF_WXYWH`.
 */
#ifndef WF_CURRXYWH
#define WF_CURRXYWH WF_WXYWH
#endif

/*
 * Opens access to the AES graphics environment without returning the
 * character and box metrics.
 *
 * Returns the VDI workstation handle supplied by AES, or zero when the
 * graphics environment is unavailable.
 */
static inline WORD gem_compat_graf_handle0(void)
{
    return graf_handle(NULL, NULL, NULL, NULL);
}

/*
 * Opens access to the AES graphics environment and returns its metrics.
 *
 * `wchar` and `hchar` receive the default character-cell dimensions.
 * `wbox` and `hbox` receive the minimum AES object-box dimensions.
 * Any output pointer may be `NULL`.
 *
 * Returns the VDI workstation handle supplied by AES, or zero when the
 * graphics environment is unavailable.
 */
static inline WORD gem_compat_graf_handle4(WORD *wchar,
                                           WORD *hchar,
                                           WORD *wbox,
                                           WORD *hbox)
{
    return graf_handle(wchar, hchar, wbox, hbox);
}

/*
 * Dispatches a compatibility `graf_handle()` call represented as four
 * output pointers. This helper is used by the public variadic macro and
 * is not normally called directly by applications.
 *
 * `args` contains the character-width, character-height, box-width, and
 * box-height output pointers in that order.
 *
 * Returns the VDI workstation handle supplied by AES.
 */
static inline WORD gem_compat_graf_handlev(WORD *args[4])
{
    return gem_compat_graf_handle4(args[0], args[1], args[2], args[3]);
}

/*
 * Implements the traditional polymorphic `wind_set()` calling form.
 *
 * `handle` selects the window and `field` selects the property. For
 * `WF_NAME` and `WF_INFO`, `w1` is interpreted as a string pointer.
 * Rectangle aliases are translated to their hosted AES selectors. All
 * other fields forward `w1` through `w4` as ordinary GEM words.
 *
 * Returns non-zero when AES accepts the requested property change.
 */
static inline WORD gem_compat_wind_set(WORD handle,
                                       WORD field,
                                       intptr_t w1,
                                       WORD w2,
                                       WORD w3,
                                       WORD w4)
{
    if (field == WF_NAME || field == WF_INFO) {
        return wind_set_str(handle, field, (const char *) (uintptr_t) w1);
    }
    if (field == WF_CURRXYWH) {
        field = WF_WXYWH;
    } else if (field == WF_WORKXYWH) {
        field = WF_CXYWH;
    }
    return wind_set(handle, field, (WORD) w1, w2, w3, w4);
}

/*
 * Adapts the five-argument historical `wind_set()` form by supplying a
 * zero fourth data word.
 *
 * Returns the result from `gem_compat_wind_set()`.
 */
static inline WORD gem_compat_wind_set5(WORD handle,
                                        WORD field,
                                        intptr_t w1,
                                        WORD w2,
                                        WORD w3)
{
    return gem_compat_wind_set(handle, field, w1, w2, w3, 0);
}

/*
 * Adapts the six-argument historical `wind_set()` form.
 *
 * Returns the result from `gem_compat_wind_set()`.
 */
static inline WORD gem_compat_wind_set6(WORD handle,
                                        WORD field,
                                        intptr_t w1,
                                        WORD w2,
                                        WORD w3,
                                        WORD w4)
{
    return gem_compat_wind_set(handle, field, w1, w2, w3, w4);
}

/*
 * Selects the five- or six-argument `wind_set()` compatibility adapter.
 * The `GEM_COMPAT_WIND_SET5` and `GEM_COMPAT_WIND_SET6` wrappers preserve
 * pointer-width values until the selected field determines their type.
 */
#define GEM_COMPAT_WIND_SET_GET(_1, _2, _3, _4, _5, _6, NAME, ...) NAME
#define GEM_COMPAT_WIND_SET5(a, b, c, d, e) \
    gem_compat_wind_set5((a), (b), (intptr_t) (c), (WORD) (d), (WORD) (e))
#define GEM_COMPAT_WIND_SET6(a, b, c, d, e, f) \
    gem_compat_wind_set6((a), (b), (intptr_t) (c), (WORD) (d), (WORD) (e), \
        (WORD) (f))

/*
 * Accepts either the no-argument compatibility form or the standard
 * four-output-pointer form of `graf_handle()`.
 */
#define graf_handle(...) \
    gem_compat_graf_handlev(((WORD *[5]) { NULL, ##__VA_ARGS__ }) + 1)

/*
 * Accepts the historical five- or six-argument forms of `wind_set()`.
 * String properties retain their native pointer width on 64-bit hosts.
 */
#define wind_set(...) \
    GEM_COMPAT_WIND_SET_GET(__VA_ARGS__, GEM_COMPAT_WIND_SET6, \
        GEM_COMPAT_WIND_SET5)(__VA_ARGS__)

#endif /* GEM_COMPAT_H */

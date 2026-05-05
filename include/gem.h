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

#ifndef WF_WORKXYWH
#define WF_WORKXYWH WF_CXYWH
#endif

#ifndef WF_CURRXYWH
#define WF_CURRXYWH WF_WXYWH
#endif

static inline WORD gem_compat_graf_handle0(void)
{
    return graf_handle(NULL, NULL, NULL, NULL);
}

static inline WORD gem_compat_graf_handle4(WORD *wchar,
                                           WORD *hchar,
                                           WORD *wbox,
                                           WORD *hbox)
{
    return graf_handle(wchar, hchar, wbox, hbox);
}

static inline WORD gem_compat_graf_handlev(WORD *args[4])
{
    return gem_compat_graf_handle4(args[0], args[1], args[2], args[3]);
}

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

static inline WORD gem_compat_wind_set5(WORD handle,
                                        WORD field,
                                        intptr_t w1,
                                        WORD w2,
                                        WORD w3)
{
    return gem_compat_wind_set(handle, field, w1, w2, w3, 0);
}

static inline WORD gem_compat_wind_set6(WORD handle,
                                        WORD field,
                                        intptr_t w1,
                                        WORD w2,
                                        WORD w3,
                                        WORD w4)
{
    return gem_compat_wind_set(handle, field, w1, w2, w3, w4);
}

#define GEM_COMPAT_WIND_SET_GET(_1, _2, _3, _4, _5, _6, NAME, ...) NAME
#define GEM_COMPAT_WIND_SET5(a, b, c, d, e) \
    gem_compat_wind_set5((a), (b), (intptr_t) (c), (WORD) (d), (WORD) (e))
#define GEM_COMPAT_WIND_SET6(a, b, c, d, e, f) \
    gem_compat_wind_set6((a), (b), (intptr_t) (c), (WORD) (d), (WORD) (e), \
        (WORD) (f))

#define graf_handle(...) \
    gem_compat_graf_handlev(((WORD *[5]) { NULL, ##__VA_ARGS__ }) + 1)

#define wind_set(...) \
    GEM_COMPAT_WIND_SET_GET(__VA_ARGS__, GEM_COMPAT_WIND_SET6, \
        GEM_COMPAT_WIND_SET5)(__VA_ARGS__)

#endif

# GEM Bitmaps

This note describes the GEM bitmap model that this project should use
internally when implementing VDI raster operations. The key GEM raster
descriptor is the `MFDB` structure declared in
[include/gem/vdi.h](/home/tstih/data/triglav-os/gem/include/gem/vdi.h).

## MFDB

An `MFDB` is a Memory Form Definition Block. It describes either:

- an off-screen bitmap in memory, or
- the physical device bitmap when `fd_addr == NULL`.

The GEM C structure is:

```c
typedef struct mfdb {
    VOID *fd_addr;
    WORD  fd_w;
    WORD  fd_h;
    WORD  fd_wdwidth;
    WORD  fd_stand;
    WORD  fd_nplanes;
    WORD  fd_r1;
    WORD  fd_r2;
    WORD  fd_r3;
} MFDB;
```

The fields are:

- `fd_addr`: pointer to the first byte of bitmap data, or `NULL` for
  the physical screen.
- `fd_w`: width in pixels.
- `fd_h`: height in pixels.
- `fd_wdwidth`: width of one scanline in 16-bit words.
- `fd_stand`: format flag. `1` means standard format, `0` means
  device-specific format.
- `fd_nplanes`: number of bitplanes.
- `fd_r1`, `fd_r2`, `fd_r3`: reserved and should be zero.

## Standard GEM Raster Layout

For this project, the most important case is the standard monochrome
layout:

- `fd_stand == 1`
- `fd_nplanes == 1`

In that layout:

- pixels are packed 1 bit per pixel,
- scanlines are padded to a 16-bit boundary,
- `fd_wdwidth` gives the scanline length in 16-bit words,
- the byte count of one scanline is `fd_wdwidth * 2`.

For a monochrome standard bitmap:

```text
row_bytes = fd_wdwidth * 2
bitmap_size = row_bytes * fd_h
```

Pixel `x,y` lives in:

```text
byte_index = y * row_bytes + x / 8
bit_mask   = 1 << (7 - (x & 7))
```

This means pixels are addressed most-significant-bit first inside each
byte.

For a project-local helper view, a monochrome standard bitmap can be
thought of like this:

```c
typedef struct gem_mono_bitmap {
    WORD     width;
    WORD     height;
    WORD     row_words;
    uint8_t *bits;
} gem_mono_bitmap_t;
```

Where:

- `width` corresponds to `fd_w`
- `height` corresponds to `fd_h`
- `row_words` corresponds to `fd_wdwidth`
- `bits` corresponds to `fd_addr`

This is not a historical GEM structure. It is just a convenient mental
model for code that reads a standard-format monochrome `MFDB`.

## Device-Specific Format

When `fd_stand == 0`, GEM allows the bitmap to be in the native layout
of the device driver. That can be faster for copies between the screen
and off-screen memory on real hardware, but it is driver-specific.

For now, this project should prefer the standard monochrome MFDB layout
for internal bitmap work, because it is simple, deterministic, and a
good match for optimized packed-bit drawing routines.

## Clipping and Coordinates

GEM raster coordinates are inclusive:

- source rectangle: `x0, y0, x1, y1`
- destination rectangle: `x0, y0, x1, y1`

That matters when computing copy widths and heights:

```text
width  = abs(x1 - x0) + 1
height = abs(y1 - y0) + 1
```

## Project Direction

The current `rasta` platform backend still presents through the platform
surface API, but the long-term fast VDI path should think in GEM MFDB
terms:

- packed 1 bpp scanlines,
- 16-bit scanline alignment,
- monochrome source and destination blits,
- clipping performed before or during blit operations.

That will let text, cursor rendering, pattern fills, and `vro_cpyfm` /
`vrt_cpyfm` share one efficient raster model.

# GEM Fonts

This note describes the classic GEM bitmap font format that VDI and
GDOS-style code expect. It also explains how that differs from the
temporary built-in font currently used by this project.

## GEM Bitmap Font Header

Classic GEM bitmap fonts are described by a `FONT_HDR` structure. A GEM
bitmap font is not stored as one tiny bitmap per glyph. Instead, the
glyph images live inside one wide monochrome raster image, and tables
describe where each character begins inside that raster.

The classic GEM bitmap font header is:

```c
typedef struct font_hdr {
    int16_t   font_id;
    int16_t   point;
    int8_t    name[32];
    uint16_t  first_ade;
    uint16_t  last_ade;
    uint16_t  top;
    uint16_t  ascent;
    uint16_t  half;
    uint16_t  descent;
    uint16_t  bottom;
    uint16_t  max_char_width;
    uint16_t  max_cell_width;
    uint16_t  left_offset;
    uint16_t  right_offset;
    uint16_t  thicken;
    uint16_t  ul_size;
    uint16_t  lighten;
    uint16_t  skew;
    uint16_t  flags;
    uint8_t  *hor_table;
    uint16_t *off_table;
    uint16_t *dat_table;
    uint16_t  form_width;
    uint16_t  form_height;
    struct font_hdr *next_font;
} FONT_HDR;
```

When reading a font file from disk, do not cast raw file bytes directly
to this structure on a modern host. GEM font files are an on-disk file
format with historical byte-order concerns, and the pointer fields in
the historical structure are logical offsets or runtime pointers
depending on context.

Important fields in a GEM bitmap font header are:

- `font_id`: font number.
- `point`: nominal point size.
- `name[32]`: font name.
- `first_ade`, `last_ade`: first and last character codes covered by
  the font.
- `top`, `ascent`, `half`, `descent`, `bottom`: vertical metrics
  relative to the baseline.
- `max_char_width`: widest glyph width.
- `max_cell_width`: widest cell width.
- `left_offset`, `right_offset`: italic skew support.
- `thicken`: bold thickening value.
- `ul_size`: underline thickness.
- `lighten`, `skew`: bitmasks used for style simulation.
- `flags`: metadata about system font status, byte order, proportional
  layout, and optional horizontal offset data.
- `hor_table`: optional horizontal adjustment table.
- `off_table`: glyph offset table.
- `dat_table`: pointer to the bitmap raster.
- `form_width`, `form_height`: raster dimensions.
- `next_font`: pointer to the next font header.

## Glyph Storage Model

The important conceptual model is:

- the font image is one monochrome packed bitmap,
- every glyph occupies a horizontal slice inside that bitmap,
- `off_table` gives the horizontal start offset of each glyph,
- glyph width is found by subtracting adjacent offsets,
- the bitmap raster height is the character image height.

So for character code `ch`:

```text
index = ch - first_ade
x0 = off_table[index]
x1 = off_table[index + 1]
glyph_width = x1 - x0
```

That makes GEM fonts naturally suited to row-based packed-bit blits.

For file parsing, it is often clearer to use a host-side decoded view
like this:

```c
typedef struct gem_font {
    int16_t         font_id;
    int16_t         point;
    char            name[33];
    uint16_t        first_ade;
    uint16_t        last_ade;
    uint16_t        top;
    uint16_t        ascent;
    uint16_t        half;
    uint16_t        descent;
    uint16_t        bottom;
    uint16_t        max_char_width;
    uint16_t        max_cell_width;
    uint16_t        left_offset;
    uint16_t        right_offset;
    uint16_t        thicken;
    uint16_t        ul_size;
    uint16_t        lighten;
    uint16_t        skew;
    uint16_t        flags;
    const uint8_t  *hor_table;
    const uint16_t *off_table;
    const uint16_t *dat_table;
    uint16_t        form_width;
    uint16_t        form_height;
} gem_font_t;
```

This second structure is not the historical GEM ABI. It is a safer
decoded in-memory representation for a loader after byte swapping and
table relocation have already been handled.

## Alignment and Packing

Like GEM bitmaps in general, the font image is monochrome and packed.
Raster lines are padded so the next line begins on a word boundary.

This is important because an efficient text renderer should usually:

- locate the glyph rectangle inside `dat_table`,
- compute its width from `off_table`,
- blit glyph rows into the destination bitmap with masking and clipping,
- avoid plotting one pixel at a time.

## Proportional vs Fixed Width

GEM fonts can be proportional. The offset table is what makes that
possible. Even when the font has a maximum cell width, actual glyph
widths may vary from character to character.

That means a proper GEM text renderer should advance by glyph width or
cell width according to the selected text behavior, not assume that
every character is a fixed 5x7 or 8x8 block.

## Byte Order Note

Historically, GEM fonts originated in Intel-oriented systems. Some GEM
documentation notes that bitmap font data may need byte interpretation
care depending on the host platform and the font header flags. For this
Linux port, it is safest to treat imported GEM font files as a defined
file format that should be parsed deliberately rather than by casting
raw bytes directly onto in-memory structs.

## Project Direction

The current VDI implementation in
[src/vdi/_vdi.c](/home/tstih/data/triglav-os/gem/src/vdi/_vdi.c) still
uses a tiny built-in fallback font for bring-up. That is acceptable as
a bootstrap mechanism, but it is not the correct long-term GEM design.

The right long-term direction is:

- keep font rendering behind private `_vdi` helpers,
- replace per-pixel glyph plotting with packed monochrome glyph blits,
- load or embed real GEM-compatible bitmap font data,
- use GEM metrics and offsets instead of a hand-written fixed font.

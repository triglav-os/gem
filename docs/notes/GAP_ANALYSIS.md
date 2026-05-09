# AES and VDI Gap Analysis

This note is intentionally about classic Atari ST-visible GEM behavior.
It excludes `proc_`, multitasking, and Atari-specific internals.
The question is simple: what is still missing before this port feels
compatible enough for a 1.0 aimed at normal GEM software?

## Current Position

The project already has the right broad shape for 1.0:

- AES has working application, event, menu, object, form, resource,
  file-selector, and window entry points in `src/aes/`.
- VDI has working workstation open/close, clipping, lines, text,
  fills, raster copy, cursor, and input entry points in `src/vdi/`.
- The public headers in `include/gem/aes.h` and `include/gem/vdi.h`
  expose a recognizably classic GEM surface.

So the remaining work is no longer "build AES and VDI from scratch."
The remaining work is mostly about compatibility depth and exactness.

## AES Gaps

## Priority 1: Object Geometry and Resource Fidelity

This is the biggest AES compatibility risk.

The current port already supports object trees, drawing, editing, and
resources, but classic GEM applications rely heavily on details:

- `rsrc_obfix()` and object layout need to match Atari ST sizing rules
  closely enough that imported `.RSC` trees render correctly without
  manual patching.
- Menu and dialog geometry should consistently respect character-cell
  assumptions, not just raw pixel values.
- `TEDINFO`, `ICONBLK`, and `BITBLK` rendering must match ST spacing,
  borders, justification, and clipping rules closely enough that old
  dialogs do not look "nearly right but off by one line."
- Object hit-testing should match the drawn object bounds exactly,
  especially for boxed text, buttons, icons, and menu titles.

If this area stays approximate, many old GEM programs will launch but
still feel broken.

## Priority 1: Form and Dialog Behavior

`form_do()`, `form_button()`, and `objc_edit()` exist, but 1.0 still
needs ST-like form behavior rather than just usable behavior.

Important remaining work:

- Confirm default button behavior, `EXIT` and `TOUCHEXIT` handling,
  radio-button groups, and `SELECTED` state transitions match ST GEM.
- Tighten text-edit behavior for cursor movement, delete/backspace,
  field-to-field navigation, validation strings, and template handling.
- Make dialog focus, redraw, and caret placement reliable for imported
  resource trees, not only hand-built hosted examples.
- Ensure alert box and form centering/layout match classic proportions
  well enough that old UI assets do not need retuning.

This is the difference between "the dialog appears" and "the dialog
acts like GEM."

## Priority 1: Window Manager Semantics

The window layer is already substantial, but ST compatibility depends on
behavioral details:

- `wind_calc()` and work/border rectangle conversion must match ST
  chrome sizing closely.
- `wind_get(WF_FIRSTXYWH/WF_NEXTXYWH)` visible rectangle enumeration
  must stay correct under overlap, move, size, top, and full events.
- Message ordering for `WM_REDRAW`, `WM_TOPPED`, `WM_NEWTOP`,
  `WM_MOVED`, `WM_SIZED`, `WM_ARROWED`, `WM_HSLID`, and `WM_VSLID`
  should match what classic event loops expect.
- Slider sizing and arrow/page behavior should feel like ST windows,
  not just generate roughly correct messages.
- The desktop/background ownership model should be stable enough for
  desktop-style applications and desk accessories.

This is core 1.0 work because nearly every non-trivial GEM app uses it.

## Priority 2: Menu Semantics

Menus are present, but compatibility is still sensitive to tree shape
and layout conventions.

What still matters:

- Broaden tolerance for classic menu tree topologies from different
  resource compilers and hand-built trees.
- Match title highlighting, submenu opening, keyboard navigation, and
  disabled/check-marked item behavior more closely.
- Keep menu geometry, spacing, and popup placement tied to ST metrics
  rather than hosted approximations.

This should be treated as part of the core UI polish for 1.0 because it
shows immediately in every menu-driven application.

## Priority 2: File Selector Behavior

The hosted `fsel_input()` is a strong functional start, but it is still
a hosted selector, not yet an ST-faithful selector.

Remaining work:

- Match classic path and wildcard behavior more closely.
- Tighten keyboard behavior, default buttons, and directory traversal.
- Decide how close the visual structure should be to the Atari ST
  selector instead of a generic hosted browser.

This is important, but less blocking than object, form, and window
compatibility.

## Priority 3: Shell and Scrap Semantics

These APIs exist, but they should be treated as compatibility support
rather than the main 1.0 blocker.

Still worth tightening:

- `scrp_read()`, `scrp_write()`, and `scrp_clear()` should behave like a
  stable GEM clipboard path contract, not just a hosted convenience.
- `shel_*` calls should return values and persist defaults in a way that
  old applications can rely on.
- `shel_find()` should be predictable for GEM-style file lookup.

Useful for completeness, but not the first place to spend time if the
goal is finishing 1.0 soon.

## VDI Gaps

## Priority 1: Text Metrics and Text Effects

For Atari ST compatibility, text is not just about drawing glyphs.
Applications assume stable metrics.

The current implementation already has bitmap font loading and text
drawing, but these areas still need tightening:

- `vst_height()`, `vst_point()`, `vqt_attributes()`, and related metric
  queries should report values that match the actual rendered font.
- `v_justified()` currently behaves like plain text truncation; proper
  word and character spacing is still needed.
- `vst_rotation()` is stored, but rotated text is not yet real text
  rendering.
- Text effects and alignment need ST-faithful behavior, especially for
  UI labels, alerts, and form text.
- Font selection should match the set and numbering that classic GEM
  software expects as closely as practical.

If text metrics are wrong, everything in AES ends up looking wrong too.

## Priority 1: Raster Operations and MFDB Fidelity

Classic GEM software relies heavily on blits and monochrome resources.

This still needs work:

- `vro_cpyfm()`, `vrt_cpyfm()`, and `vr_trnfm()` should be checked
  against ST expectations for write modes, clipping, plane handling, and
  standard-format MFDB conversion.
- Bit-image and icon drawing need to be trustworthy for 1-plane GEM UI
  assets.
- Multi-plane assumptions should be handled consistently even if the
  hosted backend stays simpler internally.
- Raster op semantics such as replace, transparent, xor, and erase need
  to match what classic code expects.

This is essential for icons, dialogs, menus, and many app redraw paths.

## Priority 1: Line, Fill, and Primitive Attribute Semantics

A large part of the primitive surface exists, but several calls are
still compatibility approximations.

Important gaps:

- Rounded rectangle calls currently fall back to simple rectangles.
  Classic GEM UI code does notice this.
- Line style, line width, end style, marker style, fill style, and fill
  perimeter behavior should affect rendering, not only state storage and
  attribute queries.
- Pattern and hatch fills should match the expected GEM look.
- Arc, pie, ellipse, and contour fill behavior should be verified for
  ST-style edge cases and clipping.

This is part compatibility and part visual polish, but it matters for a
credible 1.0.

## Priority 2: Workstation Capability Reporting

Many GEM programs branch on `work_out[]`, `vq_extnd()`, `vq_color()`,
`vq_chcells()`, and attribute queries.

That means:

- Reported capabilities must be internally consistent.
- Screen size, cell size, number of colors, number of planes, and input
  modes should describe the hosted workstation truthfully in GEM terms.
- Query functions should not return values that imply support for
  rendering features that are still stubbed or approximate.

Incorrect capability reporting causes subtle compatibility failures even
when the actual drawing code is mostly fine.

## Priority 2: Mouse, Locator, and Input Mode Semantics

The project already exposes mouse and string input calls, but classic
software expects more than "a mouse exists."

Worth tightening:

- Cursor show/hide nesting and custom form behavior should mirror GEM
  expectations more closely.
- Locator, valuator, and choice devices should return values with
  predictable GEM semantics.
- Input mode switching should behave consistently across calls.

This matters for drawing tools and some utilities, but is usually below
the main AES UI work.

## Priority 3: Alpha Text Console Compatibility

The alpha-cursor functions in `src/vdi/alpha.c` are mostly compatibility
helpers, and several behave like minimal hosted shims.

For 1.0:

- Keep them coherent and predictable.
- Do not spend major schedule time here unless a real target app uses
  them heavily.

They are useful, but not central to the Atari ST GEM desktop/UI story.

## Priority 3: Escape, Metafile, Palette, and Bit-Image Extras

This is the largest bucket of low-priority compatibility work.

Today, several calls in `src/vdi/misc.c` are mostly stubs or state-only
compatibility points:

- `v_escape()`
- `v_meta_extents()`
- `v_write_meta()`
- `vs_palette()`
- `v_bit_image()`

Unless 1.0 explicitly targets printer, metafile, or advanced device
workflows, these should stay behind the main UI and raster work.

## 1.0 Cut Line

The goal is to finish 1.0 soon, the practical cut line is:

Must have:

- Reliable resource-driven AES object geometry
- ST-like form editing and button semantics
- Stable window manager message and rectangle behavior
- Correct menu behavior for normal applications
- Faithful VDI text metrics
- Faithful raster copy and monochrome asset behavior
- Real line/fill attribute effects for normal GEM drawing

Should have:

- Better file selector fidelity
- Better workstation capability reporting
- Better mouse and input-mode semantics

Can defer:

- Metafile support
- Advanced escape functions
- Printer-oriented features
- Deep alpha-console fidelity

## Bottom Line

The project is already past the "missing major subsystems" phase.
For 1.0, the real gap is fidelity, not breadth.

The fastest route to an Atari ST-compatible 1.0 is to spend remaining
time on the parts that every normal GEM program sees immediately:
objects, forms, menus, windows, text metrics, raster copy, and drawing
attributes.

If those are made solid, the port can ship as a credible classic GEM
1.0 even if rarer VDI extras remain partial.

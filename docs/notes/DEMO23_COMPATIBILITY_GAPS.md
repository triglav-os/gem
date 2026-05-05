/*
 * Records the current compatibility gaps exposed by the classic
 * `demo23` sample. The goal is to keep the sample source close to old
 * GEM code and instead document which hosted AES/VDI behaviors still
 * diverge from historical expectations.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

# Demo23 Compatibility Gaps

`samples/demo23/main.c` is intentionally treated as a compatibility
probe rather than a hosted-native sample. The important question is not
"can we rewrite it to work?" but "which historical GEM assumptions does
the hosted port still violate?"

## Fixed In This Pass

1. `evnt_mesag()` now pumps hosted menu and window interactions instead
   of only dequeuing already-posted AES messages.
   Before this change, classic message-only event loops never saw live
   menu selections or mouse-driven window messages unless they used
   `evnt_multi()`.

2. `menu_bar(tree, 1)` now prepares and redraws the registered menu
   tree immediately.
   Before this change, the menu tree was only remembered internally, so
   classic code could reserve menu space without ever seeing the menu
   bar painted.

3. The hosted menu helper now tolerates one more classic tree shape.
   In particular, it can handle title objects that are direct children
   of the bar object instead of requiring an extra title-container box.

## Remaining Gaps

1. Classic `WORD` assumptions still leak into sample code.
   In this port, `WORD` is a 16-bit type in `include/gem/portab.h`.
   Old GEM code often used plain `int` because those targets also had
   16-bit `int`. On hosted GCC, `int` is 32-bit, so declarations such as
   `int msg[8]` or `int work_out[57]` are not ABI-clean for AES/VDI
   calls that exchange `WORD` arrays.

2. Hand-built object coordinates are still only partially compatible.
   The hosted object renderer treats `OBJECT.ob_x`, `ob_y`, `ob_width`,
   and `ob_height` as direct screen-space values. Classic GEM examples
   and resources often rely on menu/object sizing conventions derived
   from character cells and menu metrics. Small literal sizes such as
   `80,1` therefore do not yet map cleanly to historical visual output.

3. `demo23`'s menu tree does not match the exact topology used by the
   classic desktop resources in this repository.
   The shipped classic menu resources use:
   root -> bar -> active-title-container -> titles
   root -> popup-container -> popup boxes
   `demo23` is closer to a simplified hand-built tree. The hosted AES
   now tolerates more than one title layout, but popup discovery is
   still most reliable with the classic resource topology.

4. Compatibility is still stronger for resource-driven trees than for
   ad-hoc code-built trees.
   The desktop and RCS resource paths exercise more of the historical
   object layout conventions than the small handwritten sample does.

## What To Fix Next

1. Add a clean compatibility path for legacy `int` message/workstation
   arrays so classic examples do not need source edits just to satisfy
   the hosted ABI.

2. Move more menu geometry normalization into AES so classic
   character-sized menu trees are converted consistently before draw and
   hit-testing.

3. Tighten popup discovery and placement so classic hand-built trees can
   behave like resource-built trees without sample-side rewriting.

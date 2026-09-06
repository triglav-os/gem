# Demo23 compatibility status

Reviewed against [demo23](../../tests/uat/demo23/main.c) and its
[acceptance scenario](../../tests/uat/scenarios.py) on 2026-09-06.

## Current behavior

Demo23 is a Desk/File/Help menu application. It builds in direct AES/VDI and
libgem/gemd modes. Its current UAT compares the initial scene, opens Desk,
compares the menu image, dismisses it and closes the window.

The sample now uses `WORD` buffers and a repaired menu tree with explicit title
and popup containers. Earlier notes describing a missing popup hierarchy or
an unchanged legacy source are obsolete. Hosted menu/event handling and proxy
transport support this documented scenario.

## Remaining compatibility questions

- `WORD` is 16-bit; Linux `int` is not a substitute for AES/VDI array elements.
  Legacy source must use the public header types. The hosted ABI does not
  silently convert an `int *` argument into a `WORD *` buffer.
- Hand-built object coordinates are pixels, relative to their parent. Historical
  resource character-cell conversion and host-native resource layout require
  separate checks; a passing hand-built menu does not establish Atari binary
  resource compatibility.
- The scenario does not activate every menu item or exercise every keyboard
  navigation, disabled-item, check-mark and multi-application ordering case.

Future changes should add behavioral assertions for those paths. Keep the
[transport scope](../architecture/API_TRANSPORT.md) distinct from claims of
complete historical GEM compatibility.

# Source, security and build audit

Reviewed 2026-09-06. This report distinguishes confirmed fixes, executable
verification and remaining diagnostics. It does not certify the absence of
vulnerabilities or complete Atari semantic compatibility.

## Coverage and tools

- GCC Debug with `-Wall -Wextra -pedantic`, AddressSanitizer and
  UndefinedBehaviorSanitizer; native Linux and Rasta builds.
- Clang 18 static analysis: **152 Rasta compilation variants across 129 source
  files**, and **90 native Linux variants across 83 source files**. Both scans
  completed without analysis failures. Distinct preprocessor configurations are
  retained. Changed framebuffer variants were reanalyzed after the final fix.
- All **124 owned C implementation files** occur in these build databases.
  Headers are analyzed through their consumers. **153 owned C/header files**
  pass the file/naming/documentation checks and Clang Format 18 verification.
  After the 2026-09-11 reorganization the Rasta scan covers 208 variants
  across 179 files and 220 owned files pass the same checks (see below).
- The 2026-09-12 shell review covers 215 Rasta compilation variants with no
  analyzer diagnostics; all 229 current owned C/header files pass standards.
- All 16 project Python files parse; shell scripts pass Bash syntax checks.
  RPC regeneration produces the same formatted output as the checked-in code.
- The compiled Musashi core, SoftFloat, disassembler, generator and generated
  opcode tables are included. Original upstream examples/test harnesses retain
  their provenance and are outside GEM's build. Downloaded Rasta C++ and system
  package advisory scanning are separate dependency work, not covered by this
  C analyzer run.

Evidence: [Rasta analyzer results](../../build/audit/scan/results.json),
[Linux analyzer results](../../build/linux-audit/audit/scan/results.json).
Individual diagnostic logs and plists are beside those files. Rerun with
`make audit`; use `--build build/linux-audit` with `tools/scripts/audit.py` for
an already configured Linux build. Audit returns nonzero while diagnostics
remain; it does not silently suppress reviewed warnings.

## Confirmed fixes

| Area | Finding and resulting behavior | Verification |
| --- | --- | --- |
| FAT12 | Sector multiplication could wrap; invalid geometry and oversized file sizes reached reads/allocations. Bounds now precede arithmetic, reads and allocation. | `test_security_bounds`, `security_disk` |
| FAT traversal | Unsafe names could escape the destination; cyclic directories could recurse indefinitely. Names, path lengths, depth, entry counts and visited clusters are bounded. | `security_disk`: traversal, cycle and oversized-file cases |
| MSA extraction | Destination symlinks/hardlinks could overwrite unrelated files. Extraction uses directory descriptors and refuses unsafe inode types, owners and link counts before truncation. | `security_disk`: normal/repeated extraction, symlink and hardlink refusal |
| Stout | Guest VDI counts could overrun fixed stack arrays. Counts and guest spans are checked before copying; local argument arrays are initialized. System-vector dispatch and TOS trap stack ownership could regress while individual handlers still compiled. Writable FAT paths must remain inside the selected raw image, while host output must remain beneath the program's source directory without following symlinks. | `security_guest`: excessive and negative word/point counts, successful GEMDOS/BIOS/XBIOS vectors, caller-side stack cleanup, and nested relative `Fcreate` persisted into generated A: and host C: roots |
| AES object drawing | A NULL clipping rectangle could reach `memcpy`. Missing clips now resolve to screen bounds. | Existing object/tree UAT and integration suite |
| Sample bitmaps | Clock's resource lookup failure path did not free earlier cloned blocks. Partial-load cleanup now releases each prior allocation. Unused icon-loading paths were removed. | Sanitized builds and full sample session; residual analyzer notes below |
| SoftFloat | A 64-bit shift boundary could invoke C undefined behavior. The 128-bit helper handles boundary counts explicitly. | `test_security_bounds`: counts 0–128 against repeated single-bit shifts |
| Musashi generator | Unsigned EOF checks, body-limit indexing and unchecked argv copies were unsafe. EOF is signed; limits are checked before access; fatal exits are explicit. | Generated opcode build; empty/10,000-character path rejection under sanitizers |
| Framebuffer | Rectangle end arithmetic could overflow; a replacement/truncated viewer framebuffer lost pixels outside a partial update. Widened arithmetic and full restoration on remapping/resize fix both. | `test_gemd_host`: extreme coordinates, replacement and truncation followed by a one-pixel update |
| Desktop | Workspace suppressed the root-disk fallback on filtered container mounts; desktop/AES checker phases differed after dialog repaint. Disk fallback and background phase now agree. | Direct/proxy desktop UAT and prolonged system-alert all-samples regression |
| Launch scripts | Old startup helpers killed unrelated processes. Helpers now execute only their selected viewer/server; session cleanup retains ownership checks. | Combined session startup/cleanup |
| Reporting | Docker/custom-build reports linked to default-build artifacts. Evidence links now derive from the actual build directory. | `test_report` checks a nondefault build directory and a deliberately failing fixture |
| AES shell | A single last-command slot would let concurrent applications observe another launch, while direct process/environment calls tied AES to Linux. Launch state is keyed by reserved application id/PID. The historical global data area is an on-demand, bounded DESKTOP.INF projection of atomically saved `desktop.cfg`; its complete translation is mutex-protected and gemd reloads direct `libgemconf` replacements. Executable search, child-only environment construction, spawning, inspection and reaping use the platform OS layer. | `test_aes_shell`, `test_aes_rpc` |
| Desktop Trash | A rename-only move failed when the per-volume Trash was unavailable or the home fallback crossed filesystems. The OS wrapper now stages the source name, performs a no-follow copy of supported entry types, restores on copy failure and retains the complete destination if staging cleanup fails. | `test_gem_trash`: cross-device nested `tools/scripts/tool.sh`; direct/proxy failed-drop UAT followed by immediate folder activation |
| File Manager copy | Cross-window drops previously had no filesystem operation and a recursive implementation could follow links, recurse into its own destination or destroy a colliding path during cleanup. Normal-window drops now use a no-follow OS copy primitive, reject descendants and existing destinations, and bound cleanup to the destination parent. | `test_gem_trash`: regular file, nested directory, collision preservation and self-descendant rejection |

Local vendor changes are recorded in [Musashi patch notes](../notes/MUSASHI_PATCHES.md).
Public GEM APIs and legacy types retain their spelling and layout; private
reserved-prefix helpers were renamed. Source compatibility is preserved rather
than renaming Atari contracts to satisfy a mechanical style rule.

## Remaining analyzer diagnostics

The 2026-09-06 scans reported two possible leaks, in `gemscape_free_bitblk()`
and `maestro_free_bitblk()`, whose partial-load cleanup was inspected and
found to release every earlier plane. Those three per-sample copies were
replaced on 2026-09-11 by the single `sample_load_bitblks()` in
`samples/lib/resources`, and the Rasta scan of 208 compilation variants
across 179 files (`build/audit/scan/results.json`) reports **no diagnostics**.
The clean result reflects the analyzer's view of the consolidated ownership
path; it is not a proof, so allocation-failure injection remains a useful
follow-up for that helper.

The standards command checks a finite set of rules. Module size and comment
quality still require judgment; large legacy modules are listed for splitting
in [TODO](../notes/TODO.md). Vendor formatting and mandated GEM/POSIX names are
explicit compatibility exceptions. Sanitizers run only exercised paths; some
existing graphics tests disable leak detection for process-lifetime runtime
state. These checks are not exhaustive fuzzing or a sandbox assessment.

## Build and execution results

| Configuration | Outcome | Evidence |
| --- | --- | --- |
| Host GCC/Rasta | 84 tests passed, 0 failed | [Execution log](../../build/audit/host-verified.log) |
| Docker Ubuntu 24.04/GCC/Rasta | 84 tests passed, 0 failed | [Container execution log](../../build/audit/container-verified.log) |
| Native Linux Debug | Build passed without compiler warnings; physical display/input UAT not run | [Build log](../../build/audit/linux-verified.log) |
| Exported GEM SDK and standalone samples | Build passed; sample assets generated from `samples/data/` | [SDK log](../../build/audit/sdk-final.log), [samples log](../../build/audit/standalone-final.log) |
| Standards and formatting | 153 files passed at review time; 220 after the 2026-09-11 reorganization; no owned formatting violations | `make standards`, Clang Format 18 |
| Incremental resources | Unchanged inputs do not rerun resource generators | [Incremental log](../../build/audit/incremental.log) |

The full test suite includes 33 demos in direct/proxy modes, calculator and
desktop UAT in both modes, and the all-samples session with mouse/keyboard
interaction and window-lifetime checks. These tests are automated and require
no AI to run. See [the latest test report](LATEST.md) for each case's result.

Rasta and CEF are downloaded from TLS-verified, SHA-256-pinned archives into
`build/`; the default viewer is `bin/tools/rasta`, and Gemscape loads CEF from
the ignored build cache. A sibling checkout is never required. The normal
compiler toolchain is local. `make container` supplies it through Docker and
was tested with isolated container outputs. See the [development
guide](../guides/HOSTED_DEVELOPMENT.md) for dependencies and commands.

All sample-only disks, historical sample files and artwork live under
`samples/data/`; generated sample assets live beside sample executables in
`data/`. The SDK exports core resources and `resgen`, not sample artwork.
Current documentation links were audited; original manuals, vendor documents
and dated test outcomes retain their historical content. Incorrect container
artifact links in earlier reports were corrected without changing outcomes.

## Subsequent code review (2026-09-11)

A source review of the VDI, gemd transport, libgem wrappers and platform
backends found the following. Each fix has a regression that fails against
the previous code; the whole suite was rerun afterwards.

| Area | Finding and resulting behavior | Verification |
| --- | --- | --- |
| VDI raster copy | Inclusive corner widths were computed in `WORD`, so corners 65535 apart wrapped a copy width to zero and divided by zero; reachable through `BITMAP_COPY`. The screen-to-screen byte path also read source bytes at the destination's column, never clipped the source rows to the screen, copied overlapping rows top-down and rewrote whole edge bytes. Widths and offsets are `LONG`; the byte path checks the source lies on screen, walks rows away from the overlap and merges edge bytes through masks. | `test_vdi`: WORD-limit corners, byte-multiple shift, overlapping downward scroll, unaligned edges |
| VDI text | Glyph column clipping wrapped for `x` near 32767 and passed the clip test, then wrote far outside the row buffer; reachable through `V_GTEXT`. Edges are compared in `LONG`; `vdi_draw_screen_hline_direct` additionally clamps to the row. | `test_vdi`: text at 32760, 32767 and -32768 under sanitizers |
| gemd sessions | A closed exclusive VDI-only session kept its `standalone` flag until its slot was reused, refusing every later `appl_init`. The flag is cleared on close and the admission scan skips closed slots. | `test_gemd_security`: exclusive session, refused peer, release, admission |
| libgem menus | `menu_bar()` compared the full `ob_type`, so titles/entries with extended type bits sent no string while gemd requires one for the low byte, and the menu was rejected. The wrapper now masks the type like the server. | `test_aes_rpc`: extended-type title and entry accepted |
| Linux backend | `gem_raster_init()` rejected the documented width/height `0` before the code that resolves it to the framebuffer size, so a native session without `GEM_VDI_WIDTH`/`HEIGHT` could not open. Only the format is checked early. | Build; native display UAT still requires hardware |
| Terminal PTY | Closing a PTY waited unboundedly for a shell that ignores `SIGHUP`. A bounded wait now escalates to `SIGKILL`. | Sample session |
| Rasta subscription | Updated the pinned Rasta viewer to native `--inverse off --bpp 32` support; GEM publishes display-ready BGRA pixels, so inversion must remain disabled during subscriber reconfiguration. | Rebuild the verified Rasta dependency and run `test_rasta_polarity`; manual startup uses the documented options |
| UAT runner | Parallel sessions could pick the same probed UDP port and one viewer exited with "Address already in use". The runner restarts the viewer on a fresh port. | `make tests` |
| Window exposure | When an application closed a window and exited, `gemd_cleanup_app` purged every queued message carrying its id as *sender*, including the `WM_REDRAW` hints the AES had just queued to the applications its window uncovered; their work areas kept showing the closed window (observed with the calculator closed over the terminal). Only messages addressed to the leaving application are dropped now. | `test_gemd_security`: a fresh peer covers another window, closes and exits; the covered application must still receive a `WM_REDRAW` spanning the exposed area |
| gemd startup | An over-long `GEMD_SOCKET` path, or a failing `lstat`/`chmod`/`fcntl` on the new socket, made gemd exit silently. Each now reports the reason on stderr. | Manual: 108-byte socket path |
| Menu bar | Activating a window whose application has no menu kept showing the desktop owner's titles (`Desk File Arrange` over the terminal or a Stout guest), so the bar did not reflect the top window. The bar now shows an empty strip for such applications and follows the top window when the active application exits; the strip stays reserved while any menu exists so nothing moves. | `test_gemd_security`: wind_open and WF_TOP of a menu-less window give an empty bar distinct from both installed menus, topping a menu application restores its titles, the desktop still starts below the strip; headless matrix over calc, terminal, Stout and the desktop |
| Standards | `VDI_APPLY_MODE`, `VDI_CF_ROW` and `VDI_CF_PIX` replace three macros spelled with a reserved underscore-uppercase prefix. `make standards` now rejects that form outside C keywords and feature macros. | `make standards` |

Two further follow-ups from that review. `wind_set(WF_CXYWH)` positioned
the outer frame although `wind_get(WF_CXYWH)` reports the work area; it now
derives the frame from the work area through `wind_calc`, and `test_aes_rpc`
round-trips it. Stout's trap layer passed Atari window field numbers through
unchanged, so guests read swapped rectangles and only set the frame by
accident; it now translates `WF_WORKXYWH`/`WF_CURRXYWH` (4/5) onto the hosted
selectors, exercised by the guest drag in the all-samples session. The
per-present framebuffer identity check costs 1.7 µs per `stat`/`fstat` pair
on this host (under 0.2 % CPU at 1000 presents per second) and stays.

Cheaper hot paths: AES, draw and HID tracing resolve their environment
variable once instead of on every call, `evnt_multi` no longer resolves an
event's owner twice, `vdi_mfdb_draw_hline` rejects spans entirely outside a
bitmap instead of clamping them onto its edge pixel, and arc point counts are
computed without a `WORD` overflow for radii above 4095. Module sizes noted in
[TODO](../notes/TODO.md) are unchanged.

## Module reorganization (2026-09-11)

Every owned C file that exceeded the standard's 500-line guidance by more
than a few lines was split along ownership boundaries, without changing
behavior: AES windows (geometry, redraw, chrome, visible regions, tracking
and drags), object drawing, menus (model, layout, tracking), forms, alerts,
the file selector, AES core/files/geometry, VDI fonts/glyphs and
surface/lines/fill, cursor forms, gemd (main loop, sessions, VDI and AES
dispatch), libgem (transport, drawing, windows, menus), the Rasta and Linux
backends (options, keys, PTY, volumes, keymap), resgen, the desktop, calc,
terminal, Maestro, msa, Stout, FAT12 and the VDI unit tests. Helpers that
became shared lost `static`, kept their prefixes and are declared in a
private header beside their module. The 480-line window tracker became a
dispatcher over per-part handlers. Clock, Gemscape and Maestro's identical
bit-block cloning moved into `samples/lib/resources`, the duplicate Rasta
framebuffer-path lookup into `options.c`, and the file layout notes were
updated. Verification: `make standards`, Clang Format 18 on every owned file,
both platform builds without warnings, the analyzer on changed files, and
the complete suite; see [LATEST.md](LATEST.md). Remaining oversize files are
the two public API headers and `tests/uat/demo31/main.c` (see TODO).

## System menu (2026-09-11)

An always-present system menu was added to the shared desktop: a fixed
Triglav title at the far left of the bar with "About GEM" (an alert) and
"Shutdown" (a confirmed alert that stops gemd through an installed hook).
It is drawn and tracked by gemd through the AES from its HID dispatch, and
application menu titles lay out to its right. It appears only when a desktop
owner exists, so standalone single-application demos are unaffected. A
direct-mode regression was fixed in the same change: the system title left
the shared VDI fill color at ink, and an application that cleared its work
area relying on the default paper fill (demo 22/23/26) then painted a black
window in direct mode, where — unlike the proxy path — there is no
per-session VDI state to isolate it; the bar drawing now restores the paper
fill. gemd also now reports, instead of exiting silently, an over-long
`GEMD_SOCKET` path or a failed `lstat`/`chmod`/`fcntl` on its socket.
Verification: all 70 UAT demos (menu-bearing scenes 18/22/23/26/30 and the
desktop re-recorded, their menu-click coordinates shifted by the system
title width), unit, integration and the all-samples session; `make
standards` (222 files); the menu was exercised live across calc, terminal,
Stout and the desktop.

## Subsequent display-polarity correction

The original audit runs above checked raw PBMs and missed the upstream viewer's
opposite RGB mapping. The launchers now pass `--inverse off`; the pinned latest
Rasta source supplies the option natively. The new `test_rasta_polarity`
checks decoded colors as well as flag parsing/reconfiguration; current run
counts are recorded in [LATEST.md](LATEST.md). Earlier 84-test results retain
their original scope.

## Subsequent Musashi download migration

The embedded Musashi Git checkout was replaced by a pinned, SHA-256-verified
archive in ignored build storage. The checked-in patch reproduces the four
locally modified upstream files byte-for-byte. The old checkout was preserved
locally under `build/audit/musashi/previous-checkout/`; it is not a build input.
The source recipe uses Git only to apply the patch, outside the parent Git
repository's worktree/index context. See the
[dependency guide](../guides/SAMPLES.md#cached-musashi-dependency).

Verification includes an unchanged offline rebuild with blocked HTTP proxies
and unchanged archive/source timestamps, standalone SDK builds, Docker builds
and the complete acceptance suite. Latest per-test outcomes remain in
[LATEST.md](LATEST.md); migration logs are in `build/audit/musashi/`.

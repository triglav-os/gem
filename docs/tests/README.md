# Test reports

Run `make tests` at the repository root to build and execute all suites in
the Wischner SDL Docker toolchain, including the headless Rasta viewer.
No AI or manual clicking is required. `tests/report.py` invokes CTest and
writes `LATEST.md` and a UTC timestamped report here. Reports include per-test
outcomes, durations, failure output, evidence links and proposed actions.
A failing test keeps the make target unsuccessful even if reporting succeeds.

Nested test Makefiles and direct Python test entry points also dispatch into
Docker. A checkout-wide lock serializes separate invocations, including runs
with different build directories, because they can share `bin/` and reports.
`test_container_lock` verifies waiting and lock release after command failure.
`test_container_identity` checks passwd/group lookup, a writable owned home,
and login-shell startup without missing-account diagnostics.
`test_paths` verifies XDG overrides, resource lookup and private runtime
permissions. `test_runtime_paths` checks Docker mounts and non-overwriting
settings/profile migration, including shared XDG bases. `test_install_layout`
checks exact contents, strips/dependency closure, removes stale files and
launches relocated installed applications without checkout resource overrides.
`test_sdk_layout` exports a clean SDK and builds independent samples after
relocation; see [Linux paths](../guides/LINUX_PATHS.md).
CTest still runs independent tests in parallel inside each invocation; select
the parallelism with `make tests TEST_JOBS=4`.

Detailed logs (`<build-dir>/tests.log`), JUnit XML
(`<build-dir>/test-results.xml`) and UAT framebuffers (`<build-dir>/uat/`)
are generated evidence. The default build directory is `build/container-sdl/`;
`make tests BUILD_DIR=...` can select another directory inside the checkout.
Later runs overwrite
these files; removing the build directory removes them. Timestamped Markdown
preserves the outcomes recorded at that time, not the linked artifact contents.
Never change a recorded failure into a pass by editing a report.

`make unit`, `make integration` and `make uat` select suites using CTest;
these subset targets do not regenerate the full report. A direct CTest rerun
also leaves `LATEST.md` describing the previous full run. Archive reports may
have fewer tests than the current suite because tests were added later.

`test_sample_session` additionally checks the complete bundled-and-sample
application startup and cleanup in a separate headless session, including the
MSA/Stout guest path.
It also drags Stout three times, moves and closes Terminal, and verifies
window coordinates, framebuffer changes and independent RPC responsiveness.
Terminal checks require median echo latency below 50 ms, every measured echo
below 200 ms, and correct shell output from typing at 40 characters/second.
`uat_calc_direct` and `uat_calc_proxy` check all 24 calculator buttons, digit
entry, clearing and `7 + 2 = 9` using the shipped Calculator binaries.
The session logs, state and interaction captures live in `<build-dir>/sample_session_test/`.
See the [F5 interaction regression](F5_INTERACTION.md) for the reproduced
failure, fix and before/after evidence.
The [calculator and terminal report](SAMPLE_INPUT.md) records the button
failure and measured input latency before and after the changes.

Standalone samples builds and diagnostic `--capture` runs are additional
checks; they are not silently counted as numbered UAT acceptance passes.
See [UAT scenarios](UAT.md), [API scope](../architecture/API_TRANSPORT.md) and
[independent samples](../guides/SAMPLES.md).

`uat_desktop_direct` and `uat_desktop_proxy` verify disk, Workspace and Trash
bitmaps, double-click Workspace, enter a fixture directory, return to its
parent and close the file manager. They also verify the Name, Date, Size and
Type list header, centered rows without separator rules, fixed count/size
status band and metadata fields. Captured screens and results are in
`<build-dir>/uat/desktop_direct/` and `<build-dir>/uat/desktop_proxy/`.
See [desktop regression](DESKTOP.md) for the causes and repair.

Desktop acceptance also checks menu sizing, Locations and File → Open. The
full-session interaction test additionally verifies opening and
closing Workspace while all F5 samples remain alive.

The F5 check also leaves the system menu open for three seconds with a partial
RPC frame pending, then selects Shutdown above a window closer and keeps the
confirmation open seven seconds. The client connection, window geometries and
all sample processes must survive. See [the modal menu regression](DESKTOP.md) for the
reproduced disconnect and timeout-accounting fix.

That prolonged alert check also covers background request traffic across
clock ticks: an earlier pre-I/O timestamp caused an unsigned timeout
underflow and removed Clock's window. The fixed modal service checks expiry
with a fresh post-I/O timestamp.

## Security and build verification

The optional IDE-level check uses the installed VS Code C++ debug adapter on
the host, as F5 does; all GEM programs and GDB run in Docker. It uses a separate
container and private XDG bases, leaving any desktop session untouched. Run:

```sh
python3 -B tests/integration/session/debug_adapter.py \
    --adapter /path/to/ms-vscode.cpptools/debugAdapters/bin/OpenDebugAD7
```

It reads the checked-in launch/tasks configuration, hits and continues a
breakpoint at `main`, keeps Desktop and Terminal ready for ten seconds, then
checks private runtime permissions, persistent XDG logs and that the post-debug
Make task removes the test container. Its evidence lives
in `build/f5-adapter/`. This check is separate from CTest because the IDE adapter
is installed on the host. A direct GDB batch check does not cover VS Code's
terminal allocation: the previous local launch passed `/dev/pts/0` into Docker,
and GDB failed with `During startup program exited with code 1`.

Gemscape's deterministic HTTP acceptance records DOM events,
screenshots and restart evidence in `uat/gemscape_browser/`; each run uses its
own profile and configuration. `test_browser_preferences` covers selection and
bounded paste behavior plus preference validation. See the
[UAT catalog](UAT.md) for the browser scenarios.

`security_disk` extracts generated hostile FAT/MSA fixtures through the real
MSA sample. `security_guest` runs malformed VDI traps through Stout/Musashi and
executes GEMDOS, BIOS and XBIOS calls through their real 68000 trap vectors. It
also verifies TOS caller-owned trap-stack cleanup and persists a nested relative
`Fcreate` from an A: guest into the same generated raw ST image. A second guest
creates the same nested file from C:, verifying that host programs use their
source directory, create missing parents and do not produce `.TOS.files`
sidecars.
`test_security_bounds` checks sector overflow and all 128-bit shift counts from
zero through 128 under sanitizers. Fixtures and outputs remain under `build/`.
The normal sample disk and IMGVIEW files belong in `samples/data/`.
`test_gem_trash` also covers the File Manager's no-follow copy/move backend:
regular files, nested directories, collision preservation, descendant-copy
rejection and cross-filesystem directory moves.
`uat_file_drop_direct` and `uat_file_drop_proxy` exercise the actual pointer
drag between two overlapping File Manager windows, including Shift
multi-selection, immediate plain Copy, immediate Control-Move, its live `M`
badge, suppression of that badge over Trash and recursive folder copying. It
also descends five nested folders and starts a drag from an item selected by
the immediately preceding click. A generated FAT12 image verifies hidden-entry
filtering plus file and recursive-directory extraction into the Linux target.

`make standards` and `make audit` are separate audits, not extra UAT passes.
See [security/build audit](SECURITY_AUDIT.md) for scan coverage, finding
classification, standalone SDK and Docker verification. Dated reports preserve
failed intermediate runs so regressions and their fixes remain visible.

`test_aes_shell` exercises direct shell-start synthesis, environment/file
lookup, on-demand DESKTOP.INF generation, structured `desktop.cfg` updates and
concurrent legacy translations without tearing. It also verifies that a direct
`libgemconf` save is reflected by the next `shel_get`. `test_aes_rpc`
additionally checks the translation through a real gemd, its file-replacement
watch/reload path, and two parallel child launches with distinct ids, commands
and TOS tails, missing-program failure and no parent launch-variable leakage.

`test_rasta_polarity` runs the downloaded viewer's core tests plus the local
inverse-mode regression. It verifies black ink/white paper in decoded RGB,
option parsing and preservation across subscriber reconfiguration. Full-session
checks also require F5 to launch the viewer with `--inverse off --bpp 32`.

`test_rasta_startup` launches a viewer with a different framebuffer path,
80×60 1-bpp pixels and inverse enabled. A real GEM client must reconfigure it
to 640×400 BGRA with inverse disabled, render the reviewed window, and respond
to drag/close input. This catches incompatible viewer option syntax and proves
that the differing default framebuffer names do not require manual setup.
Evidence is under `<build-dir>/rasta_startup/demo17_proxy/`.


`test_gemd_host` also checks scrollbar-free `SIZER` geometry: work/border
round trips, compact frame widths, corner hit testing outside the client,
and unchanged frame geometry when any scroll-control flag is present.

The compact-frame change updates the reviewed references for demos 20, 21,
22, 23, 25 and 26: client origins and interactions are unchanged, while empty
right/bottom scrollbar strips become thin resize borders. The earlier rerun
recorded 96/101, retaining the then-unresolved demo34, Gemread and Gemscape
failures. The geometry check also covers compact grips on tiny windows under
ASan/UBSan. See the generated latest report for full-suite evidence.

The 2026-11-06 package audit additionally ran `gemscape_browser.py` with
`bin/gemix/bin/gemscape`, its installed gemd/Rasta and `share/gem` resources.
All nine browser scenarios passed against the stripped, single-locale CEF
payload. Evidence is under `build/packaged-browser/`; this additional run is
separate from the numbered CTest report. Native Release package and relocated
SDK checks also passed with `/usr` and `lib/x86_64-linux-gnu`; their log is
`build/package-native/build.log`. Native display/input hardware was not used.

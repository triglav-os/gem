# Test reports

Run `make tests` at the repository root to build and execute all suites.
No AI or manual clicking is required. `tests/report.py` invokes CTest and
writes `LATEST.md` and a UTC timestamped report here. Reports include per-test
outcomes, durations, failure output, evidence links and proposed actions.
A failing test keeps the make target unsuccessful even if reporting succeeds.

Detailed logs (`build/tests.log`), JUnit XML (`build/test-results.xml`) and
UAT framebuffers (`build/uat/`) are generated evidence. Later runs overwrite
these files; removing the build directory removes them. Timestamped Markdown
preserves the outcomes recorded at that time, not the linked artifact contents.
Never change a recorded failure into a pass by editing a report.

`make unit`, `make integration` and `make uat` select suites using CTest;
these subset targets do not regenerate the full report. A direct CTest rerun
also leaves `LATEST.md` describing the previous full run. Archive reports may
have fewer tests than the current suite because tests were added later.

`test_sample_session` additionally checks the complete F5 sample startup and
cleanup in a separate headless session, including the MSA/Stout guest path.
It also drags Stout three times, moves and closes Terminal, and verifies
window coordinates, framebuffer changes and independent RPC responsiveness.
Terminal checks require median echo latency below 50 ms, every measured echo
below 200 ms, and correct shell output from typing at 40 characters/second.
`uat_calc_direct` and `uat_calc_proxy` check all 24 calculator buttons, digit
entry, clearing and `7 + 2 = 9` using the shipped sample binaries.
The session logs, state and interaction captures live in `build/sample_session_test/`.
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
parent and close the file manager. Captured screens and results are in
`build/uat/desktop_direct/` and `build/uat/desktop_proxy/`.
See [desktop regression](DESKTOP.md) for the causes and repair.

Desktop acceptance also checks menu sizing, the information alert and File →
Open. The full-session interaction test additionally verifies opening and
closing Workspace while all F5 samples remain alive.

The F5 check also leaves Desk open for three seconds with a partial RPC frame
pending, then selects Desktop info above a window closer and keeps the alert
open seven seconds. The client connection, window geometries and all sample
processes must survive. See [the Desktop info regression](DESKTOP.md) for the
reproduced disconnect and timeout-accounting fix.

That prolonged alert check also covers background request traffic across
clock ticks: an earlier pre-I/O timestamp caused an unsigned timeout
underflow and removed Clock's window. The fixed modal service checks expiry
with a fresh post-I/O timestamp.

## Security and build verification

`security_disk` extracts generated hostile FAT/MSA fixtures through the real
MSA sample. `security_guest` runs malformed VDI traps through Stout/Musashi.
`test_security_bounds` checks sector overflow and all 128-bit shift counts from
zero through 128 under sanitizers. Fixtures and outputs remain under `build/`.
The normal sample disk and IMGVIEW files belong in `samples/data/`.

`make standards` and `make audit` are separate audits, not extra UAT passes.
See [security/build audit](SECURITY_AUDIT.md) for scan coverage, finding
classification, standalone SDK and Docker verification. Dated reports preserve
failed intermediate runs so regressions and their fixes remain visible.

`test_rasta_polarity` runs the downloaded viewer's core tests plus the local
inverse-mode regression. It verifies black ink/white paper in decoded RGB,
option parsing and preservation across subscriber reconfiguration. Full-session
checks also require F5 to launch the viewer with `--inverse`.

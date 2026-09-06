# Hosted development

The default build uses the SDL2 Rasta viewer as its display. Install GCC,
G++, GNU Make, CMake 3.20 or newer, Git, Python 3, pkg-config, SDL2 development
headers and ImageMagick before building. Rasta and Musashi are downloaded
automatically and cached under the chosen build directory.
Run commands from the repository root.

## Build and test

```sh
make
make tests
```

`make` configures GCC Debug with the Rasta backend and builds libraries,
applications and both variants of every UAT demo. `make tests` runs the full
suite and writes [the latest report](../tests/LATEST.md). Use `make unit`,
`make integration` or `make uat` to select a suite. The default viewer is `bin/tools/rasta`; set `RASTA_BIN` only for an explicit
viewer override; `UAT_SDL_DRIVER=x11` or `wayland` makes its windows
visible instead of using the default dummy driver.

See [UAT scenarios](../tests/UAT.md) for demo purposes, direct/proxy execution,
input automation, reference images and evidence locations.

Equivalent explicit build commands:

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_BUILD_TYPE=Debug -DGEM_PLATFORM=rasta
cmake --build build -j"$(nproc)"
```

`GEM_OUTPUT_ROOT` is a CMake cache path (default: `bin`). For an isolated
runtime, use `-DGEM_OUTPUT_ROOT=/absolute/path/to/build/runtime`; this keeps
libraries and generated resources together under the root build directory.

Sample applications live in `samples/src/`, including the desktop in
`samples/src/desktop/`. Built applications are in `bin/samples/`; the display
server remains in `bin/core/`. Automated demo scenarios remain in `tests/uat/`.

See [independent samples](SAMPLES.md) for SDK builds and sample dependencies.

## Run

Start these commands in three separate terminals, in order:

```sh
./bin/tools/rasta --inverse --port 5000
./bin/core/gemd
./bin/samples/desktop
```

Wait for gemd to report that it is listening before starting the desktop.
Keep Rasta and gemd running while the desktop is active. Stop the desktop,
then gemd and Rasta, with Ctrl-C in their respective terminals.

Both client and server use `GEMD_SOCKET` (default `/tmp/gemd.sock`).
The Rasta host and port are selected with `GEM_RASTA_HOST` and
`GEM_RASTA_PORT`; the framebuffer path uses `GEM_RASTA_FRAMEBUFFER`.
See [security and safety](../architecture/SECURITY.md) for private runtime
paths and ownership requirements.

## VS Code: one F5 session

Select **GEM — all samples** and press F5. This is the only launch
configuration. It builds the project in GCC Debug/Rasta mode, starts Rasta,
then launches gemd under GDB. Once gemd is listening, the supervisor starts the
desktop and waits for its first rendered scene before launching the other apps.

The session runs desktop, calculator, clock, Gemscape, Maestro, terminal and
Stout as libgem clients on one display. The MSA command-line sample first
extracts `samples/data/st.msa` into the session's `guest/` directory; Stout runs
its `DEMO.PRG`. MSA completes after extraction; it has no persistent window.
The direct `*_hosted` alternatives and numbered UAT demos are separate test
configurations, not additional clients in this interactive session.

Set `RASTA_BIN` in the VS Code environment to select the viewer. The launcher
otherwise uses the downloaded build at `bin/tools/rasta`. It uses
a private socket, framebuffer and an available UDP port. Session data lives
in `build/f5/`: `session.env`, `state.json`, per-process logs, `session.log`,
the extracted guest and `all_samples.pbm`. Windows may overlap; move or raise
them to inspect each sample. GDB breakpoints apply to gemd.

The Stop button terminates the session supervisor, clients and viewer. Closing
the desktop or Rasta also ends the session. Cleanup checks process identity
and the owned socket inode; it does not kill unrelated GEM or Rasta sessions.

The supervisor is `tools/scripts/sample_session.py`; `sample_debugger.py`
loads its environment when GDB starts, after the prelaunch task has prepared
the session. For a headless
startup, interaction and cleanup check independent of your interactive session:

```sh
SDL_VIDEODRIVER=dummy python3 -B tools/scripts/sample_session.py check \
    --directory "$PWD/build/sample_session_check"
```

`test_sample_session` runs this check as part of `make tests`. It verifies
startup order, extraction and the Stout guest's connection. It then injects
Rasta mouse events to drag Stout three times, move Terminal and close Terminal.
An independent RPC client checks actual window coordinates and server
responsiveness; framebuffer captures verify redraws. It measures terminal
key-to-framebuffer latency and types a shell command at 40 characters per
second, checking its output file. Separate calculator checks verify all
buttons and `7 + 2 = 9` against both backends. These session checks
complement the individual [UAT scenarios](../tests/UAT.md).

## Native deployment

For direct framebuffer and evdev operation, follow the
[Gemix Linux guide](GEMIX_LINUX.md). To return to hosted development after a
native Release build, run `make`, which restores GCC Debug and Rasta.

## Independent dependencies and Docker

`tools/scripts/rasta.cmake` pins upstream Rasta revision
`3dd2426f5c4bddf7d0eb4961d73c2e8c68e3510a` and verifies its archive with
SHA-256 `755505af8181467203d8b8a7a572bdb01663573a0739266a405232d6ab20ba86`.
Downloads, extracted sources and CMake intermediates remain under root `build/`.
The viewer is installed into `bin/tools/rasta`. A fresh build needs GitHub
access; subsequent unchanged builds reuse the verified download. Updating the
revision requires updating the hash and rerunning the UAT suite.

Musashi uses a pinned, verified archive and a checked-in safety patch, managed
by `samples/lib/musashi/CMakeLists.txt`. It is not a Git submodule. See the
[sample dependency guide](SAMPLES.md#cached-musashi-dependency) for cache paths,
offline rebuilds and patch updates.

The normal build uses local GCC/G++; it does not require Docker. To use the
container toolchain instead, run `make container`, or
`tools/scripts/container.sh all`. The pinned Ubuntu 24.04 base and package list
are in `tools/container/Dockerfile`. Only Docker is required when invoking the
script directly. It runs as your UID with dropped capabilities and writes to
`build/container/`, including an isolated `runtime/`. Test reports still go to
`docs/tests/`. Package updates are not frozen, so this is a reproducible build
procedure rather than a claim of byte-identical binaries.

## Code and security audit

`make standards` scans every owned C/header file for the enforceable naming,
file header, indentation and public prototype documentation rules.
`make audit` additionally runs Clang's static analyzer over every distinct C
compilation variant in the configured compilation database, including vendor
sources and generated code. Install Clang 18 or run
`tools/scripts/container.sh audit`. Diagnostics or analysis failures make the
audit command fail; inspect `build/audit/scan/results.json` and the corresponding
logs/plists. A warning is a finding to investigate, not proof of a vulnerability.
See [the audit report](../tests/SECURITY_AUDIT.md) for disposition and limits.

Formatting is defined by `.clang-format` (Clang Format 18). Apply it to owned
C/header files after changing or regenerating code. Preserve upstream Musashi
formatting and Atari data fixtures. The finite checks do not establish complete
GEM conformance or absence of vulnerabilities.

## Rasta display polarity

All GEM launchers pass `--inverse`: set framebuffer bits are black ink and
clear bits are white paper. The pinned upstream viewer defaults to the opposite
RGB mapping and lacks this switch. `tools/scripts/rasta_inverse.cmake` applies
an idempotent, checked build patch to add it after download. The archive hash
still verifies the original source. No local Rasta checkout is used.

Inversion affects the decoded SDL colors only. The GEM framebuffer, VDI color
contracts and PBM references stay unchanged. Runtime geometry reconfiguration
retains the inverse option. `test_rasta_polarity` checks parsing, reconfiguration
and the actual decoder's RGB output; raw PBM comparisons alone cannot detect
an incorrectly configured viewer. An explicit `RASTA_BIN` override must support
`--inverse` too. Restart an existing F5 session to use the corrected viewer.

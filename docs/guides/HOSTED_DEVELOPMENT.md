# Hosted development

The default build uses the SDL2 Rasta viewer as its display. Install GCC,
G++, GNU Make, CMake 3.20 or newer, Git, Python 3, pkg-config, SDL2 development
headers and ImageMagick before building. Rasta, Musashi, MuPDF and CEF are
downloaded automatically and cached under the chosen build directory.
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
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGEM_PLATFORM=rasta
cmake --build build -j"$(nproc)"
```

`GEM_OUTPUT_ROOT` is a CMake cache path (default: `bin`). For an isolated
runtime, use `-DGEM_OUTPUT_ROOT=/absolute/path/to/build/runtime`; this keeps
libraries and generated resources together under the root build directory.

Bundled applications live in `src/apps/`: Desktop, Terminal, Clock, Calculator,
Gemscape, Gemread and Stout build into `bin/apps/`. Independent examples live in `samples/src/`
and build into `bin/samples/`; the display server remains in `bin/core/`.
Automated demo scenarios remain in `tests/uat/`.

See [independent samples](SAMPLES.md) for SDK builds and sample dependencies.

## Run

Start these commands in three separate terminals, in order:

```sh
./bin/tools/rasta --inverse off --cursor off --bpp 32 --port 5000
./bin/core/gemd
./bin/apps/desktop
```

Wait for gemd to report that it is listening before starting the desktop.
Keep Rasta and gemd running while the desktop is active. Stop the desktop,
then gemd and Rasta, with Ctrl-C in their respective terminals.

Both client and server use `GEMD_SOCKET` (default `/tmp/gemd.sock`).
The Rasta host and port are selected with `GEM_RASTA_HOST` and
`GEM_RASTA_PORT`; the framebuffer path uses `GEM_RASTA_FRAMEBUFFER`.
See [security and safety](../architecture/SECURITY.md) for private runtime
paths and ownership requirements.

Rasta receives display-ready opaque BGRA32 pixels. GEM and VDI still render
into their packed monochrome shadow surface; the presenter may composite
application-owned BGRA overlays over that shadow. The GEM software cursor is
composited last so it remains visible over opaque overlays. Gemscape is the
small browser application and can be launched with `bin/apps/gemscape`;
Gemread uses the same overlay path for color document pages and thumbnails.

## VS Code: one F5 session

Select **GEM — Desktop and Terminal** and press F5. This is the only launch
configuration. It builds the project in GCC Debug/Rasta mode, starts Rasta,
then launches gemd under GDB. Once gemd is listening, the supervisor starts the
desktop and waits for its first rendered scene before launching the other apps.
The F5 Rasta viewer and GEM desktop use a 1920×1080 BGRA32 framebuffer at
scale 1. Rasta hides the Linux/SDL pointer over its surface so only GEM's
software cursor is visible.
The supervisor waits for a complete post-Desktop RPC round trip before it
starts Terminal; the larger initial redraw therefore cannot consume Terminal's
connection timeout and tear down the session.

The interactive session initially runs only Desktop and Terminal as libgem
clients on one display. The full integration check explicitly launches every
bundled application plus Maestro; those applications are not part of normal
F5 startup. The direct `*_hosted` alternatives and numbered UAT demos are
separate test configurations.

Set `RASTA_BIN` in the VS Code environment to select the viewer. The launcher
otherwise uses the downloaded build at `bin/tools/rasta`. It uses
a private socket, framebuffer and an available UDP port. Session data lives
in `build/f5/`: `session.env`, `state.json`, per-process logs, `session.log`
and `applications.pbm`. Windows may overlap; move or raise them to inspect each
application. GDB breakpoints apply to gemd.

The Stop button terminates the session supervisor, clients and viewer. Closing
the desktop or Rasta also ends the session. Cleanup checks process identity
and the owned socket inode; it does not kill unrelated GEM or Rasta sessions.

The supervisor is `tools/scripts/sample_session.py`; `sample_debugger.py`
loads its environment when GDB starts, after the prelaunch task has prepared
the session. For a headless
startup, interaction and cleanup check independent of your interactive session:

```sh
SDL_VIDEODRIVER=dummy python3 -B tools/scripts/sample_session.py check \
    --apps desktop,terminal \
    --directory "$PWD/build/sample_session_check"
```

That command checks only the initial Desktop and Terminal session. The
`test_sample_session` CTest entry deliberately omits `--apps`, launches all
bundled and sample applications as part of the full suite, and verifies
startup order. It then injects Rasta mouse events to drag Maestro three times,
move Gemread, move Gemscape with its CEF overlay, move Terminal and close
Terminal. An independent RPC client checks actual window coordinates and
server responsiveness; framebuffer captures verify redraws and overlay
occlusion. It measures terminal key-to-framebuffer latency and types a shell
command at 40 characters per second, checking its output file. Separate
calculator checks verify all buttons and `7 + 2 = 9` against both backends.
These session checks complement the individual
[UAT scenarios](../tests/UAT.md).

## Native deployment

For direct framebuffer and evdev operation, follow the
[Gemix Linux guide](GEMIX_LINUX.md). To return to hosted development after a
native Release build, run `make`, which restores GCC Debug and Rasta.

## Independent dependencies and Docker

`tools/scripts/rasta.cmake` pins upstream Rasta revision
`5f1e1511e918e8b4dfa6f8b7a51272f66133ea0e` and verifies its archive with
SHA-256 `c017b7e0884d8e94bf16a6ce549c62f2fbf2013aeaf3bf14ef1fb0cc4bc04e78`.
Downloads, extracted sources and CMake intermediates remain under root `build/`.
The viewer is installed into `bin/tools/rasta`. A fresh build needs GitHub
access; subsequent unchanged builds reuse the verified download. Updating the
revision requires updating the hash and rerunning the UAT suite.

Musashi uses a pinned, verified archive and a checked-in safety patch, managed
by `lib/musashi/CMakeLists.txt`. It is not a Git submodule. See the
[sample dependency guide](SAMPLES.md#cached-musashi-dependency) for cache paths,
offline rebuilds and patch updates.

Gemread uses MuPDF 1.26.10. Its verified source archive, extracted source and
build products stay under `build/deps/mupdf`; `make mupdf` explicitly prepares
that cache, while `make` builds it automatically when Gemread is selected.
The MuPDF build explicitly sets `HAVE_X11=no HAVE_GLUT=no`; unchanged builds
reuse the archive and compiled dependency from the ignored build tree.

Gemscape uses the minimal Linux distribution of CEF
154.0.23+g062ebe4+chromium-154.0.8037.17. Its verified archive, extracted
headers, runtime and resources stay under `build/deps/cef`; `make cef`
explicitly prepares that cache, while `make` downloads it automatically when
Gemscape is selected. CEF is never stored or generated under `include/` or
`lib/` in the source tree.

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

GEM publishes display-ready BGRA pixels, so launchers pass `--inverse off`.
The packed shadow's set bits are black ink and clear bits are white paper; the
pinned upstream viewer leaves those BGRA channels unchanged. No local Rasta
checkout is used.

Rasta uses its 32-bit BGRA framebuffer mode. The backend keeps a packed
monochrome shadow surface for VDI, then publishes opaque black/white BGRA
pixels (set shadow bit = black ink, clear bit = white paper). PBM references
and VDI color contracts remain unchanged; test helpers decode BGRA captures
back to PBM for comparison. Runtime geometry reconfiguration retains both
`--inverse off` and `--bpp 32`.
`test_rasta_polarity` checks parsing, reconfiguration and the actual decoder's
RGB output. An explicit `RASTA_BIN` override must support these options too.
Restart an existing F5 session to use the corrected viewer.

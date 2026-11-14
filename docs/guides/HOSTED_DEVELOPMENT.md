# Hosted development

The default build uses the SDL2 Rasta viewer as its display. Install Docker
and GNU Make on the Linux host and ensure your account can run Docker.
Every root Make target runs in the `wischner/gcc-x86_64-linux-sdl:1.3.0`
toolchain with GEM's additional CEF runtime dependencies. Host compilers and
SDL2 development headers are unnecessary. Rasta, Musashi, MuPDF and CEF are
downloaded automatically and cached under `build/container-sdl/` by default.
Run commands from the repository root.

## Build and test

```sh
make
make tests
```

`make` configures GCC RelWithDebInfo with the Rasta backend inside Docker and
builds Rasta, libraries, applications and both variants of every UAT demo.
`make tests` runs the full suite inside Docker using the built Rasta viewer
and writes [the latest report](../tests/LATEST.md). Use `make unit`,
`make integration` or `make uat` to select a suite. Tests use SDL's dummy video
driver and need no host display connection. Nested test Makefiles and directly
invoked Python test tools also enter Docker. `TEST_JOBS` controls CTest
parallelism; `JOBS` controls compilation. All build/test commands for one
checkout share `build/.container.lock`: overlapping invocations wait before
touching binaries, sockets, framebuffers or reports. Separate checkouts can
run independently. Do not delete the lock file while a command is running.

See [UAT scenarios](../tests/UAT.md) for demo purposes, direct/proxy execution,
input automation, reference images and evidence locations.

Build options are forwarded into Docker:

```sh
make JOBS=8 BUILD_TYPE=Debug
make tests BUILD_DIR="$PWD/build/debug" BUILD_TYPE=Debug
```

`BUILD_DIR` defaults to `build/container-sdl/`, separate from older host builds.
`GEM_OUTPUT_ROOT` defaults to `bin/`. For an isolated runtime, use
`make GEM_OUTPUT_ROOT="$PWD/build/runtime"`; this keeps libraries and generated
resources together under the root build directory. `CMAKE_FLAGS` passes extra
CMake configuration options through the wrapper. Keep overridden build and
output paths inside the checkout, which is mounted at the same path in Docker.

Bundled applications live in `src/apps/`: Desktop, Terminal, Ted, Clock, Calculator,
Gemscape, Gemread and Stout build into `bin/apps/`. Independent examples live in `samples/src/`
and build into `bin/samples/`; the display server remains in `bin/core/`.
Automated demo scenarios remain in `tests/uat/`.

See [independent samples](SAMPLES.md) for SDK builds and sample dependencies.

## Run

Run the complete interactive session in Docker:

```sh
make run
make stop   # from another terminal, to stop this checkout's session
```

The launcher starts Rasta, gemd and Desktop in the same container.
It forwards the current X11 display/socket and Xauthority file, or the current
Wayland socket. Run it from your graphical login; no host SDL2, CEF or GDB
installation is needed. F5 also needs the VS Code C/C++ extension.
Close Desktop or Rasta to end the apps;
`make stop` removes the persistent container too. Stop an existing session
before rebuilding its binaries or changing display/environment settings.

`tools/scripts/start-rasta.sh` and `start-gemd.sh` also enter this container.
For another command in the same environment, use
`bash tools/scripts/runtime.sh exec <command>`. For a build or test command,
use `tools/scripts/container.sh exec <command>`; that path holds the checkout
lock and uses a disposable headless container.

Both client and server use `GEMD_SOCKET` (default `$XDG_RUNTIME_DIR/gem/gemd.sock`).
The Rasta host and port are selected with `GEM_RASTA_HOST` and
`GEM_RASTA_PORT`; the framebuffer path uses `GEM_RASTA_FRAMEBUFFER`.
See [security and safety](../architecture/SECURITY.md) for private runtime
paths and ownership requirements.

Rasta receives display-ready opaque BGRA32 pixels. GEM and VDI still render
into their packed monochrome shadow surface; the presenter may composite
application-owned BGRA overlays over that shadow. The GEM software cursor is
composited last so it remains visible over opaque overlays. Gemscape is the
small browser application available from the desktop;
Gemread uses the same overlay path for color document pages and thumbnails.
See the [Gemscape guide](GEMSCAPE.md) for browser settings, persistent profiles
and current limits. Interactive containers share the host's XDG GEM config,
data, cache and state directories; see [Linux paths](LINUX_PATHS.md).
Gemscape uses Chromium's headless Ozone platform and software rendering for
its off-screen CEF view; it does not require X, Wayland or GPU access. See the
[Chromium Ozone platform documentation](https://chromium.googlesource.com/chromium/src/+/main/docs/ozone_overview.md).

## VS Code: one F5 session

Select **GEM — Desktop** and press F5. This is the only launch
configuration. Its prelaunch task runs `make run DEBUG=1`, which builds with
the same Wischner Docker defaults as the command line in GCC
RelWithDebInfo/Rasta mode and prepares Rasta for GDB to launch gemd.
The debug option lets the prelaunch task finish before the debugger starts;
plain `make run` launches gemd itself and stays attached to the session.
The post-debug task runs `make stop` to remove the session container.
Once gemd is listening, the supervisor starts the
desktop and waits for its first rendered scene before launching the other apps.
Docker must be available to your VS Code user. F5 build intermediates live in
`build/container-sdl/`; executables and resources go into `bin/`. The source
is mounted at its host path so container GDB resolves source locations directly.
The viewer, supervisor, server, clients and GDB all run in one checkout-specific
Docker container. VS Code's `pipeTransport` connects through `docker exec`; it
loads the session environment inside that same PID and network namespace.
This prevents the local debugger launcher from passing a host `/dev/pts/*`
terminal to GDB inside Docker. Debuggee output appears in the Debug Console.
An additional cached image layer provides passwd/group entries matching the
invoking UID/GID and a private writable `/tmp/gem-home`. Terminal can therefore
resolve user/group names without mounting host account databases or your home.
The home belongs to the container and is removed by `make stop`. After launcher
or image changes, run `make stop` followed by `make run` to replace an older
interactive container.
The F5 Rasta viewer and GEM desktop use a 1920×1080 BGRA32 framebuffer at
scale 1. Rasta hides the Linux/SDL pointer over its surface so only GEM's
software cursor is visible.
The supervisor waits for a complete post-Desktop RPC round trip before it
reports readiness or launches additional applications in the full test session.

The interactive session initially runs only Desktop as a libgem client on one
display. Terminal starts from its desktop icon when `[desktop] show_launchers
= true` is set in `desktop.cfg`. The full integration check
explicitly launches every bundled application plus Maestro; those applications are not part of normal
F5 startup. The direct `*_hosted` alternatives and numbered UAT demos are
separate test configurations.

Set `RASTA_BIN` in the VS Code environment to select the viewer. The launcher
otherwise uses the downloaded build at `bin/tools/rasta`. It uses
a private socket, framebuffer and an available UDP port. Session data lives
in the container's `/run/user/<uid>/gem/session-<checkout-id>/`: `session.env`,
`state.json`, socket, framebuffer and captures. Logs persist under the host's
`$XDG_STATE_HOME/gem/sessions/<checkout-id>/`. Windows may overlap; move or raise them to inspect each
application. GDB breakpoints apply to gemd.

The Stop button terminates the session supervisor, clients and viewer. Closing
the desktop or Rasta also ends the session. The launch configuration tells GDB
to pass SIGTERM to gemd without stopping, so gemd shuts down and the debug
session ends instead of pausing on the signal. Cleanup checks process identity
and the owned socket inode; it does not kill unrelated GEM or Rasta sessions.

The supervisor is `tools/scripts/sample_session.py`; the pipe transport uses
`runtime.sh exec` to enter Docker and `sample_debugger.py`
loads its environment when GDB starts, after the prelaunch task has prepared
the session. For a headless
startup, interaction and cleanup check independent of your interactive session:

```sh
SDL_VIDEODRIVER=dummy python3 -B tools/scripts/sample_session.py check \
    --apps desktop \
    --directory "$PWD/build/sample_session_check"
```

That command checks only the initial Desktop session. The
`test_sample_session` CTest entry deliberately omits `--apps`, launches all
bundled and sample applications as part of the full suite, and verifies
startup order. It then injects Rasta mouse events to drag Stout three times,
move Ted, move Gemread, move Gemscape with its CEF overlay, move Terminal and close
Terminal. An independent RPC client checks actual window coordinates and
server responsiveness; framebuffer captures verify redraws and overlay
occlusion. It measures terminal key-to-framebuffer latency and types a shell
command at 40 characters per second, checking its output file. Separate
calculator checks verify all buttons and `7 + 2 = 9` against both backends.
These session checks complement the individual
[UAT scenarios](../tests/UAT.md).

## Packages

`make package` produces a stripped runtime tree with only required libraries
and resources. `make sdk` produces a separate development export. Both replace
stale generated contents. See [packaging](PACKAGING.md) for dependencies, CEF
locale selection and relocation/contents checks.

## Native deployment

For direct framebuffer and evdev operation, follow the
[Gemix Linux guide](GEMIX_LINUX.md). To return to hosted development after a
native Release build, run `make`, which builds GCC RelWithDebInfo and Rasta in
the separate `build/container-sdl/` directory.

## Independent dependencies and Docker

`tools/scripts/rasta.cmake` clones [upstream Rasta](https://github.com/tstih/rasta)
and fetches the latest `origin/main` before every viewer build, including
`make`, `make rasta`, `make tests`, `make run` and F5. The viewer and its core
test executable are compiled locally with GCC/G++ inside the Docker toolchain;
no prebuilt release is downloaded and no GEM patch is applied. Upstream already
includes the framebuffer-preservation fix and its unit tests.

The Git checkout is `<build-dir>/deps/rasta-source`, compiled objects are in
`<build-dir>/deps/rasta-build`, and the installed viewer is `bin/tools/rasta`.
These caches stay under root `build/`; no sibling checkout is used. Unchanged
sources reuse compiled objects, and a missing installed viewer is restored.
The build prints the exact upstream commit after installation; inspect it with
`git -C build/container-sdl/deps/rasta-source rev-parse HEAD`.

Every viewer build requires GitHub access over verified HTTPS. A failed fetch
fails the build instead of silently using stale source. Rasta intentionally
tracks a moving branch rather than a pinned archive, so rebuilding the same
GEM revision can use newer Rasta code. Run `make tests` to verify compatibility
after upstream updates. An explicit `RASTA_BIN` launcher override still selects
that binary instead of the automatically built viewer.

Musashi uses a pinned, verified archive and a checked-in safety patch, managed
by `lib/musashi/CMakeLists.txt`. It is not a Git submodule. See the
[sample dependency guide](SAMPLES.md#cached-musashi-dependency) for cache paths,
offline rebuilds and patch updates.

Gemread uses MuPDF 1.26.10. Its verified source archive, extracted source and
build products stay under `<build-dir>/deps/mupdf`; `make mupdf` explicitly prepares
that cache, while `make` builds it automatically when Gemread is selected.
The MuPDF build explicitly sets `HAVE_X11=no HAVE_GLUT=no`; unchanged builds
reuse the archive and compiled dependency from the ignored build tree.

Gemscape uses the minimal Linux distribution of CEF
154.0.23+g062ebe4+chromium-154.0.8037.17. Its verified archive, extracted
headers, runtime and resources stay under `<build-dir>/deps/cef`; `make cef`
explicitly prepares that cache, while `make` downloads it automatically when
Gemscape is selected. CEF is never stored or generated under `include/` or
`lib/` in the source tree.

Build and test Make targets use `tools/scripts/container.sh`, which invokes the
internal recipes in `tools/scripts/build.mk` inside Docker without recursively
starting another container. This includes `configure`, `rasta`, `cef`, `mupdf`,
`sdk`, `package`, `install`, `clean`, `standards`, the suite targets and `audit`. `make container` is
an alias for `make tests`. Only Docker is required when invoking the helper
directly. It runs as your UID with dropped capabilities.
Interactive containers use the same image and UID, with `SYS_PTRACE` added
for GDB. They mount this checkout, GEM's XDG directories and the selected display connection;
they do not use host networking or a privileged container.

The helper defaults are also available as environment overrides:

```sh
export GEM_CONTAINER_IMAGE=wischner/gcc-x86_64-linux-sdl:1.3.0
export GEM_CONTAINER_BUILD_DIR="$PWD/build/container-sdl"
export GEM_CONTAINER_OUTPUT_ROOT="$PWD/bin"
tools/scripts/container.sh all
tools/scripts/container.sh tests
```

These are the same defaults used by Make and F5. Make's `BUILD_DIR` and
`GEM_OUTPUT_ROOT` options take precedence over the corresponding environment
defaults. The image is reused locally; Docker pulls it if absent.
`tools/container/Dockerfile.sdl` adds the NSS, ATK
and Xdamage libraries needed by the CEF browser to this base image; the
resulting `gem-build:sdl` layer is cached too.
The optional `GEM_CONTAINER_IMAGE=gem-build:local` override builds the older
Ubuntu recipe in `tools/container/Dockerfile`. Use a separate `BUILD_DIR` when
changing toolchains. The bind mount preserves absolute host paths for debugging.
Test reports still go to
`docs/tests/`. Package updates are not frozen, so this is a reproducible build
procedure rather than a claim of byte-identical binaries.

## Code and security audit

`make standards` scans every owned C/header file for the enforceable naming,
file header, indentation and public prototype documentation rules.
`make audit` additionally runs Clang's static analyzer over every distinct C
compilation variant in the configured compilation database, including vendor
sources and generated code. Both audit targets use the Docker toolchain.
Diagnostics or analysis failures make the
audit command fail; inspect `<build-dir>/audit/scan/results.json` and the corresponding
logs/plists. A warning is a finding to investigate, not proof of a vulnerability.
See [the audit report](../tests/SECURITY_AUDIT.md) for disposition and limits.

Formatting is defined by `.clang-format` (Clang Format 18). Apply it to owned
C/header files after changing or regenerating code. Preserve upstream Musashi
formatting and Atari data fixtures. The finite checks do not establish complete
GEM conformance or absence of vulnerabilities.

## Rasta display polarity

GEM publishes display-ready BGRA pixels, so launchers pass `--inverse off`.
The packed shadow's set bits are black ink and clear bits are white paper; the
upstream viewer leaves those BGRA channels unchanged. No sibling Rasta
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

### Older Rasta binaries and manual startup

The upstream viewer's `--help` must show `--inverse <on|off>`. A binary that
accepts bare `--inverse` is incompatible with this checkout. Run `make rasta`
to restore `bin/tools/rasta`, remove an outdated `RASTA_BIN` override, and
restart the running viewer. Use `--inverse off --bpp 32` with current GEM;
the Desktop executable is `bin/apps/desktop`.

The viewer initially defaults to `/tmp/rasta.framebuffer`, while GEM defaults
to `$XDG_RUNTIME_DIR/gem/rasta.fb` (with a private per-user fallback). GEM's subscriber datagram sends its framebuffer path,
dimensions, depth and inverse mode, and Rasta remaps to those settings. Its
log should report `reconfigured framebuffer=...` with `bpp=32 inverse=off`.
Matching the initial paths manually is unnecessary. `test_rasta_startup`
starts Rasta with a different path, 80×60 monochrome pixels and inverse mode
enabled, then verifies GEM negotiates 640×400 BGRA with inverse disabled and
completes a window drag and close.
It also delays the actual startup subscription until GEM has drawn, both with
matching viewer settings and with a different initial path/geometry. It checks
all alpha bytes and the rendered scene after that subscription and a repeated
request, then exercises window drag/close. This catches a viewer erasing a
framebuffer whose inode and final size have not changed: GEM's file-identity
checks cannot detect that destructive open reliably. Existing bytes must be
preserved by the viewer; actual size changes still require writer coordination.

### Physical keyboard consumers

Build and install headers, libgem and gemd together when using the optional
`gem/input.h` extension. An application must explicitly request `GEM_MU_KEY`;
existing applications need no changes. See [the transport contract](../architecture/API_TRANSPORT.md#optional-hosted-physical-keyboard-events).
The CPC consumer can layer these artifacts over its pinned SDK image without
replacing the published image, keyed by the local source contents.

## Compact resizable frames

A window with `SIZER` and no scroll arrows or sliders has a one-pixel
bottom/right border. `WF_WORKXYWH` and `wind_calc` exclude that border
exactly, so a status strip can meet it without extra bottom padding. AES
draws no inner frame or L-shaped grip for this mode. The native sizing
hit target remains the corner portion of the outer border; clicks inside
the work rectangle remain client input. Scrollbar frames are unchanged.

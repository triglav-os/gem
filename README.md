# GEM for Linux

GEM for Linux brings the classic GEM graphical environment to modern Linux. It
provides the GEM Desktop, windows, menus and dialogs together with a file
manager, terminal, document tools and the Stout Atari executable runner.

Project-owned code is licensed under [GPL-2.0-or-later](LICENSE).
Gemread retains [AGPL-3.0-or-later](src/apps/gemread/LICENSE); third-party
dependencies, original manuals and historical sample assets retain their
respective licenses and notices.

GEM can run in a Rasta viewer on a hosted development machine, or directly on
a Linux framebuffer with keyboard and mouse input from the Linux text console.

![GEM Desktop running its File Manager and Terminal](docs/images/screenshots/gem_desktop_2026_09_12.png)

See [What's new](CHANGELOG.md) for recent changes. Bundled applications are
under `src/apps/`; Gemread is a read-only document viewer licensed under
AGPL-3.0-or-later. It uses MuPDF for PDF, XPS/OpenXPS, EPUB, MOBI, FB2, CBZ and
CBT documents.
Per-component settings and persistent Desktop locations use ordinary text
files described in the [configuration guide](docs/guides/CONFIGURATION.md).
The [Gemscape guide](docs/guides/GEMSCAPE.md) covers browser controls and profiles.

## Build

Requires Linux, Docker and GNU Make. All build and test targets use the
Wischner SDL Docker toolchain, including the Rasta viewer. The first build
needs internet access; later builds reuse the image and verified dependencies
cached under `build/container-sdl/`.
From the repository root:

```sh
make
make tests    # optional; results in docs/tests/LATEST.md
```

Executables and resources are written to `bin/`. Build, test and interactive
launch helpers all use Docker. `make container` remains
an alias for `make tests`. See the [build guide](docs/guides/HOSTED_DEVELOPMENT.md)
for build options and display setup.

## Run with Rasta

Start Rasta, gemd and Desktop in Docker:

```sh
make run
```

Alternatively, press F5 with **GEM — Desktop** in VS Code to
start Desktop under the Docker debugger. Open Terminal, Files or Gemscape
from their desktop icons. Use `make stop`
to remove this checkout's interactive container. See the
[hosted development guide](docs/guides/HOSTED_DEVELOPMENT.md) for setup and the
[samples guide](docs/guides/SAMPLES.md) for SDK builds.
[Ted](docs/guides/TED.md) edits plain-text files; Gemread opens PDF documents.

## Run on a Linux framebuffer

For a text-console system with Linux framebuffer and evdev support, build the
native package:

```sh
tools/scripts/container.sh exec cmake -S . -B build/linux \
    -DCMAKE_BUILD_TYPE=Release -DGEM_PLATFORM=linux
tools/scripts/container.sh exec cmake --build build/linux --target gemix_package --parallel 4
```

Run it from a Linux virtual console. The account needs access to `/dev/fb0` and
the keyboard and pointer devices under `/dev/input`:

```sh
export GEM_RESOURCE_DIR="$PWD/bin/gemix/share/gem"
./bin/gemix/bin/gemd &
while [ ! -S "$XDG_RUNTIME_DIR/gem/gemd.sock" ]; do sleep 0.02; done
./bin/gemix/bin/desktop
```

Use `make package` to stage a standard Linux tree, or
`make install PREFIX="$HOME/.local"` for a user installation.
Runtime packages exclude development files; use `make sdk` for those.
See [package contents](docs/guides/PACKAGING.md), [installation and XDG paths](docs/guides/LINUX_PATHS.md) and the
[native Linux guide](docs/guides/GEMIX_LINUX.md) for device selection,
permissions and deployment configuration.

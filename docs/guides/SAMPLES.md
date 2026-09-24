# Independent samples

`samples/` is a separate CMake project containing the independent examples. It
uses public GEM headers and compiled GEM libraries; it does not compile core
sources or include private core headers. The root build also includes this
project so `make` still builds everything. Desktop, Terminal, Clock,
Calculator, Gemscape, Gemread and Stout are bundled applications under
`src/apps/`, not samples.

## Layout and ownership

The reusable FAT12, MSA, PRG, Musashi and Stout implementations now live under
`lib/`. `libstout` is the common read-only ST/MSA image and guest-trap layer;
the bundled `stout` frontend is built from `src/apps/stout/`.

- `samples/src/`: Maestro and the MSA disk utility sources, with nested CMake
  files.
- `samples/include/`: headers private to independent sample applications.
  Headers used by one application's own modules stay beside its sources.
- `samples/data/`: Atari disk images, historical IMGVIEW files, icon sources
  and sample artwork. Generated resources go to each sample runtime `data/`.
- `samples/lib/`: resource lookup and resource bit-block copying. The
  integrated Clock currently shares the resource helper with Gemscape and
  Maestro.
- `samples/CMakeLists.txt` and `samples/Makefile`: standalone build entry points.

Bundled Stout uses PRG and Musashi. The MSA utility uses MSA and FAT12. Root
`lib/` contains these reusable libraries as well as the GEM platform backends;
root `include/` retains GEM API, host abstraction and transport headers.
UAT-specific headers are in `tests/include/`. Demo19 reuses the bundled
Terminal source from `src/apps/terminal/`; the samples still have no dependency
on test sources.

## Build against a GEM SDK

First export the SDK from a configured GEM checkout:

```sh
make sdk
```

The result is `bin/sdk/`, containing public headers, GEM libraries (including
the static `libgemconf` configuration and `libgemctl` control libraries),
runtime resources and
`bin/resgen`. Sample resources are excluded from the SDK.
`gem_sdk_config.cmake` records the display backend so direct
AES/VDI applications link the matching host library. This target builds the core and resource generators without
needing the sample binaries. Both Debug and Release samples should use matching GEM
library builds. The supplied Makefile defaults to GCC Debug with sanitizers.

```sh
make -C samples GEM_SDK_ROOT="$PWD/bin/sdk"
```

This uses root `build/samples/` for artifacts; executables are in
`build/samples/runtime/`, with sample libraries in its `lib/` subdirectory.
For a copied samples tree, supply absolute SDK and build paths:

```sh
cmake -S /path/to/samples -B /path/to/build/samples \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
    -DGEM_SDK_ROOT=/path/to/gem-sdk
cmake --build /path/to/build/samples --parallel 4
```

Copy the complete samples tree when relocating it. No root GEM source, Stout
source or tests directory is needed for this build. `GEM_SDK_INCLUDE_DIR`
and the `GEM_<name>_LIBRARY` cache entries also permit explicit SDK locations.

The integrated root build writes these examples to `bin/samples/` and their
private libraries to `bin/samples/lib/`. Bundled applications are in
`bin/apps/`, and core libraries remain in `bin/lib/`.
Gemscape and Gemread are bundled applications; Gemread accepts supported
PDF, XPS/OpenXPS, EPUB, MOBI, FB2, CBZ and CBT documents from its File/Open
menu and uses cached MuPDF sources from `build/deps`. Gemread and its own UAT
driver are AGPL-3.0-or-later; they are not covered by the core's MIT license.
Build paths supply runtime library lookup; when relocating binaries, retain
sample libraries and configure lookup for the installed GEM libraries.

## Run and verify

Start a matching gemd and Rasta session as described in the
[hosted guide](HOSTED_DEVELOPMENT.md). Set `GEM_RESOURCE_DIR` to the SDK's
`share/gem/` directory and use the same `GEMD_SOCKET` for clients and server.
The default GUI binaries link libgem and can share one server. Their
`*_hosted` alternatives link AES/VDI directly and need their own display
sessions. F5 uses the shared variants; see the [session guide](HOSTED_DEVELOPMENT.md).

`make tests` in the root checkout runs the full GEM and UAT suites. The bundled
Terminal is exercised by demo19. It launches its shell through the platform OS wrapper,
advertises `TERM=vt100`, and implements the DEC VT100 control set needed by
interactive and full-screen terminal programs: cursor movement, erase and edit
operations, scrolling regions, saved cursor state, alternate screen buffers,
tabs, DEC line drawing, device/status reports and monochrome bold, underline and
reverse-video attributes. Keyboard translation includes application cursor
keys and the common editing and function keys. The PTY wrapper removes inherited
host-terminal capability variables so programs cannot mistake GEM Terminal for
VTE, Kitty or another modern color terminal. Unsupported control strings are
consumed through their terminator, and UTF-8 box-drawing characters have
readable monochrome fallbacks. The MSA command-line tool exposes usage with
`--help`. Bundled [Stout](../notes/STOUT.md) expects an Atari executable path.
Local Musashi safety and warning fixes are recorded in
[the patch notes](../notes/MUSASHI_PATCHES.md); upstream licenses and
conventions remain intact.

The scripts exporting the SDK and packaging Gemix live in `tools/scripts/`.
The downloaded Musashi `readme.txt` and `history.txt` describe the upstream core;
use this guide and `lib/musashi/CMakeLists.txt` for the GEM build.

The desktop provides configured location and Trash icons. `Add Location...`
in the three-line system menu adds persistent folder shortcuts. Double-click a
configured location to open the built-in file manager;
double-click a folder to enter it and `..` to go up. The Desk menu lists open
browser windows.
Automated direct/proxy coverage is described in [desktop UAT](../tests/DESKTOP.md).

Desktop menus expose implemented actions only: Desk has Desktop info and
open file-manager windows, File has Open, and Arrange switches between list
and icon views and sorts by name, date, size or type. List view shows all four
fields in the system font under matching clickable system-font headers. Its
metric-derived band matches Gemscape's address row and the scrollbar up-arrow
square. Centered rows use compact `/name` directory labels, and a fixed
system-font status band reports file/folder counts and
total size. Icon view uses a wider horizontal pitch so neighboring icons and
labels remain distinct. List view opens without a selected entry. Unused
browser slots take no space.
A single click selects a configured location icon; double-click its image or label,
or choose File → Open, to browse it. Other sample windows may cover the icon
and must be moved to expose the intended click target.

Sample resource lookup tries `GEM_SAMPLE_DATA`, then `data/` beside the running
executable, then the configured sample build data path. Core fonts, cursors and
alert icons use `GEM_RESOURCE_DIR`; sample artwork does not belong there.
Copy `samples/data/` with the standalone source tree. ImageMagick and the SDK
resource generator build its RSC files without a core source checkout.

The MSA extractor rejects unsafe FAT names, cyclic directories, impossible
geometry, oversized files and symlink/hardlink destinations. Extraction is
relative to an opened destination directory using `openat` and `O_NOFOLLOW`.
Atari executable inputs remain guest code, not a security sandbox; see the
[security audit](../tests/SECURITY_AUDIT.md).

## Cached Musashi dependency

`lib/musashi/CMakeLists.txt` downloads revision
`313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd` from `retro-vault/musashi` and checks
SHA-256 `ffddb3f96eba9d4c0f09327a48b5f35398c56fcb8bc79fff5ac352a401dfafcc`.
CMake's FetchContent stamps prevent repeat downloads and patch application.
The archive is cached in `<build-dir>/deps/downloads/`; patched source is in
`<build-dir>/deps/musashi-<revision>-<patch-hash>-v2/`. All supported build
directories are beneath the ignored root `build/` tree.

The checked-in `patches/hosted.patch` preserves the documented safety fixes.
Its hash is part of the source cache key, so changing the patch extracts a fresh
copy from the cached archive before applying it. Updating the revision also
requires updating its archive hash. An unchanged populated build works offline;
deleting the selected build tree removes its dependency cache and requires a
new download. Different build directories have separate caches.

There is no Git submodule or nested repository under `lib/musashi/`. Normal
`git add .` stages only the recipe and patch, and clones obtain source on their
first root build. Do not copy a Git checkout into the source tree.

# Native Gemix Linux Backend

## Build

The native package is compiled in the same Wischner Docker toolchain as the
hosted build; it supplies Linux framebuffer and evdev headers and build tools.

Configure a release build so the deployment does not depend on the
AddressSanitizer and UndefinedBehaviorSanitizer runtimes:

```sh
tools/scripts/container.sh exec cmake -S . -B build/linux \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEM_PLATFORM=linux
tools/scripts/container.sh exec cmake --build build/linux --target gemix_package --parallel 4
```

`bin/gemix` stages the same GNUInstallDirs rules as `make install`: programs
in `bin/`, required shared libraries in `lib/`, resources in `share/gem/`,
and Gemscape's private CEF runtime in `lib/gem/cef/`. Gemread and Gemscape are
included. Native packages omit Rasta and librasta. Debug symbols, headers,
archives and build tools are excluded; `make sdk` exports development files.
Documentation and component licenses live under `share/`. See
[the packaging contract](PACKAGING.md).

Install this backend with the native option explicitly retained:

```sh
make install BUILD_DIR="$PWD/build/linux" BUILD_TYPE=Release \
    CMAKE_FLAGS=-DGEM_PLATFORM=linux PREFIX="$HOME/.local"
```

Add `DESTDIR` to stage a system package. The plain root Make defaults to Rasta.
Libraries and resources resolve from the installation without requiring `GEM_RESOURCE_DIR`. See
[Linux paths and installation](LINUX_PATHS.md) for commands, XDG storage,
relocation and architecture-specific library directories.

## Devices

The backend opens `/dev/fb0` and automatically discovers keyboard,
mouse, touchpad, and touchscreen evdev devices under `/dev/input`.
The account running Gemix must have read/write access to the framebuffer
and read access to the selected input devices.

Configuration variables:

- `GEM_LINUX_FB=/dev/fb1` selects another framebuffer.
- `GEM_LINUX_INPUT=/dev/input/event2,/dev/input/event5` disables
  discovery and selects exact input devices.
- `GEM_LINUX_GRAB=1` exclusively grabs selected evdev devices. This is
  recommended for a dedicated distribution but inconvenient during
  development.
- `GEM_VDI_WIDTH` / `GEM_VDI_HEIGHT` request a desktop size. When unset
  (or `0`), the desktop fills the visible framebuffer. Requests larger
  than the framebuffer are clamped.
- `GEM_LINUX_MOUSE_SCALE=8` multiplies relative (PS/2) mouse deltas.
  Absolute devices (USB tablet) are preferred automatically when present.
- `GEM_LINUX_KEEP_REL_MOUSE=1` keeps relative mice even when a tablet is
  open (default is to drop them so they do not fight absolute input).
- `GEM_RESOURCE_DIR=/opt/gemix/share/gem` overrides resource discovery.

The desktop Trash follows the freedesktop layout without requiring a desktop
environment. It uses `$XDG_DATA_HOME/Trash` when `XDG_DATA_HOME` is an absolute
path, otherwise `$HOME/.local/share/Trash`; items on another filesystem use
`$mount/.Trash-$UID`. If that per-volume store cannot be created, Desktop falls
back to the home Trash. The platform move wrapper uses `rename()` on one
filesystem and a no-follow copy/remove transaction across filesystems, so this
fallback also works for non-empty directory trees and symbolic links.

The framebuffer presenter accepts 1, 8, 16, 24, and 32 bits per pixel.
VDI remains monochrome and uses the **same shadow packing as rasta**.
Only the Linux present step converts that buffer to fbdev pixels
(set bit → black ink, clear → white paper — matching the rasta viewer).
Do not change AES/VDI polarity for Linux; fix display only in
`lib/platform/linux/raster.c`. Mouse motion re-blits the 16×16 cursor
box; XOR rubber-band outlines flush their rectangle under `wind_update`.

## Example

```sh
export GEM_LINUX_FB=/dev/fb0
export GEM_LINUX_INPUT=/dev/input/event1,/dev/input/event4
export GEM_LINUX_GRAB=1
export GEM_RESOURCE_DIR=/opt/gemix/share/gem
/opt/gemix/bin/gemd &
while [ ! -S "$XDG_RUNTIME_DIR/gem/gemd.sock" ]; do sleep 0.02; done
exec /opt/gemix/bin/desktop
```

If the session starts without input, inspect device capabilities and
permissions with `evtest`. If `gemd` cannot initialize the display,
verify that fbdev is enabled and `/dev/fb0` exists.

The packaging target uses `tools/scripts/install.cmake` and normal CMake
installation RPATH handling. Build artifacts still belong in root `build/`; the installation
package is a final output under `bin/gemix/`.

## Application SDK

Applications include `<gem.h>` or `<gem/gem.h>` from the separate SDK's
`include/` directory and link against its `libgem.so` in `lib/`. The client library connects
to gemd through `GEMD_SOCKET` (default `$XDG_RUNTIME_DIR/gem/gemd.sock`). See
[API transport](../architecture/API_TRANSPORT.md) for interface coverage and
compatibility limits.

For a separately exported SDK and standalone sample build, use the
[samples guide](SAMPLES.md). `make sdk` uses the hosted RelWithDebInfo Docker
configuration in `build/container-sdl/`;
to export from an already configured native Release build, run
`tools/scripts/container.sh exec cmake --build build/linux --target gem_sdk`.

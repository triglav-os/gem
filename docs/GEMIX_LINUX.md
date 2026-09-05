# Native Gemix Linux Backend

## Build

Configure a release build so the deployment does not depend on the
AddressSanitizer and UndefinedBehaviorSanitizer runtimes:

```sh
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEM_PLATFORM=linux
cmake --build build --target gemix_package -j"$(nproc)"
```

`bin/gemix` is relocatable and contains:

- `bin/`: `gemd`, desktop, terminal, calculator, and clock
- `lib/`: AES, VDI, `libgem`, and the native Linux platform library
- `include/`: the minimal public GEM application SDK
- `share/gem/`: fonts and runtime resources

Copy that directory into the target filesystem. Start `bin/gemd` from
the distribution's service/session manager, wait for `/tmp/gemd.sock`,
and then start `bin/desktop`. The service must set
`GEM_RESOURCE_DIR=/opt/gemix/share/gem` (adjusted to its installation
prefix).

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
while [ ! -S /tmp/gemd.sock ]; do sleep 0.02; done
exec /opt/gemix/bin/desktop
```

If the session starts without input, inspect device capabilities and
permissions with `evtest`. If `gemd` cannot initialize the display,
verify that fbdev is enabled and `/dev/fb0` exists.

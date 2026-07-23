# GEM for Linux

This project ports the Digital Research GEM user interface to Linux. It
provides VDI, AES, the `gemd` shared display server, `libgem` clients, a
desktop, and example applications.

## Dependencies

- Linux with GCC and CMake 3.20 or newer
- Linux framebuffer (`fbdev`) and evdev headers for the native backend
- Access to `/dev/fb0` and the required `/dev/input/event*` devices

## Hosted development build

The default backend uses the Rasta viewer:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Use VS Code F5 to run the desktop development session.

## Native Linux and Gemix package

Configure the native Linux framebuffer/evdev backend as a Release build
and assemble the relocatable Gemix deployment:

```sh
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEM_PLATFORM=linux
cmake --build build --target gemix_package -j"$(nproc)"
```

The result is in `bin/gemix`:

```text
bin/gemix/
├── bin/        gemd, desktop, terminal, calculator, and clock
├── include/    public GEM application headers
├── lib/        libgem and the private gemd runtime libraries
└── share/gem/  fonts and runtime resources
```

Copy the complete `bin/gemix` directory to the target Linux system. The
account running Gemix needs read/write access to the framebuffer and
read access to the required evdev devices.

The distribution should set the resource directory, start `gemd`, wait
for its Unix socket, and then start the desktop:

```sh
export GEM_RESOURCE_DIR=/opt/gemix/share/gem
export GEM_LINUX_FB=/dev/fb0
# Optional: select exact devices instead of automatic evdev discovery.
export GEM_LINUX_INPUT=/dev/input/event1,/dev/input/event4

/opt/gemix/bin/gemd &
while [ ! -S /tmp/gemd.sock ]; do sleep 0.02; done
exec /opt/gemix/bin/desktop
```

Applications include `<gem.h>` or `<gem/gem.h>` and link only against
`libgem.so`. `libgem` communicates with `gemd` through
`/tmp/gemd.sock`.

To restore the normal Rasta-based F5 development configuration:

```sh
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DGEM_PLATFORM=rasta
```

See `docs/GEMIX_LINUX.md` for device selection and deployment details.

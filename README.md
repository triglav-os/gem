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

```sh
cmake -S . -B build-linux \
    -DCMAKE_BUILD_TYPE=Release \
    -DGEM_PLATFORM=linux
cmake --build build-linux --target gemix_package -j"$(nproc)"
```

The relocatable result is in `bin/gemix`. Run it on a Linux virtual
terminal with:

```sh
bin/gemix/bin/gemix-session
```

See `docs/GEMIX_LINUX.md` for device selection and deployment details.

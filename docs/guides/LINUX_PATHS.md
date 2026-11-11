# Linux installation and user storage

GEM follows the [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir/latest/)
for user files and [GNUInstallDirs](https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html)
for installed programs. Checkout build products remain under `build/` and `bin/`;
they are not installation locations or persistent user-settings directories.

## User files

| Content | Default location | Override |
| --- | --- | --- |
| Component settings | `~/.config/gem/*.cfg` | `XDG_CONFIG_HOME` |
| Browser cookies, profile and site data | `~/.local/share/gem/gemscape/` | `XDG_DATA_HOME` |
| Browser HTTP cache | `~/.cache/gem/gemscape/` | `XDG_CACHE_HOME` |
| Browser log | `~/.local/state/gem/gemscape/cef.log` | `XDG_STATE_HOME` |
| Interactive process logs | `~/.local/state/gem/sessions/<checkout-id>/` | `XDG_STATE_HOME` |
| Home Trash | `~/.local/share/Trash/` | `XDG_DATA_HOME` |
| Additional or replacement program, folder and file-type icons | `~/.local/share/gem/icons/` | `XDG_DATA_HOME` |
| gemd socket | `$XDG_RUNTIME_DIR/gem/gemd.sock` | `GEMD_SOCKET` |
| Rasta framebuffer | `$XDG_RUNTIME_DIR/gem/rasta.fb` | `GEM_RASTA_FRAMEBUFFER` |

Each XDG override names the base directory, not the trailing `gem` directory.
Empty or relative user-base overrides use the default under `HOME`. System
configuration comes from the absolute entries of `XDG_CONFIG_DIRS`, default
`/etc/xdg`, followed by the user configuration. GEM saves only the user file.
No installer creates or overwrites a user's settings.

Runtime directories must be owned by the current user, be real directories,
and have mode 0700. Without a usable `XDG_RUNTIME_DIR`, the OS layer tries
`/run/user/<uid>` and then a private `/tmp/gem-runtime-<uid>/gem` fallback.
It refuses an unsafe existing fallback or GEM directory. No shared
`/tmp/gemd.sock` or `/tmp/rasta.fb` default remains. An explicit socket or
framebuffer override is still supported for isolated sessions and tests.

CEF stores cookies and other durable browser data within its profile; its HTTP
cache is directed separately to the XDG cache directory. Chromium also owns
auxiliary files within the profile. `GEMSCAPE_PROFILE=/absolute/path` remains
an explicit isolated-profile override: its cache and log stay beneath that
path, useful for tests and separate browser identities.

## Docker and F5

`make run` and F5 mount only the user's `gem` configuration, data, cache and
state subdirectories, plus the home Trash. They use the host's XDG overrides
and preserve those files after `make stop`. They do not mount the whole home
directory. System GEM configuration is mounted read-only. Other host data
still needs a deliberate container mount to be accessible.

The container has a private `/run/user/<uid>` tmpfs. The supervisor, socket and
framebuffer live in its `gem/session-<checkout-id>/` directory; F5's debugger
reads the same `GEM_SESSION_DIRECTORY`. Per-process logs use persistent state
storage. The disposable container home remains separate from the host home.
`GEM_RUNTIME_CONTAINER` can select a separate container for isolated runtime
checks; use separate XDG bases too when running independent sessions. The
optional F5 adapter regression does this without stopping the desktop session.

On first use, missing component files are copied from the old
`build/f5/config/gem/`, then `build/user-data/config/gem/`. An existing
standard-location setting is never overwritten. The old
`build/user-data/gemscape/` profile (or the previous XDG `gemscape/` data
location) is copied only when the new profile does not exist. Legacy sources
are retained. Stop an existing runtime before changing layouts, so it closes
its browser profile before migration. Automated tests retain explicit,
private build-directory storage and never migrate into real user directories.

## Install and package

All commands compile and stage through Docker:

```sh
make package                         # staged tree: bin/gemix/
make install PREFIX="$HOME/.local"   # user installation
make install DESTDIR="$PWD/build/stage" PREFIX=/usr/local
```

`make install` binds the requested destination into Docker; it does not install
into a disposable container. The invoking user must be able to write there.
For a system package, stage with `DESTDIR`, then let the distribution's package
manager install it. Native framebuffer builds select `GEM_PLATFORM=linux` as
described in [the native guide](GEMIX_LINUX.md).

| Installed content | Relative to prefix |
| --- | --- |
| Applications and gemd (plus hosted Rasta) | `bin/` |
| Required shared libraries | `lib/` (or configured architecture libdir) |
| Fonts, cursors, alerts and Gemscape toolbar | `share/gem/` |
| Program, folder and file-type icons (`<name>.rsc`, `.<ext>.rsc`) | `share/gem/icons/` |
| Private CEF libraries and required companion data | `lib/gem/cef/` |
| Documentation / licenses | `share/doc/gem/`, `share/licenses/` |

Runtime packages exclude SDK headers, archives, generators, sample disks and
debug symbols. `make sdk` exports development files separately. See
[package contents and verification](PACKAGING.md) for the exact contract.

The default prefix is `/usr/local`; system distributors commonly configure
`CMAKE_INSTALL_PREFIX=/usr`. CMake chooses its architecture-specific `libdir`
at configuration time, so pass installation directory changes through
`CMAKE_FLAGS` when configuring. `--prefix`/`PREFIX` changes the prefix only.

Installed ELF lookup uses relative RPATHs. Resources are found through
`GEM_RESOURCE_DIR`, user/system XDG data directories, the executable's sibling
`share/gem`, and the configured installation data directory. Installed CEF is
private to Gemscape, and neither its loader nor its resources require the
build dependency cache. `GEM_CEF_ROOT` is an optional explicit CEF resource-root
override. Build outputs retain their development search paths.

The relocated-install regression starts installed Gemscape, Gemread and Clock
from an unrelated directory with build-resource overrides removed; it also
checks ELF dependencies and the browser's separate XDG data/cache/state paths.

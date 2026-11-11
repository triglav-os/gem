# Runtime and SDK packages

Build, stage and validate packages using the Wischner Docker toolchain:

```sh
make package                         # runtime: bin/gemix/
make sdk                             # development: bin/sdk/
make install PREFIX="$HOME/.local"   # stripped runtime installation
make install DESTDIR="$PWD/build/stage" PREFIX=/usr/local
make tests
```

Both generated exports replace their previous contents, so stale files cannot
accumulate. Installation into a user/system prefix adds the declared runtime
files without deleting unrelated files. An older installation that contained
SDK files should be removed using its old install manifest before upgrading;
GEM does not guess which user files it can remove.

## Release builds

Pushing a tag of the form `vMAJOR.MINOR.PATCH` (for example `v1.2.3`) runs
`.github/workflows/release.yml`. It builds the Release runtime package for
both backends in the same Docker toolchain, with tests disabled. It checks
that gemd, every application and the core libraries are present, and it
publishes a GitHub release containing:

- `gem-VERSION-linux-x86_64-hosted.tar.gz`: Rasta backend, including Rasta;
- `gem-VERSION-linux-x86_64-native.tar.gz`: framebuffer/evdev backend;
- `SHA256SUMS`.

Each archive holds one top-level directory with the relocatable `bin`, `lib`
and `share` tree described below. Tags that do not match the pattern, such as
`v1.2` or `v1.2.3-rc1`, do not trigger a release.

```sh
git tag v1.2.3 && git push origin v1.2.3
```

The workflow can also be started from **Actions → Release → Run workflow**
once it is on the default branch. With a version `X.Y.Z` it creates tag
`vX.Y.Z` on the selected commit and publishes the release. Without a version
it only builds `dev-COMMIT` archives as downloadable workflow artifacts.

## Runtime contract

The runtime contains gemd, Desktop/File Manager/Trash, Terminal, Calculator, Ted,
Clock, Stout, Gemread and Gemscape. Hosted packages additionally contain Rasta
and librasta; native framebuffer packages omit both. Libraries contain only
the shared dependencies of these programs. Static archives, public headers,
resgen, pkg-config development metadata, tests, source code and sample programs
are excluded. Debug symbols are stripped from staged copies; development build
outputs remain available for debugging.

Fonts, cursor/alert resources and Gemscape toolbar artwork live in share/gem.
Clock's installed proxy variant draws its own numerals and needs no numeral
resource. Built-in Desktop icons are compiled in as fallbacks; optional folder, disk,
document, program and trash resources in share/gem/icons override them. Each bundled program except
Desktop, Stout and gemd has a small icon-only `<program>.rsc` in
share/gem/icons, which Desktop uses for that program's launcher and File
Manager entry. Home, Programs, Documents and Library artwork is installed in
the same directory as `home.rsc`, `programs.rsc`, `documents.rsc` and
`library.rsc`. [Program deployment](PROGRAM_DEPLOYMENT.md) describes how
distributions add further programs and icons.
No demonstration disk is needed to run GEM, so no sample disks, extracted disk
contents or historical IMGVIEW sources are included. Stout accepts user-supplied
ST/MSA images and Atari programs. Any future optional disk pack must ship the
image once, without a second extracted copy of its files.

CEF is private to lib/gem/cef. Each library, snapshot, ICU data file, resource
pack and selected locale occurs once. ANGLE and SwiftShader are retained for
browser software rendering; chrome-sandbox is excluded because the current
embedding explicitly disables it. The configured CEF UI locale defaults to
en-US; set `CMAKE_FLAGS=-DGEM_CEF_LOCALE=de` to build and package another locale.
The application and package use the same locale. This does not restrict web
page languages or Unicode data. Required third-party notices and Chromium
credits accompany the binaries under share/licenses.

The default install prefix is /usr/local; bin, lib and share are relative to
that prefix and respect CMake's GNUInstallDirs configuration. Resources and ELF
libraries resolve from the relocated installation without the source checkout.
User settings/data/cache/logs use XDG directories and are never packaged.
The standalone packaging guide is the runtime usage document; detailed
[configuration documentation](https://github.com/triglav-os/gem/blob/main/docs/guides/CONFIGURATION.md)
is maintained with the source.

## Platform requirements and launch

These are Linux x86-64 installation trees, not whole operating-system images.
System libraries remain distribution dependencies: glibc, libm, the C++/GCC
runtime, and CEF's NSS/NSPR, GLib/GObject, ATK/AT-SPI, X11/XCB, Xcomposite,
Xdamage, Xext, Xfixes, Xrandr, GBM/DRM, XKB, ALSA, CUPS, D-Bus, Cairo and Pango
libraries, Fontconfig with installed fonts (for example DejaVu), and a system
CA certificate store for HTTPS. Terminal requires a POSIX shell. Hosted Rasta
also requires SDL2. The project Docker runtime supplies
these dependencies. Native operation requires framebuffer/input permissions.
Use `ldd` on the installed programs and private CEF libraries to check another
distribution before deployment. GEM does not duplicate the distribution's
libraries in its package.

From an installed prefix, start bin/rasta (hosted only), bin/gemd and
bin/desktop in that order, all as the same ordinary user. Rasta adopts GEM's
framebuffer geometry through the startup handshake. Wait for
$XDG_RUNTIME_DIR/gem/gemd.sock before starting Desktop. The default private
runtime fallback is /run/user/UID/gem, then /tmp/gem-runtime-UID/gem if needed.
For development use `make run` or F5; both manage startup/shutdown in Docker.

## SDK contract

The standalone SDK contains public GEM and OS headers, FAT12/MSA headers,
matching shared libraries, static gemconf/gemctl helpers, resgen and core
runtime resources. It includes the selected direct-rendering backend and
platform_linux required by the client library. It excludes Stout's private
emulator/trap implementation, application binaries/data, CEF, MuPDF and sample
disks. `gem_sdk_config.cmake` records the selected backend; gem.pc supports
pkg-config client builds. resgen requires ImageMagick's convert, provided by
the Docker toolchain. Proxy examples connect to a separately running gemd.

## Verification

The 2026-11-06 audit built both hosted and native packages. Native checks used
Release, `/usr`, and `lib/x86_64-linux-gnu`, verified the absence of Rasta, and
built the standalone samples against its relocated SDK. Native GUI execution
was not tested without framebuffer/input hardware. The hosted package was
about 350 MiB after stripping, compared with about 1.5 GiB before this audit;
CEF remains the largest dependency. The stripped hosted package also passed
the complete browser acceptance suite: controls, cookies/cache/site-data
clearing, private/persistent profiles, session-cookie restoration, and
JavaScript/image preferences.


The install regression checks the exact file allowlist, ELF dependency closure,
absence of development/debug payload, unique CEF resources, relocated startup
and browser storage. The SDK regression builds the independent samples using a
relocated export and checks that stale files are removed. Full suite results
are generated in docs/tests/LATEST.md; detailed evidence remains under build/.

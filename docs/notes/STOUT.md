# Stout Atari executable runner

[Stout](../../src/apps/stout/main.c) runs Atari `.PRG`, `.TOS`, `.TTP`, `.APP`,
`.GTP` and `.ACC` executables in an emulated 68000 environment and forwards a
limited set of TOS and GEM traps to the hosted system. It is an experimental
runner, not a complete Atari ST or TOS implementation.

The bundled command-line frontend is in `src/apps/stout/`. The reusable runner,
ST/MSA disk access and trap bridge are in `lib/stout/`; GEMDOS, BIOS
and XBIOS services are split under `lib/stout/tos/`. The PRG loader is in
`lib/prg/`, with its public interface in `include/prg/`. The Musashi recipe and
patch are in `lib/musashi/`; the pinned upstream source is fetched into the
ignored build tree. See [cached Musashi](../guides/SAMPLES.md#cached-musashi-dependency).

Musashi calls `stout_handle_trap()` for every guest trap instruction. Stout
routes trap #1 to GEMDOS, #2 to AES or VDI, #13 to BIOS and #14 to XBIOS.
It also installs a minimal Line-A exception entry for the initialization,
line, fill and pointer operations used by existing high-resolution programs.
Guest programs use Atari window field numbers (`WF_WORKXYWH` = 4 and
`WF_CURRXYWH` = 5); the AES bridge translates them to the hosted selectors,
which number the outer frame 4 and work area 5. The `security_guest` integration
test executes real 68000 calls through all four system vectors and also checks
malformed VDI array bounds.

The emulated process uses the standard contiguous TOS layout: its text begins
at `basepage + 0x100`, followed by data and BSS. `Mshrink` validates that block
and establishes the first address available to `Malloc`, so startup libraries
and self-extractors see the memory topology expected on an Atari ST.
GEMDOS `Super` changes the emulated supervisor state and returns a stable
supervisor-stack token; it does not grant host privileges.
Optional 14-byte DRI/GST symbols are decoded when present. Other linker-private
symbol layouts are skipped using the byte count in the executable header, so
they do not prevent the following relocation stream from being applied.

Stout presents a monochrome 640x400 Atari screen at the top of guest RAM.
XBIOS `Physbase`, `Logbase`, `Getrez` and `Setscreen` expose that screen,
`Iorec` and `Kbdvbase` return stable guest-memory records, and `Supexec`
temporarily enters validated guest callbacks. `Vsync` yields for one hosted
display interval, and BIOS `Kbshift` reports the hosted modifier state (zero
until the input backend exposes modifier queries). The VDI bridge translates
MFDB headers and big-endian bitmap words for raster copies, and accepts the
classic cursor and input-vector calls used by TEMPUS. The AES bridge converts 68k
object trees, strings and text records into hosted objects for menus and
dialogs. It also loads big-endian Atari `.RSC` files, relocates their object,
text, icon, bitmap, string and tree references into guest memory, initializes
the AES global resource pointers, and follows resource links added dynamically
by an application. The standard keyboard, button, mouse, message and timer
event calls are forwarded to the hosted AES. Guest callback vectors are
retained only as compatibility state; host input is delivered through AES
events rather than by calling 68k functions from the host VDI.

The root build produces `bin/apps/stout`; the native package installs the same
program as `bin/gemix/bin/stout`. With a matching gemd and display session
running:

```sh
./bin/apps/stout /path/to/program.tos [disk.st|disk.msa] [arguments...]
```

A path beginning `A:/` loads the program from the following disk image. Mounted
raw `.ST` images are drive A: and are writable; compressed `.MSA` images remain
read-only. A program loaded from A: starts with its containing folder as the
drive's current directory, and relative paths resolve from there. `Dsetpath`,
`Dgetpath`, `Fsfirst` and `Fsnext` use the same per-drive state and enumerate
the mounted FAT12 directory rather than the host filesystem. A host-launched
program uses its containing directory as drive C:.
`STOUT_DRIVE_C` can explicitly select a different drive-C root.
`Fcreate`, `Fopen`, `Fread`, `Fwrite`, `Fseek`, `Fclose`, `Fdelete`, `Frename`
and directory creation update either that host tree or FAT12 files and both FAT
copies. Missing parent directories in an `Fcreate` path are created for
compatibility with historical self-extractors, without following host symlinks,
and `Fdatime` preserves their packed FAT timestamps. Each committed FAT update
atomically replaces the mounted raw image, so callers should make a backup when
the original image must remain unchanged.

The `TEMPUS.ST` `TEMPUS.TOS` and `OUTSIDE.TOS` self-extractors are exercised
against a disposable image copy: they create nested directories and persist
their complete output on A:. Their startup also verifies the standard TOS
basepage layout and stack ABI. Trap handlers leave arguments on the guest stack,
as TOS does; guest wrappers remain responsible for removing them.

The extracted `TEMPDEMO.PRG` is also used as a compatibility probe. It now
passes its XBIOS screen and keyboard setup, installs its 131-object menu,
draws its startup form and reaches the AES event loop. This does not imply
complete Atari hardware emulation; direct guest framebuffer writes and guest
VDI callback execution remain outside the hosted bridge.

Set `STOUT_TRACE=1` to trace dispatch. Interactive execution is unlimited by
default and yields between CPU slices. Set `STOUT_MAX_CYCLES` to a positive
decimal value for a bounded diagnostic run; zero also means unlimited.
Stout opens and registers its console bridge before starting `.TOS` and `.TTP`
guests. GUI program types register directly with AES without opening an unused
Terminal; any console output they emit falls back to process standard output.
When no GEM Terminal is available, console output from console guests also
falls back to process standard output.

Unsupported GEMDOS, BIOS, XBIOS, AES, VDI and processor operations are reported
on stderr. The full-session integration check launches a guest and verifies its
gemd connection. The numbered visual UAT scenarios do not establish arbitrary
Atari application compatibility.

An earlier development reference was the
[Atarimania GEM drawing demo](https://www.atarimania.com/demo-atari-st-simple-gem-window-based-drawing-demo_37707.html).
This link records the reference, not a current verified compatibility result.

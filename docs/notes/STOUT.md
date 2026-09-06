# Stout Atari executable runner

[Stout](../../samples/src/stout/main.c) loads simple Atari `.PRG` and `.APP`
executables into an emulated 68000 environment and forwards a limited set of
GEM-visible traps to libgem. It is an experimental runner, not a complete
Atari ST or TOS implementation.

Its application code is in `samples/src/stout/`. The PRG loader is in
`samples/lib/prg/`, with its interface in `samples/include/prg/`. The Musashi recipe and patch are in `samples/lib/musashi/`; the pinned
upstream source is fetched into the ignored build tree. See
[cached Musashi](../guides/SAMPLES.md#cached-musashi-dependency).

The root build produces `bin/samples/stout`. It also builds independently as
part of the [samples project](../guides/SAMPLES.md). With a matching gemd and
display session running:

```sh
./bin/samples/stout /path/to/program.prg
```

Set `STOUT_TRACE=1` to trace trap dispatch. The default execution limit is
5,000,000 emulated cycles. `STOUT_MAX_CYCLES` sets another decimal limit;
`STOUT_MAX_CYCLES=0` runs until the guest exits and yields between CPU slices.
F5 selects this interactive mode and runs `DEMO.PRG` extracted by the MSA sample.

The Musashi build enables trap and illegal-instruction callbacks. Stout selects
user mode before initializing the guest stack bank, so its GEM calls reach the
host callback bridge. The full-session integration test verifies that the guest
connects to gemd.

 Unsupported GEMDOS, XBIOS, AES,
VDI and processor operations are reported on stderr. A successful samples
build does not establish compatibility with an arbitrary Atari executable.
The 33 numbered UAT scenarios do not execute Stout guest programs.

An earlier development reference was the
[Atarimania GEM drawing demo](https://www.atarimania.com/demo-atari-st-simple-gem-window-based-drawing-demo_37707.html).
This link records the reference, not a current verified compatibility result.

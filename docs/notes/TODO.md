# Proposed work

Updated 2026-09-06. These are follow-up proposals, not claims that untested GEM
behavior is implemented. See the [audit](../tests/SECURITY_AUDIT.md) and
[latest execution report](../tests/LATEST.md) for measured results.

- Add coverage-guided fuzzing for RSC relocation, RPC tree decoding, FAT12/MSA
  and PRG relocation. Keep minimized failures as regression fixtures.
- Render file/program icons in Desktop and apply ordered
  `[association.NNN]` rules: registered data patterns launch their executable,
  while recognized GEM/TOS program files launch directly. The configuration
  and DESKTOP.INF `#G/#F/#P/#D` translation already preserve these rules.
- Reduce the remaining Clang analyzer diagnostics to small reproductions.
  Track bitmap ownership explicitly if integer-valued GEM pointer fields keep
  confusing analysis; preserve the public BITBLK representation.
- Expand Atari conformance tests against original manuals and real application
  resources. An API being present does not establish every historical semantic.
- Extend Stout's writable-volume support from raw `.ST` images to an explicitly
  requested copy-on-write or MSA re-encoding mode. Current raw-image writes are
  atomic but deliberately update the exact mounted image; `.MSA` stays read-only.
- Add native framebuffer/input acceptance on supported hardware. The Linux
  backend can be compiled and analyzed in CI, but dummy SDL tests cannot verify
  framebuffer ioctls, device permissions or physical keyboard mappings.
- Add focused acceptance for Clock numerals, Gemscape toolbar and Maestro tree
  resource rendering. Their startup is covered in the combined session; it is
  not exhaustive visual coverage. Clock's proxy path still uses text numerals.
- Every owned source now sits near the 500-line guidance except the
  public API headers, whose per-function documentation defines the SDK, and
  `tests/uat/demo31/main.c`, a deliberately self-contained acceptance demo
  that diverged from the Terminal application. Fold demo31 onto the Terminal
  modules only if its distinct timings become part of a scenario.
- Profile dirty-region merging and RPC batch sizes before further drawing
  optimizations. Preserve GEM update locking, write modes and clipping.
- Establish a reviewed dependency-update procedure for Rasta, Musashi and the
  Docker base/packages; add an SBOM and a scheduled dependency advisory scan.
  Archive hashes pin inputs but do not establish dependency safety.
- Isolate untrusted Atari programs in a separate restricted process if running
  arbitrary guest binaries becomes a supported use case. Musashi is an emulator,
  not a security sandbox.

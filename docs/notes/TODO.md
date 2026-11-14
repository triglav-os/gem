# Proposed work

Updated 2026-11-14. These are follow-up proposals, not claims that untested GEM
behavior is implemented. See the [audit](../tests/SECURITY_AUDIT.md) and
[latest execution report](../tests/LATEST.md) for measured results.

- Review and refresh the reference images for demos 20, 21, 22, 23, 25 and 26
  after the one-pixel compact window border. Those UAT cases fail on pixel
  comparison until the new chrome is accepted as the reference.
- Extend desktop icon dragging to drop actions (moving a file icon onto Trash
  or a folder). Icons currently only change grid cell.
- Add focused acceptance for the Linux framebuffer overlay re-presentation;
  the Rasta presenter path is covered, the Linux path is only compiled.
- Add coverage-guided fuzzing for RSC relocation, RPC tree decoding, FAT12/MSA
  and PRG relocation. Keep minimized failures as regression fixtures.
- Replace the temporary Files, Gemscape and Terminal desktop launcher artwork
  with reviewed icons. Ordered file associations and default PDF/Ted launches
  now have direct/proxy acceptance coverage.
- Extend Ted with a shared system clipboard, Unicode font/input support and
  multi-level undo if required; the current small editor keeps its clipboard
  local, preserves UTF-8 bytes and supports one undo/redo snapshot.
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
- Add focused acceptance for Clock numerals and Maestro tree
  resource rendering. Their startup is covered in the combined session; it is
  not exhaustive visual coverage. Clock's proxy path still uses text numerals.
- Extend Gemscape beyond its tested embedded-browser controls with native
  file-picker/download management, per-site permissions, a reviewed CEF sandbox,
  host clipboard integration and Unicode input. Current profile and privacy
  behavior is documented in the [browser guide](../guides/GEMSCAPE.md).
- Most owned sources sit near the 500-line guidance. Split Gemscape's CEF
  adapter and application event loop into smaller private modules as further
  browser services are added. Other exceptions include the public API headers,
  whose per-function documentation defines the SDK, and
  `tests/uat/demo31/main.c`, a deliberately self-contained acceptance demo
  that diverged from the Terminal application. Fold demo31 onto the Terminal
  modules only if its distinct timings become part of a scenario.
- Profile dirty-region merging and RPC batch sizes before further drawing
  optimizations. Preserve GEM update locking, write modes and clipping.
- Establish dependency review for the automatically updated Rasta `main`,
  pinned Musashi and Docker base/packages; add an SBOM and a scheduled
  dependency advisory scan. Archive hashes pin inputs but do not establish
  dependency safety. Runtime package dependency notices and exact-content
  checks are now implemented;
  automated advisory/SBOM generation remains future work.
- Isolate untrusted Atari programs in a separate restricted process if running
  arbitrary guest binaries becomes a supported use case. Musashi is an emulator,
  not a security sandbox.

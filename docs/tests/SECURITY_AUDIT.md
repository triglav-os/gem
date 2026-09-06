# Source, security and build audit

Reviewed 2026-09-06. This report distinguishes confirmed fixes, executable
verification and remaining diagnostics. It does not certify the absence of
vulnerabilities or complete Atari semantic compatibility.

## Coverage and tools

- GCC Debug with `-Wall -Wextra -pedantic`, AddressSanitizer and
  UndefinedBehaviorSanitizer; native Linux and Rasta builds.
- Clang 18 static analysis: **152 Rasta compilation variants across 129 source
  files**, and **90 native Linux variants across 83 source files**. Both scans
  completed without analysis failures. Distinct preprocessor configurations are
  retained. Changed framebuffer variants were reanalyzed after the final fix.
- All **124 owned C implementation files** occur in these build databases.
  Headers are analyzed through their consumers. **153 owned C/header files**
  pass the file/naming/documentation checks and Clang Format 18 verification.
- All 16 project Python files parse; shell scripts pass Bash syntax checks.
  RPC regeneration produces the same formatted output as the checked-in code.
- The compiled Musashi core, SoftFloat, disassembler, generator and generated
  opcode tables are included. Original upstream examples/test harnesses retain
  their provenance and are outside GEM's build. Downloaded Rasta C++ and system
  package advisory scanning are separate dependency work, not covered by this
  C analyzer run.

Evidence: [Rasta analyzer results](../../build/audit/scan/results.json),
[Linux analyzer results](../../build/linux-audit/audit/scan/results.json).
Individual diagnostic logs and plists are beside those files. Rerun with
`make audit`; use `--build build/linux-audit` with `tools/scripts/audit.py` for
an already configured Linux build. Audit returns nonzero while diagnostics
remain; it does not silently suppress reviewed warnings.

## Confirmed fixes

| Area | Finding and resulting behavior | Verification |
| --- | --- | --- |
| FAT12 | Sector multiplication could wrap; invalid geometry and oversized file sizes reached reads/allocations. Bounds now precede arithmetic, reads and allocation. | `test_security_bounds`, `security_disk` |
| FAT traversal | Unsafe names could escape the destination; cyclic directories could recurse indefinitely. Names, path lengths, depth, entry counts and visited clusters are bounded. | `security_disk`: traversal, cycle and oversized-file cases |
| MSA extraction | Destination symlinks/hardlinks could overwrite unrelated files. Extraction uses directory descriptors and refuses unsafe inode types, owners and link counts before truncation. | `security_disk`: normal/repeated extraction, symlink and hardlink refusal |
| Stout | Guest VDI counts could overrun fixed stack arrays. Counts and guest spans are checked before copying; local argument arrays are initialized. | `security_guest`: excessive and negative word/point counts through real 68000 traps |
| AES object drawing | A NULL clipping rectangle could reach `memcpy`. Missing clips now resolve to screen bounds. | Existing object/tree UAT and integration suite |
| Sample bitmaps | Clock's resource lookup failure path did not free earlier cloned blocks. Partial-load cleanup now releases each prior allocation. Unused icon-loading paths were removed. | Sanitized builds and full sample session; residual analyzer notes below |
| SoftFloat | A 64-bit shift boundary could invoke C undefined behavior. The 128-bit helper handles boundary counts explicitly. | `test_security_bounds`: counts 0–128 against repeated single-bit shifts |
| Musashi generator | Unsigned EOF checks, body-limit indexing and unchecked argv copies were unsafe. EOF is signed; limits are checked before access; fatal exits are explicit. | Generated opcode build; empty/10,000-character path rejection under sanitizers |
| Framebuffer | Rectangle end arithmetic could overflow; a replacement/truncated viewer framebuffer lost pixels outside a partial update. Widened arithmetic and full restoration on remapping/resize fix both. | `test_gemd_host`: extreme coordinates, replacement and truncation followed by a one-pixel update |
| Desktop | Workspace suppressed the root-disk fallback on filtered container mounts; desktop/AES checker phases differed after dialog repaint. Disk fallback and background phase now agree. | Direct/proxy desktop UAT and prolonged Desktop info all-samples regression |
| Launch scripts | Old startup helpers killed unrelated processes. Helpers now execute only their selected viewer/server; session cleanup retains ownership checks. | Combined session startup/cleanup |
| Reporting | Docker/custom-build reports linked to default-build artifacts. Evidence links now derive from the actual build directory. | `test_report` checks a nondefault build directory and a deliberately failing fixture |

Local vendor changes are recorded in [Musashi patch notes](../notes/MUSASHI_PATCHES.md).
Public GEM APIs and legacy types retain their spelling and layout; private
reserved-prefix helpers were renamed. Source compatibility is preserved rather
than renaming Atari contracts to satisfy a mechanical style rule.

## Remaining analyzer diagnostics

Both configurations report the same two possible leaks, in
`gemscape_free_bitblk()` and `maestro_free_bitblk()`. Inspection shows that
partial-load cleanup visits each earlier bitmap, frees `bi_pdata` and clears
the descriptor. The diagnostic path involves arrays of `BITBLK` objects whose
owned pointers are carried in GEM's integer-valued `LONG` field. No leak was
confirmed from that path, but the warning is retained as **unresolved analysis**,
not relabeled as a clean scan. Next action: reduce the ownership path to a
minimal reproducer and add allocation-failure injection before closing it.

The standards command checks a finite set of rules. Module size and comment
quality still require judgment; large legacy modules are listed for splitting
in [TODO](../notes/TODO.md). Vendor formatting and mandated GEM/POSIX names are
explicit compatibility exceptions. Sanitizers run only exercised paths; some
existing graphics tests disable leak detection for process-lifetime runtime
state. These checks are not exhaustive fuzzing or a sandbox assessment.

## Build and execution results

| Configuration | Outcome | Evidence |
| --- | --- | --- |
| Host GCC/Rasta | 84 tests passed, 0 failed | [Execution log](../../build/audit/host-verified.log) |
| Docker Ubuntu 24.04/GCC/Rasta | 84 tests passed, 0 failed | [Container execution log](../../build/audit/container-verified.log) |
| Native Linux Debug | Build passed without compiler warnings; physical display/input UAT not run | [Build log](../../build/audit/linux-verified.log) |
| Exported GEM SDK and standalone samples | Build passed; sample assets generated from `samples/data/` | [SDK log](../../build/audit/sdk-final.log), [samples log](../../build/audit/standalone-final.log) |
| Standards and formatting | 153 files passed; no owned formatting violations | `make standards`, Clang Format 18 |
| Incremental resources | Unchanged inputs do not rerun resource generators | [Incremental log](../../build/audit/incremental.log) |

The full test suite includes 33 demos in direct/proxy modes, calculator and
desktop UAT in both modes, and the all-samples session with mouse/keyboard
interaction and window-lifetime checks. These tests are automated and require
no AI to run. See [the latest test report](LATEST.md) for each case's result.

Rasta is downloaded from a TLS-verified, SHA-256-pinned source archive into
`build/`; the default viewer is `bin/tools/rasta`. A sibling checkout is never
required. The normal compiler toolchain is local. `make container` supplies
it through Docker and was tested with isolated container outputs. See the
[development guide](../guides/HOSTED_DEVELOPMENT.md) for dependencies and commands.

All sample-only disks, historical sample files and artwork live under
`samples/data/`; generated sample assets live beside sample executables in
`data/`. The SDK exports core resources and `resgen`, not sample artwork.
Current documentation links were audited; original manuals, vendor documents
and dated test outcomes retain their historical content. Incorrect container
artifact links in earlier reports were corrected without changing outcomes.

## Subsequent display-polarity correction

The original audit runs above checked raw PBMs and missed the upstream viewer's
opposite RGB mapping. The launchers now pass `--inverse`, supplied by a checked
build patch in `tools/scripts/rasta_inverse.cmake`. The new `test_rasta_polarity`
checks decoded colors as well as flag parsing/reconfiguration; current run
counts are recorded in [LATEST.md](LATEST.md). Earlier 84-test results retain
their original scope.

## Subsequent Musashi download migration

The embedded Musashi Git checkout was replaced by a pinned, SHA-256-verified
archive in ignored build storage. The checked-in patch reproduces the four
locally modified upstream files byte-for-byte. The old checkout was preserved
locally under `build/audit/musashi/previous-checkout/`; it is not a build input.
The source recipe uses Git only to apply the patch, outside the parent Git
repository's worktree/index context. See the
[dependency guide](../guides/SAMPLES.md#cached-musashi-dependency).

Verification includes an unchanged offline rebuild with blocked HTTP proxies
and unchanged archive/source timestamps, standalone SDK builds, Docker builds
and the complete acceptance suite. Latest per-test outcomes remain in
[LATEST.md](LATEST.md); migration logs are in `build/audit/musashi/`.

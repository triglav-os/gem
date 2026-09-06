# Musashi integration patches

Only the build recipe and `patches/hosted.patch` live in
`samples/lib/musashi/`. CMake downloads the pinned upstream source into the
ignored build tree and applies that patch; see the
[dependency guide](../guides/SAMPLES.md#cached-musashi-dependency). Preserve
upstream licenses and conventions. This records local changes made during the September
2026 audit; it is not a claim that the snapshot otherwise equals an upstream
release.

- `m68kmake.c`: make fatal exits `_Noreturn`; return a signed line length so EOF
  checks work; reject empty/oversized argv paths before fixed-buffer copies;
  reject a full opcode body before indexing past its array; check the prototype
  stream before output. The generator consumes the patched upstream opcode input.
- `m68k_in.c`: remove unused variables from the opcode templates and declare
  conditional cycle-cost state where it is used. Regenerate `m68kops.c` through
  CMake; never edit generated opcode output in place.
- `m68kmmu.h`: remove the final unused `resolved` assignment, retaining the
  earlier assignments that control table traversal.
- `softfloat/softfloat-macros`: define 128-bit left-shift boundary cases without
  shifting a 64-bit C value by 64. The ordinary 0–63 cases retain their existing
  behavior. `test_security_bounds` compares counts 0–128 with repeated one-bit
  shifts under UBSan.

Upstream example programs and the standalone upstream test harness remain in
the downloaded archive as provenance and are not built by GEM. The compiled core, disassembler,
SoftFloat, generator and generated opcodes are included in the analyzer run;
`m68kfpu.c` is included by the CPU translation unit. A separate upstream CPU
conformance/fuzzing campaign remains proposed work.

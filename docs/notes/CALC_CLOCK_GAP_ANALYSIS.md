# Calculator and clock status

Reviewed against the current sample sources on 2026-09-06. Earlier notes
about `calclock`, `calcif.c`, `ccsmain.c` and copied DOS bindings described
an imported accessory tree that is no longer the runnable implementation.

## Current implementation

| Application | Entry point | Implementation | Header |
| --- | --- | --- | --- |
| Calculator | [calc.c](../../samples/src/calc/calc.c) | [calc_ui.c](../../samples/src/calc/calc_ui.c) | [calc.h](../../samples/include/calc.h) |
| Clock | [clock.c](../../samples/src/clock/clock.c) | [clock_ui.c](../../samples/src/clock/clock_ui.c) | [clock.h](../../samples/include/clock.h) |

Both applications use the public GEM interfaces and their own event loops.
Their host services use `<gem/os.h>`. They no longer contain local copies of
the old accessory binding layer or `portab.h`.

The integrated build produces `bin/samples/calc` and `bin/samples/clock`
as libgem clients. `calc_hosted` and `clock_hosted` link AES/VDI directly.
The same sources build independently against an exported SDK; see the
[samples guide](../guides/SAMPLES.md).

The direct clock uses `data/clock_numbers.rsc`, generated from
`samples/data/artwork/clock/`. The proxy clock currently draws text numerals. Its editable
time/date behavior uses a process-local offset; it does not set the Linux
system clock. Both applications use monochrome GEM rendering.

## Compatibility and verification

These are hosted applications, not a reproduction of the original GEM
accessory lifecycle, DOS binding layer or print-spooler integration. Restoring
those historical behaviors would be additional compatibility work.

SDK isolation checks have built both application variants and launched their
rendered windows through Rasta. `test_sample_session` also starts their proxy
variants together with the desktop and other samples. Calculator acceptance
tests now check all 24 buttons, digit entry, clearing and `7 + 2 = 9` in both
direct and proxy modes. Its tree marks the final array entry with `LASTOB`,
so all objects are included in RPC serialization. Clock-editing, alarm and
exhaustive calculator arithmetic coverage remain additional work.
The numbered [UAT catalog](../tests/UAT.md) covers separate demos; do not count
all sample application behaviors as covered by those 33 scenarios.

Next useful checks are calculator keyboard/mouse equivalence and arithmetic
edge cases, clock date rollover and editing, and redraws after overlap and
resize. Add explicit scenarios before claiming these behaviors are verified.

# Implementation notes

Use the [architecture](../architecture/API_TRANSPORT.md) and
[development guides](../guides/HOSTED_DEVELOPMENT.md) for current operation.
These notes provide implementation context and compatibility worklists:

- [Bitmaps](BITMAPS.md): MFDB layout, raster implementation and proxy transfer.
- [Fonts](FONTS.md): historical format and current loader/renderer.
- [Compatibility priorities](GAP_ANALYSIS.md): remaining historical AES/VDI fidelity work.
- [Demo23](DEMO23_COMPATIBILITY_GAPS.md): menu scenario coverage and remaining questions.
- [Calculator and clock](CALC_CLOCK_GAP_ANALYSIS.md): current source ownership and limitations.
- [Stout](STOUT.md): experimental Atari executable runner and dependencies.
- [Implementation history](HOSTED_IMPLEMENTATION_HISTORY.md): earlier dated verification notes.
- [Reference links](LINKS.md): external background material.

A worklist item is not automatically a reproduced failure. Verify behavior
against source and add a test before describing an issue as confirmed. Dated
history retains the test counts and scope of the original work; use the
[latest report](../tests/LATEST.md) for the most recent full run.

See [proposed work](TODO.md) for priorities identified during the full source,
build, resource-ownership and security audit.

[Musashi patch notes](MUSASHI_PATCHES.md) record local changes to the vendor core.

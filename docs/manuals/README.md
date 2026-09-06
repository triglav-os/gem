# Original GEM references

This directory retains the original AES and VDI reference material:

- [AES_ALL.BUK](AES_ALL.BUK): Digital Research GEM Programmer's Guide, AES.
- [VDI_ALL.TXT](VDI_ALL.TXT): Digital Research GEM Programmer's Guide, VDI.
- Locally obtained PDF manuals, when present, are excluded from version control.

These are historical source documents, not descriptions of the Linux port's
current implementation. Preserve their original content and attribution;
record project-specific corrections or compatibility findings in `docs/notes/`.
Current public declarations are in `include/gem/`, and hosted behavior and
limits are described by [API transport](../architecture/API_TRANSPORT.md) and
the [compatibility worklist](../notes/GAP_ANALYSIS.md).

The upstream Musashi documentation accompanies the downloaded source under
the ignored build tree; the recipe and patch are in `samples/lib/musashi/`.
`samples/data/imgview/IMG.TXT` is documentation accompanying historical test data;
it is not the build or usage guide for this project.

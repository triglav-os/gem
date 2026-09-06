# Desktop sample

This directory implements the portable GEM desktop. Its application header
and icon declarations are in `samples/include/desktop_assets.h`.

`desktop` links libgem and shares a gemd display with other clients.
`desktop_hosted` links AES/VDI directly and runs its own display session.
Integrated builds place both executables in `bin/samples/`.

See the [samples guide](../../../docs/guides/SAMPLES.md) for independent SDK
builds and resources, and the [hosted guide](../../../docs/guides/HOSTED_DEVELOPMENT.md)
for launching the desktop. Build and directory rules are linked from
[project guidance](../../../AGENTS.md).

Desktop menus expose implemented actions only: Desk has Desktop info and
open file-manager windows, File has Open, and Arrange has Show as icons and
Sort by name. Unused browser slots take no space. A single click selects a
Workspace/disk icon; double-click its image or label, or choose File → Open,
to browse it. Other sample windows may cover the icon and must be moved to
expose the intended click target.

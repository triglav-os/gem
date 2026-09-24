# Desktop application

This directory implements the portable GEM desktop. Its application header
and icon declarations stay beside its implementation.

`desktop` links libgem and shares a gemd display with other clients.
`desktop_hosted` links AES/VDI directly and runs its own display session.
Integrated builds place both executables in `bin/apps/`.

See the [hosted guide](../../../docs/guides/HOSTED_DEVELOPMENT.md) for launching
the desktop. Build and directory rules are linked from
[project guidance](../../../AGENTS.md).

Desktop menus expose implemented actions only: Desk lists open file-manager
windows, File has Open, Delete, Restore and Empty Trash, and Arrange switches
between list and icon views and sorts by name, date, size or type. The list is a
details view with system-font rows and clickable system-font Name, Date, Size and
Type headers on the normal background. The header matches Gemscape's
address-band height and the scrollbar up-arrow square. Vertically centered rows
have no separator rules, use compact `/name` directory labels, and scroll between
the fixed header and a system-font file/folder-count status bar. List view opens
with no selection.
File-manager icons use a wider horizontal pitch to keep neighboring icons and
labels apart. A press must leave a three-pixel mouse rectangle before it starts
dragging, so a selected item can still be dragged during the double-click
interval while a stationary double-click opens it normally.
Icon dragging uses a dotted XOR union contour: the icon rectangle is its upper
stem and the measured title rectangle is its lower bar, producing the classic
upside-down-T outline. List rows use the same live drop-target tracking with a
dotted XOR rectangle. The shared release path recognizes Trash and normal File
Manager windows: a directory entry is a destination folder, while unused work
area means the displayed directory. Plain drops copy immediately and
Control-drops move; an `M` badge appears in the live contour only over a valid
normal destination. Shift-click toggles items in a multi-selection, whose
members receive individual contours and move or copy together. Existing names
are preserved, recursive folders are copied without following symbolic links,
and every affected open window refreshes. Mounted FAT12 images are read-only
drag sources: plain drops extract files and directory trees into Linux folders,
while Move and Trash operations are rejected.
Unused browser slots take no space. A single click selects a configured
folder/disk icon; double-click its image or label, or choose File → Open, to
browse it.
Other application windows may cover the icon and must be moved to expose the intended
click target.

The Trash icon opens the same browser UI on the virtual `trash://` namespace.
Location selection is published to gemd; gemd owns configured-location add and
remove operations, including the Delete-key confirmation and persistence.
The `desktop.cfg` `show_hidden` setting defaults to false and applies equally
to File Manager, mounted-image, Trash and Add Location lists.
Ordered `[association.NNN]` sections are applied when files are opened.
`.PRG`, `.TOS`, `.TTP`, `.APP`, `.GTP` and `.ACC` default to Stout, and both
host and mounted-image launches use the AES shell command/tail path. A mounted
program receives its `A:/` path plus the source image rather than a temporary
extracted copy.
Delete and dragging a File Manager item onto the icon or an open Trash window
are Desktop-owned operations rather than a generic AES payload protocol. While
an icon drag remains held over the desktop Trash, its title is inverted; moving
away restores it before release. Releasing on Trash keeps that feedback visible
behind the confirmation and applies the file operation after confirmation.
Files and non-empty folders are placed in the freedesktop home or same-volume
Trash store. If the volume store is unavailable, an OS-layer cross-filesystem
move safely falls back to home Trash. Alert buttons consume their release event,
so cancelling or acknowledging an error cannot poison the next click. Trashed folders can
be browsed without exposing their host paths; Restore handles destination
collisions without overwriting, while permanent Delete and Empty Trash require
cancel-default confirmation alerts. Return, arrow keys and Delete operate on
the active browser.

Classic AES provides addressed application messages but no standard filesystem
change broadcast. Successful Trash moves and restores therefore publish an
internal directory-change notification: every open File Manager showing the
affected parent directory reloads immediately, as do the virtual Trash windows.
Empty Trash is a global operation and is enabled from any File Manager whenever
the Trash contains an item; it does not depend on which window is active.

AES sizes popup menus from rendered labels and optional shortcuts. Popup item
text uses equal left and right padding; leading resource blanks remain available
as the checkmark gutter rather than becoming visible label indentation.

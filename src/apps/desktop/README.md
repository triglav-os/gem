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
windows, File has Open, Delete, Restore, Empty Trash, Rename, Cut, Copy, Paste,
New Folder, text/Markdown/HTML/CSV creation and Select All. Arrange switches
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

## File commands and launchers

Right-click an item or empty browser space for the same File commands. A
right-click on an existing multi-selection preserves it; blank space clears it.
Unavailable commands are disabled. Ctrl+C/X/V copy, cut and paste; Ctrl+A selects
all children, F2 renames, Ctrl+N creates text, and Ctrl+Shift+N creates a folder.
Naming dialogs support caret placement, selection, typing, Enter and Escape.
Names must be single components; existing entries (including dangling links)
are never replaced. New files use text, Markdown, HTML and CSV templates.

The process-local clipboard survives navigation and closing its source window.
Paste targets the current folder, preflights name collisions across the batch,
and retains unfinished cut entries on failure. Copy can extract mounted-image
entries; mounted images remain read-only in File Manager. File operations run
synchronously. Very large directory trees can keep the UI busy until completion.

Delete moves regular files and directories to Trash after confirmation. At the
Trash root, the shared menu offers multi-item Restore, Copy/Cut out, permanent
Delete and Empty Trash. Rename, Paste and creation remain disabled there; restore
items before editing them. Nested Trash browsing is read-only, with Copy enabled.
Successful moves out remove their `.trashinfo` records. Permanent deletion and
emptying keep Cancel as the default choice.

By default the desktop shows only Trash and any configured locations. With
`[desktop] show_launchers = true` in `desktop.cfg` it also supplies Files
(File Manager), Gemscape and Terminal launch icons. File Manager opens the launch working directory and uses the folder
icon. Gemscape and Terminal use the AES shell launcher and its normal executable
search path.

## Program icons

Program icons live in one icon directory, named after the program:
`icons/<name>.rsc` on the GEM resource search path. Installed icons are in
`<prefix>/share/gem/icons/` (for example `/usr/share/gem/icons/terminal.rsc`);
a user can add or override icons in `~/.local/share/gem/icons/`
(`$XDG_DATA_HOME/gem/icons/`), and `GEM_RESOURCE_DIR/icons/` takes precedence
over both. The name is the program's file name without any extension, so
`/opt/gem/Programs/terminal` and `TERMINAL.PRG` look for `terminal.rsc` and
`TERMINAL.rsc` respectively. Icons are no longer read from the program's own
directory.

Desktop launchers and executable entries in File Manager windows use the icon
when the file holds an icon-only GEM resource: Desktop draws its first
ICONBLK, up to 32x32 pixels, instead of the standard program icon. Otherwise
the standard icon stays. Resources with object trees are ignored, so an
application's own dialog resource is never mistaken for its icon. Every offset
is bounds-checked because the file is untrusted. File Manager remembers icon
lookups until the window lists a directory again, so newly installed icons
appear after the folder is reopened or refreshed.

Desktop, File Manager and Trash share the same lookup rules. File Manager
and Trash entries first try `<entry-name-without-extension>.rsc`, including
folders and documents. If that icon is absent or invalid, they try the common
`folder.rsc`, `document.rsc` or `program.rsc`, then the built-in artwork.
The parent-directory entry uses `folder.rsc`. Desktop locations use
`folder.rsc` or `disk.rsc`; Trash uses `trash.rsc`. The Files launcher tries
`filemanager.rsc`, then `folder.rsc`. These resources all live in the same
`icons/` directory and use the same XDG override order and validation as
program icons. Browser refresh reloads common icons as well as per-entry
lookups. Launcher artwork is loaded when the desktop launchers are created.

Icons use the same monochrome format as the built-in artwork: draw a 32x32
PNG in `samples/data/artwork/icons/` (transparent outside the icon, black
ink, white paper) and convert it with `resgen`:

```sh
resgen icons -o gemscape.rsc samples/data/artwork/icons/GEMSCAPE.PNG
```

The build does this for Gemscape, Terminal, Clock, Calculator, Gemread and
Ted, writes each resource to `bin/resources/icons/` and installs it in
`share/gem/icons/`. `FYODOR.PNG`,
`ISAAC.PNG` and `BLAISE.PNG` are ready for the planned word processor,
spreadsheet and email client.
Configured locations and the bottom-right Trash icon retain their positions.

# Deploying programs and icons

This guide is for distributions and packagers that add GEM programs to an
installed system, such as Triglav OS's GEMiX image. It says where a program and
its desktop icon go and how Desktop finds them. The rules apply to every
installation prefix. Packages built from this repository already follow them;
see the [packaging contract](PACKAGING.md) for their contents.

## Summary

| Item | Location | Rule |
| --- | --- | --- |
| Executable | `<prefix>/bin/<name>` | Any GEM client or Linux program Desktop may launch |
| Program entry | A folder that Desktop shows as a location, e.g. `<prefix>/Programs/<name>` | Lowercase name; usually a relative symlink to `../bin/<name>` |
| Icon | `<prefix>/share/gem/icons/<name>.rsc` | Same `<name>` as the entry, without any extension |
| File-type icon | `<prefix>/share/gem/icons/.<ext>.rsc` | Leading dot, lowercase extension; used for every file with that extension, e.g. `.txt.rsc` |
| Location | `[locations]` in `desktop.cfg` | Shows the program folder on the desktop |

`<name>` is the entry's file name without its extension. Desktop looks for an
icon under the name the user sees, which is the entry's own name, not the name
of the link's target. Use lowercase names, the same as the Linux command.
Triglav OS requires lowercase names.

## Program

Install the executable in `<prefix>/bin`, owned by root and not writable by
other users. Expose it through a folder that Desktop lists, usually one
`Programs` directory of relative symlinks:

```sh
ln -s ../bin/blaise <prefix>/Programs/blaise
```

Leave services and helpers that users should not start out of the folder,
for example `gemd`, the login greeter and `resgen`. Show the folder as a desktop
location in the system Desktop configuration (`/etc/xdg/gem/desktop.cfg`):

```ini
[locations]
/opt/gem/Programs = Programs
```

Users can override this in `~/.config/gem/desktop.cfg`; see the
[configuration guide](CONFIGURATION.md).

## Icon

An icon is an icon-only GEM resource with a single monochrome ICONBLK of up to
32x32 pixels. Draw it as a 32x32 PNG with black ink and white paper, and
transparent pixels outside the icon. Convert it with `resgen`, which ships in
the SDK:

```sh
resgen icons -o blaise.rsc BLAISE.PNG
install -m 0644 blaise.rsc <prefix>/share/gem/icons/blaise.rsc
```

Desktop searches for `icons/<name>.rsc` in this order and uses the first match:

1. `$GEM_RESOURCE_DIR/icons/`, for development and tests.
2. `$XDG_DATA_HOME/gem/icons/` (default `~/.local/share/gem/icons/`), for the
   user's own icons and overrides.
3. `<dir>/gem/icons/` for each absolute directory in `$XDG_DATA_DIRS`
   (default `/usr/local/share:/usr/share`).
4. `../share/gem/icons/` relative to the Desktop executable. With Desktop in
   `/opt/gem/bin`, this is `/opt/gem/share/gem/icons/`.
5. The data directory compiled into the build.

Desktop shows the standard program icon when no file is found, or when the file
is not a valid icon-only resource, for example an application's dialog
resource. Icons are never read from the program's own directory. Desktop reads
the icon files when it lists a directory, so an icon installed while a window
is open appears once that window is refreshed or opened again.

Desktop, File Manager and Trash share the same lookup rules. File Manager
and Trash entries first try `<entry-name-without-extension>.rsc`, including
folders and documents. Documents and programs then try an extension icon: a
name that starts with a dot, such as `.txt.rsc`, is used for every file with
that extension. Extensions match without regard to case, so `README.TXT` uses
`.txt.rsc`. Name extension icons in lowercase. A multi-part extension
is tried longest first, so `backup.tar.gz` uses `.tar.gz.rsc` before
`.gz.rsc`. Folders and hidden files such as `.profile` never use extension
icons. The GEM package ships `.txt.rsc`, `.md.rsc` and `.pdf.rsc`; a user icon
of the same name overrides them. If no icon is found, or it is invalid, they
try the common
`folder.rsc`, `document.rsc` or `program.rsc`, then the built-in artwork.
The parent-directory entry uses `folder.rsc`. Desktop locations use
`folder.rsc` or `disk.rsc`; Trash uses `trash.rsc`. The Files launcher tries
`filemanager.rsc`, then `folder.rsc`. These resources all live in the same
`icons/` directory and use the same XDG override order and validation as
program icons. Browser refresh reloads common icons as well as per-entry
lookups. Launcher artwork is loaded when the desktop launchers are created.

## Checking an installation

- `<prefix>/Programs/<name>` exists, is lowercase and resolves to an
  executable.
- `<prefix>/share/gem/icons/<name>.rsc` exists for each program that should
  show its own icon.
- Opening the Programs folder in icon view shows the program's icon. Programs
  without an icon file show the standard icon.

Desktop's own details are in its
[README](../../src/apps/desktop/README.md#program-icons). The full install
layout is in [Linux paths and installation](LINUX_PATHS.md).

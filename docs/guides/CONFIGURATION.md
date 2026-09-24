# GEM configuration

GEM uses small, ordinary INI-style text files. Each component owns one file,
so global settings can live in `gem.cfg`, Desktop settings in `desktop.cfg`,
Clock settings in `clock.cfg`, and another application can choose its own
component name without sharing a parser or schema.

## Standard locations

System files are read from each absolute directory in `XDG_CONFIG_DIRS`, or
from `/etc/xdg` when that variable is unset. User settings are then read from
`XDG_CONFIG_HOME`, or `$HOME/.config` when it is unset. GEM adds its own
subdirectory and the component filename:

```text
/etc/xdg/gem/desktop.cfg
$HOME/.config/gem/desktop.cfg
```

The directory rules are supplied by the selected platform implementation via
`gem_os_system_config_dirs()` and `gem_os_user_config_dir()`. Configuration
code does not access Linux/XDG files directly; another platform can provide
the same HAL functions with its own locations.

The first directory in `XDG_CONFIG_DIRS` has the normal XDG precedence over
later system directories, and the user file overrides all system values.
Missing files are valid. `gemconf_save()` creates the user `gem` directory
privately and replaces the selected component file atomically.

## File format

Files support section headers, `key = value` pairs, blank lines, and full-line
comments beginning with `#` or `;`:

```ini
# ~/.config/gem/desktop.cfg
[desktop]
show_trash = true
show_hidden = false

[locations]
/home/alex/Projects = Projects
/srv/archive = Archive
```

Whitespace surrounding a section, key or value is ignored. Values are plain
text: there is no quoting, escaping, interpolation or include mechanism.
Booleans accept `true/false`, `yes/no`, `on/off` and `1/0`. Files are limited
to 64 KiB; section names, keys and values are limited to 63, 127 and 1023
bytes respectively.

gemd owns the Desktop configuration in the hosted build. Every valid absolute
directory key in `[locations]` becomes a folder shortcut, with its value used
as the label. Desktop does not add mounted volumes, the root directory, the
current working directory or other automatically discovered folders. Use
`Add Location...` to add entries immediately. Select a folder icon and use
`Remove Location...` or press Delete to remove it; gemd persists the change
and tells Desktop to refresh. The direct hosted Desktop reads and writes the
same file locally. gemd watches the atomically replaced user `desktop.cfg` and
reloads it within 100 ms, so a save made through `libgemconf` also refreshes a
running proxy Desktop.

`[desktop] show_hidden` defaults to `false`. It controls every Desktop-owned
file list: Linux File Manager windows, mounted Atari-image windows, Trash and
the Add Location selector. The default omits dot-prefixed/OS-hidden host
entries and FAT12 entries carrying the Hidden attribute. Changing the value
through `libgemconf` updates a running proxy Desktop and reloads its open File
Manager windows.

## Legacy DESKTOP.INF compatibility

Classic Desktop programs use the AES shell scrap as their `DESKTOP.INF`
image. GEMix keeps `desktop.cfg` authoritative instead: `shel_get()` loads it
and builds a DRI/FreeGEM byte stream on demand, while `shel_put()` parses a
complete byte stream and atomically saves the corresponding structured keys.
No `DESKTOP.INF` file or cached legacy image is created. `shel_read()` and
`shel_write()` remain the separate command-and-tail launch interface.

The adapter understands these record classes:

| INF record | `desktop.cfg` representation |
| --- | --- |
| `#E` | view, sort, confirmation, click, date/time and sound keys in `[desktop]` |
| `#W` | ordered `[window.000]`, `[window.001]`, ... sections |
| `#M` | `[locations]` plus ordered `[location.000]`, ... icon metadata |
| `#T` | `show_trash` and `[trash]` |
| `#G`, `#F`/`#f`, `#P`/`#p`, `#D` | ordered `[association.000]`, ... sections |
| `#A` | ordered `[accessory.000]`, ... sections |
| unknown valid `#` record | hex-encoded `[legacy.000]`, ... record for lossless re-emission |

`#M` cannot carry a host path. On export, each `[locations]` entry supplies
the durable path and label while its matching `[location.NNN]` section supplies
the legacy drive, coordinates and icons. On import, `#M` records are matched in
order to the existing locations: the paths remain intact, while labels and icon
metadata change. Removing a legacy icon removes the corresponding location.
An extra `#M` with no existing host location is retained as unmapped metadata,
but it does not become a usable shortcut until a path is assigned through the
modern interface.

For example:

```ini
[desktop]
show_trash = true
show_hidden = false
view = icons
sort = name
confirm_delete = true
confirm_copy = true
confirm_overwrite = true
double_click = 2

[locations]
/home/alex/Documents = Documents

[location.000]
path = /home/alex/Documents
drive = C
x = 1
y = 0
application_icon = 0
document_icon = -1

[association.000]
kind = gem
executable = EDIT.APP
patterns = *.TXT
application_icon = 12
document_icon = 44
full_memory = false
memory_kib = 0
```

`confirm_copy` remains part of the classic `DESKTOP.INF` preference mapping for
legacy clients. The bundled multitasking Desktop does not show an operation
chooser: a plain File Manager drop copies immediately and a Control-drop moves.

Association order is significant. `kind = gem`, `dos`, `dos_parameter` or
`directory` maps to the classic `#G`, `#F`, `#P` or `#D` behavior; lowercase
`#f`/`#p` is represented by `full_memory = true`. These entries preserve old
Desktop “Install Application” data and drive File Manager launches. Patterns
are matched case-insensitively in section order; multiple patterns may be
separated by commas or semicolons. A matching data file such as `.TXT` is
passed to its registered executable through `shel_write()`. `.PRG`, `.TOS`,
`.TTP`, `.APP`, `.GTP` and `.ACC` have a built-in `stout` fallback when no
configured association overrides them. Mounted-image launches pass an `A:/`
guest path and the source image as separate quoted tail arguments, while host
files pass their full quoted path. The association is therefore the same for
normal and mounted File Manager windows and retains the normal AES application
id, command-tail and multitasking lifecycle.

The complete translation is guarded by the AES shell mutex. A legacy writer
therefore cannot interleave its parse/save transaction with another legacy
reader or writer. `gemconf_save()` uses atomic replacement, so gemd and modern
applications see either the old or new file, never a partial file. Independent
modern clients should still follow the normal load/change/save discipline and
avoid concurrently editing the same component because `libgemconf` does not
merge two simultaneous read-modify-write transactions.

## C API

Include `<gem/gemconf.h>` and link the static `libgemconf.a` plus the selected
platform library. A component supplies defaults in code, so no file needs to
exist:

```c
gemconf_t config;

gemconf_init(&config);
if (gemconf_load(&config, "clock")) {
    int show_seconds =
        gemconf_get_bool(&config, "clock", "show_seconds", 1);
    (void)show_seconds;
}
gemconf_clear(&config);
```

`gemconf_get()` and the typed boolean/integer readers return caller-provided
fallbacks. `gemconf_set()`, `gemconf_remove()` and `gemconf_save()` update the
user component file. The entry enumeration functions support repeatable
schemas such as Desktop's `[locations]` section. `gemconf_init()` must precede
the first load, and every initialized object must eventually be cleared.

# Ted text editor

Ted is the bundled plain-text editor in `src/apps/ted/`. It uses GEM and the
shared controls library; it has no browser engine or external editor dependency.
`make` builds `bin/apps/ted`, and the runtime package installs `bin/ted`.
Launch it by double-clicking a `.text` or `.txt` file in File Manager. Markdown,
CSV and log files also use Ted unless a configured association overrides it.

The toolbar uses 16×16 monochrome icons for New, Open, Save, Save As, Undo/Redo,
Cut, Copy and Paste, in that order. Unavailable actions are dimmed and hovering
an enabled icon shows its name and shortcut in the status bar. The File/Edit
commands remain in the desktop menu bar. Toolbar and status heights follow the
same font-based layout as Gemscape and Gemread (20 pixels at the standard font),
with square toolbar targets and centered icons. Narrow windows show only full
icon slots; all commands remain available through the menus and shortcuts.

The File menu and toolbar provide New, Open, Save and Save As. Ctrl+N, Ctrl+O,
Ctrl+S, Ctrl+Shift+S and Ctrl+Q provide the corresponding shortcuts. The standard
file chooser selects paths; normal save replaces the file atomically. A failed
save retains the in-memory edits. Closing, opening or creating a document with
unsaved changes offers Save, Discard and Cancel, defaulting to Cancel.

Click to place the caret; drag or use Shift+arrows to select. Ctrl+A selects all;
Ctrl+C/X/V copy, cut and paste within Ted. Ctrl+Z toggles the last edit and its
redo snapshot. Arrow keys, Home/End and Page Up/Down move the caret; Ctrl+Home/End jump
to the beginning/end of the document. The vertical
scrollbar tracks document lines, and caret movement pans long lines horizontally.
The status bar shows line, byte column, length and modification status.

Ted accepts text up to 1 MiB and rejects NUL-containing input. Existing UTF-8
bytes are preserved; cursor movement skips continuation bytes. The current GEM
font displays unsupported characters as `?`, and keyboard insertion supports
printable ASCII, tabs and newlines. Existing CRLF files retain CRLF for newly
inserted lines. The clipboard is local to this editor instance. Ted has no
persistent settings file or background service.

Validation lives in `tests/unit/ted/` and `tests/uat/samples/ted.py`. All build,
run and test commands use the repository's Docker entry points.

# Automated user acceptance tests

UAT means User Acceptance Testing: exercising a program as a user would and
checking its observable results. These scenarios are automated Python scripts;
running them requires the build tools and Rasta, not AI.

`make` builds the complete project and two binaries for each of the 34 demos.
`make tests` runs CTest's unit, integration and UAT suites. `make unit`,
`make integration`, and `make uat` select a suite. Nested makefiles expose the
same suites from `tests/` and each suite directory.
Full runs publish per-test results and proposed actions in
[`docs/tests/LATEST.md`](LATEST.md), retaining a timestamped report.

## Layout and execution

- `tests/unit`: header, renderer, and malformed protocol checks.
- `tests/integration`: real-server RPC, ownership/security, VDI and AES tests.
- `tests/uat/demo1`–`demo34`: numbered acceptance scenarios and applications.
  Demo19 builds the Terminal application's sources instead of a duplicate.
- `tests/include`: headers used only by acceptance fixtures and helpers.
- `tests/uat/manual`: shared drawing implementation for demos 3–16.
- `tests/uat/samples`: acceptance scripts for bundled and independent
  applications. Calculator checks both shipped variants for visible buttons,
  digit entry, clearing and `7 + 2 = 9`; Gemread and Gemscape have dedicated
  application flows.
- `tests/data/uat`: reviewed framebuffer checkpoints, shared by both modes.
- `<build-dir>/uat/demoN_MODE`: actual PBMs, process logs, and `result.json`.
  The default build directory is `build/container-sdl/`.

Demos 17, 18 and 19 replace the names `multi`, `menu_demo` and `terminal`.
Existing numbered demos retain their numbers. `terminal` is also built as a
normal bundled application from `src/apps/terminal/`; demo19 builds those same
sources. UAT-only helper headers live in `tests/include/`.

The independent applications in `samples/src/` are separate from these numbered
demos. Their complete interactive behavior is not implied by a passing UAT run.

Every UAT launches an actual Rasta viewer, relays its subscription and input
protocol, and starts a demo. Proxy tests also start their own `gemd`. No test
kills an existing desktop or viewer. The viewer must accept GEM's subscription
datagram: a rejected reconfiguration fails the test, because a manually started
viewer relies on it for its geometry and framebuffer path. A viewer that loses
the ephemeral-port race against a parallel session is restarted on a new port. SDL dummy video is the default, so a
window server is unnecessary. `RASTA_BIN` selects Rasta; `UAT_SDL_DRIVER`
selects a visible SDL backend when desired. Tests use a 640×400 BGRA viewer
surface and decode its monochrome GEM pixels for PBM assertions.
UAT enables AddressSanitizer and UndefinedBehaviorSanitizer with immediate
failure; leak detection is disabled for these process-lifetime UI runs.
The two small cursor regions are masked. Demo34 also excludes its variable
path text and discovered location rows as described below; all remaining
checkpoint pixels must match.

Static scene tests compare the framebuffer twice, including a later stable
frame. Interactive tests assert a visible response, compare the response with
the reviewed checkpoint, and require normal application exit. The terminals
instead execute `echo uat123 > output` through real HID input and verify the
result in their own working directory, then display it with `cat` and exit the
shell. Their output and blinking cursor are not treated as a static image.
Demo19 also verifies through that PTY that the child shell receives
`TERM=vt100` without inherited VTE/color-terminal capability variables. A
focused unit test feeds cursor, editing, SGR, DEC-graphics, UTF-8 box drawing,
control strings and alternate-screen sequences directly into the Terminal
parser.

## Catalog

| Demo | Intended behavior | Automated acceptance |
|---|---|---|
| 1 | Broadcast test card: lines, fills, circles, arcs, markers, text, clipping, blits | Complete rendered scene and stable repeat |
| 2 | List loaded fonts in native-size specimen columns | Font names, specimens and layout |
| 3 | Programmer's Guide listing 2-1 polygon | Polygon and captions |
| 4 | Clip a bar against an output window | Clipped fill and surrounding boundary |
| 5 | Polyline and polymarkers | Connected line segments and marker positions |
| 6 | Font and text background modes | System and small-font text specimens |
| 7 | Filled polygon and rectangle | Fill shapes and geometry |
| 8 | Cell array | Alternating 6×4 cell pattern |
| 9 | Contour fill | Filled enclosed region |
| 10 | Circle, arc, pie and ellipse families | All six primitive groups |
| 11 | Rounded rectangle outline and fill | Paired shapes |
| 12 | Justified text | Both text rows and font selection |
| 13 | Replace, XOR and erase write modes | Resulting cleared regions and captions |
| 14 | Opaque and transparent MFDB blits | Both bitmap destinations |
| 15 | VDI inquiries | Rendered dimensions, font and text metrics |
| 16 | Alpha cursor movement and text | Text at original and moved cursor locations |
| 17 | Small independent client window | Drag, redraw and close |
| 18 | Per-application menus | Menu opening/selection; proxy additionally switches between two live clients and closes B independently |
| 19 | Menu-equipped shell terminal | HID→AES→PTY→shell execution, visible output and shell exit |
| 20 | Two overlapping windows with different fonts | Both texts; close top window and verify exposed redraw |
| 21 | Checkboxes and radio controls | Toggle checkbox, select Compact, close |
| 22 | File/Options/Help menus | Open File menu, dismiss and close |
| 23 | Desk/File/Help menu application | Open Desk menu, dismiss and close |
| 24 | Modal checkbox/radio dialog | Toggle checkbox, change radio, cancel |
| 25 | Two editable TEDINFO fields | Type into Name, Clear, cancel |
| 26 | Menu and two-window application | Open Windows menu, top main window and close |
| 27 | External resource dialog | Load/relocate resource, render and cancel |
| 28 | Text, boxed text, image and icon gallery | Render all object classes, click image and verify status |
| 29 | BOXCHAR keypad and USERDEF gauge | Click keypad and verify callback-rendered update |
| 30 | Form and menu compatibility probes | Invoke form_keybd and verify displayed result |
| 31 | Scrollback shell terminal | Real shell execution, visible output and shell exit |
| 32 | Alert and error dialog launcher | Open note, accept it and verify returned button status |
| 33 | Ten-field form_do editor | Type, Tab, type, accept, inspect saved-fields alert, dismiss |
| 34 | Modern GEM file choosers | Render and cancel open, save and directory modes |

Demo 34 enumerates the checked-in `demo34_fixture` directory instead of the
host's changing `/tmp` contents. Its editable absolute path text and discovered
location rows are masked from reference comparison because mount lists differ
between hosts and containers. The field border, all three navigation buttons,
Locations header/frame, deterministic file rows and actions remain checked.
The reviewed references include the Up button and standard vertical scrollbar;
long path text must stay inside its field instead of crossing the right border.

The catalog describes representative acceptance paths, not exhaustive coverage
of every combination or every historical GEM behavior. Integration tests cover
additional entry points, bitmap round trips, attributes, resource bounds,
editable-buffer ownership, callbacks, malformed requests and application isolation.

## Gemread and Gemscape applications

`uat_gemread_direct` and `uat_gemread_proxy` verify the visible File menu, its
Open command, the document chooser, cancel restoration and a clean close from
the initially empty viewer. They then type a filename containing `y` into the
selector and require that selected PDF to render. The PDF is generated by the
test and needs no locally obtained manual. Both modes also exercise the settings
button, Display/Files tabs, Cancel, saved preferences, immediate rerendering,
Ctrl+W document close and Ctrl+O. Each reader scenario has isolated configuration.
`uat_gemread_pdf_proxy`
builds a deterministic three-page color PDF, verifies direct BGRA color output,
uses the thumbnail rail to move through all three pages, exercises the shared
zoom toolbar and standard GEM vertical scrollbar, and requires resize to
rerasterize the page for the new viewport. `uat_gemread_epub_proxy` builds a
deterministic DRM-free EPUB 2 fixture, verifies MuPDF's reflow layout is
rendered and performs the same zoom, scrollbar and resize checks. Both document
tests also require the shared top toolbar and bottom status bar plus a standard
GEM resize handle.

`uat_gemscape_proxy` uses two local HTML pages without network access. CEF's
headless software backend renders them without a host display server or GPU. It
checks the complete toolbar bottom border at startup and during address editing,
and rejects routine Chromium D-Bus diagnostics in normal terminal output. It
clicks a web-page button through CEF, scrolls a page using the GEM scrollbar,
clicks a page input and types letters, digits and the complete printable HID
punctuation set into that browser control,
then scrolls using the GEM scrollbar. It separately types an address containing
`y`, navigates with Enter, and uses the Back toolbar button. It then covers the
browser client with another GEM application and clicks the still-exposed
Gemscape title, requiring z-order, CEF content and the full client viewport to
be restored. Cursor movement across the opaque CEF viewport must leave a
visible GEM pointer at its new position and erase it from the old one. The test
closes Gemscape normally and rejects sanitizer,
undefined-behavior and stack-smashing diagnostics.

`uat_gemscape_browser` runs a loopback HTTP server with deterministic pages.
It observes actual form submissions, checkboxes, dropdowns, pointer dragging,
website confirmation/prompt replies, history and reload, address clicks and drag
selection, and plain-text clipboard transfer. It compares GEM scrollbar positions
with page-reported CSS coordinates at both endpoints and after dynamic growth.
Settings checks cover all three tabs, Save and Cancel, zoom and frame rate.
Fresh browser processes verify persistent cookies/local storage, session-cookie
opt-in, memory-only profiles, blocked cookies, and confirmed cookie/cache/site-data
clearing. The site-data case seeds two distinct HTTP origins and verifies that
clearing one preserves the other's cookies and local storage. A framebuffer
assertion requires native website dialog text to remain visible above CEF.
Profiles, config, event logs and screenshots stay under
`<build-dir>/uat/gemscape_browser/`, independent of interactive data.

## Reference maintenance

Ordinary tests never create or update references. To intentionally regenerate
specific direct checkpoints after reviewing an intended UI change:

```sh
python3 -B tests/uat/record.py 21 26
```

The November 2026 dialog review updates demos 24, 27 and 33 for the `11010`
enclosure on untitled forms. Titled alerts in demos 32/33 and the file chooser
in demo 34 use only standard window borders.

Inspect the PBMs in `tests/data/uat/` and compare them with the demo's source and
catalog requirements before retaining them. Run `make tests` afterwards: proxy
checkpoints must match the same direct references. `--capture` on `run.py`
collects diagnostic images only and never records an acceptance pass.

During this work the tests exposed misplaced LASTOB flags, malformed menu
sibling links, an unused uninitialized demo30 object, implicit dependence on
AES drawing colors, and invisible test-card/transparent-blit foreground colors.
The demos now state their intended tree structure and drawing colors explicitly.

## Desktop sample

`uat_desktop_direct` and `uat_desktop_proxy` run the shipped desktop binaries
with Rasta. They compare the configured Workspace and Trash pixels to the
source bitmap assets, double-click Workspace, enter `uat_child`, return to
the parent and close the browser. Title and content changes establish
navigation. The list-view checks require a header and visible Date, Size and
Type metadata beside each name. They also require no initial row selection and
a normal-background, system-font header aligned with the scrollbar arrow. The
header and file-count status stay fixed while centered rows scroll; the exposed
background must be restored after closing. Fixtures and
captured PBMs live under `<build-dir>/uat/desktop_<mode>/`. These checks use
deterministic HID input and require no AI or manual clicking. The fixture
configures its session directory as the sole folder shortcut.

Desktop checks also open the three-line system menu, require distinct
Add Location, Remove Location and Shutdown rows with separators, open/cancel
the directory chooser, assert that Desk has no blank rows with one browser
window, and reopen the configured Workspace with File → Open.
The F5 integration test separately runs every bundled application plus
Maestro, checks the Shutdown confirmation, and exercises overlapping window
movement and redraw through real HID and independent RPC.

`uat_trash_direct` and `uat_trash_proxy` open the Trash icon as a `trash://`
File Manager, drop a non-empty folder and a file on the Trash icon, verify that
Cancel is the default, confirm the moves, then cancel and confirm Empty Trash.
Source and destination frames verify the icon view's dotted XOR upside-down-T
union contour around the icon and measured title rectangles. A stationary
double-click first opens that folder to guard click/drag disambiguation. The
destination frame is captured while the button remains held and verifies that
Desktop hit-testing has already inverted the Trash title; the release-point
frame verifies that the title remains inverted behind the confirmation alert.
The list-row rectangle drag checks the same held-button Trash inversion.
The move and Restore path also keeps the original Workspace browser open and
verifies that its rows disappear and reappear through Desktop's internal
directory-change notification.
The
isolated `test_gem_trash` backend test covers File → Delete's shared move
operation, directory browsing containment, restore and rename-on-collision,
permanent purge, symlink safety, `.Trash-$UID` placement and a cross-filesystem
move of nested `tools/scripts/tool.sh` through the OS abstraction. Both Trash
UAT modes force a move failure, dismiss the resulting alert and immediately
double-click another folder to verify that modal button-up state cannot disable
later activation.

The Trash scenario also captures the Arrange popup. Popup menu content is
measured with equal left and right padding plus a distinct checkmark gutter
after conventional leading checkmark blanks are removed from the visible item
label. Menu demos exercise the same layout in direct and proxied modes.

The Shutdown confirmation is additionally exercised in the F5 integration session with a
three-second menu wait, a pending split RPC request and a seven-second alert
wait. Every pre-existing window and sample process must survive selection and
dismissal; this guards against transport expiry and modal timestamp races.

The viewer runs with `--inverse off --bpp 32` to display the GEM mono surface as black ink
on white paper. PBM checkpoints inspect the shared bits before display decoding;
`test_rasta_polarity` separately verifies the viewer's RGB mapping and retention
of inverse mode after subscriber reconfiguration.

### Opt-in held keyboard input

Use an extended-SDK consumer to hold and release a letter and modifier,
transfer focus, and exercise a menu shortcut after typing in an editor.
No key may remain held after reset; editor clipboard shortcuts take priority
when the consumer owns accelerator routing. Repeat with a standard MU_KEYBD
application: it must retain key-down events and AES menu shortcuts. The
`test_gemd_host` regression checks standard key-down compatibility alongside
physical releases and queue-overflow reset. Native's GEM menu/input tests
exercise the consuming adapter and focused editor route.

### Background window text updates

`test_gemd_host` covers fully and partially obscured windows. Updating the
background name and info must preserve every foreground work-area pixel;
closing the foreground window reveals the latest stored title. This exercises
standard `wind_set_str` calls without requiring the hosted input extension.

## File Manager commands

`uat_file_commands_direct` and `uat_file_commands_proxy` use real Rasta right
clicks and keyboard input to create folders and all four file templates, rename,
reject collisions and parent traversal, copy/paste across navigation, reject a
repeat paste, cut/move, select all, cancel and confirm deletion, and cut out of
Trash. They assert file contents and Trash metadata, not only screenshots.
The Desktop icon check also requires the File Manager and Gemscape launchers.
`test_gem_trash` covers exclusive creation, existing and dangling symlinks,
Trash export copy/move, collision preservation and metadata removal.

## Package integration coverage

`test_install_layout` stages the runtime with the actual package publisher,
checks the program/library/resource allowlists and stripped ELF dependencies,
and launches installed Desktop, Terminal, Calculator, Clock, Gemread and
Gemscape from unrelated directories. It rejects duplicate CEF payload and
obsolete staging files. `test_sdk_layout` builds independent samples against
a relocated export, including the FAT12/MSA CLI and pkg-config metadata. These
are integration checks, not additional numbered demo UAT cases. Native package
contents/dependencies can be checked with `install_layout.py --no-gui`; physical
framebuffer/input acceptance still requires hardware.

## Dialog, document and editor regressions

File-command scenarios assert the context menu's top/left enclosure, standard
modal window borders without an extra inner frame, parent-relative placement,
title dragging and successful submission
from the moved prompt. Desktop cases check the Terminal shortcut and quoted,
case-insensitive PDF/Ted file associations. Gemread verifies the visible menu
icon and Ctrl+G page navigation through a movable prompt. The browser control
fixture paints an alternating canvas while settings move; repeated captured
frames must preserve every dialog pixel, excluding the mouse pointer.

`uat_ted_proxy` edits a real `.text` file, exercises multiline input, selection,
copy/paste, cut, undo, atomic save, Save As, New/Open and cancellation of an
unsaved close.
`test_ted_document` covers UTF-8 character boundaries, selection replacement,
NUL rejection, the 1 MiB limit and unchanged data after a rejected edit.

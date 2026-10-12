# Hosted AES and VDI transport

`libgem` exports all 114 functions declared by `include/gem/vdi.h` and all
79 declared by `include/gem/aes.h`. Applications link it without `libaes` or
`libvdi`; only `gemd` owns the rendering libraries and physical display.

## Components and ownership

`src/aes/` and `src/vdi/` implement the native APIs. `src/gem/` implements
client transport, and `src/gemd/` dispatches server requests: `main.c` runs
the poll loop and modal servicing, `session.c` owns the connection table and
authorization, and `dispatch_vdi.c`/`dispatch_aes.c` execute the two request
families. GEM platform
backends live in `lib/platform/`; public and transport headers live in `include/`.
The shared AES, VDI, bitmap and object-tree transport declarations are
consolidated in [`include/gem/gemd.h`](../../include/gem/gemd.h), included as
`<gem/gemd.h>` by transport implementations and protocol tests.
Sample applications and their private libraries live entirely under `samples/`.
They can build against the [exported SDK](../guides/SAMPLES.md), independently
of core source and test trees. Direct applications link AES/VDI in their own
process; proxy applications use the server described here.

## Serialization and state

Scalar calls, attributes, inquiries, primitives and text use typed copied
payloads. Generated source is checked in; `src/gem/generate_vdi.py` and
`generate_aes.py` contain the argument-size schemas. They generate both client
wrappers and server dispatch/validation. Review schema changes on both peers.
Each generator updates only its marked section in `include/gem/gemd.h`,
preserving the other transport declarations. Apply `.clang-format` with Clang
Format 18 after regeneration; the generated sources follow the same standards
as handwritten C. Dispatch offsets are derived directly from validated counts.
The private protocol is version 2 and remains a same-host ABI, with matching
builds required. Editing state uses session-scoped opaque tree identifiers.

Each extended scalar request holds at most 16384 WORDs of aggregate array data.
Strings are bounded and terminated before dispatch. Invalid counts and sizes
are rejected. Mono MFDBs use 4096-byte chunks and connection-owned storage,
up to 8 MiB per bitmap. Source/destination aliasing and destination downloads
preserve off-screen and screen copy behavior without transmitting addresses.

A VDI-only application opening a workstation automatically registers a client
and requests an exclusive standalone display session. It cannot take over a
session with other applications, and other clients cannot attach until it exits.
AES applications continue to share gemd, with per-client VDI attributes and
visible-window clipping.

### Presenter overlays

`include/gem/overlay.h` adds a presenter-only BGRA API for browser-like
content. `gem_overlay_create()` attaches a layer to an AES window; resize,
map/unmap, destroy and dirty-rectangle updates are copied over the private RPC
transport. Updates are split below the 64 KiB payload limit, and gemd owns the
copied pixels. The layer is never exposed as an MFDB, BITBLK, AES object-tree
address, or VDI surface.

Rasta composites BGRA32 directly over the monochrome expansion. Linux converts
the composited result to its native framebuffer format without discarding RGB
channels on true-color devices. Alpha is supported in the presenter only; GEM
chrome and VDI drawing remain monochrome. Gemscape uses this API for CEF output
and Gemread uses it for MuPDF pages and thumbnails while both retain a native
monochrome GEM frame. The presenter reapplies the software cursor's masked
monochrome pixels after overlay composition, so opaque application content
cannot cover the GEM pointer.

AES object trees use at most 128 objects and a 48 KiB copied data arena. Graphs,
string termination, aligned descriptors and nonoverlapping referenced regions
are checked before pointer relocation. Edited text is copied back into the
original client buffer. Temporary server tree references are detached after
operations. USERDEF callbacks execute in the client through a bounded callback
round trip; their nested VDI drawing is handled by gemd. Client code addresses
are never executed in the server. Object-tree drawing includes the desktop
owner’s exposed background, with menu and open-window occlusion, as well as
owned window work areas and explicit dialog bounds. Menu arrays must have one
LASTOB on the final object and keep unused entries linked (hide them with
HIDETREE); truncating a popup’s links leaves an invalid disconnected graph. Hidden
children are excluded from popup row layout and saved-region bounds. Object
types are matched on their low byte on both peers, so extended type bits in the
high byte travel with the object without changing which entries need strings.

Tree linking/hit testing, shell environment/path lookups, and host-native
resource addresses remain client-local. Loaded resource offsets are
range-checked before relocation. Resource file and environment access uses the
platform OS abstraction. Resource trees then use the same copied-tree renderer.
Cooperative input convenience wrappers use the existing event RPC rather than
blocking other applications while waiting for a key. Historical VDI callback
registration retains function pointers in the client, matching the native
port's registration behavior (the native backend does not invoke these
historical vectors).

gemd routes physical input centrally before delivering application events.
Ordinary client `evnt_multi` calls consume their queues without polling HID;
synchronous modal panels retain their own input tracking. Window chrome
consumes its mouse presses, including a drag's release, instead of replaying
the original press into an application exposed by the moved window.
An application-owned `form_dial`/`form_do` tree captures clicks inside its
declared rectangle, while clicks and keyboard focus outside it continue to
follow the target application. While that form waits, gemd services ordinary
requests from other sessions and marks each in-flight request so a bounded
`evnt_multi` poll cannot recursively dispatch itself. Standard alerts and file
selectors remain the one shared, globally input-modal panel. The custom form
is a z-ordered redraw layer: gemd keeps its decoded tree alive for the blocking
`form_do`, clips both it and ordinary window drawing against higher layers, and
repaints its visible fragments when covering windows move or close.
Without an active application or menu, input falls back to the first live
application so standalone forms can receive keyboard and mouse events.
The top window's application owns the menu bar. An application without a
menu shows an empty strip, never another application's titles; the strip
stays reserved whenever a desktop owner exists, so the desktop and windows
keep their positions. Clicking the desktop activates the desktop owner. When
an application exits, the bar follows the window now on top, and only
messages addressed to the leaving application are discarded: the redraw
hints its closing windows queued to other applications are still delivered.

An always-present system menu (`src/aes/system_menu.c`) occupies the far left
of the bar, like the classic Apple menu. `menu_register()` installs up to six
copied service labels and returns their 0–5 ids; selection sends `AC_OPEN` to
the owner with that id in message word 3. Entries are removed explicitly with
`menu_unregister()` or automatically when their owner exits. A separator keeps
these registered GEM services apart from the fixed Shutdown command. Desktop
registers Add Location there, so it remains reachable even when no icon or
file-manager menu exists. The server draws and tracks this menu from central
HID dispatch, and gemd installs the Shutdown hook. Application menu titles are
laid out to its right. The strip appears only within a shared desktop context;
a lone standalone application keeps the whole screen.

The file-selector request carries an explicit open, save or directory mode and
an optional 64-byte dialog title. All modes run through the same server-side
modal chooser; `fsel_input(path, name, button)` and the titled
`fsel_exinput(path, name, button, title)` open the same custom selector.
Save and directory selection are additive wrappers over that validated request
rather than separate protocol opcodes. Hidden and dot-prefixed directory
entries are omitted by default. `fsel_options(FSEL_SHOW_HIDDEN, 1)` enables
them for later selectors, and the option query/update has a dedicated validated
RPC so direct and proxied applications use the same policy.
Desktop location records are also gemd-owned. The proxy Desktop publishes its
selected configured location, while gemd handles location list, add and remove
requests, persists `desktop.cfg`, owns the Delete-key confirmation, and sends a
refresh message after a successful change. The location-list response also
carries the Desktop `show_hidden` preference so proxy and direct File Manager
enumeration use the same configuration. That option uses a reserved word in
the final location slot so extending the response did not change its wire size;
a Desktop and gemd restarted from adjacent builds cannot desynchronize their
RPC stream at startup. gemd also watches the atomically replaced user
configuration file and reloads its parsed cache when an independent
`libgemconf` client saves it.
Normal event RPCs check queues without sleeping in gemd. libgem maintains
timer deadlines and yields for one millisecond between empty polls, keeping
idle applications from delaying other clients' drawing and keyboard input.

The historical shell scrap is now a compatibility projection of
`desktop.cfg`. `shel_get()` loads the current configuration and constructs a
bounded DRI/FreeGEM `DESKTOP.INF` image on demand;
`shel_get(SHEL_BUFSIZE)` returns that generated size. `shel_put()` validates and
decodes the complete image, saves structured configuration atomically and asks
gemd to reload its Desktop cache. One AES mutex covers each load/format or
parse/save transaction, so concurrent legacy clients cannot expose a torn
translation. There is no cached legacy blob and no real `DESKTOP.INF` file.

Shell launches are separate. `shel_read()` returns the command and TOS tail for
the calling child, while `shel_write()` reserves an application id before
starting it. gemd keeps a bounded id/PID command record, and the OS backend
supplies the same child-only launch metadata in the new process environment.
Consequently two concurrently launched applications read their own command and
exact TOS tail. File Manager resolves ordered `desktop.cfg` associations before
this call; Atari executable suffixes default to Stout and mounted programs
retain both their `A:/` guest path and image path in the quoted tail.
`doex == 0` is a successful no-op; `doex == 1` starts in
parallel, while the graphics and overlay flags are retained for compatibility.

## Scope of “all APIs”

The proxy surface now covers the whole public interface, rather than only the
original window/text subset. The direct Linux implementation is the behavioral
reference. This does not turn existing native compatibility stubs into a complete
historical Atari implementation: printer/metafile escapes, text rotation,
resource address substitution, unsupported distinctions between shell launch
flags and other explicitly documented
native compatibility behavior retain their existing semantics. Read the public
header documentation for those limits. The [UAT catalog](../tests/UAT.md) states
which user paths are actually exercised; [compatibility notes](../notes/GAP_ANALYSIS.md)
track historical behavior still needing work.

Native menu and drag tracking suspend RPC servicing while waiting for user
input. gemd excludes time spent in physical-input dispatch from session
transport, initialization and update-lock deadlines. A menu pause must not
disconnect clients with partially received frames or queued replies. This
preserves their remaining timeout budget rather than restarting it.

During cooperative alert/file-selector servicing, transport deadlines use a
clock reading taken after socket I/O, since receiving a new frame updates
its start timestamp. Comparing a pre-I/O reading with that timestamp can
underflow and incorrectly disconnect an active application.

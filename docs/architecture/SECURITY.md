# Hosted GEM security and safety

Original security review: 2026-09-05; protocol and documentation inventory
updated on 2026-09-06; VDI/transport code review on 2026-09-11; shell process
review on 2026-09-12. This describes implemented checks, not a claim that
GEM is a security sandbox. Public AES/VDI headers, signatures, constants and
object layouts are unchanged. The private libgem/gemd protocol remains a
same-host ABI protocol; it is not suitable for exposure as a network service.

## Deployment boundary

Run gemd as an ordinary user, never as a privileged service. Use a private
directory owned by that user (mode 0700) for `GEMD_SOCKET` and
`GEM_RASTA_FRAMEBUFFER`. Keep resource directories and the Rasta viewer
trusted. The daemon and clients check Unix peer credentials against their
effective UID. The socket is created with mode 0600 and is close-on-exec.

Startup refuses an existing socket path, regular file or symlink. It does not
delete a possibly live server's socket. Normal termination removes only the
socket inode created by this process. After SIGKILL, an operator must verify
that a socket is stale before removing it; a private directory per session
avoids this ambiguity. The compatibility default `/tmp/gemd.sock` still
exists, but a private session directory is the recommended deployment.

The Rasta framebuffer is opened without following its final symlink and is
checked through its open descriptor before truncation: regular file, same
owner, one link. New and accepted files use mode 0600. FIFOs, devices and
hard links are rejected. Normal viewer replacement of the framebuffer inode
still works. These checks do not defeat a malicious process of the same UID
changing an ancestor directory or concurrently truncating a mapped file.

Rasta input uses a connected UDP socket: only correctly sized six-byte
packets from the configured viewer address and port are accepted. This is
endpoint filtering, not cryptographic authentication. Keep the viewer on
loopback; a remote/untrusted network requires a separate authenticated
transport. HID polling has a bounded per-turn packet budget. Optional
`GEM_TRACE_AES`, `GEM_TRACE_DRAW` and `GEM_TRACE_HID` diagnostics go to stderr,
not predictable appendable files in `/tmp`; traces can contain input and
should be enabled only for local debugging.

## Request validation and scheduling

Each connection has a bounded incremental reader and one queued reply.
Incomplete headers, partial bodies and clients that stop reading cannot
block socket I/O for other applications. Dispatch validates magic, version,
exact opcode size, array counts and supported serialized menu objects before
using request fields. Replies are bounded too. Pointer-valued `wind_set`
fields must use the existing copied-string transport, never raw addresses.

Menu validation checks all indices, string-to-object assignments, final
`LASTOB`, supported types, and a rooted tree with one parent per object.
An iterative walk rejects cycles and disconnected objects before AES sees
the tree. `menu_tnormal` checks both the stored object count and title type.
Replacing/removing a menu detaches its old storage before freeing it.

Current hosted service limits are:

| Resource | Limit |
| --- | --- |
| Connections | 16 |
| Request/reply payload | 65536 bytes |
| Extended scalar array data | 16384 WORDs per request |
| Copied AES tree | 128 objects; 48 KiB data arena |
| Bitmap buffer | 8 MiB per bitmap; 4096-byte transfer chunks |
| Incomplete frame or pending output | 2 seconds absolute |
| Connection without `appl_init` | 5 seconds |
| Windows per application | 8, within the existing 16-window global pool |
| Nested application update locks | 64 |
| Outstanding update lock | 5 seconds absolute |
| Normal server-side event poll | Nonblocking; libgem waits in the client |
| Menu objects / copied strings | 64 / 32 |
| Concurrent AES launch records | 32 |
| Generated DESKTOP.INF compatibility image | 32767 bytes |
| System-menu registrations | 6 labels; 63 bytes each |
| Component configuration file | 64 KiB; 63/127/1023-byte section/key/value |

Expired clients are disconnected and their owned resources/locks released.
The libgem event wrapper preserves the application's requested logical timer
by repeating bounded waits. A debugger stopped in an outstanding update
transaction for over five seconds will lose that session deliberately.
Limits bound individual peers, not a hostile same-UID process continuously
opening many connections or issuing expensive legitimate requests.

## Multiple applications

`appl_init` is idempotent on one connection. Positive application IDs wrap
without colliding with live IDs. Window mutation requests require ownership;
an application cannot close, delete, rename or raise another application's
window through RPC. Each session owns its VDI drawing attributes, font and
clip. Raster requests are clipped to its own visible client regions; the
desktop owner may additionally draw exposed desktop. Copied object-tree
drawing applies the same desktop ownership and visible-region clipping;
USERDEF icons cannot paint through another application’s window. AES paints shared
window chrome. Even `v_clrwk` cannot erase another application's content.

The menu/desktop remain shared. Inactive menu removal leaves the active menu
alone, and active/desktop-owner exit selects a surviving owner. Keyboard
events follow the focused application. Update locks cannot be ended by
another application; disconnect/expiry unwinds only the owner's lock depth.
Polling uses connection generations so callbacks cannot reuse stale readiness
information after a session slot and file descriptor are recycled.

Classic `form_alert` and the open/save/directory selectors remain synchronous
and input-modal.
Their waits now service other connections, messages and timers, protect the
panel's visible area from background drawing, and cancel when their requester
disconnects. A second standard panel waits rather than recursively opening.
These panels are not independent per-application input-modal windows: the
classic shared desktop still has one interactive standard panel at a time.
Classic applications may also bracket a custom object tree with
`form_dial(FMD_START/FMD_FINISH)`. gemd records that owner and rectangle so
clicks inside the form stay with its owner, while another application's
uncovered window, menu and keyboard focus remain usable. gemd continues
servicing non-waiting requests from those applications and rejects recursive
dispatch of a request already in progress. An unexpected client exit clears
the ownership and redraws the exposed region. The decoded custom-form tree is
retained only for the lifetime of its blocking `form_do` request; window and
form repaint paths clip it through the same z-ordered visible-region model.

`shel_write` executes as the gemd user and is not a privilege boundary. It
does not invoke a command shell: the platform backend resolves an executable,
derives `argv` from the bounded 127-byte TOS tail, constructs a child-only
environment and calls `execve`. Existing shell metadata variables are replaced
in that environment and are never installed in the parent. AES reserves the
application id before the fork and associates command/tail state with that id
and PID, avoiding a global last-command race. Failed execs are synchronously
reaped; completed children are reaped through the OS wrapper and release their
bounded launch slot. Deployments should treat `GEMIX_HOME`, `PATH` and
`GEM_AES_PATH` as trusted executable search paths.

The desktop Trash is a virtual `trash://` browser rather than a normal browser
opened on `Trash/files`. Trash rules build freedesktop metadata, but all
environment, mount, directory, metadata, move and deletion operations pass
through `gem_os_*`. Same-filesystem moves use `rename`; cross-filesystem moves
first stage the source under a hidden sibling name, copy regular files,
directories and symbolic links without following links, and expose the
destination only after the original name is gone. Unsupported special files
are refused. A failed copy restores the original name and removes its partial
destination; after a complete copy, cleanup never removes the destination even
if a hidden staging remnant cannot be deleted. Directory navigation is resolved against the owning
store's `files/` root. Recursive deletion opens each component relative to an
already validated directory descriptor with no-follow semantics and refuses
the store root, preventing a symlink payload from redirecting deletion outside
Trash. Restores never overwrite an existing entry; the UI can skip, cancel or
select a generated collision name.

## Resource parsing

Font paths use checked formatting rather than an unbounded stack copy.
Font files are limited to 16 MiB. The loader validates character ranges,
bitmap dimensions, table/bitmap extents and monotonically increasing glyph
offsets within each row. Malformed-font cleanup has one owner for the file
buffer; it no longer frees it twice. Normal bundled fonts still load.

File-selector wildcard matching uses iterative, bounded backtracking rather
than exponential recursion. Directory scans yield to other connections every
64 entries, including entries rejected by the filter. Individual filesystem
calls are still synchronous and can block on an unhealthy mounted filesystem.

`libgemconf` accepts only simple component names and absolute XDG base paths.
It bounds files and every section/key/value, rejects embedded line breaks and
oversized existing files, and atomically replaces user files after creating a
private configuration directory. Configuration is ordinary same-user input,
not a trust or privilege boundary; a same-UID process can still change it.
gemd detects replacement of the user `desktop.cfg`, reloads its parsed cache
and sends Desktop a refresh message. The legacy shell adapter additionally
bounds lines, record counts and text fields while translating DRI/FreeGEM
records. One mutex covers each legacy load/format or parse/atomic-save
transaction; unknown syntactically valid records are stored as bounded hex.
Independent `libgemconf` read-modify-write transactions are not merged, so
cooperating modern applications must avoid writing the same component at once.

Copied AES trees also validate offsets, aligned descriptors and nonoverlapping
regions before relocation. USERDEF callbacks execute in the originating client;
gemd services nested requests with bounded callback waits. The wire protocol
is version 2; use matching clients and server builds. See
[API transport](API_TRANSPORT.md) for the full current interface and limits.

## Regression coverage

Hosted Debug builds use GCC warnings plus AddressSanitizer and
UndefinedBehaviorSanitizer. `make tests` runs unit, integration and UAT suites.
Current outcomes are in the [generated report](../tests/LATEST.md). Core
security-related checks include:

- `test_vdi`: existing reference drawing/state tests, normal fonts,
  overlong paths, truncated and malformed font metadata and glyph offsets,
  WORD-limit copy corners and text positions, screen scroll overlap and edges.
- `test_gem_header`: public header compatibility.
- `test_gemconf`: XDG precedence, typed reads, entry enumeration, length and
  component validation, removal and atomic user-file persistence.
- `test_gem_rpc`: existing fragmented/malformed traffic, normal proxy output
  buffers, scrap/menu lifecycle, HID modifiers and redraw/button state;
  wrong-source and oversized HID datagrams and invalid client buffer requests.
- `test_gemd_security`: three simultaneous independent clients; stalled
  readers/writers, authorization, drawing/clip isolation, focused keyboard
  routing, timer/lock limits, menu lifetime, dialog acceptance/disconnect,
  `WF_KIND` output, all four `1011` alert edges, 231 deterministic
  mutations of menu tree links, admission after an exclusive session exits,
  an empty menu strip for raised windows of menu-less applications, and
  `WM_REDRAW` delivery to an application uncovered by a window whose owner
  exits immediately.
- `test_gemd_host`: symlink/hardlink/FIFO rejection without changing the
  target file, normal framebuffer publication/replacement, ID rollover, and
  standard/hostile wildcard patterns.
- `test_aes_shell`: shell-start command synthesis, environment and path lookup,
  concurrent DESKTOP.INF translations without tearing, location/association
  round trips and direct `libgemconf` changes visible through `shel_get`.
- `test_aes_rpc`: parallel Clock/Terminal launches with distinct application
  ids and tails, missing-executable failure, no shell metadata leakage into the
  parent environment, legacy configuration translation, and gemd reload after
  an independent `libgemconf` save.

The RPC runner also verifies refusal of regular-file, symlink and live-socket
collisions, preservation of those paths, and normal socket removal on exit.
RPC test executables live in the output root's `tests/` directory so their
`$ORIGIN/../lib` runtime path loads the freshly built libraries, not an
installed older SDK.

Additional checks cover extended AES/VDI RPC behavior, resource ownership,
bitmap round trips, malformed tree/array packets, API exports and report failure
handling. All 33 numbered UAT demos run in both direct and proxy modes; their
[scenario catalog](../tests/UAT.md) defines the tested interactions.

Earlier external Native-gallery results are recorded in
[implementation history](../notes/HOSTED_IMPLEMENTATION_HISTORY.md), not counted
as additional tests in this repository's current suite.

## Remaining trust and limitations

Classic AES/VDI APIs accept caller pointers and often have no destination
capacity parameter. Direct callers must supply valid objects, strings and
adequately sized buffers. Preserving that ABI cannot make arbitrary in-process
calls memory-safe. Wire validation covers the serialized RPC surface; it does not protect
all direct AES/VDI entry points or every legacy resource parser.

The same UID still has ordinary filesystem/process privileges. Shared scrap,
menu, cursor and window metadata are cooperative desktop services, not secret
per-application storage. Standard selectors access the server user's files;
they are not a filesystem confinement mechanism. Terminal samples intentionally run
a host shell. The Linux framebuffer/evdev backend and the external Rasta
viewer have not received a complete adversarial audit in this pass.

Use OS-level isolation for genuinely untrusted programs. Passing these tests
does not establish resistance to every denial-of-service, race, malicious
resource file or future protocol extension.

Timeout accounting excludes synchronous physical-input tracking intervals:
gemd cannot service client sockets while native menu/drag tracking waits for
the user. Each session retains its remaining transport/init/update-lock time
budget across that server-imposed pause. Client bytes do not renew an existing
frame or lock deadline. The F5 interaction regression covers a partial valid
frame spanning a menu wait and confirms the client's window survives.

Modal transport expiry checks sample the clock after receiving/sending data.
This keeps the observation at or after a new frame's start timestamp and
avoids unsigned-underflow disconnects of healthy background applications.

## September 2026 audit additions

The build obtains Rasta and CEF over verified TLS from pinned archives with
checked SHA-256 hashes. Their downloaded and extracted files remain in ignored
build storage. Default launchers use the downloaded Rasta binary and never kill
unrelated viewer or server processes. The optional Docker compiler runs with
the caller's UID, no additional capabilities and no-new-privileges; its
workspace bind remains writable because it is a build environment, not a
sandbox for hostile code.

FAT12 reads check sector multiplication, image bounds, FAT capacity and file
size before allocation. Directory traversal rejects unsafe names, tracks visited
clusters and limits depth and entry count. MSA geometry is bounded to 256 tracks,
two sides and 128 sectors per track. Extraction walks directory file descriptors
and refuses symlinks, non-regular files, foreign owners and multiply linked files.
Stout checks VDI array counts and guest memory spans before stack-buffer copies.
A writable raw `.ST` mount is an explicit mutation target. Guest-relative A:
paths are normalized as FAT 8.3 components and cannot escape the image; cluster
chains and directory growth are bounded by validated FAT12 geometry. Updates
modify both FAT copies in memory and atomically replace only the selected image.
`.MSA` mounts remain read-only. This protects unrelated host paths but does not
make an untrusted guest or its selected disk image a security boundary.
A NULL AES object clipping rectangle now resolves to screen bounds.

The regression inputs and scan disposition are in
[the security audit](../tests/SECURITY_AUDIT.md). Static analysis, sanitizers and
these bounded adversarial cases do not prove the absence of vulnerabilities.

Dirty-rectangle bounds use widened arithmetic on both backends. If Rasta
replaces or truncates its framebuffer, the next partial present restores the
complete shadow frame before publishing damage, preserving untouched pixels.

Raster copy widths and glyph clipping edges are also widened: corners at the
WORD limits can no longer produce a zero-width division or a text run that
escapes the row buffer, both of which `BITMAP_COPY` and `V_GTEXT` could reach.
An exclusive VDI-only session releases its admission claim when its connection
closes. The [audit](../tests/SECURITY_AUDIT.md) lists the regressions.

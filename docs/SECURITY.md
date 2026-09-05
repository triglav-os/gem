# Hosted GEM security and safety

Review date: 2026-09-05. This describes implemented checks, not a claim that
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
| Request/reply payload | 8192 bytes |
| Incomplete frame or pending output | 2 seconds absolute |
| Connection without `appl_init` | 5 seconds |
| Windows per application | 8, within the existing 16-window global pool |
| Nested application update locks | 64 |
| Outstanding update lock | 5 seconds absolute |
| One server-side event wait | At most 2 ms |
| Menu objects / copied strings | 64 / 32 |

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
desktop owner may additionally draw exposed desktop. AES paints shared
window chrome. Even `v_clrwk` cannot erase another application's content.

The menu/desktop remain shared. Inactive menu removal leaves the active menu
alone, and active/desktop-owner exit selects a surviving owner. Keyboard
events follow the focused application. Update locks cannot be ended by
another application; disconnect/expiry unwinds only the owner's lock depth.
Polling uses connection generations so callbacks cannot reuse stale readiness
information after a session slot and file descriptor are recycled.

Classic `form_alert` and `fsel_input` remain synchronous and input-modal.
Their waits now service other connections, messages and timers, protect the
panel's visible area from background drawing, and cancel when their requester
disconnects. A second standard panel waits rather than recursively opening.
These panels are not independent per-application input-modal windows: the
classic shared desktop still has one interactive standard panel at a time.

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

## Regression coverage

Hosted Debug builds use GCC warnings plus AddressSanitizer and
UndefinedBehaviorSanitizer. All five CTest executables pass:

- `test_vdi`: existing reference drawing/state tests, normal fonts,
  overlong paths, truncated and malformed font metadata and glyph offsets.
- `test_gem_header`: public header compatibility.
- `test_gem_rpc`: existing fragmented/malformed traffic, normal proxy output
  buffers, scrap/menu lifecycle, HID modifiers and redraw/button state;
  wrong-source and oversized HID datagrams and invalid client buffer requests.
- `test_gemd_security`: three simultaneous independent clients; stalled
  readers/writers, authorization, drawing/clip isolation, focused keyboard
  routing, timer/lock limits, menu lifetime, dialog acceptance/disconnect,
  `WF_KIND` output, all four `1011` alert edges, and 231 deterministic
  mutations of menu tree links.
- `test_gemd_host`: symlink/hardlink/FIFO rejection without changing the
  target file, normal framebuffer publication/replacement, ID rollover, and
  standard/hostile wildcard patterns.

The RPC runner also verifies refusal of regular-file, symlink and live-socket
collisions, preservation of those paths, and normal socket removal on exit.
RPC test executables live in the output root's `tests/` directory so their
`$ORIGIN/../lib` runtime path loads the freshly built libraries, not an
installed older SDK.

The matching Native direct and proxy builds each pass six CTests,
including prior painting, editing, clipboard, opening/closing, tab/status
edges and splitter regressions. The added real-loop input test covers Rasta
click/key bursts, both text modes, clipboard buttons and shortcuts, and input
after modal/modeless and file-selector closure. This is coverage of those recorded cases,
not proof that every historical GEM behavior has an automated test.

## Remaining trust and limitations

Classic AES/VDI APIs accept caller pointers and often have no destination
capacity parameter. Direct callers must supply valid objects, strings and
adequately sized buffers. Preserving that ABI cannot make arbitrary in-process
calls memory-safe. The wire validator protects the supported RPC subset, not
all direct AES/VDI entry points or every legacy resource parser.

The same UID still has ordinary filesystem/process privileges. Shared scrap,
menu, cursor and window metadata are cooperative desktop services, not secret
per-application storage. Standard selectors access the server user's files;
they are not a filesystem confinement mechanism. No shell-command execution
was introduced. The Linux framebuffer/evdev backend and the external Rasta
viewer have not received a complete adversarial audit in this pass.

Use OS-level isolation for genuinely untrusted programs. Passing these tests
does not establish resistance to every denial-of-service, race, malicious
resource file or future protocol extension.

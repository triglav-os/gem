# Hosted implementation history

These historical notes preserve earlier implementation and verification details from the
README. They are not current operating instructions. Dated test counts describe
those runs, not current coverage. Consult
[API transport](../architecture/API_TRANSPORT.md),
[security boundaries](../architecture/SECURITY.md), and the
[latest test report](../tests/LATEST.md) for current scope and evidence.

## Security and multi-application hardening (2026-09-05)

The hosted server now checks peer identity and window ownership, isolates
client drawing state, validates menu graphs, and bounds stalled connections
and update locks. Standard dialog waits service other clients and unwind on
requester disconnect. Font parsing and Rasta framebuffer/input paths also
have additional checks. Public AES/VDI interfaces remain unchanged.

See [Hosted GEM security and safety](../architecture/SECURITY.md) for deployment rules,
service limits, the five passing hosted tests, and explicit remaining trust
boundaries. Run as an ordinary user with private socket/framebuffer paths;
this is hardening of a shared desktop, not a sandbox for hostile applications.

## Native gallery proxy verification (2026-09-05)

The modal-frame follow-up returns the actual window flags for `WF_KIND`
instead of falling through to work-area coordinates. Untitled `form_alert`
panels use a four-pixel black/white/black/black (`1011`) enclosure. Real-server
regressions check the query and all four alert edges. Public AES/VDI APIs
are unchanged. Native's matching direct/proxy suites each pass six tests,
including actual Rasta input through its application loop for both text
modes, clipboard buttons/shortcuts and input after dialog closure.

Native can link only `libgem`, with a separate Rasta-backed `gemd` process
owning AES/VDI. Both processes accept the private `GEMD_SOCKET` environment
variable (default `/tmp/gemd.sock`) and must share file-selector and scrap
paths. No public AES/VDI headers, signatures, opcodes or structures were
changed for this verification.

The client now marshals `fsel_input`, `graf_mkstate`, `scrp_read`, `scrp_write`,
`vqt_fontinfo`, `v_hide_c` and `v_show_c`. File buffers, font arrays and mouse
outputs are copied back into client memory; process pointers are not used for
these outputs. The existing menu transport relocates supported title/string
objects in server-owned memory. Menu replacement, hiding and disconnected
client cleanup detach that memory from AES before freeing it. A regression
reproduced a server heap-use-after-free when a menu-owning client disappeared;
that path now passes with AddressSanitizer.

RPC dispatch checks exact request lengths, polyline counts and menu indices,
counts and supported object types before accessing payloads. The client
disconnects on an unexpected reply size/magic instead of leaving the stream
misaligned, and a broken socket no longer kills it with SIGPIPE. The current
private protocol remains a same-host ABI protocol, not a cross-endian or
cross-architecture network format. That earlier verification covered Native’s calls. The expanded transport now
exports every public AES/VDI function; object rendering uses the native server
renderer with copied trees and client-side USERDEF callbacks.

`test_gem_rpc` runs against a private server through `tests/integration/rpc/run.sh`.
It checks metrics, geometry, drawing, scrap state across reconnects, fragmented
headers, rejected short requests/invalid counts, menu replacement and abrupt
client exit. It also drives the server's Rasta HID socket to check modifier
bursts and held-button state across queued redraw replies. `evnt_multi` now
fills pointer/button/modifier outputs on that message path; zero-filled proxy
replies previously invented mouse edges in clients. Build the `gemd` and
`test_gem_rpc` targets, then run
`ctest --test-dir <build-directory> -R test_gem_rpc --output-on-failure`.
The visual Native gallery pass additionally checks file acceptance/cancellation,
editing, combo choices, dialogs, collections, tables, splitters and menu Exit.
Directory activation by mouse or OK now immediately redraws the selector;
previously the directory changed but its old listing remained visible.
Regional and title redraws preserve pending presentation inside an outer
`wind_update` transaction. Native uses this to close a window and repaint its
exposed content before publishing, without an intermediate desktop flash.
These fixes change implementation only, not public AES/VDI interfaces.
Desktop and window-frame damage shares the client's higher-window subtraction.
The desktop checker is filled only in exposed fragments; hidden window frames
and fully obscured client redraw notifications are skipped. Native's opening
regression checks intermediate pixels for modal/modeless windows fully above
their owner and partly above desktop, and fails against the preceding runtime.
All five hosted CTests pass, including VDI and public-header checks. The
VDI test host implements the regional-present callback used by the renderer.

## Hosted rendering and file-selector behavior

The rasta backend draws into a private packed surface and publishes completed
VDI updates to the mapped viewer framebuffer. AES pattern/inversion helpers
respect the VDI clip, and bitmap copies mark their destination as dirty.
AES paints the desktop checker when it first acquires its workstation,
before showing the cursor; a fresh session does not depend on window
movement to initialize the surrounding background. VDI-only applications
retain their plain initial surface.
The synchronous file selector restores exposed caller regions from a
pre-dialog snapshot while it is moved. It uses the hosted paper/ink colors,
font-centered buttons, one parent-directory entry, and a typeable filename;
suggested filenames are preserved even when they do not exist yet. These
are implementation changes; the public AES and VDI interfaces are unchanged.

File-selector buttons invert while held and activate on release inside the
same button. Closing the top window refreshes the complete newly active
title, including portions outside the closed window's footprint. Removing a
menu restores its desktop strip. Saved-region restoration records damage and
requests presentation; nested partial redraws preserve the outer batch's
pending presentation so dismissed popups do not leave fragments behind.


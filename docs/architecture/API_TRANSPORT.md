# Hosted AES and VDI transport

`libgem` exports all 114 functions declared by `include/gem/vdi.h` and all
76 declared by `include/gem/aes.h`. Applications link it without `libaes` or
`libvdi`; only `gemd` owns the rendering libraries and physical display.

## Components and ownership

`src/aes/` and `src/vdi/` implement the native APIs. `src/gem/` implements
client transport, and `src/gemd/` dispatches server requests. GEM platform
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

Each extended scalar request holds at most 2048 WORDs of aggregate array data.
Strings are bounded and terminated before dispatch. Invalid counts and sizes
are rejected. Mono MFDBs use 4096-byte chunks and connection-owned storage,
up to 8 MiB per bitmap. Source/destination aliasing and destination downloads
preserve off-screen and screen copy behavior without transmitting addresses.

A VDI-only application opening a workstation automatically registers a client
and requests an exclusive standalone display session. It cannot take over a
session with other applications, and other clients cannot attach until it exits.
AES applications continue to share gemd, with per-client VDI attributes and
visible-window clipping.

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
children are excluded from popup row layout and saved-region bounds.

Tree linking/hit testing, environment pointers, and host-native resource
addresses remain client-local. Loaded resource offsets are range-checked before
relocation. Resource trees then use the same copied-tree renderer. Cooperative
input convenience wrappers use the existing event RPC rather than blocking
other applications while waiting for a key. Historical VDI callback registration
retains function pointers in the client, matching the native port's registration
behavior (the native backend does not invoke these historical vectors).

gemd routes physical input centrally before delivering application events.
Ordinary client `evnt_multi` calls consume their queues without polling HID;
synchronous modal panels retain their own input tracking. Window chrome
consumes its mouse presses, including a drag's release, instead of replaying
the original press into an application exposed by the moved window.
Without an active application or menu, input falls back to the first live
application so standalone forms can receive keyboard and mouse events.
Normal event RPCs check queues without sleeping in gemd. libgem maintains
timer deadlines and yields for one millisecond between empty polls, keeping
idle applications from delaying other clients' drawing and keyboard input.

## Scope of “all APIs”

The proxy surface now covers the whole public interface, rather than only the
original window/text subset. The direct Linux implementation is the behavioral
reference. This does not turn existing native compatibility stubs into a complete
historical Atari implementation: printer/metafile escapes, text rotation,
resource address substitution, shell launch flags and other explicitly documented
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

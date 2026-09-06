# F5 sample interaction regression

The previous F5 check verified startup and process liveness, which did not
establish that windows could still be manipulated.

## Finding and change

Normal gemd client event polls could read physical HID events before the
central window manager processed them. Forwarding such a press to another
application's queue skipped window tracking. Separately, central tracking
could consume an entire drag and then queue its original press using the
window layout after the move, exposing another application to a stale press.

gemd now routes normal input centrally. Client event polls consume their
application queues; synchronous modal panels keep their input handling.
Window chrome presses are consumed by window tracking and are not replayed
into applications underneath.
Forms without a window or menu retain input through a fallback to the first
live application when no active application or menu owner exists.

## Executed checks

The updated `test_sample_session` starts Rasta, gemd, desktop and all samples,
including the extracted MSA guest in Stout. It injects real Rasta HID packets
and queries window geometry through an independent GEM RPC connection with a
two-second reply timeout.

- Against the previous input implementation, the regression failed: the first
  Stout drag did not reach the expected outer rectangle `(100, 102, 512, 373)`.
  See [before-fix execution](../../build/drag-before.log).
- With the fix, Stout completed three successive drags. Terminal then moved
  and handled its close button, exiting successfully. Each move checked actual
  coordinates and a changed framebuffer; RPC replies remained responsive.
  See [focused execution](../../build/drag-session.log) and
  [session evidence](../../build/sample_session_test/state.json).

These checks reproduce lost interaction and verify recovery of repeated
window operations. They do not establish that every possible cause of an
interactive freeze has been excluded. The full suite result is in the
[latest generated report](LATEST.md).

## Actions

Keep this interaction check in `make tests`. Restart the F5 session to load
the rebuilt libraries and server. Future interactive regressions should add
input and observable outcome checks rather than relying on live processes.

The session test also captures the initial desktop, locates Workspace from
its bitmap asset, moves Stout to expose the left edge of the Workspace label,
and double-clicks that label while Terminal and all other samples remain
running. Independent window queries verify that the file manager opens and
closes. Evidence includes `desktop_ready.pbm`, `workspace_before.pbm` and
`workspace_open.pbm` in `build/sample_session_test/`.

The F5 check also leaves Desk open for three seconds with a partial RPC frame
pending, then selects Desktop info above a window closer and keeps the alert
open seven seconds. The client connection, window geometries and all sample
processes must survive. See [the Desktop info regression](DESKTOP.md) for the
reproduced disconnect and timeout-accounting fix.

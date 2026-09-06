# Calculator rendering and terminal input

## Findings

Calculator placed `LASTOB` on its root despite having 26 subsequent objects.
libgem uses that flag to determine the object array length, so its serialized
tree excluded every button. The marker now belongs to the final array entry,
independently of sibling order.

Each normal `evnt_multi` RPC previously waited up to two milliseconds inside
gemd. These waits accumulated across clients and across the drawing requests
needed to echo a terminal character. Normal server event polls now return
immediately; libgem manages timer deadlines and yields between empty polls.

Terminal also forwards Control combinations as control characters and ignores
modifier-only events instead of injecting NUL bytes into the PTY. The typed
command scenario exercises Control-U and shifted punctuation. The longer
session test exposed an exit acknowledgement race: gemd now starts a fresh
initialization deadline after `appl_exit`, allowing its reply to drain.

## Executed evidence

- The calculator proxy check failed against the old object marker with
  `Missing button 0,0`. See [before-fix log](../../build/input-debug/calc-before.log).
- Both calculator variants passed visible-button checks, digit entry, clearing
  and `7 + 2 = 9`. See [direct captures](../../build/uat/calc_direct/result.json)
  and [proxy captures](../../build/uat/calc_proxy/result.json).
- Ten isolated terminal keystrokes with every F5 sample running had median
  key-to-framebuffer latency of 141.9 ms before the polling change and 2.4 ms
  afterward on this machine. See [before](../../build/input-debug/before.log)
  and [after](../../build/input-debug/after.log). This measures the shared
  framebuffer, not physical display refresh or VS Code UI latency.
- The session regression additionally types a command at 40 characters/second
  and checks its exact output file. It requires median echo below 50 ms and
  each echo below 200 ms. Raw timings are retained in
  [session state](../../build/sample_session_test/state.json).

See the [latest full-suite report](LATEST.md) for final results and
[focused checks](../../build/input-debug/focused.log) for this change.

## Actions

Restart F5 to load the rebuilt samples and libraries. Keep calculator and
terminal input checks in `make tests`; extend calculator coverage with
keyboard equivalence and arithmetic edge cases when those behaviors change.

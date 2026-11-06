# Gemscape browser

Gemscape embeds CEF/Chromium in a native GEM window. Launch it from Desktop
inside `make run` or the F5 session. Builds and tests use Wischner Docker.

## Navigation and input

The toolbar provides Back, Forward, Reload/Stop, the address field and the
three-line Settings button. Unavailable history buttons are dimmed; Reload
becomes Stop while loading. The title and address follow page navigation and
redirects. The status line reports loading, errors and data-clearing results.

Clicking an unfocused address selects it. Subsequent clicks position the caret;
dragging or Shift+arrow extends the selection. Home, End, Backspace, Delete and
Ctrl+A work. Enter navigates, Escape restores the current address, and Ctrl+L
focuses the field. Addresses without a scheme use HTTPS, except localhost and
127.x loopback addresses, which use HTTP. Explicit HTTP, HTTPS, file URLs and
`about:blank` are supported. Spaces and control characters are rejected.

Ctrl+R/F5 reload, Alt+Left/Right traverse history, and Alt+Home opens the saved
home page. Web controls receive pointer press/release, drag, hover, modifiers,
double-clicks and keyboard events. Select dropdowns are painted above the page.
Website alerts, confirmations and prompts use native GEM dialogs; Escape cancels
confirmation/prompt dialogs. User-initiated links requesting another window
open in the current browser; unsolicited popup windows are suppressed.

Ctrl+C/X/V share a bounded plain-text clipboard between the address field and
page selections. This is a browser-session clipboard, not integration with the
host desktop clipboard. The GEM address/prompt editor accepts printable ASCII;
use percent-encoded URLs for non-ASCII address characters.

Both native GEM scrollbars track Chromium's CSS viewport and document dimensions.
Line/page actions and thumb dragging scroll the page; page scripts, dynamic
content and zoom update the thumb sizes and positions. Nested scrollable web
controls retain their own scrollbars. The initial window fits the GEM desktop;
maximizing and resizing also resize the Chromium viewport.

## Settings

The three-line button opens a native settings window with three tabs. Browser
rendering continues while it is open. Save commits the draft; Cancel or the
close box discards it.

| Tab | Options |
| --- | --- |
| General | Home page; zoom from 50% to 200% |
| Privacy | Persistent or memory-only profile; allow cookies; restore session cookies; clear cookies, HTTP cache, or current-site data |
| Advanced | JavaScript, images, and rendering rate of 15, 30 or 60 frames/second |

Zoom applies on Save. Other options apply on the next browser launch. Clearing
data requires confirmation and reports success only after CEF acknowledges it.
Clear cookies signs out websites but preserves other site storage. Clear cache
removes HTTP cache entries. Clear this site removes the current HTTP/HTTPS
origin's cookies and stored data, including local storage and IndexedDB.

Persistent cookies, local storage and cache survive browser and Docker-session
restarts. Session cookies are not restored unless explicitly enabled. Memory
mode uses an incognito CEF context and loses its browsing data on exit. Switching
to memory mode does **not** delete a previously saved disk profile. CEF can still
write installation metadata and diagnostics in this mode.

Preferences use the `gemscape` component in the
[configuration system](CONFIGURATION.md):

```ini
[browser]
homepage=https://www.wikipedia.org/
zoom=100
[privacy]
persistent=true
cookies=true
session_cookies=false
[advanced]
javascript=true
images=true
frame_rate=30
```

Configuration uses `$XDG_CONFIG_HOME/gem/gemscape.cfg` (default
`~/.config/gem/gemscape.cfg`). Profiles use `$XDG_DATA_HOME/gem/gemscape`,
falling back to `~/.local/share/gem/gemscape`; HTTP cache uses
`$XDG_CACHE_HOME/gem/gemscape` and logs use `$XDG_STATE_HOME/gem/gemscape`.
Docker shares these host directories and `make stop` preserves them.
`GEMSCAPE_PROFILE` selects a separate absolute profile with its own cache/log.
See [Linux paths](LINUX_PATHS.md) for migration and installation.
Packaged CEF resources are stored once beside its private libraries. The UI
locale defaults to `en-US`; `GEM_CEF_LOCALE` selects another at build time and
the package includes that matching locale. See [packaging](PACKAGING.md).

Normal launches log only fatal CEF diagnostics;
Gemscape still reports startup and page-load errors. To enable detailed CEF
diagnostics, start a new Docker runtime with `GEMSCAPE_DEBUG=1 make run`
(use `make stop` first if one is already running). Detailed messages are written
to the state directory's `cef.log` (or the explicit profile override) and Chromium may also print them to the terminal.
`GEMSCAPE_URL` overrides the initial page for controlled launches/tests.

One browser process owns each profile. An advisory lock prevents concurrent
owners; stale Chromium singleton links from a killed container are recovered
only after this lock has been acquired. Close the window normally to flush
profile data. Tests use separate profiles and configuration directories.

## Coverage and limits

`uat_gemscape_proxy` covers framebuffer rendering, complete punctuation input,
navigation, activation and scrolling. `uat_gemscape_browser` uses a loopback HTTP
server to check actual DOM events, address selection, settings, profile modes,
cookies, cache and site-data operations. `test_browser_preferences` covers edit
boundaries and preference validation. Run `make tests` for the complete report.

This remains an embedded browser: downloads/file-picker integration, permission
management, password management, browser tabs and extensions are not provided.
The hosted CEF process still uses its existing `no_sandbox` configuration.
Docker mounts the development checkout writable and is not a security boundary
for hostile web content; see [security and safety](../architecture/SECURITY.md).
The separate `gemscape_hosted` sample provides the GEM frame without CEF.

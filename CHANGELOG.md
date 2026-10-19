# Change log

Significant user-facing changes to GEM for Linux are recorded here. The project
does not yet assign release version numbers, so current work remains under
Unreleased.

## Unreleased

- F5 now builds with the `wischner/gcc-x86_64-linux-sdl:1.3.0` Docker
  toolchain, keeping executables in `bin/` for the host debugger. The container
  helper accepts image, build-directory and output-directory overrides.

- Added `make rasta` to prepare the cached Rasta viewer separately. Normal
  builds now recheck its outputs incrementally and restore missing binaries
  before running tests, without repeating the download.

_Updated 2026-09-25._

- Clean GCC builds now complete without project, test or downloaded-dependency
  warnings; bounded text copies, file-selector history, window-title geometry,
  font names and child-shell directory changes are checked explicitly.

- CEF is now a pinned, checksum-verified build dependency cached under
  `build/deps` instead of a 1.5 GiB extracted binary tree in the repository.

- Gemscape CEF overlays now follow AES window stacking: they are hidden while
  another GEM window is on top and restored when Gemscape is raised. Overlay
  movement repaints the old location, and CEF is notified about content-size
  changes during resize.
- Gemscape hides the browser's native scrollbar and keeps the GEM frame and
  status bands outside the browser content area.
- Gemscape now preserves every printable punctuation key when forwarding GEM
  keyboard input to CEF, including period, and keeps the software GEM pointer
  visible above opaque browser pixels.

![What's new: GEM File Manager and Grok running in the VT100 Terminal](docs/images/screenshots/gem_desktop_2026_09_12.png)

### Added

- Added bidirectional classic `DESKTOP.INF` compatibility to `shel_get()` and
  `shel_put()`. Legacy programs receive a generated DRI/FreeGEM view while
  locations, window state, preferences, accessories, application/file
  associations and unknown records persist as structured `desktop.cfg` data.
  gemd reloads direct `libgemconf` saves and refreshes Desktop; concurrent
  legacy translations are serialized and no real `DESKTOP.INF` is created.

- Moved Gemscape into the bundled applications directory and added Gemread, a
  read-only PDF, XPS/OpenXPS, EPUB, MOBI, FB2, CBZ and CBT viewer with a GEM
  File/Open menu and standard window sliders. Gemread now has a color BGRA
  document surface, fit-to-window resizing, zoom controls, a status bar and a
  clickable multipage thumbnail rail. MuPDF is downloaded once into ignored
  `build/deps` storage and reused by subsequent builds. Gemread is licensed
  AGPL-3.0-or-later, separately from the MIT-licensed GEM core.
- Added reusable application toolbar/status-band layout and drawing controls
  to `libgemctl`; Gemscape and Gemread use the same geometry and rendering.

- Routed Stout, BIOS disk-image loading, PRG/MSA loaders, GEM configuration
  paths, VDI binary resources and font-directory enumeration through the
  platform OS HAL. Host-specific filesystem calls now remain in the platform
  backend.
- Stout now accepts `.PRG`, `.TOS`, `.TTP`, `.APP`, `.GTP` and `.ACC` images,
  builds a TOS basepage and command tail, searches platform accessory paths,
  and starts or reuses the GEM Terminal for guest console input/output.
- Added platform `AUTO` startup search locations alongside accessory paths;
  bare startup-program names can resolve from configured `auto/` directories.
- Split Stout's BIOS, XBIOS and GEMDOS services into `lib/stout/tos/` and
  added GEMDOS console, drive, DTA/search, file-handle, memory, directory,
  rename/delete and process-termination services.
- Added writable raw-ST drive support to Stout. GEMDOS file create/open,
  read/write/seek/close, delete, rename and directory operations now persist
  through both FAT12 copies with atomic image replacement. Relative paths stay
  on the drive containing the launched program, and the Tempus and Outside
  self-extractors now unpack nested files and original timestamps back into a
  mounted A: image.
- Fixed the file selector scrollbar thumb and drag/track interaction.
- Replaced the file selector's custom scrollbar with the standard GEM window
  scrollbar (`UPARROW`, `DNARROW`, `VSLIDE`) in all selector modes.
- Extended Stout's mounted-drive services with BIOS `Getbpb`, `Rwabs`,
  `Mediach`, `Drvmap` and XBIOS `Floprd` and `Flopwr`
  handling. F5 now launches the guest with the bundled MSA mounted.
- Added a GEM Terminal console bridge for Stout BIOS/GEMDOS stdin and stdout.
- Fixed disk-image location selection so the selected `.ST`/`.MSA` filename is
  retained instead of saving only its containing directory.

- Moved the reusable FAT12, MSA, PRG, Musashi and Stout implementations into
  `lib/`; `libstout` provides shared writable raw-ST and read-only MSA access.
- Added configured `.ST`/`.MSA` locations. The selector accepts either a
  directory or disk image, images use the disk icon and FAT12 browser, and
  Stout services BIOS `Rwabs` and XBIOS `Floprd` reads from a mounted image.

- Desktop folder icons now come only from configured `[locations]` entries;
  mounted volumes, root and the automatic Workspace folder are no longer added.

- Added the always-available `Add Location...` and `Remove Location...`
  system-menu commands. Remove operates on the selected configured folder
  icon, and is separated from Shutdown.
- Moved configured-location ownership into gemd. Desktop publishes the
  selected location, while gemd handles location RPCs, persistence, Delete-key
  confirmation and refresh notifications.
- Added statically linked `libgemconf`, a small XDG/INI configuration API for
  independent files such as `gem.cfg`, `desktop.cfg` and `clock.cfg`. Desktop
  settings control mounted volumes and Trash, while `[locations]`
  entries provide persistent folder shortcuts.
- Added modern GEM-styled open, save and directory selectors with Locations,
  path/history navigation, file type and size columns, filename editing and
  save-overwrite confirmation. Existing `fsel_input()` callers keep the same
  ABI, and `fsel_exinput()` now opens the same selector with its requested
  title; `fsel_save()` and `fsel_directory()` expose the additional modes.

- Implemented a Desktop Trash Can following the freedesktop Linux Trash
  convention. Home-volume items use `$XDG_DATA_HOME/Trash` or
  `$HOME/.local/share/Trash`; other volumes use `.Trash-$UID`. Each item has a
  percent-encoded `.trashinfo` record containing its original path and deletion
  time.
- Added moving files and non-empty directories to Trash, restoring them to
  their original locations, collision-safe restore names, permanent deletion
  and confirmed Empty Trash. Cross-filesystem moves use the portable OS layer
  and preserve symbolic-link containment.
- Implemented a DEC VT100-compatible Terminal parser and screen model. It
  supports cursor addressing, erase and insertion operations, scrolling
  regions, origin and autowrap modes, tabs, saved cursor state, primary and
  alternate screens, DEC line drawing, device/status reports, and bold,
  underline and reverse-video attributes.
- Added terminal input sequences for cursor, editing and function keys, plus
  application cursor-key mode. The PTY is resized with the GEM window and
  advertises `TERM=vt100` to child programs.
- Added safe handling for modern terminal control strings and UTF-8 box-drawing
  fallbacks. Host VTE, Kitty and color-terminal variables are removed from the
  child environment so applications do not enable capabilities GEM Terminal
  did not advertise.

### Changed

- The VS Code F5 Rasta session now opens a Full HD 1920×1080 GEM desktop and
  hides the Linux cursor over Rasta so only GEM's cursor is visible.
- F5 startup now waits for gemd to complete the Full HD Desktop redraw before
  launching Terminal, preventing an intermittent startup shutdown caused by
  Terminal's connection timeout.
- Added presenter-only BGRA overlay transport and compositing for Rasta and
  Linux. Gemscape now exercises a chunked color content pane without changing
  GEM/VDI monochrome surfaces; Rasta runs display-ready BGRA32 with inverse
  disabled.

- Switched the hosted Rasta backend to its 32-bit BGRA framebuffer mode. VDI
  remains monochrome: its packed shadow is expanded to opaque black/white
  pixels, leaving room for later CEF/Chrome content without changing GEM color
  semantics.

- Expanded File Manager list view with Name, Date, Size and Type columns, a
  fixed header and a file/folder-count status bar. Its
  list text uses the GEM system font; icon captions retain the small icon font.
- Normal F5 startup now launches only Desktop and Terminal. The all-samples
  session remains an explicit integration test.
- Fixed File Manager single-click handling so a second click can open a
  directory, and added launch support for `.TOS` and the other Atari program
  suffixes. Programs inside mounted `.ST`/`.MSA` images are now passed to
  Stout with the image mounted as drive A:.
- Fixed raw Atari ST floppy images whose BPB uses the native zero-reserved-
  sector convention; they now open correctly in File Manager.
- Implemented XBIOS `Supexec` (opcode 38), allowing mounted-drive programs
  such as GNU Chess to execute supervisor callbacks under Stout.
- Guarded `Supexec` callback targets so invalid program pointers cannot jump
  into the TOS basepage and get reported as illegal 68000 instructions.
- Promoted Stout from the independent samples to `src/apps/`; it now builds as
  `bin/apps/stout` and is included in the native Gemix package. Interactive
  guests run without an arbitrary cycle cutoff by default, while
  `STOUT_MAX_CYCLES` can still bound diagnostic runs.
- Made Stout's Musashi callback routing explicit for GEMDOS trap #1, AES/VDI
  trap #2, BIOS trap #13 and XBIOS trap #14, with real guest-vector regression
  coverage for each route.
- Fixed Stout's TOS basepage field layout and initial stack argument, preserved
  the caller-owned trap stack, and ended Musashi's current timeslice on process
  termination. Relocatable self-extracting `.TOS` startup code now runs without
  corrupting its return stack.
- Promoted Desktop, Terminal, Clock and Calculator from independent samples to
  bundled applications under `src/apps/`; their integrated binaries now live
  in `bin/apps/`.
- Popup items use balanced left and right margins with a dedicated checkmark
  gutter.

### Fixed

- Custom `form_dial`/`form_do` dialogs are now modal only to their owning
  application. Other applications can receive input and continue issuing
  drawing and event requests; recursive polling of an already active request
  is guarded, fixing the gemd crash triggered by starting another application
  while QIndex 1.6 was open. Standard alerts and file selectors remain global
  synchronous panels. Custom forms also participate in the shared z-order and
  retain their live tree while `form_do` waits, so a covering window clips the
  form and closing that window reconstructs the exposed form exactly.
- Stout's legacy TEDINFO bridge now follows classic AES object geometry:
  unboxed text begins at the declared x-coordinate, boxed text uses its border
  thickness, and only whole characters that fit the inner rectangle are drawn.
  This fixes QIndex's exact-width percentage fields without changing native
  hosted applications.
- Stout's AES `graf_handle` adapter now reports the Atari high-resolution
  8x16 character cell without changing hosted AES metrics. TEDINFO objects
  honor the Atari 6x6 `SMALL` metrics in embedded resources such as QIndex 1.6,
  while Desktop and File Manager icon labels retain their established unscaled
  `SMALL` face. Stout `form_center` also reserves the 19-pixel high-resolution
  menu strip instead of placing the top of tall dialogs underneath it.
- The Rasta VDI startup handshake now waits for subscriber-driven framebuffer
  reconfiguration before its first remap and present, closing an intermittent
  gemd SIGBUS race during Desktop startup.
- Stout now distinguishes launched applications from desk accessories through
  the basepage parent field. Added the BIOS `Setexc`, XBIOS `Setpalette`,
  `Setcolor`, `Keytbl`, `Setprt` and `Blitmode`, VDI text-attribute and
  `vq_gdos` probes, plus AES `appl_find`, `objc_change` and `shel_find` used by
  QIndex and GEMBench; standard XBIOS floppy argument offsets are now decoded
  correctly. QIndex reaches and responds to its dialog, GEMBench no longer
  falls into the illegal-opcode path, and GMap's Quit flow completes after its
  own final information dialog is dismissed.
- Stout now starts mounted-disk programs in their containing A: directory and
  implements A: `Dsetpath`, `Dgetpath`, `Fsfirst` and `Fsnext`, so programs on
  the Delta and QIndex disks can find relative resources and enumerate sibling
  files. Added classic AES event/resource calls, big-endian `.RSC` relocation,
  AES global resource pointers, GEMDOS `Super`, and the common VDI text calls
  those programs use. GUI guests no longer open a Terminal on startup, and a
  guest `appl_exit` no longer terminates gemd or destabilizes the next client.
- File Manager now applies ordered `desktop.cfg` associations through the real
  AES `shel_write()` command-tail path. `.PRG`, `.TOS`, `.TTP`, `.APP`, `.GTP`
  and `.ACC` default to Stout, with mounted programs receiving both their A:
  path and source image. Classic `form_dial` applications retain input inside
  their dialog, remain above the launching File Manager, and restore the
  covered area if the guest exits unexpectedly without blocking unrelated
  applications.
- Stout now skips optional linker-private symbol records it does not decode,
  while still locating and validating the declared relocation stream. BIOS
  `Kbshift` and XBIOS `Vsync` are routed for programs that poll keyboard state
  once per display interval.
- Stout now runs the extracted TEMPUS demo past startup instead of stopping at
  unsupported XBIOS opcode 3. Added `Physbase`, `Logbase`, `Getrez`,
  `Setscreen`, `Iorec`, `Kbdvbase`, high-resolution Line-A state, classic
  cursor/input-vector VDI calls, endian-safe MFDB raster copies and the AES
  menu/form/object calls used by the demo. BIOS `Drvmap` now reports drive C:
  even when no floppy image is mounted, and AES/VDI dispatch honors the
  low-word trap selector used by 68000 callers.
- Host-launched Stout programs now use their source directory as drive C:
  instead of an empty `.TOS.files` sidecar, and host `Fcreate` securely creates
  missing parent directories. STZip self-extracting `.TOS` files can therefore
  unpack their nested contents after being copied out of a mounted disk. Stout
  now also attaches plain TOS programs to GEM Terminal before execution and
  falls back to standard output when no Terminal is available.
- Stout now places executable text at the mandatory `basepage + 0x100` address
  and applies `Mshrink` to its guest allocator. This prevents STZip's archive
  buffers from being based on an inconsistent process-memory layout.
- Preserved the location-list RPC wire size while adding Desktop preferences,
  preventing a mixed-version Desktop/gemd pair from disconnecting during
  startup.
- Added the `desktop.cfg` `show_hidden` preference, defaulting to false, and
  applied it consistently to Linux, mounted-image, Trash and Add Location file
  lists. Mounted Atari ST images can now be drag-copy sources for files and
  recursive directory trees; they remain read-only for Move and Trash.
- Removed the File Manager's dotted list-row separators. Selected-item drags
  now take precedence over double-click activation once the pointer crosses
  the drag threshold, restoring drag-to-Trash and cross-window drops after an
  item has first been selected. Directory entry paths are preserved before a
  new snapshot replaces the entry array, so navigation no longer stops after
  a few nested folders; mounted-image navigation uses the same safeguard.
- File Manager drag-and-drop now accepts normal filesystem windows as
  destinations instead of recognizing only Trash. Dropping on a folder or a
  window background copies immediately; holding Control moves instead, and
  Shift-click toggles multi-selection. Control-move adds an `M` badge to the
  live drag contour over normal destinations but never over Trash. Recursive
  directory copies do not follow symlinks, collisions are preserved, and every
  affected open window refreshes after a successful operation.
- Fixed Gemread File/Open path assembly so a selected filename replaces the
  selector wildcard instead of being appended after it. Gemread now exposes a
  standard resize handle and clamps its scroll position after resizing.
- Fixed USB-HID scan codes for printable `y`, `k` and `l` being mistaken for
  legacy Enter, Backspace and Tab controls in editable text. File selectors
  now omit dot/OS-hidden entries by default; `fsel_options()` can opt in to
  showing them.
- Gemscape now gives keyboard focus to clicked browser content and forwards
  key events to CEF, while address-bar editing remains active only after an
  address-bar click.
- Fixed Gemread's rejected menu tree, blank first frame, off-screen initial
  scrollbar and swapped MFDB byte order. The File/Open menu, PDF rendering,
  arrow/page scrolling and slider position now have direct/proxy UAT coverage.
- Fixed Gemscape's browser event pump so web buttons, address editing (including
  `y`), Enter navigation and toolbar buttons continue working after the first
  frame. GEM sliders now track and scroll the real web document dimensions.
- Clicking an inactive GEM window's title now runs the complete top-window
  path, restores its client/CEF overlay and requests the exposed repaint.
- Native overlays now recompute occlusion whenever GEM windows open, close,
  move, resize, iconify or change stacking order, and remain hidden while
  their owning window is closed.
- Gemscape now waits for CEF's browser-close callback before global shutdown
  and uses Chromium's direct-exec helper mode to avoid its Linux zygote
  stack-guard failure.
- File selectors now allow typing and editing the toolbar directory path,
  including in Select Folder mode.
- Added the reusable `libgemctl` control library and moved selector text-edit
  behavior into its generic text-edit control.
- Fixed Enter navigation from the selector path field and added an I-beam
  cursor over editable selector fields.
- File Manager drag/drop now distinguishes stationary double-clicks from
  drags, draws the classic icon/title union outline and inverts the Trash title
  while a valid drop remains held over it.
- Successful Trash moves and restores refresh every open File Manager viewing
  an affected directory. Empty Trash remains enabled whenever Trash contains
  items, regardless of which File Manager window is active.

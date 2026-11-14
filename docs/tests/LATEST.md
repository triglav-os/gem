# Automated GEM test report

UTC: 2026-11-14T10:17:07+00:00 to 2026-11-14T10:20:26+00:00

**106 passed, 12 failed, 0 skipped; CTest exit status 8.**

Command: `ctest --test-dir /home/tomaz/repos/triglav-os/gem/build/container-sdl --output-on-failure --parallel 4 --output-junit /home/tomaz/repos/triglav-os/gem/build/container-sdl/test-results.xml`

Build: GCC RelWithDebInfo, Rasta platform; UAT rejects AddressSanitizer and UndefinedBehaviorSanitizer diagnostics.

[Complete execution log](../../build/container-sdl/tests.log) · [Machine-readable JUnit results](../../build/container-sdl/test-results.xml)

## Coverage

Unit checks cover public headers, API exports, VDI behavior and malformed transport data. Integration checks exercise real gemd connections, session isolation, AES trees, resources, reverse USERDEF callbacks and bitmap transfer. Security regressions exercise hostile FAT/MSA extraction, guest VDI argument bounds, disk-sector overflow and SoftFloat shift boundaries. The full sample session checks repeated Stout drags, repaint of an enlarged Stout window beyond 640x400, Terminal movement and closure, framebuffer changes and independent RPC responsiveness. It measures terminal echo latency and checks a real command typed at 40 characters/second. Both calculator variants check visible buttons, digit entry, clear and 7 + 2 = 9. Installed-layout tests check minimal file allowlists, stripped ELF dependencies, unique CEF resources and relocated application startup; the SDK check builds independent samples against a relocated export. XDG path and migration tests check private runtime storage and preservation of existing settings. Both desktop variants verify location, Files, Gemscape, Terminal and Trash icons, open the file manager, navigate into a fixture directory and back, and close it. They check the global Locations command and directory chooser, compact Desk menus, sliding between the system menu and Desk, File Open and PDF/Ted associations. File command scenarios drag desktop icons onto the grid and keep vacated cells empty, add a document to the desktop, and check popup borders, centered movable dialogs, standard window borders without extra inner frames, right-click menus, creation templates, rename, clipboard transfers and Trash metadata. Ted checks real-file editing, selection, copy/paste, undo, save, compact toolbar actions, resized icon hit targets and cancellation of an unsaved close; a Markdown run checks bold headings and the I-beam pointer over text. Separate Gemread scenarios verify its shared toolbar/status chrome, color BGRA PDF pages, zoom, responsive resize, multipage thumbnails, Go to Page, tabbed settings and preference restoration across restart; Gemscape verifies complete punctuation input, browser focus, GEM cursor visibility above CEF, navigation, redraw and scrollbar synchronization. Its HTTP browser scenarios verify form controls, address caret/selection, clipboard, native website dialogs, both scrollbar endpoints, tabbed settings, dialog occlusion over animation, persistent/private profiles, cookie policy, cache/site-data clearing, and JavaScript/image preferences across restarts. Separate Trash scenarios exercise icon-view double-clicks, an upside-down-T XOR icon/title contour, live Trash-title inversion before button-up, file and non-empty-folder moves, plus cancel-default and confirmed Empty Trash in direct and proxy modes. They also force a failed move and immediately open another folder. The Trash unit test covers restore, collision handling, symlink-safe purge, same-volume store selection and cross-filesystem directory moves. The full sample session also opens Workspace through its exposed label while all applications are running. It holds the system menu open across a partial RPC frame and verifies that the Shutdown alert preserves all windows and client connections through a seven-second alert wait. All 34 demos run through Rasta in direct and proxy modes. A separate startup regression verifies Rasta adopts GEM's framebuffer path, geometry, BGRA depth and inverse mode before window input. It also delays configuration until after GEM draws and repeats it, checking preserved scene pixels and every alpha byte with matching and mismatched initial viewer settings. UAT checks framebuffer references and scripted input; terminal cases verify a real shell command by reading its output file. The SDL viewer uses its dummy video driver unless UAT_SDL_DRIVER is set.

Passing these cases establishes the documented coverage, not every possible input or historical Atari device extension. See [transport scope](../architecture/API_TRANSPORT.md) and [demo scenarios](UAT.md).

## Results

| Test | Result | Seconds | Evidence |
| --- | --- | ---: | --- |
| test_report | PASS | 0.05 | [CTest log](../../build/container-sdl/tests.log) |
| test_vdi | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_header | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_api_surface | PASS | 0.08 | [CTest log](../../build/container-sdl/tests.log) |
| test_protocol | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_aes_shell | PASS | 0.30 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_trash | PASS | 0.04 | [CTest log](../../build/container-sdl/tests.log) |
| test_terminal_vt100 | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemconf | PASS | 0.01 | [CTest log](../../build/container-sdl/tests.log) |
| test_browser_preferences | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_prg | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_security_bounds | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_paths | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_ted_document | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_desktop_icon_rsc | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_desktop_icon_lookup | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_rasta_polarity | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_rasta_update | PASS | 3.93 | [CTest log](../../build/container-sdl/tests.log) |
| test_container_identity | PASS | 0.04 | [CTest log](../../build/container-sdl/tests.log) |
| test_container_lock | PASS | 0.30 | [CTest log](../../build/container-sdl/tests.log) |
| test_rasta_startup | PASS | 9.96 | [CTest log](../../build/container-sdl/tests.log) |
| test_runtime_paths | PASS | 0.07 | [CTest log](../../build/container-sdl/tests.log) |
| test_install_layout | PASS | 23.16 | [CTest log](../../build/container-sdl/tests.log) |
| test_sdk_layout | PASS | 1.28 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_rpc | PASS | 0.21 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemd_security | PASS | 6.44 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemd_host | PASS | 0.28 | [CTest log](../../build/container-sdl/tests.log) |
| test_vdi_rpc | PASS | 0.22 | [CTest log](../../build/container-sdl/tests.log) |
| test_aes_rpc | PASS | 0.34 | [CTest log](../../build/container-sdl/tests.log) |
| test_menu_limits | PASS | 0.23 | [CTest log](../../build/container-sdl/tests.log) |
| test_sample_session | PASS | 40.51 | [Session state](../../build/container-sdl/sample_session_test/state.json) |
| security_disk | PASS | 0.05 | [CTest log](../../build/container-sdl/tests.log) |
| security_guest | PASS | 0.15 | [CTest log](../../build/container-sdl/tests.log) |
| uat_demo1_direct | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo1_direct/result.json) |
| uat_demo1_proxy | PASS | 2.01 | [Artifacts](../../build/container-sdl/uat/demo1_proxy/result.json) |
| uat_demo2_direct | PASS | 1.95 | [Artifacts](../../build/container-sdl/uat/demo2_direct/result.json) |
| uat_demo2_proxy | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo2_proxy/result.json) |
| uat_demo3_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo3_direct/result.json) |
| uat_demo3_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo3_proxy/result.json) |
| uat_demo4_direct | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo4_direct/result.json) |
| uat_demo4_proxy | PASS | 2.02 | [Artifacts](../../build/container-sdl/uat/demo4_proxy/result.json) |
| uat_demo5_direct | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo5_direct/result.json) |
| uat_demo5_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo5_proxy/result.json) |
| uat_demo6_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo6_direct/result.json) |
| uat_demo6_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo6_proxy/result.json) |
| uat_demo7_direct | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo7_direct/result.json) |
| uat_demo7_proxy | PASS | 2.02 | [Artifacts](../../build/container-sdl/uat/demo7_proxy/result.json) |
| uat_demo8_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo8_direct/result.json) |
| uat_demo8_proxy | PASS | 2.01 | [Artifacts](../../build/container-sdl/uat/demo8_proxy/result.json) |
| uat_demo9_direct | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo9_direct/result.json) |
| uat_demo9_proxy | PASS | 2.03 | [Artifacts](../../build/container-sdl/uat/demo9_proxy/result.json) |
| uat_demo10_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo10_direct/result.json) |
| uat_demo10_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo10_proxy/result.json) |
| uat_demo11_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo11_direct/result.json) |
| uat_demo11_proxy | PASS | 2.01 | [Artifacts](../../build/container-sdl/uat/demo11_proxy/result.json) |
| uat_demo12_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo12_direct/result.json) |
| uat_demo12_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo12_proxy/result.json) |
| uat_demo13_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo13_direct/result.json) |
| uat_demo13_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo13_proxy/result.json) |
| uat_demo14_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo14_direct/result.json) |
| uat_demo14_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo14_proxy/result.json) |
| uat_demo15_direct | PASS | 1.95 | [Artifacts](../../build/container-sdl/uat/demo15_direct/result.json) |
| uat_demo15_proxy | PASS | 2.00 | [Artifacts](../../build/container-sdl/uat/demo15_proxy/result.json) |
| uat_demo16_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo16_direct/result.json) |
| uat_demo16_proxy | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo16_proxy/result.json) |
| uat_demo17_direct | PASS | 2.73 | [Artifacts](../../build/container-sdl/uat/demo17_direct/result.json) |
| uat_demo17_proxy | PASS | 2.78 | [Artifacts](../../build/container-sdl/uat/demo17_proxy/result.json) |
| uat_demo18_direct | PASS | 2.60 | [Artifacts](../../build/container-sdl/uat/demo18_direct/result.json) |
| uat_demo18_proxy | PASS | 5.72 | [Artifacts](../../build/container-sdl/uat/demo18_proxy/result.json) |
| uat_demo19_direct | PASS | 19.53 | [Artifacts](../../build/container-sdl/uat/demo19_direct/result.json) |
| uat_demo19_proxy | PASS | 19.54 | [Artifacts](../../build/container-sdl/uat/demo19_proxy/result.json) |
| uat_demo20_direct | FAIL | 1.69 | [Artifacts](../../build/container-sdl/uat/demo20_direct/result.json) |
| uat_demo20_proxy | FAIL | 1.69 | [Artifacts](../../build/container-sdl/uat/demo20_proxy/result.json) |
| uat_demo21_direct | FAIL | 1.70 | [Artifacts](../../build/container-sdl/uat/demo21_direct/result.json) |
| uat_demo21_proxy | FAIL | 1.71 | [Artifacts](../../build/container-sdl/uat/demo21_proxy/result.json) |
| uat_demo22_direct | FAIL | 1.69 | [Artifacts](../../build/container-sdl/uat/demo22_direct/result.json) |
| uat_demo22_proxy | FAIL | 1.71 | [Artifacts](../../build/container-sdl/uat/demo22_proxy/result.json) |
| uat_demo23_direct | FAIL | 1.70 | [Artifacts](../../build/container-sdl/uat/demo23_direct/result.json) |
| uat_demo23_proxy | FAIL | 1.70 | [Artifacts](../../build/container-sdl/uat/demo23_proxy/result.json) |
| uat_demo24_direct | PASS | 3.46 | [Artifacts](../../build/container-sdl/uat/demo24_direct/result.json) |
| uat_demo24_proxy | PASS | 3.50 | [Artifacts](../../build/container-sdl/uat/demo24_proxy/result.json) |
| uat_demo25_direct | FAIL | 1.70 | [Artifacts](../../build/container-sdl/uat/demo25_direct/result.json) |
| uat_demo25_proxy | FAIL | 1.71 | [Artifacts](../../build/container-sdl/uat/demo25_proxy/result.json) |
| uat_demo26_direct | FAIL | 1.66 | [Artifacts](../../build/container-sdl/uat/demo26_direct/result.json) |
| uat_demo26_proxy | FAIL | 1.72 | [Artifacts](../../build/container-sdl/uat/demo26_proxy/result.json) |
| uat_demo27_direct | PASS | 2.02 | [Artifacts](../../build/container-sdl/uat/demo27_direct/result.json) |
| uat_demo27_proxy | PASS | 2.01 | [Artifacts](../../build/container-sdl/uat/demo27_proxy/result.json) |
| uat_demo28_direct | PASS | 2.72 | [Artifacts](../../build/container-sdl/uat/demo28_direct/result.json) |
| uat_demo28_proxy | PASS | 2.76 | [Artifacts](../../build/container-sdl/uat/demo28_proxy/result.json) |
| uat_demo29_direct | PASS | 2.74 | [Artifacts](../../build/container-sdl/uat/demo29_direct/result.json) |
| uat_demo29_proxy | PASS | 2.79 | [Artifacts](../../build/container-sdl/uat/demo29_proxy/result.json) |
| uat_demo30_direct | PASS | 2.71 | [Artifacts](../../build/container-sdl/uat/demo30_direct/result.json) |
| uat_demo30_proxy | PASS | 2.76 | [Artifacts](../../build/container-sdl/uat/demo30_proxy/result.json) |
| uat_demo31_direct | PASS | 14.94 | [Artifacts](../../build/container-sdl/uat/demo31_direct/result.json) |
| uat_demo31_proxy | PASS | 14.96 | [Artifacts](../../build/container-sdl/uat/demo31_proxy/result.json) |
| uat_demo32_direct | PASS | 3.25 | [Artifacts](../../build/container-sdl/uat/demo32_direct/result.json) |
| uat_demo32_proxy | PASS | 3.27 | [Artifacts](../../build/container-sdl/uat/demo32_proxy/result.json) |
| uat_demo33_direct | PASS | 3.86 | [Artifacts](../../build/container-sdl/uat/demo33_direct/result.json) |
| uat_demo33_proxy | PASS | 3.92 | [Artifacts](../../build/container-sdl/uat/demo33_proxy/result.json) |
| uat_demo34_direct | PASS | 3.23 | [Artifacts](../../build/container-sdl/uat/demo34_direct/result.json) |
| uat_demo34_proxy | PASS | 3.25 | [Artifacts](../../build/container-sdl/uat/demo34_proxy/result.json) |
| uat_file_drop_direct | PASS | 19.16 | [Artifacts](../../build/container-sdl/uat/file_drop_direct/result.json) |
| uat_file_drop_proxy | PASS | 19.15 | [Artifacts](../../build/container-sdl/uat/file_drop_proxy/result.json) |
| uat_file_commands_direct | PASS | 44.23 | [Artifacts](../../build/container-sdl/uat/file_commands_direct/result.json) |
| uat_file_commands_proxy | PASS | 46.07 | [Artifacts](../../build/container-sdl/uat/file_commands_proxy/result.json) |
| uat_trash_direct | PASS | 20.25 | [Artifacts](../../build/container-sdl/uat/trash_direct/result.json) |
| uat_trash_proxy | PASS | 20.29 | [Artifacts](../../build/container-sdl/uat/trash_proxy/result.json) |
| uat_calc_direct | PASS | 5.52 | [Artifacts](../../build/container-sdl/uat/calc_direct/result.json) |
| uat_calc_proxy | PASS | 5.51 | [Artifacts](../../build/container-sdl/uat/calc_proxy/result.json) |
| uat_gemread_direct | PASS | 19.71 | [Artifacts](../../build/container-sdl/uat/gemread_direct/result.json) |
| uat_gemread_proxy | PASS | 19.77 | [Artifacts](../../build/container-sdl/uat/gemread_proxy/result.json) |
| uat_gemread_pdf_proxy | PASS | 9.81 | [Artifacts](../../build/container-sdl/uat/gemread_pdf_proxy/result.json) |
| uat_gemread_epub_proxy | PASS | 4.23 | [Artifacts](../../build/container-sdl/uat/gemread_epub_proxy/result.json) |
| uat_gemscape_proxy | PASS | 45.81 | [Artifacts](../../build/container-sdl/uat/gemscape_proxy/result.json) |
| uat_gemscape_browser | PASS | 122.97 | [Artifacts](../../build/container-sdl/uat/gemscape_browser/result.json) |
| uat_desktop_direct | PASS | 23.02 | [Artifacts](../../build/container-sdl/uat/desktop_direct/result.json) |
| uat_desktop_proxy | PASS | 23.00 | [Artifacts](../../build/container-sdl/uat/desktop_proxy/result.json) |
| uat_ted_proxy | PASS | 29.85 | [Artifacts](../../build/container-sdl/uat/ted_proxy/result.json) |

## Proposed actions

- **uat_demo20_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo20_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo21_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo21_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo22_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo22_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo23_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo23_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo25_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo25_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo26_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo26_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.

### uat_demo20_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo20_initial.pbm

```

### uat_demo20_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo20_initial.pbm

```

### uat_demo21_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo21_initial.pbm

```

### uat_demo21_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo21_initial.pbm

```

### uat_demo22_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo22_initial.pbm

```

### uat_demo22_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo22_initial.pbm

```

### uat_demo23_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo23_initial.pbm

```

### uat_demo23_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo23_initial.pbm

```

### uat_demo25_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo25_initial.pbm

```

### uat_demo25_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo25_initial.pbm

```

### uat_demo26_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo26_initial.pbm

```

### uat_demo26_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 256, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 242, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 192, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 102, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 42, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 39 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo26_initial.pbm

```

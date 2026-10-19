# Automated GEM test report

UTC: 2026-10-19T19:28:01+00:00 to 2026-10-19T19:29:51+00:00

**96 passed, 5 failed, 0 skipped; CTest exit status 8.**

Command: `ctest --test-dir /home/tomaz/repos/triglav-os/gem/build/container-sdl --output-on-failure --parallel 4 --output-junit /home/tomaz/repos/triglav-os/gem/build/container-sdl/test-results.xml`

Build: GCC RelWithDebInfo, Rasta platform; UAT rejects AddressSanitizer and UndefinedBehaviorSanitizer diagnostics.

[Complete execution log](../../build/container-sdl/tests.log) · [Machine-readable JUnit results](../../build/container-sdl/test-results.xml)

## Coverage

Unit checks cover public headers, API exports, VDI behavior and malformed transport data. Integration checks exercise real gemd connections, session isolation, AES trees, resources, reverse USERDEF callbacks and bitmap transfer. Security regressions exercise hostile FAT/MSA extraction, guest VDI argument bounds, disk-sector overflow and SoftFloat shift boundaries. The full sample session checks repeated Stout drags, Terminal movement and closure, framebuffer changes and independent RPC responsiveness. It measures terminal echo latency and checks a real command typed at 40 characters/second. Both calculator variants check visible buttons, digit entry, clear and 7 + 2 = 9. Both desktop variants verify disk, Workspace and Trash icons, open the file manager, navigate into a fixture directory and back, and close it. They check the global Locations command and directory chooser, compact Desk menus and File Open. Separate Gemread scenarios verify its shared toolbar/status chrome, color BGRA PDF pages, zoom, responsive resize and multipage thumbnail navigation; Gemscape verifies complete punctuation input, browser focus, GEM cursor visibility above CEF, navigation, redraw and scrollbar synchronization. Separate Trash scenarios exercise icon-view double-clicks, an upside-down-T XOR icon/title contour, live Trash-title inversion before button-up, file and non-empty-folder moves, plus cancel-default and confirmed Empty Trash in direct and proxy modes. They also force a failed move and immediately open another folder. The Trash unit test covers restore, collision handling, symlink-safe purge, same-volume store selection and cross-filesystem directory moves. The full sample session also opens Workspace through its exposed label while all applications are running. It holds the system menu open across a partial RPC frame and verifies that the Shutdown alert preserves all windows and client connections through a seven-second alert wait. All 34 demos run through Rasta in direct and proxy modes. UAT checks framebuffer references and scripted input; terminal cases verify a real shell command by reading its output file. The SDL viewer uses its dummy video driver unless UAT_SDL_DRIVER is set.

Passing these cases establishes the documented coverage, not every possible input or historical Atari device extension. See [transport scope](../architecture/API_TRANSPORT.md) and [demo scenarios](UAT.md).

## Results

| Test | Result | Seconds | Evidence |
| --- | --- | ---: | --- |
| test_report | PASS | 0.05 | [CTest log](../../build/container-sdl/tests.log) |
| test_vdi | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_header | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_api_surface | PASS | 0.08 | [CTest log](../../build/container-sdl/tests.log) |
| test_protocol | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_aes_shell | PASS | 0.33 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_trash | PASS | 0.04 | [CTest log](../../build/container-sdl/tests.log) |
| test_terminal_vt100 | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemconf | PASS | 0.01 | [CTest log](../../build/container-sdl/tests.log) |
| test_prg | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_security_bounds | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_rasta_polarity | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_gem_rpc | PASS | 0.22 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemd_security | PASS | 6.43 | [CTest log](../../build/container-sdl/tests.log) |
| test_gemd_host | PASS | 0.00 | [CTest log](../../build/container-sdl/tests.log) |
| test_vdi_rpc | PASS | 0.22 | [CTest log](../../build/container-sdl/tests.log) |
| test_aes_rpc | PASS | 0.34 | [CTest log](../../build/container-sdl/tests.log) |
| test_sample_session | PASS | 33.37 | [Session state](../../build/container-sdl/sample_session_test/state.json) |
| security_disk | PASS | 0.08 | [CTest log](../../build/container-sdl/tests.log) |
| security_guest | PASS | 0.16 | [CTest log](../../build/container-sdl/tests.log) |
| uat_demo1_direct | PASS | 1.93 | [Artifacts](../../build/container-sdl/uat/demo1_direct/result.json) |
| uat_demo1_proxy | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo1_proxy/result.json) |
| uat_demo2_direct | PASS | 1.93 | [Artifacts](../../build/container-sdl/uat/demo2_direct/result.json) |
| uat_demo2_proxy | PASS | 1.95 | [Artifacts](../../build/container-sdl/uat/demo2_proxy/result.json) |
| uat_demo3_direct | PASS | 1.95 | [Artifacts](../../build/container-sdl/uat/demo3_direct/result.json) |
| uat_demo3_proxy | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo3_proxy/result.json) |
| uat_demo4_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo4_direct/result.json) |
| uat_demo4_proxy | PASS | 2.00 | [Artifacts](../../build/container-sdl/uat/demo4_proxy/result.json) |
| uat_demo5_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo5_direct/result.json) |
| uat_demo5_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo5_proxy/result.json) |
| uat_demo6_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo6_direct/result.json) |
| uat_demo6_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo6_proxy/result.json) |
| uat_demo7_direct | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo7_direct/result.json) |
| uat_demo7_proxy | PASS | 2.00 | [Artifacts](../../build/container-sdl/uat/demo7_proxy/result.json) |
| uat_demo8_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo8_direct/result.json) |
| uat_demo8_proxy | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo8_proxy/result.json) |
| uat_demo9_direct | PASS | 1.94 | [Artifacts](../../build/container-sdl/uat/demo9_direct/result.json) |
| uat_demo9_proxy | PASS | 2.03 | [Artifacts](../../build/container-sdl/uat/demo9_proxy/result.json) |
| uat_demo10_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo10_direct/result.json) |
| uat_demo10_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo10_proxy/result.json) |
| uat_demo11_direct | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo11_direct/result.json) |
| uat_demo11_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo11_proxy/result.json) |
| uat_demo12_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo12_direct/result.json) |
| uat_demo12_proxy | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo12_proxy/result.json) |
| uat_demo13_direct | PASS | 1.96 | [Artifacts](../../build/container-sdl/uat/demo13_direct/result.json) |
| uat_demo13_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo13_proxy/result.json) |
| uat_demo14_direct | PASS | 1.99 | [Artifacts](../../build/container-sdl/uat/demo14_direct/result.json) |
| uat_demo14_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo14_proxy/result.json) |
| uat_demo15_direct | PASS | 1.94 | [Artifacts](../../build/container-sdl/uat/demo15_direct/result.json) |
| uat_demo15_proxy | PASS | 1.98 | [Artifacts](../../build/container-sdl/uat/demo15_proxy/result.json) |
| uat_demo16_direct | PASS | 1.95 | [Artifacts](../../build/container-sdl/uat/demo16_direct/result.json) |
| uat_demo16_proxy | PASS | 1.97 | [Artifacts](../../build/container-sdl/uat/demo16_proxy/result.json) |
| uat_demo17_direct | PASS | 2.74 | [Artifacts](../../build/container-sdl/uat/demo17_direct/result.json) |
| uat_demo17_proxy | PASS | 2.72 | [Artifacts](../../build/container-sdl/uat/demo17_proxy/result.json) |
| uat_demo18_direct | PASS | 2.61 | [Artifacts](../../build/container-sdl/uat/demo18_direct/result.json) |
| uat_demo18_proxy | PASS | 5.67 | [Artifacts](../../build/container-sdl/uat/demo18_proxy/result.json) |
| uat_demo19_direct | PASS | 19.53 | [Artifacts](../../build/container-sdl/uat/demo19_direct/result.json) |
| uat_demo19_proxy | PASS | 19.54 | [Artifacts](../../build/container-sdl/uat/demo19_proxy/result.json) |
| uat_demo20_direct | PASS | 2.83 | [Artifacts](../../build/container-sdl/uat/demo20_direct/result.json) |
| uat_demo20_proxy | PASS | 2.86 | [Artifacts](../../build/container-sdl/uat/demo20_proxy/result.json) |
| uat_demo21_direct | PASS | 3.58 | [Artifacts](../../build/container-sdl/uat/demo21_direct/result.json) |
| uat_demo21_proxy | PASS | 3.59 | [Artifacts](../../build/container-sdl/uat/demo21_proxy/result.json) |
| uat_demo22_direct | PASS | 2.78 | [Artifacts](../../build/container-sdl/uat/demo22_direct/result.json) |
| uat_demo22_proxy | PASS | 2.80 | [Artifacts](../../build/container-sdl/uat/demo22_proxy/result.json) |
| uat_demo23_direct | PASS | 2.77 | [Artifacts](../../build/container-sdl/uat/demo23_direct/result.json) |
| uat_demo23_proxy | PASS | 2.79 | [Artifacts](../../build/container-sdl/uat/demo23_proxy/result.json) |
| uat_demo24_direct | PASS | 3.46 | [Artifacts](../../build/container-sdl/uat/demo24_direct/result.json) |
| uat_demo24_proxy | PASS | 3.49 | [Artifacts](../../build/container-sdl/uat/demo24_proxy/result.json) |
| uat_demo25_direct | PASS | 3.79 | [Artifacts](../../build/container-sdl/uat/demo25_direct/result.json) |
| uat_demo25_proxy | PASS | 3.83 | [Artifacts](../../build/container-sdl/uat/demo25_proxy/result.json) |
| uat_demo26_direct | PASS | 3.51 | [Artifacts](../../build/container-sdl/uat/demo26_direct/result.json) |
| uat_demo26_proxy | PASS | 3.52 | [Artifacts](../../build/container-sdl/uat/demo26_proxy/result.json) |
| uat_demo27_direct | PASS | 2.00 | [Artifacts](../../build/container-sdl/uat/demo27_direct/result.json) |
| uat_demo27_proxy | PASS | 2.02 | [Artifacts](../../build/container-sdl/uat/demo27_proxy/result.json) |
| uat_demo28_direct | PASS | 2.75 | [Artifacts](../../build/container-sdl/uat/demo28_direct/result.json) |
| uat_demo28_proxy | PASS | 2.77 | [Artifacts](../../build/container-sdl/uat/demo28_proxy/result.json) |
| uat_demo29_direct | PASS | 2.71 | [Artifacts](../../build/container-sdl/uat/demo29_direct/result.json) |
| uat_demo29_proxy | PASS | 2.76 | [Artifacts](../../build/container-sdl/uat/demo29_proxy/result.json) |
| uat_demo30_direct | PASS | 2.74 | [Artifacts](../../build/container-sdl/uat/demo30_direct/result.json) |
| uat_demo30_proxy | PASS | 2.75 | [Artifacts](../../build/container-sdl/uat/demo30_proxy/result.json) |
| uat_demo31_direct | PASS | 14.90 | [Artifacts](../../build/container-sdl/uat/demo31_direct/result.json) |
| uat_demo31_proxy | PASS | 14.89 | [Artifacts](../../build/container-sdl/uat/demo31_proxy/result.json) |
| uat_demo32_direct | PASS | 3.20 | [Artifacts](../../build/container-sdl/uat/demo32_direct/result.json) |
| uat_demo32_proxy | PASS | 3.24 | [Artifacts](../../build/container-sdl/uat/demo32_proxy/result.json) |
| uat_demo33_direct | PASS | 3.87 | [Artifacts](../../build/container-sdl/uat/demo33_direct/result.json) |
| uat_demo33_proxy | PASS | 3.90 | [Artifacts](../../build/container-sdl/uat/demo33_proxy/result.json) |
| uat_demo34_direct | FAIL | 1.67 | [Artifacts](../../build/container-sdl/uat/demo34_direct/result.json) |
| uat_demo34_proxy | FAIL | 1.68 | [Artifacts](../../build/container-sdl/uat/demo34_proxy/result.json) |
| uat_file_drop_direct | PASS | 19.12 | [Artifacts](../../build/container-sdl/uat/file_drop_direct/result.json) |
| uat_file_drop_proxy | PASS | 19.11 | [Artifacts](../../build/container-sdl/uat/file_drop_proxy/result.json) |
| uat_trash_direct | PASS | 20.57 | [Artifacts](../../build/container-sdl/uat/trash_direct/result.json) |
| uat_trash_proxy | PASS | 20.60 | [Artifacts](../../build/container-sdl/uat/trash_proxy/result.json) |
| uat_calc_direct | PASS | 5.46 | [Artifacts](../../build/container-sdl/uat/calc_direct/result.json) |
| uat_calc_proxy | PASS | 5.49 | [Artifacts](../../build/container-sdl/uat/calc_proxy/result.json) |
| uat_gemread_direct | FAIL | 0.05 | [Artifacts](../../build/container-sdl/uat/gemread_direct/result.json) |
| uat_gemread_proxy | FAIL | 0.03 | [Artifacts](../../build/container-sdl/uat/gemread_proxy/result.json) |
| uat_gemread_pdf_proxy | PASS | 7.33 | [Artifacts](../../build/container-sdl/uat/gemread_pdf_proxy/result.json) |
| uat_gemread_epub_proxy | PASS | 4.14 | [Artifacts](../../build/container-sdl/uat/gemread_epub_proxy/result.json) |
| uat_gemscape_proxy | FAIL | 9.99 | [Artifacts](../../build/container-sdl/uat/gemscape_proxy/result.json) |
| uat_desktop_direct | PASS | 15.69 | [Artifacts](../../build/container-sdl/uat/desktop_direct/result.json) |
| uat_desktop_proxy | PASS | 15.73 | [Artifacts](../../build/container-sdl/uat/desktop_proxy/result.json) |

## Proposed actions

- **uat_demo34_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_demo34_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_gemread_direct:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_gemread_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.
- **uat_gemscape_proxy:** Inspect actual and reference PBMs and the demo/gemd sanitizer logs; repair the rendering or input path and rerun this case.

### uat_demo34_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 248, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 234, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 184, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 98, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 38, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 1142 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo34_initial.pbm

```

### uat_demo34_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 248, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 234, in main
    session.run()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 184, in run
    check(self, initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 98, in check
    checkpoint(session,'initial',initial)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/scenarios.py", line 38, in checkpoint
    assert len(data)==len(expected) and difference==0, (
AssertionError: initial: 1142 pixels differ from reviewed reference /home/tomaz/repos/triglav-os/gem/tests/data/uat/demo34_initial.pbm

```

### uat_gemread_direct (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemread.py", line 347, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemread.py", line 323, in main
    shutil.copyfile(source, selector_document)
  File "/usr/lib/python3.10/shutil.py", line 254, in copyfile
    with open(src, 'rb') as fsrc:
FileNotFoundError: [Errno 2] No such file or directory: '/home/tomaz/repos/triglav-os/gem/docs/manuals/The_Atari_Compendium.pdf'

```

### uat_gemread_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemread.py", line 347, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemread.py", line 323, in main
    shutil.copyfile(source, selector_document)
  File "/usr/lib/python3.10/shutil.py", line 254, in copyfile
    with open(src, 'rb') as fsrc:
FileNotFoundError: [Errno 2] No such file or directory: '/home/tomaz/repos/triglav-os/gem/docs/manuals/The_Atari_Compendium.pdf'

```

### uat_gemscape_proxy (FAIL)

```text
Traceback (most recent call last):
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemscape.py", line 176, in <module>
    main()
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemscape.py", line 164, in main
    check(session, args.cover, next_page)
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/samples/gemscape.py", line 74, in check
    session.wait(lambda: field(session, browser, 16)[0] < 1000 and
  File "/home/tomaz/repos/triglav-os/gem/tests/uat/run.py", line 95, in wait
    raise AssertionError('Timed out: ' + description)
AssertionError: Timed out: CEF page metrics to resize both GEM slider thumbs

```

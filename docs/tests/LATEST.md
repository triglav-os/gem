# Automated GEM test report

UTC: 2026-11-04T19:21:38+00:00 to 2026-11-04T19:23:23+00:00

**96 passed, 5 failed, 0 skipped; CTest exit status 8.**

Command: `ctest --test-dir /home/tomaz/repos/triglav-os/gem/build/cpc-sdk --output-on-failure --parallel 4 --output-junit /home/tomaz/repos/triglav-os/gem/build/cpc-sdk/test-results.xml`

Build: GCC RelWithDebInfo, Rasta platform; UAT rejects AddressSanitizer and UndefinedBehaviorSanitizer diagnostics.

[Complete execution log](../../build/cpc-sdk/tests.log) · [Machine-readable JUnit results](../../build/cpc-sdk/test-results.xml)

## Coverage

Unit checks cover public headers, API exports, VDI behavior and malformed transport data. Integration checks exercise real gemd connections, session isolation, AES trees, resources, reverse USERDEF callbacks and bitmap transfer. Security regressions exercise hostile FAT/MSA extraction, guest VDI argument bounds, disk-sector overflow and SoftFloat shift boundaries. The full sample session checks repeated Stout drags, Terminal movement and closure, framebuffer changes and independent RPC responsiveness. It measures terminal echo latency and checks a real command typed at 40 characters/second. Both calculator variants check visible buttons, digit entry, clear and 7 + 2 = 9. Both desktop variants verify disk, Workspace and Trash icons, open the file manager, navigate into a fixture directory and back, and close it. They check the global Locations command and directory chooser, compact Desk menus and File Open. Separate Gemread scenarios verify its shared toolbar/status chrome, color BGRA PDF pages, zoom, responsive resize and multipage thumbnail navigation; Gemscape verifies complete punctuation input, browser focus, GEM cursor visibility above CEF, navigation, redraw and scrollbar synchronization. Separate Trash scenarios exercise icon-view double-clicks, an upside-down-T XOR icon/title contour, live Trash-title inversion before button-up, file and non-empty-folder moves, plus cancel-default and confirmed Empty Trash in direct and proxy modes. They also force a failed move and immediately open another folder. The Trash unit test covers restore, collision handling, symlink-safe purge, same-volume store selection and cross-filesystem directory moves. The full sample session also opens Workspace through its exposed label while all applications are running. It holds the system menu open across a partial RPC frame and verifies that the Shutdown alert preserves all windows and client connections through a seven-second alert wait. All 34 demos run through Rasta in direct and proxy modes. UAT checks framebuffer references and scripted input; terminal cases verify a real shell command by reading its output file. The SDL viewer uses its dummy video driver unless UAT_SDL_DRIVER is set.

Passing these cases establishes the documented coverage, not every possible input or historical Atari device extension. See [transport scope](../architecture/API_TRANSPORT.md) and [demo scenarios](UAT.md).

## Results

| Test | Result | Seconds | Evidence |
| --- | --- | ---: | --- |
| test_report | PASS | 0.05 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_vdi | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gem_header | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_api_surface | PASS | 0.13 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_protocol | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_aes_shell | PASS | 1.26 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gem_trash | PASS | 0.09 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_terminal_vt100 | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gemconf | PASS | 0.01 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_prg | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_security_bounds | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_rasta_polarity | PASS | 0.00 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gem_rpc | PASS | 0.21 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gemd_security | PASS | 6.45 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_gemd_host | PASS | 0.01 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_vdi_rpc | PASS | 0.22 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_aes_rpc | PASS | 0.34 | [CTest log](../../build/cpc-sdk/tests.log) |
| test_sample_session | PASS | 34.57 | [Session state](../../build/cpc-sdk/sample_session_test/state.json) |
| security_disk | PASS | 0.04 | [CTest log](../../build/cpc-sdk/tests.log) |
| security_guest | PASS | 0.15 | [CTest log](../../build/cpc-sdk/tests.log) |
| uat_demo1_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo1_direct/result.json) |
| uat_demo1_proxy | PASS | 2.01 | [Artifacts](../../build/cpc-sdk/uat/demo1_proxy/result.json) |
| uat_demo2_direct | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo2_direct/result.json) |
| uat_demo2_proxy | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo2_proxy/result.json) |
| uat_demo3_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo3_direct/result.json) |
| uat_demo3_proxy | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo3_proxy/result.json) |
| uat_demo4_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo4_direct/result.json) |
| uat_demo4_proxy | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo4_proxy/result.json) |
| uat_demo5_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo5_direct/result.json) |
| uat_demo5_proxy | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo5_proxy/result.json) |
| uat_demo6_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo6_direct/result.json) |
| uat_demo6_proxy | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo6_proxy/result.json) |
| uat_demo7_direct | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo7_direct/result.json) |
| uat_demo7_proxy | PASS | 2.01 | [Artifacts](../../build/cpc-sdk/uat/demo7_proxy/result.json) |
| uat_demo8_direct | PASS | 2.01 | [Artifacts](../../build/cpc-sdk/uat/demo8_direct/result.json) |
| uat_demo8_proxy | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo8_proxy/result.json) |
| uat_demo9_direct | PASS | 1.98 | [Artifacts](../../build/cpc-sdk/uat/demo9_direct/result.json) |
| uat_demo9_proxy | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo9_proxy/result.json) |
| uat_demo10_direct | PASS | 1.94 | [Artifacts](../../build/cpc-sdk/uat/demo10_direct/result.json) |
| uat_demo10_proxy | PASS | 1.96 | [Artifacts](../../build/cpc-sdk/uat/demo10_proxy/result.json) |
| uat_demo11_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo11_direct/result.json) |
| uat_demo11_proxy | PASS | 2.02 | [Artifacts](../../build/cpc-sdk/uat/demo11_proxy/result.json) |
| uat_demo12_direct | PASS | 1.97 | [Artifacts](../../build/cpc-sdk/uat/demo12_direct/result.json) |
| uat_demo12_proxy | PASS | 1.98 | [Artifacts](../../build/cpc-sdk/uat/demo12_proxy/result.json) |
| uat_demo13_direct | PASS | 1.98 | [Artifacts](../../build/cpc-sdk/uat/demo13_direct/result.json) |
| uat_demo13_proxy | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo13_proxy/result.json) |
| uat_demo14_direct | PASS | 1.95 | [Artifacts](../../build/cpc-sdk/uat/demo14_direct/result.json) |
| uat_demo14_proxy | PASS | 1.96 | [Artifacts](../../build/cpc-sdk/uat/demo14_proxy/result.json) |
| uat_demo15_direct | PASS | 1.98 | [Artifacts](../../build/cpc-sdk/uat/demo15_direct/result.json) |
| uat_demo15_proxy | PASS | 1.98 | [Artifacts](../../build/cpc-sdk/uat/demo15_proxy/result.json) |
| uat_demo16_direct | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo16_direct/result.json) |
| uat_demo16_proxy | PASS | 1.99 | [Artifacts](../../build/cpc-sdk/uat/demo16_proxy/result.json) |
| uat_demo17_direct | PASS | 2.73 | [Artifacts](../../build/cpc-sdk/uat/demo17_direct/result.json) |
| uat_demo17_proxy | PASS | 2.76 | [Artifacts](../../build/cpc-sdk/uat/demo17_proxy/result.json) |
| uat_demo18_direct | PASS | 2.63 | [Artifacts](../../build/cpc-sdk/uat/demo18_direct/result.json) |
| uat_demo18_proxy | PASS | 5.69 | [Artifacts](../../build/cpc-sdk/uat/demo18_proxy/result.json) |
| uat_demo19_direct | PASS | 19.48 | [Artifacts](../../build/cpc-sdk/uat/demo19_direct/result.json) |
| uat_demo19_proxy | PASS | 19.54 | [Artifacts](../../build/cpc-sdk/uat/demo19_proxy/result.json) |
| uat_demo20_direct | PASS | 2.88 | [Artifacts](../../build/cpc-sdk/uat/demo20_direct/result.json) |
| uat_demo20_proxy | PASS | 2.89 | [Artifacts](../../build/cpc-sdk/uat/demo20_proxy/result.json) |
| uat_demo21_direct | PASS | 3.61 | [Artifacts](../../build/cpc-sdk/uat/demo21_direct/result.json) |
| uat_demo21_proxy | PASS | 3.63 | [Artifacts](../../build/cpc-sdk/uat/demo21_proxy/result.json) |
| uat_demo22_direct | PASS | 2.80 | [Artifacts](../../build/cpc-sdk/uat/demo22_direct/result.json) |
| uat_demo22_proxy | PASS | 2.83 | [Artifacts](../../build/cpc-sdk/uat/demo22_proxy/result.json) |
| uat_demo23_direct | PASS | 2.80 | [Artifacts](../../build/cpc-sdk/uat/demo23_direct/result.json) |
| uat_demo23_proxy | PASS | 2.82 | [Artifacts](../../build/cpc-sdk/uat/demo23_proxy/result.json) |
| uat_demo24_direct | PASS | 3.47 | [Artifacts](../../build/cpc-sdk/uat/demo24_direct/result.json) |
| uat_demo24_proxy | PASS | 3.53 | [Artifacts](../../build/cpc-sdk/uat/demo24_proxy/result.json) |
| uat_demo25_direct | PASS | 3.80 | [Artifacts](../../build/cpc-sdk/uat/demo25_direct/result.json) |
| uat_demo25_proxy | PASS | 3.85 | [Artifacts](../../build/cpc-sdk/uat/demo25_proxy/result.json) |
| uat_demo26_direct | PASS | 3.54 | [Artifacts](../../build/cpc-sdk/uat/demo26_direct/result.json) |
| uat_demo26_proxy | PASS | 3.55 | [Artifacts](../../build/cpc-sdk/uat/demo26_proxy/result.json) |
| uat_demo27_direct | PASS | 2.00 | [Artifacts](../../build/cpc-sdk/uat/demo27_direct/result.json) |
| uat_demo27_proxy | PASS | 2.02 | [Artifacts](../../build/cpc-sdk/uat/demo27_proxy/result.json) |
| uat_demo28_direct | PASS | 2.78 | [Artifacts](../../build/cpc-sdk/uat/demo28_direct/result.json) |
| uat_demo28_proxy | PASS | 2.76 | [Artifacts](../../build/cpc-sdk/uat/demo28_proxy/result.json) |
| uat_demo29_direct | PASS | 2.77 | [Artifacts](../../build/cpc-sdk/uat/demo29_direct/result.json) |
| uat_demo29_proxy | PASS | 2.79 | [Artifacts](../../build/cpc-sdk/uat/demo29_proxy/result.json) |
| uat_demo30_direct | PASS | 2.78 | [Artifacts](../../build/cpc-sdk/uat/demo30_direct/result.json) |
| uat_demo30_proxy | PASS | 2.77 | [Artifacts](../../build/cpc-sdk/uat/demo30_proxy/result.json) |
| uat_demo31_direct | PASS | 14.91 | [Artifacts](../../build/cpc-sdk/uat/demo31_direct/result.json) |
| uat_demo31_proxy | PASS | 14.92 | [Artifacts](../../build/cpc-sdk/uat/demo31_proxy/result.json) |
| uat_demo32_direct | PASS | 3.26 | [Artifacts](../../build/cpc-sdk/uat/demo32_direct/result.json) |
| uat_demo32_proxy | PASS | 3.24 | [Artifacts](../../build/cpc-sdk/uat/demo32_proxy/result.json) |
| uat_demo33_direct | PASS | 3.89 | [Artifacts](../../build/cpc-sdk/uat/demo33_direct/result.json) |
| uat_demo33_proxy | PASS | 3.88 | [Artifacts](../../build/cpc-sdk/uat/demo33_proxy/result.json) |
| uat_demo34_direct | FAIL | 1.70 | [Artifacts](../../build/cpc-sdk/uat/demo34_direct/result.json) |
| uat_demo34_proxy | FAIL | 1.73 | [Artifacts](../../build/cpc-sdk/uat/demo34_proxy/result.json) |
| uat_file_drop_direct | PASS | 19.17 | [Artifacts](../../build/cpc-sdk/uat/file_drop_direct/result.json) |
| uat_file_drop_proxy | PASS | 19.21 | [Artifacts](../../build/cpc-sdk/uat/file_drop_proxy/result.json) |
| uat_trash_direct | PASS | 20.78 | [Artifacts](../../build/cpc-sdk/uat/trash_direct/result.json) |
| uat_trash_proxy | PASS | 20.76 | [Artifacts](../../build/cpc-sdk/uat/trash_proxy/result.json) |
| uat_calc_direct | PASS | 5.53 | [Artifacts](../../build/cpc-sdk/uat/calc_direct/result.json) |
| uat_calc_proxy | PASS | 5.57 | [Artifacts](../../build/cpc-sdk/uat/calc_proxy/result.json) |
| uat_gemread_direct | FAIL | 0.03 | [Artifacts](../../build/cpc-sdk/uat/gemread_direct/result.json) |
| uat_gemread_proxy | FAIL | 0.05 | [Artifacts](../../build/cpc-sdk/uat/gemread_proxy/result.json) |
| uat_gemread_pdf_proxy | PASS | 7.47 | [Artifacts](../../build/cpc-sdk/uat/gemread_pdf_proxy/result.json) |
| uat_gemread_epub_proxy | PASS | 4.21 | [Artifacts](../../build/cpc-sdk/uat/gemread_epub_proxy/result.json) |
| uat_gemscape_proxy | FAIL | 10.05 | [Artifacts](../../build/cpc-sdk/uat/gemscape_proxy/result.json) |
| uat_desktop_direct | PASS | 15.99 | [Artifacts](../../build/cpc-sdk/uat/desktop_direct/result.json) |
| uat_desktop_proxy | PASS | 16.02 | [Artifacts](../../build/cpc-sdk/uat/desktop_proxy/result.json) |

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

## CPC physical-input follow-up

The five failures match the previous recorded full-suite failure set. Standard
GEM API/header/export and RPC checks pass alongside the opt-in keyboard test.
Native's consuming menu/input regressions pass, and actual Rasta input types
into CPC and switches main/modeless menus. A separate Debug run passes the
five focused API/RPC tests with ASan/UBSan; its full application run exposes
existing duplicate-global sanitizer failures in direct application linkage.

After this full run, GCC's pie-slice scratch-array diagnostic was removed by
explicit initialization. `test_vdi`, `test_gemd_host` and `test_aes_rpc` then
passed again; no VDI signature, screenshot reference or warning flag changed.

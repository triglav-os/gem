#!/usr/bin/env python3
"""Run CTest and publish an evidence-based Markdown acceptance report.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import os
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT/'build')
    parser.add_argument('--jobs', default='4')
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    evidence_root = Path(os.path.relpath(build, ROOT/'docs/tests')).as_posix()
    xml = build/'test-results.xml'
    xml.unlink(missing_ok=True)
    start = datetime.now(timezone.utc)
    command = ['ctest', '--test-dir', str(build), '--output-on-failure',
               '--parallel', args.jobs, '--output-junit', str(xml)]
    with (build/'tests.log').open('w') as log:
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end='', flush=True)
            log.write(line)
        status = process.wait()
    cases = ET.parse(xml).getroot().findall('.//testcase') if xml.exists() else []
    results = []
    for case in cases:
        failure = case.find('failure')
        error = case.find('error')
        skipped = case.find('skipped')
        state = ('FAIL' if failure is not None or error is not None else
                 'SKIP' if skipped is not None else 'PASS')
        output = case.findtext('system-out', '')
        detail = '\n'.join(x for x in [
            failure.text if failure is not None else '',
            error.text if error is not None else '', output] if x)
        results.append((case.get('name', '?'), state, case.get('time', '0'), detail))
    counts = {state: sum(row[1] == state for row in results)
              for state in ('PASS', 'FAIL', 'SKIP')}
    end = datetime.now(timezone.utc)
    lines = ['# Automated GEM test report', '',
             f'UTC: {start.isoformat(timespec="seconds")} to {end.isoformat(timespec="seconds")}', '',
             f'**{counts["PASS"]} passed, {counts["FAIL"]} failed, '
             f'{counts["SKIP"]} skipped; CTest exit status {status}.**', '',
             f'Command: `{" ".join(command)}`', '',
             'Build: GCC Debug, Rasta platform, AddressSanitizer and UndefinedBehaviorSanitizer.', '',
             f'[Complete execution log]({evidence_root}/tests.log) · '
             f'[Machine-readable JUnit results]({evidence_root}/test-results.xml)', '',
             '## Coverage', '',
             'Unit checks cover public headers, API exports, VDI behavior and malformed transport data. '
             'Integration checks exercise real gemd connections, session isolation, AES trees, '
             'resources, reverse USERDEF callbacks and bitmap transfer. '
             'Security regressions exercise hostile FAT/MSA extraction, guest VDI '
             'argument bounds, disk-sector overflow and SoftFloat shift boundaries. '
             'The full sample session checks repeated Stout drags, Terminal movement '
             'and closure, framebuffer changes and independent RPC responsiveness. '
             'It measures terminal echo latency and checks a real command typed at '
             '40 characters/second. Both calculator variants check visible buttons, '
             'digit entry, clear and 7 + 2 = 9. '
             'Both desktop variants verify disk, Workspace and Trash icons, open the '
             'file manager, navigate into a fixture directory and back, and close it. '
             'They check compact Desk menus, Desktop info and File Open. The full '
             'sample session also opens Workspace through its exposed label while '
             'all samples are running. It holds Desk open across a partial RPC frame '
             'and verifies that Desktop info preserves all windows and client '
             'connections through a seven-second alert wait. '
             'All 33 demos run through Rasta in direct and proxy modes. '
             'UAT checks framebuffer references and scripted input; terminal cases verify a '
             'real shell command by reading its output file. The SDL viewer uses its dummy '
             'video driver unless UAT_SDL_DRIVER is set.', '',
             'Passing these cases establishes the documented coverage, not every possible '
             'input or historical Atari device extension. '
             'See [transport scope](../architecture/API_TRANSPORT.md) and [demo scenarios](UAT.md).', '',
             '## Results', '', '| Test | Result | Seconds | Evidence |',
             '| --- | --- | ---: | --- |']
    for name, state, elapsed, detail in results:
        evidence = f'[CTest log]({evidence_root}/tests.log)'
        if name.startswith('uat_'):
            evidence = f'[Artifacts]({evidence_root}/uat/{name[4:]}/result.json)'
        elif name == 'test_sample_session':
            evidence = f'[Session state]({evidence_root}/sample_session_test/state.json)'
        lines.append(f'| {name} | {state} | {float(elapsed):.2f} | {evidence} |')
    lines += ['', '## Proposed actions', '']
    unsuccessful = [row for row in results if row[1] != 'PASS']
    if not cases:
        lines += ['- Restore CTest execution and rerun `make tests`; no test results were produced.']
    elif not unsuccessful and status == 0:
        lines += ['- No corrective action is indicated by this run.',
                  '- Rerun `make tests` after changes; review framebuffer references explicitly '
                  'when intended rendering changes. Do not regenerate references to conceal failures.']
    else:
        for name, state, _, detail in unsuccessful:
            action = ('Inspect actual and reference PBMs and the demo/gemd sanitizer logs; '
                      'repair the rendering or input path and rerun this case.' if name.startswith('uat_')
                      else 'Inspect the assertion and sanitizer output; repair the affected implementation and rerun this case.')
            if state == 'SKIP': action = 'Restore the missing runtime prerequisite and rerun this case.'
            lines += [f'- **{name}:** {action}']
        if status and not unsuccessful:
            lines += ['- Inspect the CTest execution log for a runner failure and rerun the suite.']
    for name, state, _, detail in unsuccessful:
        lines += ['', f'### {name} ({state})', '', '```text',
                  detail[-6000:].replace('```', "'''"), '```']
    directory = ROOT/'docs/tests'
    directory.mkdir(parents=True, exist_ok=True)
    report = '\n'.join(lines)+'\n'
    (directory/'LATEST.md').write_text(report)
    (directory/f'{start:%Y%m%dT%H%M%SZ}.md').write_text(report)
    print(f'Report: {directory / "LATEST.md"}')
    return status or (1 if not cases or counts['FAIL'] else 0)

if __name__ == '__main__':
    sys.exit(main())

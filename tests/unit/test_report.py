#!/usr/bin/env python3
"""Verify that a failing CTest run still publishes truthful report evidence.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import importlib.util
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('gem_report', root/'tests/report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
report.ROOT = Path(sys.argv[1]).resolve()
execution = report.ROOT/'execution'
execution.mkdir(parents=True, exist_ok=True)
(execution/'CTestTestfile.cmake').write_text(
    'add_test(report_probe_pass "/usr/bin/true")\n'
    'add_test(report_probe_fail "/usr/bin/false")\n')
sys.argv = ['report.py', '--build', str(execution)]
status = report.main()
assert status != 0, 'CTest failure was hidden'
text = (report.ROOT/'docs/tests/LATEST.md').read_text()
assert '1 passed, 1 failed, 0 skipped' in text
assert '| report_probe_pass | PASS |' in text
assert '| report_probe_fail | FAIL |' in text
assert '**report_probe_fail:** Inspect' in text
assert list((report.ROOT/'docs/tests').glob('20*.md'))
assert '[Complete execution log](../../execution/tests.log)' in text
assert '[Machine-readable JUnit results](../../execution/test-results.xml)' in text
assert '[CTest log](../../execution/tests.log)' in text
print('Failed runs retain a nonzero status and publish both outcomes and actions')

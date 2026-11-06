#!/usr/bin/env python3
"""Verify overlapping commands wait and release the shared checkout lock.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/scripts'))
from docker_context import ensure_container
ensure_container()


def wait_for(path, process):
    deadline = time.monotonic() + 5
    while not path.exists():
        assert process.poll() is None, 'Lock holder exited before its command'
        assert time.monotonic() < deadline, 'Locked command never started'
        time.sleep(.01)


with tempfile.TemporaryDirectory(dir=sys.argv[1], prefix='container-lock-') as tmp:
    workspace = Path(tmp)
    script = workspace / 'tools/scripts/container-entry.sh'
    script.parent.mkdir(parents=True)
    shutil.copyfile(ROOT / 'tools/scripts/container-entry.sh', script)
    first = workspace / 'first'
    second = workspace / 'second'
    release = workspace / 'release'
    holder = subprocess.Popen(['bash', str(script), sys.executable, '-c',
        'from pathlib import Path; import sys,time; '
        'Path(sys.argv[1]).touch(); '
        '\nwhile not Path(sys.argv[2]).exists(): time.sleep(.01)',
        str(first), str(release)])
    follower = None
    try:
        wait_for(first, holder)
        follower = subprocess.Popen(['bash', str(script), sys.executable, '-c',
            'from pathlib import Path; import sys; Path(sys.argv[1]).touch()',
            str(second)], stderr=subprocess.PIPE, text=True)
        time.sleep(.2)
        assert not second.exists(), 'Overlapping commands entered the same checkout'
        release.touch()
        assert holder.wait(timeout=5) == 0
        _, diagnostic = follower.communicate(timeout=5)
        assert follower.returncode == 0 and second.exists()
        assert 'Waiting for another GEM' in diagnostic
        assert subprocess.run(['bash', str(script), '/usr/bin/false']).returncode == 1
        subprocess.run(['bash', str(script), '/usr/bin/true'], check=True, timeout=5)
    finally:
        release.touch()
        for process in (holder, follower):
            if process is not None and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
print('PASS shared checkout exclusion, waiting and release after failure')

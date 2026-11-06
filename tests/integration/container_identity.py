#!/usr/bin/env python3
"""Check account lookup and login-shell startup for the Docker runtime UID.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import grp
import os
from pathlib import Path
import pwd
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/scripts'))
from docker_context import ensure_container
ensure_container()

account = pwd.getpwuid(os.getuid())
group = grp.getgrgid(os.getgid())
home = Path(os.environ['HOME'])
assert account.pw_gid == os.getgid()
assert Path(account.pw_dir) == home
assert home.is_dir() and home.stat().st_uid == os.getuid()
with tempfile.TemporaryFile(dir=home) as probe:
    probe.write(b'writable home')
shell = subprocess.run(
    ['/bin/bash', '--login', '-c', 'id -un; id -gn; groups; test -w "$HOME"'],
    text=True, capture_output=True, timeout=5, check=True)
assert shell.stderr == '', shell.stderr
assert shell.stdout.splitlines()[:2] == [account.pw_name, group.gr_name]
print(f'PASS login shell, user {account.pw_name}, group {group.gr_name}, writable home')

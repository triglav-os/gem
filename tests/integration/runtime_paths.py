#!/usr/bin/env python3
"""Verify private XDG mounts and non-overwriting legacy settings migration.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/scripts'))
from docker_context import ensure_container
ensure_container()
with tempfile.TemporaryDirectory(prefix='gem-layout-') as directory:
    base = Path(directory)
    legacy = base/'checkout/build/f5/config/gem'
    legacy.mkdir(parents=True)
    (legacy/'desktop.cfg').write_text('old desktop')
    (legacy/'gemread.cfg').write_text('old reader')
    profile = base/'checkout/build/user-data/gemscape/profile'
    profile.mkdir(parents=True)
    (profile/'Cookies').write_bytes(b'old cookies')
    config = base/'home/.config/gem'
    config.mkdir(parents=True)
    (config/'desktop.cfg').write_text('new desktop')
    env = dict(os.environ, HOME=str(base/'home'), XDG_CONFIG_HOME='relative',
               XDG_DATA_HOME=str(base/'data'), XDG_CACHE_HOME=str(base/'cache'),
               XDG_STATE_HOME=str(base/'state'), XDG_CONFIG_DIRS='/missing')
    env.pop('GEMSCAPE_PROFILE', None)
    env.pop('GEM_SESSION_DIRECTORY', None)
    script = 'root="$1"; key=1234567890123456; container_env=(); source "$2"; printf "%s\\n" "${container_env[@]}" "${storage_args[@]}"'
    command = ['bash', '-eu', '-c', script, 'paths', str(base/'checkout'), str(ROOT/'tools/scripts/runtime-paths.sh')]
    result = subprocess.run(command, env=env, text=True, capture_output=True, check=True)
    assert (config/'desktop.cfg').read_text() == 'new desktop'
    assert (config/'gemread.cfg').read_text() == 'old reader'
    assert (base/'data/gem/gemscape/profile/Cookies').read_bytes() == b'old cookies'
    assert 'XDG_RUNTIME_DIR=/run/user/' in result.stdout
    for directory in (base/'data/gem', base/'cache/gem', base/'state/gem', base/'data/Trash'):
        assert directory.stat().st_mode & 0o777 == 0o700
    (legacy/'gemread.cfg').write_text('changed old reader')
    subprocess.run(command, env=env, text=True, capture_output=True, check=True)
    assert (config/'gemread.cfg').read_text() == 'old reader'
    assert profile.is_dir(), 'Migration removed the legacy profile'
    shared = str(base/'shared')
    env.update({key: shared for key in ('XDG_CONFIG_HOME', 'XDG_DATA_HOME',
                                       'XDG_CACHE_HOME', 'XDG_STATE_HOME', 'XDG_CONFIG_DIRS')})
    result = subprocess.run(command, env=env, text=True, capture_output=True, check=True)
    mounts = [line for line in result.stdout.splitlines() if line.startswith('type=bind,')]
    assert len(mounts) == len(set(mounts)) == 2, mounts
print('PASS XDG overrides, private mounts and non-overwriting migration')

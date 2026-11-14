#!/usr/bin/env python3
"""Exercise Rasta's real build recipe against an advancing local Git remote.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/scripts'))
from docker_context import ensure_container
ensure_container()


with tempfile.TemporaryDirectory(dir=sys.argv[1], prefix='rasta-update-') as tmp:
    workspace = Path(tmp)
    upstream = workspace / 'upstream'
    upstream.mkdir()
    project = workspace / 'project'
    project.mkdir()
    build = workspace / 'build'
    installed = workspace / 'runtime/rasta'
    env = dict(os.environ, GIT_CONFIG_COUNT='1',
        GIT_CONFIG_KEY_0=f'url.{upstream.as_uri()}.insteadOf',
        GIT_CONFIG_VALUE_0='https://github.com/tstih/rasta.git',
        GIT_AUTHOR_NAME='GEM test', GIT_AUTHOR_EMAIL='test@example.invalid',
        GIT_COMMITTER_NAME='GEM test', GIT_COMMITTER_EMAIL='test@example.invalid')

    def run(*args, cwd=workspace, check=True):
        result = subprocess.run(args, cwd=cwd, env=env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        if check and result.returncode:
            raise AssertionError(result.stdout)
        return result

    def commit(version):
        (upstream / 'main.cpp').write_text(
            '#include <iostream>\n'
            f'int main() {{ std::cout << "{version}\\n"; }}\n')
        run('git', 'add', '.', cwd=upstream)
        run('git', 'commit', '-m', version, cwd=upstream)
        return run('git', 'rev-parse', 'HEAD', cwd=upstream).stdout.strip()

    def rebuild(version, revision):
        run('cmake', '--build', str(build), '--target', 'rasta_viewer')
        checkout = build / 'deps/rasta-source'
        assert run('git', 'rev-parse', 'HEAD', cwd=checkout).stdout.strip() == revision
        assert run(str(installed)).stdout.strip() == version
        assert (checkout / 'bin/test_rasta').is_file()

    run('git', 'init', '-b', 'main', str(upstream))
    (upstream / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(rasta_fixture LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
add_executable(rasta main.cpp)
add_executable(test_rasta main.cpp)
''')
    first = commit('first')
    (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(rasta_update NONE)
set(GEM_TOOLS_DIR "{installed.parent}")
set_property(DIRECTORY PROPERTY EP_UPDATE_DISCONNECTED TRUE)
include("{ROOT / 'tools/scripts/rasta.cmake'}")
''')
    run('cmake', '-S', str(project), '-B', str(build), '-G', 'Unix Makefiles')
    rebuild('first', first)
    timestamp = installed.stat().st_mtime_ns
    rebuild('first', first)
    assert installed.stat().st_mtime_ns == timestamp, 'Unchanged viewer was replaced'
    second = commit('second')
    rebuild('second', second)  # No parent reconfiguration or cache cleanup.
    installed.unlink()
    rebuild('second', second)
    run('git', 'reset', '--hard', first, cwd=upstream)
    rewritten = commit('rewritten')
    rebuild('rewritten', rewritten)
    upstream.rename(workspace / 'unavailable')
    failure = run('cmake', '--build', str(build), '--target', 'rasta_viewer', check=False)
    assert failure.returncode != 0, 'Failed fetch silently reused stale source'
    assert 'Performing update step' in failure.stdout, failure.stdout

print('PASS latest Rasta updates, unchanged reuse, missing output restoration, '
      'rewritten history and fetch failure')

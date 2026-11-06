#!/usr/bin/env python3
"""Build independent samples using a clean, relocated SDK export.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/scripts'))
from docker_context import ensure_container
ensure_container()
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
stage = build/'sdk-layout'
shutil.rmtree(stage, ignore_errors=True)
(stage/'sdk').mkdir(parents=True)
(stage/'sdk/obsolete.txt').write_text('must not survive export')
with (stage/'build.log').open('w') as log:
    def run(command, **kwargs):
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, **kwargs)
    run(['cmake', '-DSDK_ROOT=' + str(stage/'sdk'), '-P', str(build/'sdk-export.cmake')])
    sdk = stage/'relocated'
    (stage/'sdk').rename(sdk)
    assert not (sdk/'obsolete.txt').exists()
    backend = re.search(r'set\(GEM_SDK_PLATFORM (\w+)\)', (sdk/'gem_sdk_config.cmake').read_text())[1]
    libraries = {'lib' + name + '.so' for name in ('gem', 'aes', 'vdi', 'platform_linux', 'fat12', 'msa')}
    if backend == 'rasta':
        libraries.add('librasta.so')
    assert {p.name for p in (sdk/'lib').iterdir()} == libraries | {'libgemconf.a', 'libgemctl.a', 'pkgconfig'}
    assert {p.name for p in (sdk/'bin').iterdir()} == {'resgen'}
    assert {p.name for p in (sdk/'include').iterdir()} == {'gem.h', 'gem', 'platform', 'fat12', 'msa'}
    assert not list(sdk.rglob('*.msa')) and not list(sdk.rglob('*.st'))
    for library in libraries:
        text = subprocess.check_output(['ldd', str(sdk/'lib'/library)], text=True)
        assert 'not found' not in text and str(ROOT/'bin/lib') not in text, text
    env = dict(os.environ, PKG_CONFIG_PATH=str(sdk/'lib/pkgconfig'))
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'gem'], env=env, text=True)
    assert str(sdk) in flags
    shutil.copytree(ROOT/'samples', stage/'samples', ignore=shutil.ignore_patterns('__pycache__'))
    run(['cmake', '-S', str(stage/'samples'), '-B', str(stage/'build'),
         '-DGEM_SDK_ROOT=' + str(sdk), '-DCMAKE_BUILD_TYPE=RelWithDebInfo'])
    run(['cmake', '--build', str(stage/'build'), '--parallel', '2'])
    run([str(stage/'build/runtime/msa'), 'ls', str(stage/'samples/data/st.msa')])
print('PASS clean SDK export, transitive libraries, pkg-config and standalone samples')

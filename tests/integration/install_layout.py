#!/usr/bin/env python3
"""Stage and relocate the Linux layout, then launch installed GUI applications.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import argparse
import re
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/scripts'))
from docker_context import ensure_container
ensure_container()
sys.path.insert(0, str(ROOT/'tests/uat'))
from run import Session

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--no-gui', action='store_true')
args = parser.parse_args()
build = args.build.resolve()
stage = build/'install-layout'
relocated = stage/'relocated'
stage.mkdir(exist_ok=True)
shutil.rmtree(relocated, ignore_errors=True)
# Exercise the real package publisher, including cleanup of obsolete payload.
prefix = stage/'gemix'
prefix.mkdir(exist_ok=True)
(prefix/'obsolete.txt').write_text('must not survive staging')
with (stage/'install.log').open('w') as log:
    subprocess.run([sys.executable, str(ROOT/'tools/scripts/package_stage.py'),
                    '--build', str(build), '--destination', str(prefix)],
                   stdout=log, stderr=subprocess.STDOUT, check=True)
prefix.rename(relocated)
cache = dict(re.findall(r'^([A-Za-z0-9_]+):[^=\n]+=(.*)$',
                       (build/'CMakeCache.txt').read_text(), re.M))
libdir, bindir, datadir = (cache['CMAKE_INSTALL_' + name] for name in ('LIBDIR', 'BINDIR', 'DATADIR'))
datadir = datadir or cache['CMAKE_INSTALL_DATAROOTDIR']
assert not Path(libdir).is_absolute() and not Path(bindir).is_absolute()
programs = {'gemd', 'desktop', 'terminal', 'calc', 'clock', 'stout', 'gemread', 'gemscape', 'rasta', 'ted'}
libraries = {'lib' + name + '.so' for name in ('gem', 'aes', 'vdi', 'platform_linux', 'rasta', 'fat12', 'msa', 'prg')}
if cache['GEM_PLATFORM'] == 'linux':
    programs.remove('rasta')
    libraries.remove('librasta.so')
program_icons = {'icons/' + name + '.rsc' for name in ('gemscape', 'terminal', 'clock', 'calc', 'gemread', 'ted',
                                                       'home', 'programs', 'documents', 'library',
                                                       'blaise', 'fyodor', 'isaac', 'disk', 'document',
                                                       'folder', 'program', 'trash',
                                                       '.txt', '.md', '.pdf')}
assert {p.name for p in (relocated/bindir).iterdir()} == programs
assert {p.name for p in (relocated/libdir).iterdir()} == libraries | {'gem'}
assert {p.relative_to(relocated).parts[0] for p in relocated.iterdir()} == {bindir.split('/')[0], libdir.split('/')[0], datadir.split('/')[0]}
resources = {'fonts/AtariSTHigh.fnt', 'fonts/8x16_z1.fnt', 'cursors.rsc', 'alert_icons.rsc',
             'gemscape_toolbar.rsc'} | program_icons
assert {str(p.relative_to(relocated/datadir/'gem')) for p in (relocated/datadir/'gem').rglob('*') if p.is_file()} == resources
cef = relocated/libdir/'gem/cef'
cef_binaries = {'libcef.so', 'libEGL.so', 'libGLESv2.so', 'libvk_swiftshader.so', 'libvulkan.so.1'}
cef_files = cef_binaries | {'v8_context_snapshot.bin', 'vk_swiftshader_icd.json', 'icudtl.dat',
    'chrome_100_percent.pak', 'chrome_200_percent.pak', 'resources.pak', 'locales/' + cache['GEM_CEF_LOCALE'] + '.pak'}
assert {str(p.relative_to(cef)) for p in cef.rglob('*') if p.is_file()} == cef_files
assert {p.name for p in (relocated/datadir/'doc/gem').iterdir()} == {'PACKAGING.md'}
assert (relocated/datadir/'licenses/gemscape/CREDITS.html').is_file()
assert (relocated/datadir/'licenses/stout/SoftFloat.txt').is_file()
assert (relocated/datadir/'licenses/mupdf/COPYING').is_file()
# No two CEF paths may carry the same payload, including resources outside cef/.
for name in ('icudtl.dat', 'resources.pak', 'chrome_100_percent.pak', 'chrome_200_percent.pak'):
    assert len(list(relocated.rglob(name))) == 1
needed = set()
env = {k: v for k, v in os.environ.items() if k != 'LD_LIBRARY_PATH'}
for binary in ([relocated/bindir/name for name in programs] +
               [relocated/libdir/name for name in libraries] + [cef/name for name in cef_binaries]):
    dynamic = subprocess.check_output(['readelf', '-d', str(binary)], text=True)
    assert str(ROOT) not in dynamic, f'{binary} retains a checkout RPATH'
    needed.update(re.findall(r'NEEDED.*\[(.*?)\]', dynamic))
    dependencies = subprocess.check_output(['ldd', str(binary)], text=True, env=env)
    assert 'not found' not in dependencies, dependencies
    sections = subprocess.check_output(['readelf', '-SW', str(binary)], text=True)
    assert '.debug_info' not in sections and '.symtab' not in sections, binary
assert libraries <= needed, 'Unused shared libraries were packaged'
if args.no_gui:
    print('PASS native runtime contents, stripped ELF dependencies and resource allowlists')
    sys.exit(0)
os.environ['RASTA_BIN'] = str(relocated/bindir/'rasta')
for app in ('gemscape', 'gemread', 'clock', 'desktop', 'terminal', 'calc', 'ted'):
    scenario = SimpleNamespace(demo='installed_' + app, capture=True, mode='proxy',
        binary=str(relocated/bindir/app), gemd=str(relocated/bindir/'gemd'),
        artifacts=str(stage), resources=str(relocated/datadir/'gem'))
    session = Session(scenario)
    for key in ('GEM_RESOURCE_DIR', 'GEMIX_HOME', 'GEMSCAPE_PROFILE', 'GEM_CEF_ROOT', 'LD_LIBRARY_PATH'):
        session.env.pop(key, None)
    for variable, directory in (('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data'),
                                ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state')):
        session.env[variable] = str(session.path/directory)
    session.env['GEMSCAPE_URL'] = 'data:text/html,<h1>Installed browser</h1>'
    try:
        session.run()
        session.pause(1)
        assert session.demo.poll() is None
        maps = Path(f'/proc/{session.demo.pid}/maps').read_text()
        assert str(ROOT/'bin/lib') not in maps, 'Installed app loaded checkout libraries'
        if app == 'gemscape':
            assert str(cef/'libcef.so') in maps
            assert (session.path/'data/gem/gemscape/profile').is_dir()
            assert (session.path/'cache/gem/gemscape').is_dir()
            assert (session.path/'state/gem/gemscape').is_dir()
        session.frame('installed_application')
    finally:
        session.close()
print('PASS relocated install, private CEF, resource discovery and XDG browser storage')

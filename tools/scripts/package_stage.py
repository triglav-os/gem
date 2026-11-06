#!/usr/bin/env python3
"""Replace a generated runtime stage only after installation succeeds.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from docker_context import ensure_container
ensure_container()
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--destination', type=Path, required=True)
args = parser.parse_args()
destination = args.destination.absolute()
if destination.name != 'gemix' or destination.is_symlink():
    parser.error('Destination must be a generated gemix directory, not a symlink')
destination.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='.gemix-stage-', dir=destination.parent) as temporary:
    staged = Path(temporary)/'gemix'
    environment = dict(os.environ)
    environment.pop('DESTDIR', None)
    subprocess.run(['cmake', '--install', str(args.build), '--prefix', str(staged), '--strip'],
                   check=True, env=environment)
    if destination.exists():
        shutil.rmtree(destination)
    staged.rename(destination)

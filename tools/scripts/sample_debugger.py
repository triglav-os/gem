#!/usr/bin/env python3
"""Load the prepared F5 environment when GDB starts, after prelaunch tasks.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
directory = Path(os.environ.get('GEM_SESSION_DIRECTORY', root/'build/f5'))
if '--version' not in sys.argv:
    try:
        for line in (directory/'session.env').read_text().splitlines():
            key, separator, value = line.partition('=')
            if not separator or not key:
                raise ValueError('Malformed session environment')
            os.environ[key] = value
    except (OSError, ValueError) as error:
        sys.exit(f'Start the GEM sample session before debugging: {error}')
os.environ['DEBUGINFOD_URLS'] = ''
os.execv('/usr/bin/gdb', ['/usr/bin/gdb', *sys.argv[1:]])

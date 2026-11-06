"""Re-enter the project's Docker toolchain for directly invoked Python tools.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import os
from pathlib import Path
import sys


def ensure_container():
    if os.environ.get('GEM_IN_CONTAINER') == '1':
        return
    root = Path(__file__).resolve().parents[2]
    script = root / 'tools/scripts/container.sh'
    os.environ['GEM_EXEC_WORKDIR'] = str(Path.cwd())
    os.execv(str(script), [str(script), 'exec', 'python3', '-B',
                          str(Path(sys.argv[0]).resolve()), *sys.argv[1:]])

#!/usr/bin/env python3
"""Explicitly run direct UATs and record checkpoints for visual review.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path as _Path
import sys as _sys
_sys.path.insert(0, str(_Path(__file__).resolve().parents[2] / "tools/scripts"))
from docker_context import ensure_container
ensure_container()
import concurrent.futures
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2]
def run(number):
    command=[sys.executable,'-B',str(root/'tests/uat/run.py'),'--demo',str(number),
        '--mode','direct','--binary',str(root/f'bin/tests/uat/demo{number}_direct'),
        '--gemd',str(root/'bin/core/gemd'),'--resources',str(root/'bin/resources'),
        '--artifacts',str(root/'build/uat'),'--record']
    result=subprocess.run(command,capture_output=True,text=True)
    return number,result.returncode,result.stderr[-1000:]
if __name__=='__main__':
    numbers=list(map(int,sys.argv[1:])) or list(range(1,35))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results=list(pool.map(run,numbers))
    for number,status,error in results:
        print(f'demo{number}: '+('PASS' if not status else error),flush=True)
    sys.exit(any(status for _,status,_ in results))

#!/usr/bin/env python3
"""Check public API exports and proxy/native dynamic-link separation.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import re
import subprocess
import sys
root = Path(sys.argv[1])
library = Path(sys.argv[2])
symbols = {line.split()[-1] for line in subprocess.check_output(
    ['nm', '-D', '--defined-only', str(library)], text=True).splitlines()}
for api in ('aes', 'vdi'):
    header = (root/f'include/gem/{api}.h').read_text()
    header = re.sub(r'/\*.*?\*/', '', header, flags=re.S)
    functions = set(re.findall(r'^\s*(?:VOID|WORD|LONG|VDI_HANDLE|void|char\s*\*)\s+(\w+)\s*\([^;]*\);', header, re.M))
    assert functions, f'No {api} declarations parsed'
    missing = functions - symbols
    assert not missing, f'Missing {api} exports: {sorted(missing)}'
    print(f'{api}: all {len(functions)} declared functions exported')
for binary in [library, *sorted((root/'bin/tests/uat').glob('demo*_proxy'))]:
    dynamic = subprocess.check_output(['readelf', '-d', str(binary)], text=True)
    assert not re.search(r'NEEDED.*\[lib(?:aes|vdi)\.so', dynamic), str(binary)
print('libgem and every proxy demo are independent of native AES/VDI libraries')

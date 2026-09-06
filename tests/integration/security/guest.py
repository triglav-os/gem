#!/usr/bin/env python3
"""Run malformed Atari VDI traps through the actual Musashi guest boundary."""
import pathlib
import struct
import subprocess
import sys
import tempfile

stout = pathlib.Path(sys.argv[1]).resolve()
workspace = pathlib.Path(sys.argv[2]).resolve()
workspace.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=workspace) as temporary:
    path = pathlib.Path(temporary) / 'bad.prg'
    for words, points in ((65, 0), (0, 129), (-1, 0), (0, -1)):
        # PC-relative addresses survive the loader's chosen text address.
        code = bytes.fromhex('203c0000007341fa000e220843fa001c20894e424afc')
        control = [6, points, 0, words] + [0] * 8
        text = code + bytes(20) + struct.pack('>12h', *control)
        header = struct.pack('>H6IH', 0x601a, len(text), 0, 0, 0, 0, 0, 1)
        path.write_bytes(header + text)
        result = subprocess.run([str(stout), str(path)], capture_output=True,
                                text=True, timeout=10)
        assert result.returncode != 0, result.stdout
        assert 'invalid VDI parameter counts' in result.stderr, result.stderr
        assert 'Sanitizer' not in result.stderr, result.stderr
        print('PASS: rejected VDI counts', words, points)

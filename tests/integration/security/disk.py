#!/usr/bin/env python3
"""Exercise disk extraction with malformed FAT entries and hostile destinations."""
import pathlib
import struct
import subprocess
import sys
import tempfile

msa = pathlib.Path(sys.argv[1]).resolve()
workspace = pathlib.Path(sys.argv[2]).resolve()
workspace.mkdir(parents=True, exist_ok=True)


def run(*args, success=True):
    result = subprocess.run([str(msa), *map(str, args)], capture_output=True,
                            text=True, timeout=10)
    assert (result.returncode == 0) == success, (args, result.stderr)
    assert 'Sanitizer' not in result.stderr, result.stderr
    return result


def disk(name=b'SAFE    TXT', size=4, directory=False):
    data = bytearray(720 * 512)
    struct.pack_into('<HBHBHHBHHH', data, 11,
                     512, 1, 1, 1, 16, 720, 0xF9, 3, 9, 1)
    data[512:518] = bytes.fromhex('f9ffffffff0f')
    entry = bytearray(32)
    entry[:11] = name
    entry[11] = 0x10 if directory else 0x20
    struct.pack_into('<H', entry, 26, 2)
    struct.pack_into('<I', entry, 28, size)
    data[4 * 512:4 * 512 + 32] = entry
    data[5 * 512:5 * 512 + (32 if directory else 4)] = (
        entry if directory else b'SAFE')
    return data


with tempfile.TemporaryDirectory(dir=workspace) as temporary:
    root = pathlib.Path(temporary)
    raw, packed = root / 'disk.st', root / 'disk.msa'
    raw.write_bytes(disk())
    run('pack', raw, packed)
    output = root / 'normal'
    run('extract', packed, output)
    assert (output / 'SAFE.TXT').read_bytes() == b'SAFE'
    run('extract', packed, output)
    print('PASS: normal extraction and replacement')
    outside = root / 'outside'
    outside.write_bytes(b'KEEP')
    for kind in ('symlink', 'hardlink'):
        dest = root / kind
        dest.mkdir()
        target = dest / 'SAFE.TXT'
        if kind == 'symlink':
            target.symlink_to(outside)
        else:
            target.hardlink_to(outside)
        run('extract', packed, dest, success=False)
        assert outside.read_bytes() == b'KEEP'
        print('PASS: refuses', kind)
    for label, data in (
        ('traversal', disk(b'../EVIL TXT')),
        ('cycle', disk(b'LOOP       ', 0, True)),
        ('oversized', disk(size=0xffffffff)),
    ):
        raw.write_bytes(data)
        run('pack', raw, packed)
        run('extract', packed, root / label, success=False)
        print('PASS: rejects', label)

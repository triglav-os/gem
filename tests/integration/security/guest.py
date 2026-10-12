#!/usr/bin/env python3
"""Exercise Stout's Atari trap vectors through the Musashi guest boundary."""
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

    # Each program pushes a harmless service number, raises the real Atari
    # vector, then returns to Stout's private exit stub. A missing route
    # therefore becomes a guest fault or timeout rather than a false pass.
    for name, vector, service in (
            ('GEMDOS', 1, 25),       # Dgetdrv
            ('BIOS', 13, 1),         # Bconstat
            ('BIOS Kbshift', 13, 11),
            ('XBIOS Getrez', 14, 4),
            ('XBIOS Logbase', 14, 3),
            ('XBIOS Vsync', 14, 37)):
        code = struct.pack('>HHHHHH', 0x3f3c, service,
                           0x4e40 | vector, 0x548f, 0x7000, 0x4e75)
        header = struct.pack('>H6IH', 0x601a, len(code), 0, 0, 0, 0, 0, 1)
        path.write_bytes(header + code)
        result = subprocess.run([str(stout), str(path)], capture_output=True,
                                text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        assert 'unsupported trap' not in result.stderr, result.stderr
        assert 'Sanitizer' not in result.stderr, result.stderr
        print('PASS: routed', name, 'trap', vector)

    # Programs launched by the desktop have a parent basepage, whereas desk
    # accessories do not. Several historical runtimes use this distinction
    # to choose between their application and accessory event loops.
    parent_test = bytes.fromhex(
        '206f0004'                    # movea.l 4(sp),a0
        '20280024'                    # move.l 36(a0),d0
        '4a80'                        # tst.l d0
        '6704'                        # beq.s failure
        '70004e75'                    # success: moveq #0,d0; rts
        '70014e75')                   # failure: moveq #1,d0; rts
    accessory_test = bytes.fromhex(
        '206f0004'                    # movea.l 4(sp),a0
        '20280024'                    # move.l 36(a0),d0
        '4a80'                        # tst.l d0
        '6704'                        # beq.s success
        '70014e75'                    # failure: moveq #1,d0; rts
        '70004e75')                   # success: moveq #0,d0; rts
    for suffix, code in (('.PRG', parent_test), ('.ACC', accessory_test)):
        path = pathlib.Path(temporary) / ('parent' + suffix)
        header = struct.pack('>H6IH', 0x601a, len(code), 0, 0, 0, 0, 0, 1)
        path.write_bytes(header + code)
        result = subprocess.run([str(stout), str(path)], capture_output=True,
                                text=True, timeout=10)
        assert result.returncode == 0, result.stderr
    print('PASS: basepage parent distinguishes applications and accessories')

    # These compatibility probes are used by GEMBench and other common ST
    # programs before they create any windows. They must return through the
    # real trap vectors instead of falling into the guest illegal-opcode path.
    compatibility_calls = (
        ('BIOS Setexc', bytes.fromhex(
            '2f3cffffffff'            # move.l #-1,-(sp): inquire
            '3f3c0005'                # move.w #5,-(sp): vector
            '3f3c0005'                # move.w #5,-(sp): Setexc
            '4e4d4fef0008'            # trap #13; lea 8(sp),sp
            '70004e75')),
        ('VDI vq_gdos', bytes.fromhex(
            '70fe4e42'                # moveq #-2,d0; trap #2
            '0c80fffffffe'            # cmpi.l #-2,d0
            '6604'                    # bne.s failure
            '70004e75'                # success
            '70014e75')),              # failure
        ('XBIOS Setcolor', bytes.fromhex(
            '3f3c0123'                # move.w #0x123,-(sp): color
            '3f3c0000'                # move.w #0,-(sp): index
            '3f3c0007'                # move.w #7,-(sp): Setcolor
            '4e4e5c8f'                # trap #14; addq.l #6,sp
            '70004e75')),
        ('XBIOS Setpalette', bytes.fromhex(
            '2f08'                    # move.l a0,-(sp): 16-word palette
            '3f3c0006'                # move.w #6,-(sp): Setpalette
            '4e4e5c8f'                # trap #14; addq.l #6,sp
            '70004e75')),
        ('XBIOS Keytbl', bytes.fromhex(
            '2f3cffffffff' * 3 +      # caps, shift and normal: no change
            '3f3c0010'                # move.w #16,-(sp): Keytbl
            '4e4e4fef000e'            # trap #14; lea 14(sp),sp
            '70004e75')),
        ('XBIOS Setprt', bytes.fromhex(
            '3f3cffff'                # move.w #-1,-(sp): inquire
            '3f3c0021'                # move.w #33,-(sp): Setprt
            '4e4e588f'                # trap #14; addq.l #4,sp
            '70004e75')),
        ('XBIOS Blitmode', bytes.fromhex(
            '3f3cffff'                # move.w #-1,-(sp): inquire
            '3f3c0040'                # move.w #64,-(sp): Blitmode
            '4e4e588f'                # trap #14; addq.l #4,sp
            '4a40'                    # tst.w d0: no blitter is installed
            '6604'                    # bne.s failure
            '70004e75'                # success
            '70014e75')),
    )
    for name, code in compatibility_calls:
        path = pathlib.Path(temporary) / 'compat.prg'
        header = struct.pack('>H6IH', 0x601a, len(code), 0, 0, 0, 0, 0, 1)
        path.write_bytes(header + code)
        result = subprocess.run([str(stout), str(path)], capture_output=True,
                                text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        assert 'unsupported' not in result.stderr, result.stderr
        assert 'illegal opcode' not in result.stderr, result.stderr
        print('PASS:', name, 'compatibility probe')

    # Run Fcreate/Fwrite/Fclose from a program stored on A:. Relative output,
    # including missing parent directories, must be allocated in the same
    # FAT12 image and survive Stout shutdown. Real TOS callers remove their
    # own trap arguments, so the test also guards that stack ABI.
    code = bytes.fromhex(
        '3f3c0000'                  # move.w #0,-(sp): attributes
        '2f3c0000813e'              # move.l #filename,-(sp)
        '3f3c003c4e41'              # Fcreate; trap #1
        '4fef0008'                  # lea 8(sp),sp
        '3200'                      # move.w d0,d1: handle
        '2f3c0000814a'              # move.l #payload,-(sp)
        '2f3c00000005'              # move.l #5,-(sp)
        '3f01'                      # move.w d1,-(sp)
        '3f3c00404e41'              # Fwrite; trap #1
        '4fef000c'                  # lea 12(sp),sp
        '3f01'                      # move.w d1,-(sp)
        '3f3c003e4e41'              # Fclose; trap #1
        '4fef0004'                  # lea 4(sp),sp
        '70004e75')                 # moveq #0,d0; rts
    text = code + b'DIR/NEW.TXT\0HELLO'
    program = struct.pack('>H6IH', 0x601a, len(text), 0, 0, 0, 0, 0, 1) + text
    image = bytearray(720 * 512)
    struct.pack_into('<HBHBHHBHHH', image, 11,
                     512, 1, 1, 1, 16, 720, 0xf9, 3, 9, 1)
    image[512:517] = bytes.fromhex('f9ffffff0f')
    entry = bytearray(32)
    entry[:11] = b'CREATE  TOS'
    entry[11] = 0x20
    struct.pack_into('<H', entry, 26, 2)
    struct.pack_into('<I', entry, 28, len(program))
    image[4 * 512:4 * 512 + 32] = entry
    image[5 * 512:5 * 512 + len(program)] = program
    disk_path = pathlib.Path(temporary) / 'writable.st'
    disk_path.write_bytes(image)
    result = subprocess.run([str(stout), 'A:/CREATE.TOS', str(disk_path)],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    written = disk_path.read_bytes()
    directory = written[4 * 512 + 32:4 * 512 + 64]
    assert directory[:11] == b'DIR        ', directory
    assert directory[11] == 0x10, directory
    directory_cluster = struct.unpack_from('<H', directory, 26)[0]
    directory_offset = (5 + directory_cluster - 2) * 512
    created = written[directory_offset + 64:directory_offset + 96]
    assert created[:11] == b'NEW     TXT', created
    cluster = struct.unpack_from('<H', created, 26)[0]
    size = struct.unpack_from('<I', created, 28)[0]
    assert size == 5 and cluster >= 2
    assert written[(5 + cluster - 2) * 512:
                   (5 + cluster - 2) * 512 + size] == b'HELLO'
    print('PASS: nested relative Fcreate persisted into source ST image')

    # A host-launched program sees its containing directory as drive C:, just
    # as a mounted program sees its source image as drive A:. Historical
    # self-extractors also rely on Fcreate making their nested output parents.
    host_program = pathlib.Path(temporary) / 'CREATE.TOS'
    host_program.write_bytes(program)
    with tempfile.TemporaryDirectory(dir=workspace) as outside_name:
        outside = pathlib.Path(outside_name)
        (pathlib.Path(temporary) / 'DIR').symlink_to(
            outside, target_is_directory=True)
        result = subprocess.run([str(stout), str(host_program)],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        assert not (outside / 'NEW.TXT').exists()
        (pathlib.Path(temporary) / 'DIR').unlink()
    result = subprocess.run([str(stout), str(host_program)], capture_output=True,
                            text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    assert (pathlib.Path(temporary) / 'DIR/NEW.TXT').read_bytes() == b'HELLO'
    assert not pathlib.Path(f'{host_program}.files').exists()
    print('PASS: host Fcreate uses source directory and creates parents')

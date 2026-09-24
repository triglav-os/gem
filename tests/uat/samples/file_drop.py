#!/usr/bin/env python3
"""Exercise Copy and Move drops between two real File Manager windows.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""

import argparse
import json
from pathlib import Path
import shutil
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session


def double_click(session, x, y):
    session.event(3, x, y)
    for _ in range(2):
        session.event(10, x, y)
        session.event(11, x, y)
    session.event(3, 639, 399)
    session.pause()


def crop(frame, x, y, w, h):
    return bytes((frame[row * 80 + col // 8] >> (7 - col % 8)) & 1
                 for row in range(y, y + h) for col in range(x, x + w))


def modified_click(session, x, y, modifier):
    session.event(1, modifier)
    session.click(x, y)
    session.event(2, modifier)


def select_then_drag(session, source_x, source_y, target_x, target_y,
                     modifier=None, capture=None):
    """Start a drag while the preceding click is still a double-click candidate."""
    session.event(3, source_x, source_y)
    session.event(10, source_x, source_y)
    session.event(11, source_x, source_y)
    session.pause(.05)
    return drag(session, source_x, source_y, target_x, target_y,
                modifier=modifier, capture=capture)


def drag(session, source_x, source_y, target_x, target_y, modifier=None,
         capture=None):
    if modifier is not None:
        session.event(1, modifier)
    session.event(3, source_x, source_y)
    session.event(10, source_x, source_y)
    session.event(3, target_x, target_y)
    session.pause(.15)
    frame = session.frame(capture) if capture else None
    session.event(11, target_x, target_y)
    if modifier is not None:
        session.event(2, modifier)
    session.pause()
    return frame


def make_atari_disk(path):
    image = bytearray(720 * 512)
    struct.pack_into('<HBHBHHBHHH', image, 11,
                     512, 1, 1, 1, 16, 720, 0xf9, 3, 9, 1)
    image[510:512] = b'\x55\xaa'
    image[512:515] = bytes.fromhex('f9ffff')

    def set_fat(cluster, value):
        offset = 512 + cluster + cluster // 2
        pair = struct.unpack_from('<H', image, offset)[0]
        if cluster % 2:
            pair = (pair & 0x000f) | ((value & 0x0fff) << 4)
        else:
            pair = (pair & 0xf000) | (value & 0x0fff)
        struct.pack_into('<H', image, offset, pair)

    def entry(name, attr, cluster, size=0):
        result = bytearray(32)
        result[:11] = name
        result[11] = attr
        struct.pack_into('<H', result, 26, cluster)
        struct.pack_into('<I', result, 28, size)
        return result

    for cluster in range(2, 6):
        set_fat(cluster, 0x0fff)
    root = 4 * 512
    image[root:root + 32] = entry(b'ATARI      ', 0x10, 2)
    image[root + 32:root + 64] = entry(b'IMAGE   TXT', 0x20, 3, 10)
    image[root + 64:root + 96] = entry(b'HIDDEN     ', 0x12, 4)
    image[5 * 512:5 * 512 + 32] = entry(b'CHILD   TXT', 0x20, 5, 11)
    image[6 * 512:6 * 512 + 10] = b'ATARI FILE'
    image[8 * 512:8 * 512 + 11] = b'ATARI CHILD'
    path.write_bytes(image)


def check(session, source, destination, disk):
    # Configured locations are laid out vertically. Open Destination first;
    # Source then cascades on top while leaving Destination's left edge exposed.
    double_click(session, 52, 54)
    double_click(session, 52, 120)
    session.frame('two_file_managers')

    # Entry paths live in the same array that directory loading replaces.
    # Descending repeatedly verifies each target path was preserved before
    # the next directory snapshot overwrote that array.
    previous = session.frame('depth_root')
    double_click(session, 300, 126)  # 02_folder.
    current = session.frame('depth_02_folder')
    assert crop(current, 280, 62, 320, 16) != \
        crop(previous, 280, 62, 320, 16), 'First folder did not open'
    for depth in range(1, 5):
        previous = current
        double_click(session, 300, 126)  # Parent, then the only child folder.
        current = session.frame(f'depth_{depth}')
        assert crop(current, 280, 62, 320, 16) != \
            crop(previous, 280, 62, 320, 16), \
            f'Folder navigation stopped at depth {depth + 1}'
    for _ in range(5):
        double_click(session, 300, 108)  # Parent row.

    # Source has a parent row, its folder, then two alphabetically ordered
    # files. Shift-click selects the folder and copy file together. Dragging
    # either selected row must copy the complete selection without a dialog.
    modified_click(session, 300, 126, 225)  # Left Shift.
    modified_click(session, 300, 144, 225)
    copy_outline = drag(session, 300, 144, 230, 180,
                        capture='plain_copy_outline')
    assert (source / '00_copy.txt').read_text() == 'copy\n', \
        'Copy removed or changed the source'
    assert (destination / '00_copy.txt').read_text() == 'copy\n', \
        'Cross-window Copy did not create the destination'
    assert (source / '02_folder' / 'child.txt').read_text() == 'child\n', \
        'Multi-selection Copy removed or changed the source tree'
    assert (destination / '02_folder' / 'child.txt').read_text() == 'child\n', \
        'Shift-selected folder was not copied with the dragged file'

    # Control changes the operation to Move. The source row is positioned so
    # its translated outline exactly overlaps the previous primary outline;
    # the only local difference is the live M badge.
    move_outline = select_then_drag(
        session, 300, 162, 230, 180, modifier=224,
        capture='control_move_outline')  # Left Control.
    assert crop(move_outline, 258, 174, 12, 12) != \
        crop(copy_outline, 258, 174, 12, 12), \
        'Control-move badge was not added to the drag outline'
    assert not (source / '01_move.txt').exists(), \
        'Cross-window Move left the source in place'
    assert (destination / '01_move.txt').read_text() == 'move\n', \
        'Cross-window Move did not create the destination'

    # Control must not add the move badge over Trash; Trash retains its own
    # confirmation and operation semantics. Compare the exact would-be badge
    # region for otherwise identical plain and Control drags, cancelling both.
    plain_trash = drag(session, 300, 144, 592, 348,
                       capture='plain_trash_outline')
    session.key(40)  # Default Cancel.
    control_trash = drag(session, 300, 144, 592, 348, modifier=224,
                         capture='control_trash_outline')
    session.key(40)
    assert crop(control_trash, 620, 342, 11, 12) == \
        crop(plain_trash, 620, 342, 11, 12), \
        'Control move badge leaked onto the Trash target'

    # A mounted Atari image is a valid read-only drag source. Its hidden
    # directory is omitted, leaving ATARI and IMAGE.TXT as the first rows.
    disk_before = disk.read_bytes()
    double_click(session, 52, 186)
    modified_click(session, 300, 128, 225)
    modified_click(session, 300, 146, 225)
    drag(session, 300, 146, 230, 180, capture='mounted_copy_outline')
    assert (destination / 'IMAGE.TXT').read_bytes() == b'ATARI FILE', \
        'Mounted-image file was not copied to the Linux folder'
    assert (destination / 'ATARI' / 'CHILD.TXT').read_bytes() == \
        b'ATARI CHILD', 'Mounted-image directory was not copied recursively'
    assert not (destination / 'HIDDEN').exists(), \
        'Hidden mounted-image directory unexpectedly joined the selection'
    assert disk.read_bytes() == disk_before, \
        'Copying unexpectedly modified the mounted Atari image'
    session.frame('file_drop_complete')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.demo, args.capture = 'file_drop', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve() / ('file_drop_' + args.mode)
    session.path.mkdir(parents=True, exist_ok=True)
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path / 'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path / 'socket')
    source = session.path / 'source'
    destination = session.path / 'destination'
    shutil.rmtree(source, ignore_errors=True)
    shutil.rmtree(destination, ignore_errors=True)
    source.mkdir()
    destination.mkdir()
    (source / '00_copy.txt').write_text('copy\n')
    (source / '01_move.txt').write_text('move\n')
    (source / '.hidden.txt').write_text('hidden\n')
    (source / '02_folder').mkdir()
    (source / '02_folder' / 'child.txt').write_text('child\n')
    nested = source / '02_folder'
    for depth in range(1, 5):
        nested = nested / f'depth_{depth}'
        nested.mkdir()
    (nested / 'marker.txt').write_text('deep\n')
    disk = session.path / 'mounted.st'
    make_atari_disk(disk)
    config_home = session.path.parent / ('.file-drop-config-' + args.mode)
    shutil.rmtree(config_home, ignore_errors=True)
    session.env['XDG_CONFIG_HOME'] = str(config_home)
    config_dir = config_home / 'gem'
    config_dir.mkdir(parents=True)
    (config_dir / 'desktop.cfg').write_text(
        '[desktop]\nshow_trash = true\nshow_hidden = false\n\n'
        '[locations]\n' + str(destination) + ' = A Destination\n' +
        str(source) + ' = B Source\n' + str(disk) + ' = C Disk\n')
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session, source, destination, disk)
        finally:
            session.close()
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (session.path / 'result.json').write_text(
            json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()

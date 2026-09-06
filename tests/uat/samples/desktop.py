#!/usr/bin/env python3
"""Check desktop icons and file browsing with real Rasta mouse input.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session


def crop(frame, x, y, w, h):
    return bytes((frame[row * 80 + col // 8] >> (7 - col % 8)) & 1
                 for row in range(y, y + h) for col in range(x, x + w))


def double_click(session, x, y):
    session.event(3, x, y)
    for _ in range(2):
        session.event(10, x, y)
        session.event(11, x, y)
    session.event(3, 639, 399)
    session.pause()


def check(session):
    import re
    assets = (Path(__file__).resolve().parents[3] /
              'samples/src/desktop/desktop_assets.c').read_text()
    def words(name):
        body = assets.split('static const UWORD ' + name + '[] = {')[1]
        return [int(v, 16) for v in re.findall(r'0x[0-9a-f]+', body.split('};')[0])]
    def matches(frame, name, x, y):
        mask, data = words(name + '_mask'), words(name + '_data')
        pixels = crop(frame, x, y, 32, 32)
        return all(pixels[row * 32 + col] == bool(data[row * 2 + col // 16] & (0x8000 >> (col % 16)))
                   for row in range(32) for col in range(32)
                   if mask[row * 2 + col // 16] & (0x8000 >> (col % 16)))
    initial = session.frame('desktop_icons')
    positions = [(36 + col * 78, 38 + row * 66)
                 for col in range(7) for row in range(5)]
    folders = [(x, y) for x, y in positions if matches(initial, 'folder', x, y)]
    assert len(folders) == 1, 'Workspace folder icon is missing'
    assert any(matches(initial, 'disk', x, y) for x, y in positions), 'Disk icons missing'
    assert matches(initial, 'trash', 576, 332), 'Trash icon missing'
    # With no browser windows, Desk contains only its information row.
    session.event(3, 20, 10)
    session.event(10, 20, 10)
    menu = session.frame('desk_empty')
    assert crop(menu, 4, 48, 165, 130) == crop(initial, 4, 48, 165, 130), 'Desk reserves blank rows for unused slots'
    session.event(3, 80, 32)
    session.event(11, 80, 32)
    info = session.frame('desktop_info')
    assert crop(info, 230, 150, 180, 90) != crop(initial, 230, 150, 180, 90), 'Desktop info did not open a dialog'
    session.key(40)
    dismissed = session.frame('desktop_info_closed')
    assert crop(dismissed, 230, 150, 180, 90) == crop(initial, 230, 150, 180, 90), 'Desktop info did not close'
    x, y = folders[0]
    double_click(session, x + 16, y + 16)
    opened = session.frame('workspace_open')
    assert crop(opened, 240, 42, 280, 16) != crop(initial, 240, 42, 280, 16), 'No file manager title'
    session.event(3, 20, 10)
    session.event(10, 20, 10)
    populated = session.frame('desk_one_window')
    assert crop(populated, 4, 90, 165, 80) == crop(opened, 4, 90, 165, 80), 'Desk reserves blank rows below its browser entry'
    session.event(3, 630, 380)
    session.event(11, 630, 380)
    session.pause()
    double_click(session, 300, 89)  # First child directory, after the parent row.
    child = session.frame('child_open')
    assert crop(child, 240, 42, 280, 16) != crop(opened, 240, 42, 280, 16), 'Folder navigation did not change title'
    assert crop(child, 240, 100, 280, 140) != crop(opened, 240, 100, 280, 140), 'Directory contents did not change'
    double_click(session, 300, 71)  # Parent directory.
    parent = session.frame('parent_open')
    assert crop(parent, 240, 42, 280, 16) == crop(opened, 240, 42, 280, 16), 'Parent navigation failed'
    session.click(229, 50)  # Close file manager.
    closed = session.frame('browser_closed')
    assert crop(closed, 240, 42, 280, 220) == crop(initial, 240, 42, 280, 220), 'Desktop was not restored after close'
    session.event(3, 75, 10)
    session.event(10, 75, 10)
    file_menu = session.frame('file_menu')
    assert crop(file_menu, 62, 48, 60, 120) == crop(closed, 62, 48, 60, 120), 'File contains unused commands'
    session.event(3, 85, 31)
    session.event(11, 85, 31)
    reopened = session.frame('workspace_file_open')
    assert crop(reopened, 240, 42, 280, 16) == crop(opened, 240, 42, 280, 16), 'File Open did not reopen Workspace'
    session.click(229, 50)
    print('PASS disk, Workspace and Trash icons, double-click, folder/parent navigation and close')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.demo, args.capture = 'desktop', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve()/f'desktop_{args.mode}'
    session.path.mkdir(parents=True, exist_ok=True)
    # Session environment was prepared for the original generated path.
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path/'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path/'socket')
    (session.path/'uat_child').mkdir(exist_ok=True)
    (session.path/'uat_child'/'marker.txt').write_text('Desktop UAT fixture\n')
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session)
        finally:
            session.close()
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (session.path/'result.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()

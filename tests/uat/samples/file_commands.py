#!/usr/bin/env python3
"""Exercise native File Manager commands and clipboard against real files.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools/scripts'))
from docker_context import ensure_container
ensure_container()
import argparse
import json
import shutil
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session
from file_drop import double_click
from dialog_checks import check_dialog, pixel


def shortcut(session, scan, shift=False):
    session.event(1, 224)
    if shift:
        session.event(1, 225)
    session.key(scan)
    if shift:
        session.event(2, 225)
    session.event(2, 224)


def type_text(session, text):
    for char in text:
        scan = ord(char) - ord('a') + 4 if char.isalpha() else {'.': 55, '/': 56}[char]
        session.event(1, scan)
        session.event(2, scan)


def popup(session, row, x=300, y=108, capture=None):
    session.event(3, x, y)
    session.event(30, x, y)
    session.event(31, x, y)
    # 15 command rows and three 8-pixel group separators (after Empty Trash,
    # Paste and New CSV File) are clamped to the 400-pixel screen bottom.
    px, py = min(x, 404), min(y, 72)
    if capture:
        for index in range(3):
            frame = session.frame(capture + (f'_{index}' if index else ''))
            assert all(pixel(frame, px+i, py) for i in range(236)), 'Popup top border missing'
            assert all(pixel(frame, px, py+i) for i in range(304)), 'Popup left border missing'
    separators = sum(row > last for last in (3, 7, 12))
    session.click(px + 80, py + row * 20 + separators * 8 + 12)


def region(frame, x, y, w, h):
    return bytes(pixel(frame, x + col, y + row)
                 for row in range(h) for col in range(w))


def name(session, text):
    type_text(session, text)  # The naming form selects its initial text.
    session.key(40)


def drag_icon(session, x, y, tx, ty):
    for kind, ex, ey in ((3, x, y), (10, x, y), (3, x + 8, y + 8),
                         (3, tx, ty), (11, tx, ty)):
        session.event(kind, ex, ey)
    session.pause(.5)


def check(session, work, data):
    # Desktop icons drag onto the grid and keep their cell. Dropping a little
    # off a cell snaps to it; the icon is then dragged back home.
    before = session.frame('icon_drag_before')
    drag_icon(session, 52, 54, 52 + 3 * 78 + 9, 54 + 2 * 66 - 7)
    moved = session.frame('icon_dragged')
    positions = session.path/'config/gem/desktop-icons.cfg'
    assert f'{work} = 3,2' in positions.read_text(), 'Icon cell was not saved'
    assert region(moved, 16, 36, 72, 58) != region(before, 16, 36, 72, 58), \
        'Dragged icon left its old cell drawn'
    assert region(moved, 16 + 3 * 78, 36 + 2 * 66, 72, 58) != \
        region(before, 16 + 3 * 78, 36 + 2 * 66, 72, 58), 'Icon not drawn in its new cell'
    drag_icon(session, 52 + 3 * 78, 54 + 2 * 66, 50, 50)
    assert f'{work} = 0,0' in positions.read_text(), 'Icon did not return home'
    session.click(400, 300)  # Deselect, then park the pointer off the icons.
    session.event(3, 639, 399)
    assert region(session.frame('icon_home'), 0, 20, 640, 380) == \
        region(before, 0, 20, 640, 380), 'Desktop differs after dragging back'
    double_click(session, 52, 54)
    session.click(300, 108)  # First file, using only normal left-click selection.
    session.key(76)
    check_dialog(session)
    session.key(30)  # Explicit first confirmation button.
    assert not (work/'initial.txt').exists(), 'Delete acted on the desktop location instead of the file'
    popup(session, 3, 500, 240)  # Empty this test's Trash before the batch checks.
    session.key(30)  # Explicit first confirmation button.
    # Right-click blank space and create a folder, then exercise all templates.
    popup(session, 8, 500, 240, 'folder_context')
    check_dialog(session)
    name(session, 'target')
    assert (work/'target').is_dir(), 'Context menu New Folder failed'
    for row, filename in ((9, 'note.txt'), (10, 'note.md'), (11, 'page.html'), (12, 'table.csv')):
        popup(session, row, 500, 240)
        name(session, filename)
        assert (work/filename).is_file(), f'Failed to create {filename}'
    assert (work/'page.html').read_text().startswith('<!doctype html>')
    assert (work/'note.md').read_text().startswith('# ')
    assert (work/'table.csv').read_text() == 'name,value\n'
    # Sorting places the parent, target folder, then note.md and note.txt.
    popup(session, 4, 300, 144, 'rename_context')
    name(session, 'renamed.txt')
    assert not (work/'note.txt').exists() and (work/'renamed.txt').exists(), 'Rename failed'
    # Add to Desktop (last row) places note.md below the Workspace icon and
    # persists it as a location. A second request does not duplicate it.
    before = session.frame('add_desktop_before')
    popup(session, 14, 300, 126, 'add_desktop_context')
    session.pause(.5)
    added = session.frame('add_desktop_after')
    config = (session.path/'config/gem/desktop.cfg').read_text()
    assert config.count(str(work/'note.md')) == 1, 'Add to Desktop did not save note.md'
    assert region(added, 0, 96, 96, 70) != region(before, 0, 96, 96, 70), \
        'Add to Desktop did not show a desktop icon'
    popup(session, 14, 300, 126)
    session.pause(.5)
    config = (session.path/'config/gem/desktop.cfg').read_text()
    assert config.count(str(work/'note.md')) == 1, 'Add to Desktop duplicated note.md'
    # Moving the top icon leaves its cell empty: note.md below must not move
    # up, and no stale copy of either icon may remain.
    stacked = session.frame('icons_stacked')
    drag_icon(session, 52, 54, 52 + 2 * 78, 54)
    session.event(3, 639, 399)
    spaced = session.frame('icons_spaced')
    layout = (session.path/'config/gem/desktop-icons.cfg').read_text()
    assert f'{work} = 2,0' in layout and f"{work/'note.md'} = 0,1" in layout, \
        'Moving an icon did not pin the others'
    assert region(spaced, 16, 102, 72, 58) == region(stacked, 16, 102, 72, 58), \
        'The icon below moved into the vacated cell'
    assert region(spaced, 16, 36, 72, 58) == region(spaced, 16, 168, 72, 58), \
        'The vacated cell still shows an icon'
    # Collision and traversal rejection leave the source and destination intact.
    popup(session, 4, 300, 162)
    name(session, 'page.html')
    session.key(40)
    assert (work/'renamed.txt').exists() and (work/'page.html').read_text().startswith('<!doctype')
    popup(session, 4, 300, 162)
    name(session, '../escape.txt')
    session.key(40)
    assert not (work.parent/'escape.txt').exists(), 'Naming allowed parent traversal'
    # Selecting the parent as well must not make Rename act on that parent.
    session.click(300, 162)
    session.event(1, 225)
    session.click(300, 90)
    session.event(2, 225)
    session.key(59)  # F2.
    name(session, 'renamed.txt')
    assert (work/'renamed.txt').exists() and work.is_dir(), 'Rename targeted the parent'
    # Copy survives navigation. Repeated paste cannot replace the first copy.
    popup(session, 6, 300, 162)
    double_click(session, 300, 108)
    shortcut(session, 25)
    assert (work/'target/renamed.txt').exists(), 'Copy/Paste failed'
    shortcut(session, 25)
    session.key(40)
    assert (work/'renamed.txt').exists(), 'Copy removed its source'
    double_click(session, 300, 90)
    popup(session, 5, 300, 126)  # Cut note.md.
    double_click(session, 300, 108)
    shortcut(session, 25)
    assert not (work/'note.md').exists() and (work/'target/note.md').exists(), 'Cut/Paste failed'
    # Select All includes both children but never the parent directory.
    shortcut(session, 4)
    session.key(76)
    session.key(40)  # Default Cancel must leave both files intact.
    assert len(list((work/'target').iterdir())) == 2
    session.key(76)
    session.key(30)  # Explicit first confirmation button.
    assert not list((work/'target').iterdir()), 'Multi-selection Delete failed'
    assert (work/'target').is_dir(), 'Select All included the parent'
    double_click(session, 592, 348)
    # Trash root has no parent: its first row is note.md, second renamed.txt.
    popup(session, 5, 320, 110, 'trash_context')
    session.click(251, 72)  # Close Trash, exposing target folder.
    session.click(300, 160)
    shortcut(session, 25)
    assert (work/'target/note.md').exists(), 'Cut out of Trash failed'
    info = list((data/'Trash/info').glob('*.trashinfo'))
    assert len(info) == 1, 'Cut out left stale Trash metadata'
    session.frame('file_commands_complete')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.demo, args.capture = 'file_commands', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve()/('file_commands_' + args.mode)
    session.path.mkdir(parents=True, exist_ok=True)
    for key, leaf in (('GEM_RASTA_FRAMEBUFFER', 'framebuffer'), ('GEMD_SOCKET', 'socket'),
                      ('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data')):
        session.env[key] = str(session.path/leaf)
    work, data = session.path/'work', session.path/'data'
    for directory in (work, data, session.path/'config'):
        shutil.rmtree(directory, ignore_errors=True)
        directory.mkdir()
    (work/'initial.txt').write_text('left-click delete\n')
    config = session.path/'config/gem'
    config.mkdir()
    (config/'desktop.cfg').write_text('[locations]\n' + str(work) + ' = Workspace\n')
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session, work, data)
        finally:
            session.close()
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (session.path/'result.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()

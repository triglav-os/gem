#!/usr/bin/env python3
"""Exercise File Manager drops and destructive Trash confirmation dialogs.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""

import argparse
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session


def double_click(session, x, y):
    session.event(3, x, y)
    for _ in range(2):
        session.event(10, x, y)
        session.event(11, x, y)
    session.pause()


def crop(frame, x, y, w, h):
    return bytes((frame[row * 80 + col // 8] >> (7 - col % 8)) & 1
                 for row in range(y, y + h) for col in range(x, x + w))


def drag(session, source_x, source_y, target_x, target_y, capture=None):
    session.event(3, source_x, source_y)
    session.event(10, source_x, source_y)
    if capture:
        session.event(3, source_x + 5, source_y)
        source = session.frame(capture + '_outline_source')
    else:
        source = None
    session.event(3, target_x, target_y)
    session.pause(.15)
    if capture:
        moved = session.frame(capture + '_outline_moved')
        assert moved != source, 'Drag outline did not follow the pointer'
    else:
        moved = None
    session.event(11, target_x, target_y)
    session.pause()
    return moved


def select_for_immediate_drag(session, x, y):
    session.event(3, x, y)
    session.event(10, x, y)
    session.event(11, x, y)
    session.pause(.05)


def choose_file_item(session, y):
    session.event(3, 100, 10)
    session.event(10, 100, 10)
    session.event(3, 120, y)
    session.event(11, 120, y)
    session.pause()


def toggle_icon_view(session, capture=None):
    session.event(3, 150, 10)
    session.event(10, 150, 10)
    if capture:
        session.frame(capture)
    session.event(3, 150, 32)
    session.event(11, 150, 32)
    session.pause()


def check(session, fixture, folder, open_folder, blocked_folder, xdg):
    double_click(session, 52, 54)  # Configured Workspace location.

    # A stationary double-click in icon view must open rather than drag.
    root = session.frame('list_root')
    toggle_icon_view(session, capture='arrange_popup')
    icons = session.frame('icon_root')
    assert icons != root, 'Arrange -> Show as icons did not change the view'
    double_click(session, 368, 94)  # First folder after the parent icon.
    child = session.frame('icon_folder_open')
    assert crop(child, 240, 42, 280, 16) != crop(icons, 240, 42, 280, 16), \
        'Icon-view folder double-click was treated as a drag'
    double_click(session, 272, 94)  # Parent icon.

    # Force a real move failure, dismiss its alert, then immediately open a
    # different folder. The failed operation must not poison later clicks.
    workspace = folder.parent
    workspace_mode = workspace.stat().st_mode
    workspace.chmod(0o555)
    try:
        drag(session, 272, 164, 592, 348)
        session.click(274, 224)  # Confirm the move; rename must fail.
        workspace.chmod(workspace_mode)
        failed = session.frame('folder_drop_failed')
        assert blocked_folder.exists(), 'Failed move unexpectedly removed source'
        assert crop(failed, 150, 150, 340, 100) != \
            crop(icons, 150, 150, 340, 100), 'Move failure alert did not open'
        session.click(320, 224)  # Dismiss Cannot move.
    finally:
        workspace.chmod(workspace_mode)

    before_open = session.frame('after_failed_drop')
    double_click(session, 464, 94)
    after_open = session.frame('post_failure_folder_open')
    assert crop(after_open, 240, 42, 280, 16) != \
        crop(before_open, 240, 42, 280, 16), \
        'Double-click stopped working after a failed Trash move'
    double_click(session, 272, 94)  # Parent icon.
    shutil.rmtree(open_folder)
    shutil.rmtree(blocked_folder)

    # The test folder sorts before xdg. Drag its icon-and-title T contour.
    before = session.frame('before_folder_drop')
    select_for_immediate_drag(session, 368, 94)
    hover = drag(session, 368, 94, 592, 348, capture='icon_drag')
    assert crop(hover, 568, 368, 64, 24) != \
        crop(before, 568, 368, 64, 24), \
        'Trash title was not inverted while the drag button was held'
    pending = session.frame('folder_drop_pending')
    assert crop(pending, 568, 368, 64, 24) != \
        crop(before, 568, 368, 64, 24), 'Trash title was not inverted'
    session.click(274, 224)
    folder_payload = xdg / 'Trash' / 'files' / folder.name
    folder_info = xdg / 'Trash' / 'info' / (folder.name + '.trashinfo')
    assert not folder.exists(), 'Confirmed folder drop left the source in place'
    assert (folder_payload / 'child.txt').exists(), 'Folder payload is incomplete'
    assert folder_info.exists(), 'Folder trashinfo is missing'
    toggle_icon_view(session)

    # After the folder is removed, xdg is first and the fixture is first file.
    before_list_drag = session.frame('before_list_drag')
    select_for_immediate_drag(session, 300, 126)
    list_hover = drag(session, 300, 126, 592, 348, capture='list_drag')
    assert crop(list_hover, 568, 368, 64, 24) != \
        crop(before_list_drag, 568, 368, 64, 24), \
        'Trash title was not inverted during a list-row drag'
    session.frame('drop_cancel')
    session.key(40)  # Return activates the default Cancel button.
    assert fixture.exists(), 'Default Cancel moved the dropped item'

    workspace_before_move = session.frame('workspace_before_file_move')
    drag(session, 300, 126, 592, 348)
    session.click(274, 224)  # Trash, the non-default left button.
    moved_workspace = session.frame('file_drop_result')
    payload = xdg / 'Trash' / 'files' / fixture.name
    info = xdg / 'Trash' / 'info' / (fixture.name + '.trashinfo')
    assert not fixture.exists(), 'Confirmed drop left the source in place'
    assert payload.exists() and info.exists(), 'Trash payload/info pair missing'
    assert crop(moved_workspace, 220, 100, 360, 54) != \
        crop(workspace_before_move, 220, 100, 360, 54), \
        'Open source File Manager did not refresh after the move'

    double_click(session, 592, 348)  # Open virtual Trash beside Workspace.
    opened = session.frame('trash_open')
    assert sum(opened) > 0, 'Trash browser did not render'

    # Restore the file and verify that the still-open Workspace browser receives
    # the same directory-change notification before it is exposed again.
    session.click(350, 128)  # File row below the folder in Trash.
    choose_file_item(session, 72)  # File -> Restore.
    assert fixture.exists(), 'Restore did not return the file'
    assert not payload.exists() and not info.exists(), \
        'Restore left the Trash payload/info pair behind'
    session.click(251, 72)  # Close Trash to expose the original browser.
    restored_workspace = session.frame('workspace_after_restore')
    assert crop(restored_workspace, 220, 100, 360, 54) != \
        crop(moved_workspace, 220, 100, 360, 54), \
        'Open destination File Manager did not refresh after Restore'

    double_click(session, 592, 348)  # Reopen Trash for Empty Trash checks.
    session.click(230, 126)  # Make the exposed source browser active again.

    choose_file_item(session, 92)  # File -> Empty Trash.
    session.frame('empty_cancel')
    session.key(40)
    assert folder_payload.exists() and folder_info.exists(), \
        'Default Cancel emptied Trash'

    choose_file_item(session, 92)
    session.click(274, 224)  # Empty, the non-default left button.
    assert not folder_payload.exists() and not folder_info.exists(), \
        'Confirmed Empty failed'
    session.frame('trash_emptied')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.demo, args.capture = 'trash', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve() / ('trash_' + args.mode)
    session.path.mkdir(parents=True, exist_ok=True)
    xdg = session.path / 'xdg'
    shutil.rmtree(xdg, ignore_errors=True)
    xdg.mkdir()
    fixture = session.path / '00_trash_me.txt'
    fixture.write_text('Trash UAT fixture\n')
    folder = session.path / '00_trash_folder'
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir()
    (folder / 'child.txt').write_text('Trash folder child\n')
    open_folder = session.path / '01_open_folder'
    shutil.rmtree(open_folder, ignore_errors=True)
    open_folder.mkdir()
    (open_folder / 'child.txt').write_text('Open-after-failure child\n')
    blocked_folder = session.path / '02_tools'
    shutil.rmtree(blocked_folder, ignore_errors=True)
    (blocked_folder / 'scripts').mkdir(parents=True)
    (blocked_folder / 'scripts' / 'tool.sh').write_text('#!/bin/sh\n')
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path / 'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path / 'socket')
    session.env['XDG_DATA_HOME'] = str(xdg)
    shutil.rmtree(session.path / 'config', ignore_errors=True)
    shutil.rmtree(session.path / '.config', ignore_errors=True)
    config_home = session.path.parent / ('.trash-config-' + args.mode)
    session.env['XDG_CONFIG_HOME'] = str(config_home)
    config_dir = config_home / 'gem'
    config_dir.mkdir(parents=True, exist_ok=True)
    (config_dir / 'desktop.cfg').write_text(
        '[desktop]\nshow_trash = true\nshow_hidden = false\n\n'
        '[locations]\n' + str(session.path) + ' = Workspace\n')
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session, fixture, folder, open_folder, blocked_folder, xdg)
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

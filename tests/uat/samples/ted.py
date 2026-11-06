#!/usr/bin/env python3
"""Exercise Ted's editing, selection, undo, save and unsaved-change guard.

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
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session
from file_commands import shortcut, type_text
from gemread import field, crop

BAND = 20


def toolbar(session, slot):
    x, y, _, _ = field(session, 1, 5)
    session.click(x + slot*BAND + BAND//2, y + BAND//2)


def check_chrome(session, frame, work):
    x, y, w, h = work
    assert sum(crop(frame, x, y+BAND-1, w, 1)) == w, \
        "Toolbar separator is not aligned with the shared 20-pixel band"
    assert sum(crop(frame, x, y+h-BAND, w, 1)) == w, \
        "Status separator is not aligned with the shared 20-pixel band"
    for slot in range(min(8, w//BAND)):
        left = x + slot*BAND
        assert 5 < sum(crop(frame, left+2, y+2, 16, 16)) < 200, "Toolbar icon missing"
        assert sum(crop(frame, left, y, BAND-1, 2)) == 0, "Icon exceeds its 16x16 area"
    visible = min(8, w//BAND)*BAND
    if w > visible:
        assert sum(crop(frame, x+visible, y, w-visible, BAND-1)) == 0, "Toolbar text or clipped icon in unused space"


def check(session, document):
    initial = session.frame('ted_open')
    work = field(session, 1, 5)
    check_chrome(session, initial, work)
    assert sum(crop(initial, work[0], work[1]+BAND, 200, 40)) > 10
    session.event(3, work[0]+10, work[1]+10)
    hint = session.frame('ted_toolbar_hint')
    assert crop(hint, work[0], work[1]+work[3]-BAND, work[2], BAND) != crop(initial, work[0], work[1]+work[3]-BAND, work[2], BAND), 'Toolbar status hint missing'
    toolbar(session, 7)  # Disabled Paste cannot change the document.
    session.click(work[0]+10, work[1]+BAND+7)
    shortcut(session, 4)
    type_text(session, 'first')
    session.key(40)
    type_text(session, 'second')
    toolbar(session, 2)
    session.frame('ted_after_save')
    assert document.read_text() == 'first\nsecond', 'Typing or save failed'
    shortcut(session, 4)
    toolbar(session, 6)
    session.key(79)
    session.key(77)
    session.key(40)
    toolbar(session, 7)
    shortcut(session, 22)
    assert document.read_text() == 'first\nsecond\nfirst\nsecond', 'Copy/paste failed'
    toolbar(session, 4)
    shortcut(session, 22)
    assert document.read_text() == 'first\nsecond\n', 'Undo failed'
    shortcut(session, 4)
    toolbar(session, 5)
    shortcut(session, 22)
    assert document.read_bytes() == b'', 'Cut failed'
    shortcut(session, 29)
    shortcut(session, 22)
    assert document.read_text() == 'first\nsecond\n', 'Undo cut failed'
    # Mouse selection of the first word, replacing it without touching line 2.
    x, y = work[0]+4, work[1]+BAND
    session.event(3, x, y+5)
    session.event(10, x, y+5)
    session.event(3, x+40, y+5)
    session.event(11, x+40, y+5)
    type_text(session, 'hello')
    shortcut(session, 22)
    assert document.read_text() == 'hello\nsecond\n', 'Mouse selection failed'
    type_text(session, 'x')
    shortcut(session, 20)
    session.key(41)  # Cancel close without discarding edits.
    assert session.demo.poll() is None, 'Cancel lost unsaved edits'
    shortcut(session, 22)
    assert document.read_text() == 'hellox\nsecond\n'
    session.frame('ted_edited')
    toolbar(session, 3)
    chooser = field(session, 0, 10)[0]
    assert chooser != 1, 'Save As icon did not open its chooser'
    cx, cy, cw, ch = field(session, chooser, 5)
    session.click(cx+110, cy+ch-20)
    # The common file chooser edits its filename at the end of the text.
    for _ in range(len(document.name)):
        session.event(1, 42)
        session.event(2, 42)
    type_text(session, 'saved.text')
    session.click(cx+cw-164, cy+ch-20)
    saved = document.parent/'saved.text'
    session.wait(saved.exists, 'Save As output')
    assert saved.read_text() == document.read_text(), 'Save As lost text'
    toolbar(session, 0)
    empty = session.frame('ted_new')
    assert sum(crop(empty, work[0]+5, work[1]+BAND, 200, 40)) < 10, 'New kept old text'
    toolbar(session, 1)
    chooser = field(session, 0, 10)[0]
    cx, cy, cw, ch = field(session, chooser, 5)
    session.click(cx+110, cy+ch-20)
    type_text(session, 'saved.text')
    session.click(cx+cw-164, cy+ch-20)
    session.pause(.2)
    shortcut(session, 4)
    type_text(session, 'reopened')
    shortcut(session, 22)
    assert saved.read_text() == 'reopened', 'Open did not select the saved document'
    # Narrow resize keeps full icon slots clickable and both bands aligned.
    outer = field(session, 1, 4)
    sx, sy = outer[0]+outer[2]-5, outer[1]+outer[3]-5
    for kind, x, y in ((3,sx,sy), (10,sx,sy), (3,outer[0]+142,outer[1]+150),
                       (11,outer[0]+142,outer[1]+150)):
        session.event(kind, x, y)
    session.wait(lambda: field(session, 1, 4)[2:] != outer[2:], 'Ted resize')
    resized = session.frame('ted_narrow_toolbar')
    small = field(session, 1, 5)
    check_chrome(session, resized, small)
    toolbar(session, 1)
    assert field(session, 0, 10)[0] != 1, 'Resized Open icon lost its click target'
    session.key(41)
    shortcut(session, 20)
    session.wait(lambda: session.demo.poll() is not None, 'Ted clean shutdown')
    assert session.demo.returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--'+name, required=True)
    args = parser.parse_args()
    args.mode, args.demo, args.capture = 'proxy', 'ted', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve()/'ted_proxy'
    session.path.mkdir(parents=True, exist_ok=True)
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path/'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path/'socket')
    document = session.path/'fixture.text'
    document.write_text('original\n')
    (session.path/'saved.text').unlink(missing_ok=True)
    args.binary_args = [str(document)]
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session, document)
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

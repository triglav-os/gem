"""Shared pixel and placement checks for titled modal GEM windows.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import struct


def field(session, handle, number):
    status, data = session.rpc(33, struct.pack('=hh', handle, number))
    assert status == 1
    return struct.unpack('=4h', data)


def pixel(frame, x, y):
    return (frame[y*80+x//8] >> (7-x%8)) & 1


def check_standard_border(frame, work, outer):
    x, y, w, h = work
    for inset in range(5):
        assert pixel(frame, x+50, y+inset) == 0, 'Extra top dialog border'
        assert pixel(frame, x+50, y+h-1-inset) == 0, 'Extra bottom dialog border'
        assert pixel(frame, x+inset, y+20) == 0, 'Extra left dialog border'
        assert pixel(frame, x+w-1-inset, y+20) == 0, 'Extra right dialog border'
    ox, oy, ow, oh = outer
    for px, py in ((x+50,y-1), (x+50,oy+oh-1), (ox,y+20), (ox+ow-1,y+20)):
        assert pixel(frame, px, py) == 1, 'Standard window border missing'


def check_dialog(session, parent=1):
    if session.args.mode != 'proxy':
        return
    handle = field(session, 0, 10)[0]
    assert handle != parent and field(session, handle, 1)[0] & 0x000b == 0x000b
    outer = field(session, handle, 4)
    area = field(session, parent, 4)
    expected_x = max(0, min(640-outer[2], area[0]+(area[2]-outer[2])//2))
    expected_y = max(22, min(400-outer[3], area[1]+(area[3]-outer[3])//2))
    assert outer[:2] == (expected_x, expected_y), (outer, area)
    work = field(session, handle, 5)
    before = session.frame('modal_centered')
    check_standard_border(before, work, outer)
    x, y = outer[0]+100, outer[1]+10
    for kind, px, py in ((3,x,y), (10,x,y), (3,x-10,y+15), (11,x-10,y+15)):
        session.event(kind, px, py)
    session.wait(lambda: field(session, handle, 4)[:2] != outer[:2], 'modal dialog movement')
    session.event(3, 639, 399)
    moved = session.frame('modal_moved')
    work = field(session, handle, 5)
    check_standard_border(moved, work, field(session, handle, 4))

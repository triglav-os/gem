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
    assert [pixel(before, work[0]+50, work[1]+i) for i in range(5)] == [1,1,0,1,0]
    x, y = outer[0]+100, outer[1]+10
    for kind, px, py in ((3,x,y), (10,x,y), (3,x-10,y+15), (11,x-10,y+15)):
        session.event(kind, px, py)
    session.wait(lambda: field(session, handle, 4)[:2] != outer[:2], 'modal dialog movement')
    session.event(3, 639, 399)
    moved = session.frame('modal_moved')
    work = field(session, handle, 5)
    assert [pixel(moved, work[0]+50, work[1]+i) for i in range(5)] == [1,1,0,1,0]

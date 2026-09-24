#!/usr/bin/env python3
"""Exercise Gemscape CEF clicks, navigation, activation and GEM sliders.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session


def crop(frame, x, y, w, h):
    return bytes((frame[row * 80 + col // 8] >> (7 - col % 8)) & 1
                 for row in range(y, y + h) for col in range(x, x + w))


def field(session, handle, number):
    status, data = session.rpc(33, struct.pack('=hh', handle, number))
    assert status == 1
    return struct.unpack('=4h', data)


def window_at(session, x, y):
    handle, data = session.rpc(36, struct.pack('=hh', x, y))
    assert data == b''
    return handle


def type_text(session, text):
    shifted = {'!': 30, '@': 31, '#': 32, '$': 33, '%': 34, '^': 35,
               '&': 36, '*': 37, '(': 38, ')': 39, '_': 45, '+': 46,
               '{': 47, '}': 48, '|': 49, ':': 51, '"': 52, '~': 53,
               '<': 54, '>': 55, '?': 56}
    plain = {'1': 30, '2': 31, '3': 32, '4': 33, '5': 34, '6': 35,
             '7': 36, '8': 37, '9': 38, '0': 39, ' ': 44, '-': 45,
             '=': 46, '[': 47, ']': 48, '\\': 49, ';': 51, "'": 52,
             '`': 53, ',': 54, '.': 55, '/': 56}
    for char in text:
        if 'a' <= char <= 'z':
            scan, modifier = ord(char) - ord('a') + 4, None
        elif char in shifted:
            scan, modifier = shifted[char], 225
        elif char in plain:
            scan, modifier = plain[char], None
        else:
            raise AssertionError(f'Unsupported Gemscape UAT character {char!r}')
        if modifier is None:
            session.key(scan)
        else:
            session.event(1, modifier, 0)
            session.key(scan)
            session.event(2, modifier, 0)


def wait_frame_change(session, before, name, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        after = session.frame(name)
        if after != before:
            return after
        session.pause(.1)
    raise AssertionError(f'{name}: framebuffer did not change')


def check(session, cover_binary, next_page):
    initial = session.frame('gemscape_initial')
    browser = 1
    assert field(session, browser, 4)[2] > 0, 'Gemscape window is missing'
    session.wait(lambda: field(session, browser, 16)[0] < 1000 and
                  field(session, browser, 15)[0] < 1000,
                  'CEF page metrics to resize both GEM slider thumbs', 8)

    session.event(3, 535, 335)
    cursor_left = session.frame('gemscape_cursor_left')
    session.event(3, 585, 335)
    cursor_right = session.frame('gemscape_cursor_right')
    assert sum(crop(cursor_left, 530, 330, 24, 24)) > \
        sum(crop(cursor_right, 530, 330, 24, 24)) + 3, \
        'GEM cursor was not visible above the CEF overlay at its old position'
    assert sum(crop(cursor_right, 580, 330, 24, 24)) > \
        sum(crop(cursor_left, 580, 330, 24, 24)) + 3, \
        'GEM cursor was not visible above the CEF overlay at its new position'

    session.click(200, 125)
    clicked = wait_frame_change(session, initial, 'gemscape_button_clicked')
    assert sum(crop(clicked, 98, 212, 374, 105)) > 25000, \
        'CEF web button did not update its result area'

    session.click(400, 125)
    type_text(session, 'page.y-_=+[]{}\\|;:\'"`~,<>/?09')
    page_typed = wait_frame_change(session, clicked, 'gemscape_page_typed')
    assert sum(crop(page_typed, 355, 175, 270, 70)) > 15000, \
        'Keyboard text was not delivered to the clicked CEF page input'

    session.click(636, 300)
    session.wait(lambda: field(session, browser, 9)[0] > 0,
                  'CEF scroll position to update the GEM vertical slider', 8)
    scrolled = wait_frame_change(session, page_typed, 'gemscape_gem_scrolled')

    session.click(300, 58)
    target = next_page.as_uri() + '?y'
    type_text(session, target)
    session.key(40)
    navigated = wait_frame_change(session, scrolled, 'gemscape_enter_navigated')
    session.pause(.8)
    navigated = session.frame('gemscape_enter_navigated')
    assert sum(crop(navigated, 74, 69, 550, 280)) > 100000, \
        'Address edit containing y plus Enter did not navigate to the dark page'

    cover = session.start('gemscape_cover', [cover_binary])
    session.pause(1)
    covered = session.frame('gemscape_covered')
    outer = field(session, browser, 4)
    title_x = outer[0] + outer[2] // 2
    title_y = outer[1] + 10
    assert window_at(session, title_x, title_y) == browser, \
        'Cover application hid the Gemscape title used by the activation UAT'
    assert window_at(session, 200, 125) != browser, \
        'Cover application did not obscure the Gemscape client area'
    session.click(title_x, title_y)
    session.wait(lambda: window_at(session, 200, 125) == browser,
                 'Gemscape to become top after its title was clicked')
    topped = wait_frame_change(session, covered, 'gemscape_title_topped')
    assert sum(crop(topped, 100, 70, 360, 220)) > 50000, \
        'Title activation did not repaint the CEF viewport'
    assert cover.poll() is None, 'Cover application exited during activation UAT'

    session.click(82, 58)
    returned = wait_frame_change(session, topped, 'gemscape_back_button')
    assert sum(crop(returned, 74, 69, 550, 280)) < 100000, \
        'Gemscape Back button did not restore the light page'

    session.click(outer[0] + 9, outer[1] + 10)
    session.wait(lambda: session.demo.poll() is not None,
                 'Gemscape to close cleanly', 10)
    assert session.demo.returncode == 0, 'Gemscape returned an error on close'
    print('PASS Gemscape click, complete punctuation input, page keyboard '
          'focus, y/Enter URL navigation, Back, title repaint and sliders')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'cover', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.mode, args.demo, args.capture = 'proxy', 91, True
    session = Session(args)
    session.path = Path(args.artifacts).resolve()/'gemscape_proxy'
    session.path.mkdir(parents=True, exist_ok=True)
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path/'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path/'socket')
    page = Path(__file__).resolve().parents[2]/'data/uat/gemscape.html'
    next_page = page.with_name('gemscape-next.html')
    session.env['GEMSCAPE_URL'] = page.as_uri()
    result = {'passed': False}
    try:
        try:
            session.run()
            check(session, args.cover, next_page)
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

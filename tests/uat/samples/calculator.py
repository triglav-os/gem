#!/usr/bin/env python3
"""Check real calculator buttons and arithmetic through both GEM backends.

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


def check(session):
    initial = session.frame('calculator_initial')
    # Work area starts at (100,60); all 24 buttons must contain ink and paper.
    for row in range(6):
        for col in range(4):
            pixels = crop(initial, 118 + col * 60, 114 + row * 30, 48, 18)
            assert 0 < sum(pixels) < len(pixels), f'Missing button {row},{col}'
    zero = crop(initial, 118, 78, 228, 18)
    session.click(262, 183)  # 9
    nine = crop(session.frame('digit_nine'), 118, 78, 228, 18)
    assert nine != zero, 'Clicking 9 did not update the display'
    session.click(202, 123)  # C
    assert crop(session.frame('cleared'), 118, 78, 228, 18) == zero
    for x, y in ((142, 183), (322, 273), (202, 243), (262, 273)):
        session.click(x, y)  # 7 + 2 =
    result = crop(session.frame('seven_plus_two'), 118, 78, 228, 18)
    assert result == nine, '7 + 2 did not display 9'
    print('PASS all 24 buttons, digit entry, clear and 7 + 2 = 9')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.demo, args.capture = 'calc', True
    session = Session(args)
    session.path = Path(args.artifacts).resolve()/f'calc_{args.mode}'
    session.path.mkdir(parents=True, exist_ok=True)
    # Session environment was prepared for the original generated path.
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path/'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path/'socket')
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

#!/usr/bin/env python3
"""Check GEM reconfigures a viewer started with different raster settings.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path as _Path
import sys as _sys
_sys.path.insert(0, str(_Path(__file__).resolve().parents[2] / "tools/scripts"))
from docker_context import ensure_container
ensure_container()
import argparse
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'uat'))
from run import Session


class UnconfiguredViewer(Session):
    def start(self, name, command):
        if name == 'rasta':
            command = list(command)
            overrides = {'--width': '80', '--height': '60', '--bpp': '1',
                         '--inverse': 'on',
                         '--framebuffer': str(self.path / 'viewer-initial')}
            for option, value in overrides.items():
                command[command.index(option) + 1] = value
        return super().start(name, command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'gemd', 'artifacts', 'resources', 'rasta'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    help_text = subprocess.check_output([args.rasta, '--help'], text=True,
                                        stderr=subprocess.STDOUT)
    assert '--inverse <on|off>' in help_text, \
        'Incompatible Rasta: run make rasta and use bin/tools/rasta'
    args.demo, args.mode = 17, 'proxy'
    args.capture, args.record = False, False
    session = UnconfiguredViewer(args)
    session.env['GEM_RASTA_INVERSE'] = 'off'
    try:
        # Demo17 additionally proves drawing and HID drag/close after negotiation.
        session.run()
        assert (session.path / 'viewer-initial').stat().st_size == 600
        assert (session.path / 'framebuffer').stat().st_size == 640 * 400 * 4
    finally:
        session.close()
    # The viewer flushes its buffered configuration log on clean shutdown.
    log = (session.path / 'rasta.log').read_text()
    assert 'reconfigured framebuffer=' + session.env['GEM_RASTA_FRAMEBUFFER'] in log
    assert 'raster=640x400 bpp=32 inverse=off scale=1' in log
    print('PASS Rasta path, geometry, depth and inverse negotiation plus HID input')


if __name__ == '__main__':
    main()

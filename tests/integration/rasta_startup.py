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
from scenarios import normalized


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


class DelayedConfiguration(Session):
    """Deliver the real subscription only after the demo has drawn its scene."""

    def __init__(self, args):
        super().__init__(args)
        self.subscription = None
        self.delay_subscription = True

    def start(self, name, command):
        if name == 'rasta':
            # Wait for actual configuration processing, not an assumed delay.
            command = ['stdbuf', '-oL', *command]
        return super().start(name, command)

    def pump(self):
        try:
            while True:
                data, addr = self.relay.recvfrom(65535)
                if addr[1] == self.viewer_port:
                    if self.peer:
                        self.relay.sendto(data, self.peer)
                else:
                    self.peer = addr
                    if self.delay_subscription:
                        self.subscription = data
                    else:
                        self.relay.sendto(data, ('127.0.0.1', self.viewer_port))
        except BlockingIOError:
            pass
        return self.peer

    def frame(self, name):
        if name != 'initial' or not self.delay_subscription:
            return super().frame(name)
        before = super().frame('before_subscription')
        assert self.subscription, 'No startup configuration to delay'
        self.delay_subscription = False
        log = self.path / 'rasta.log'
        marker = 'reconfigured framebuffer='
        for attempt in range(2):
            count = log.read_text().count(marker)
            self.relay.sendto(self.subscription, ('127.0.0.1', self.viewer_port))
            self.wait(lambda: log.read_text().count(marker) > count,
                      'viewer to process configuration')
            after = super().frame(f'after_subscription_{attempt}')
            raw = (self.path / 'framebuffer').read_bytes()
            assert raw[3::4] == b'\xff' * (640 * 400), \
                'Late/repeated configuration erased framebuffer alpha'
            assert normalized(after) == normalized(before), \
                'Late/repeated configuration erased the rendered scene'
        return super().frame(name)


class DelayedUnconfiguredViewer(DelayedConfiguration, UnconfiguredViewer):
    """Also preserve a drawn scene when adopting another path and geometry."""


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

    artifacts = Path(args.artifacts)
    for name, viewer in (('delayed', DelayedConfiguration),
                         ('delayed-other', DelayedUnconfiguredViewer)):
        args.artifacts = str(artifacts / name)
        session = viewer(args)
        session.env['GEM_RASTA_INVERSE'] = 'off'
        try:
            session.run()
        finally:
            session.close()
    print('PASS delayed/repeated configuration preserves BGRA and window input')


if __name__ == '__main__':
    main()

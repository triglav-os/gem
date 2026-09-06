#!/usr/bin/env python3
"""Run real GEM demos with Rasta, relay HID, and check acceptance scenarios.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import time


def wait_for(predicate, description, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.02)
    raise AssertionError("Timed out: " + description)


class Session:
    def __init__(self, args):
        self.args = args
        self.path = Path(args.artifacts).resolve() / f"demo{args.demo}_{args.mode}"
        self.path.mkdir(parents=True, exist_ok=True)
        self.processes = []
        self.logs = []
        self.peer = None
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(('127.0.0.1', 0))
        self.relay.setblocking(False)
        self.viewer_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.viewer_socket.bind(('127.0.0.1', 0))
        self.viewer_port = self.viewer_socket.getsockname()[1]
        self.viewer_socket.close()
        self.env = dict(os.environ, GEM_VDI_WIDTH='640', GEM_VDI_HEIGHT='400',
                        GEM_RASTA_FRAMEBUFFER=str(self.path / 'framebuffer'),
                        GEM_RASTA_HOST='127.0.0.1',
                        GEM_RASTA_PORT=str(self.relay.getsockname()[1]),
                        GEM_RASTA_CURSOR='off', GEM_RASTA_SCALE='1',
                        GEMD_SOCKET=str(self.path / 'socket'),
                        GEM_RESOURCE_DIR=str(Path(args.resources).resolve()),
                        ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',
                        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
                        SDL_VIDEODRIVER=os.environ.get('UAT_SDL_DRIVER', 'dummy'),
                        GEM_SHELL='/bin/sh', PS1='UAT> ', HISTFILE='/dev/null', TERM='dumb', SHELL='/bin/sh')

    def start(self, name, command):
        log = (self.path / (name + '.log')).open('w')
        self.logs.append(log)
        proc = subprocess.Popen(command, cwd=self.path, env=self.env,
                                stdout=log, stderr=log, start_new_session=True)
        self.processes.append(proc)
        return proc

    def pump(self):
        try:
            while True:
                data, addr = self.relay.recvfrom(65535)
                if addr[1] == self.viewer_port:
                    if self.peer:
                        self.relay.sendto(data, self.peer)
                else:
                    self.peer = addr
                    self.relay.sendto(data, ('127.0.0.1', self.viewer_port))
        except BlockingIOError:
            pass
        return self.peer

    def pause(self, seconds=0.2):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.pump()
            time.sleep(.01)

    def event(self, kind, x, y=0):
        assert self.peer, 'No Rasta HID subscription'
        self.relay.sendto(struct.pack('!Hhh', kind, x, y), self.peer)
        self.pause(.06)

    def click(self, x, y):
        self.event(3, x, y)
        self.event(10, x, y)
        self.event(11, x, y)
        self.event(3, 639, 399)
        self.pause()

    def key(self, scan):
        self.event(1, scan)
        self.event(2, scan)
        self.pause()

    def frame(self, name):
        self.pause(.25)
        data = (self.path / 'framebuffer').read_bytes()
        assert len(data) == 32000, f'Wrong framebuffer length: {len(data)}'
        (self.path / (name + '.pbm')).write_bytes(b'P4\n640 400\n' + data)
        return data

    def run(self):
        rasta = os.environ.get('RASTA_BIN', str(Path(__file__).resolve().parents[2]/'bin/tools/rasta'))
        assert Path(rasta).is_file(), 'Run make to build the pinned Rasta viewer'
        for name in ['socket', 'framebuffer']:
            (self.path / name).unlink(missing_ok=True)
        self.viewer = self.start('rasta', [rasta, '--inverse', '--width', '640', '--height', '400',
            '--bpp', '1', '--scale', '1', '--port', str(self.viewer_port),
            '--framebuffer', self.env['GEM_RASTA_FRAMEBUFFER'], '--cursor', 'off'])
        self.pause(.3)
        assert self.viewer.poll() is None, 'Rasta exited; see rasta.log'
        if self.args.mode == 'proxy':
            self.server = self.start('gemd', [self.args.gemd])
            wait_for(lambda: (self.path / 'socket').exists(), 'gemd socket')
        self.demo = self.start('demo', [self.args.binary])
        wait_for(self.pump, 'HID subscription')
        self.pause(1)
        assert self.demo.poll() is None, 'Demo exited during startup; see demo.log'
        initial = self.frame('initial')
        assert len(set(initial)) > 8, 'Demo did not render a scene'
        if not self.args.capture:
            from scenarios import check
            check(self, initial)
        assert self.viewer.poll() is None, 'Rasta died during UAT'
        if self.args.mode == 'proxy':
            assert self.server.poll() is None, 'gemd died during UAT'

    def close(self):
        for proc in reversed(self.processes):
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGTERM)
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
        for log in self.logs:
            log.close()
        self.relay.close()
        for p in self.path.glob('*.log'):
            text = p.read_text(errors='replace')
            assert 'ERROR: AddressSanitizer' not in text, f'Sanitizer failure: {p}'
            assert 'runtime error:' not in text, f'Undefined behavior: {p}'
            assert 'AddressSanitizer:DEADLYSIGNAL' not in text, f'Crash: {p}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', type=int, required=True)
    parser.add_argument('--mode', choices=['direct', 'proxy'], required=True)
    for name in ['binary', 'gemd', 'artifacts', 'resources']:
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--record', action='store_true', help='Explicitly record direct reference checkpoints')
    parser.add_argument('--capture', action='store_true', help='Capture only; no acceptance verdict')
    args = parser.parse_args()
    session = Session(args)
    result = {'demo': args.demo, 'mode': args.mode, 'passed': False}
    try:
        try:
            session.run()
        finally:
            session.close()
        result['passed'] = not args.capture and not args.record
        result['recorded'] = args.record
        result['capture_only'] = args.capture
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (session.path / 'result.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()

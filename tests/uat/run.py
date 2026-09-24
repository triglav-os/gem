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
        self.connection = None
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(('127.0.0.1', 0))
        self.relay.setblocking(False)
        self.viewer_port = self.probe_port()
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

    @staticmethod
    def probe_port():
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
        probe.close()
        return port

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

    def wait(self, predicate, description, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            self.pause(.02)
        raise AssertionError('Timed out: ' + description)

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
        raw = (self.path / 'framebuffer').read_bytes()
        assert len(raw) == 640 * 400 * 4, \
            f'Wrong BGRA framebuffer length: {len(raw)}'
        data = bytearray(32000)
        for y in range(400):
            for x in range(640):
                if raw[(y * 640 + x) * 4] < 128:
                    data[y * 80 + x // 8] |= 0x80 >> (x % 8)
        data = bytes(data)
        (self.path / (name + '.pbm')).write_bytes(b'P4\n640 400\n' + data)
        return data

    def receive(self, size):
        data = b''
        while len(data) < size:
            part = self.connection.recv(size - len(data))
            assert part, 'gemd closed the UAT probe connection'
            data += part
        return data

    def rpc(self, opcode, payload=b''):
        assert self.args.mode == 'proxy', 'RPC probing requires proxy mode'
        connected = False
        if self.connection is None:
            self.connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.connection.settimeout(2)
            self.connection.connect(self.env['GEMD_SOCKET'])
            connected = True
        if connected and opcode != 1:
            self.connection.sendall(struct.pack('=IHHI', 0x47454d31, 2, 1, 0))
            magic, status, size = struct.unpack('=IiI', self.receive(12))
            assert magic == 0x47454d31 and status > 0 and size == 0
        self.connection.sendall(struct.pack('=IHHI', 0x47454d31, 2, opcode,
                                            len(payload)) + payload)
        magic, status, size = struct.unpack('=IiI', self.receive(12))
        assert magic == 0x47454d31 and size <= 65536
        return status, self.receive(size)

    def run(self):
        rasta = os.environ.get('RASTA_BIN', str(Path(__file__).resolve().parents[2]/'bin/tools/rasta'))
        assert Path(rasta).is_file(), 'Run make to build the pinned Rasta viewer'
        for name in ['socket', 'framebuffer']:
            (self.path / name).unlink(missing_ok=True)
        # The probed port is released before the viewer binds it, so parallel
        # sessions can collide; a fresh port and viewer resolve that race.
        for attempt in range(5):
            self.viewer = self.start('rasta', [rasta, '--inverse', 'off', '--width', '640', '--height', '400',
                '--bpp', '32', '--scale', '1', '--port', str(self.viewer_port),
                '--framebuffer', self.env['GEM_RASTA_FRAMEBUFFER'], '--cursor', 'off'])
            self.pause(.3)
            if self.viewer.poll() is None:
                break
            text = (self.path / 'rasta.log').read_text(errors='replace')
            assert 'Address already in use' in text and attempt < 4, 'Rasta exited; see rasta.log'
            self.viewer_port = self.probe_port()
        if self.args.mode == 'proxy':
            self.server = self.start('gemd', [self.args.gemd])
            wait_for(lambda: (self.path / 'socket').exists(), 'gemd socket')
        self.demo = self.start(
            'demo', [self.args.binary] +
            list(getattr(self.args, 'binary_args', [])))
        wait_for(self.pump, 'HID subscription')
        self.pause(1)
        assert self.demo.poll() is None, 'Demo exited during startup; see demo.log'
        initial = self.frame('initial')
        assert any(initial), 'Demo did not render a scene'
        if not self.args.capture:
            from scenarios import check
            check(self, initial)
        assert self.viewer.poll() is None, 'Rasta died during UAT'
        if self.args.mode == 'proxy':
            assert self.server.poll() is None, 'gemd died during UAT'
        # A rejected datagram would leave a manually started viewer at its
        # own defaults, so the subscription GEM sends must always parse.
        viewer_log = (self.path / 'rasta.log').read_text(errors='replace')
        assert 'ignoring subscriber reconfigure' not in viewer_log, 'Rasta rejected the GEM subscription'

    def close(self):
        if self.connection is not None:
            try:
                self.rpc(2)
            except (AssertionError, OSError):
                pass
            self.connection.close()
            self.connection = None
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
            assert 'stack smashing detected' not in text, \
                f'Stack corruption: {p}'


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

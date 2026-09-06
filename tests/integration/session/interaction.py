"""Exercise real F5 windows over Rasta HID and check gemd responsiveness.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import re
import socket
import struct
import statistics
import threading
import time


class Interaction:
    def __init__(self, session, directory):
        self.session = session
        self.directory = directory
        self.peer = None
        self.running = True
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(('127.0.0.1', 0))
        self.relay.settimeout(.1)
        self.viewer_port = int(session.env['GEM_RASTA_PORT'])
        session.env['GEM_RASTA_PORT'] = str(self.relay.getsockname()[1])
        session.env.update(GEM_SHELL='/bin/sh', PS1='UAT> ',
                           uatoutput=str(directory/'typed_output'))
        self.thread = threading.Thread(target=self.pump, daemon=True)
        self.thread.start()
        self.connection = None

    def pump(self):
        while self.running:
            try:
                data, address = self.relay.recvfrom(65535)
            except socket.timeout:
                continue
            if address[1] == self.viewer_port:
                if self.peer:
                    self.relay.sendto(data, self.peer)
            else:
                self.peer = address
                self.relay.sendto(data, ('127.0.0.1', self.viewer_port))

    def receive(self, size):
        data = b''
        while len(data) < size:
            part = self.connection.recv(size - len(data))
            assert part, 'gemd closed the responsiveness probe'
            data += part
        return data

    def rpc(self, opcode, payload=b''):
        # Version 2 local wire format from src/gem/gem_protocol.h.
        self.connection.sendall(struct.pack('=IHHI', 0x47454d31, 2,
                                            opcode, len(payload)) + payload)
        magic, status, size = struct.unpack('=IiI', self.receive(12))
        assert magic == 0x47454d31 and size <= 65536
        return status, self.receive(size)

    def rect(self, handle):
        status, data = self.rpc(33, struct.pack('=hh', handle, 4))
        assert status == 1, f'Window {handle} disappeared'
        return struct.unpack('=4h', data)

    def event(self, kind, x, y):
        assert self.peer, 'Missing Rasta HID subscription'
        self.relay.sendto(struct.pack('!Hhh', kind, x, y), self.peer)
        self.session.pause(.08)

    def snapshot(self, name):
        data = (self.directory/'framebuffer').read_bytes()
        (self.directory/f'{name}.pbm').write_bytes(b'P4\n1024 768\n' + data)
        return data

    def key(self, scan, modifier=None):
        events = [(1, scan), (2, scan)]
        if modifier:
            events = [(1, modifier)] + events + [(2, modifier)]
        for kind, code in events:
            self.relay.sendto(struct.pack('!Hhh', kind, code, 0), self.peer)

    def keyboard(self, terminal):
        status, data = self.rpc(33, struct.pack('=hh', terminal, 5))
        assert status == 1
        x, y, w, _ = struct.unpack('=4h', data)

        def text_row():
            frame = (self.directory/'framebuffer').read_bytes()
            return b''.join(frame[row * 128 + (x + 8) // 8:
                                 row * 128 + (x + w - 8) // 8]
                            for row in range(y + 4, y + 20))

        self.session.pause(.5)
        latencies = []
        for scan in range(4, 14):  # a through j; no modifier or repeat ambiguity.
            before = text_row()
            started = time.monotonic()
            self.key(scan)
            while text_row() == before and time.monotonic() - started < 1:
                time.sleep(.001)
            latencies.append((time.monotonic() - started) * 1000)
            self.session.pause(.05)
        self.session.state['keyboard_latency_ms'] = latencies
        median = statistics.median(latencies)
        assert median < 50 and max(latencies) < 200, \
            f'Terminal echo too slow: median {median:.1f} ms, samples {latencies}'
        self.key(24, 224)  # Control-U discards the unexecuted latency input.
        self.session.pause(.1)
        output = self.directory/'typed_output'
        output.unlink(missing_ok=True)
        special = {' ': (44, None), '\n': (40, None),
                   '>': (55, 225), '$': (33, 225), '"': (52, 225)}
        for char in 'echo ok > "$uatoutput"\n':
            self.key(*(special[char] if char in special else
                       (ord(char) - ord('a') + 4, None)))
            time.sleep(.025)
        self.session.pause(.2)
        self.snapshot('terminal_typed')
        self.session.wait(lambda: output.exists() and output.read_text() == 'ok\n',
                          'typed command to produce the exact shell output', 2)
        self.snapshot('terminal_typed')
        print(f'PASS terminal echo median {median:.1f} ms, '
              'typed command at 40 characters/second', flush=True)

    def drag(self, handle, dx, dy, name):
        x, y, w, h = self.rect(handle)
        before = self.snapshot(name + '_before')
        px, py = x + w // 2, y + 10
        for kind, ex, ey in ((3, px, py), (10, px, py),
                             (3, px + dx, py + dy), (11, px + dx, py + dy)):
            self.event(kind, ex, ey)
        expected = (x + dx, y + dy, w, h)
        self.session.wait(lambda: self.rect(handle) == expected,
                          f'{name}: window must move to {expected}', 3)
        self.session.pause(.4)
        after = self.snapshot(name)
        assert before != after, f'{name}: framebuffer did not redraw'
        print(f'PASS {name}: {expected}', flush=True)

    def workspace(self):
        assets = (Path(__file__).resolve().parents[3] /
                  'samples/src/desktop/desktop_assets.c').read_text()
        def words(name):
            body = assets.split('static const UWORD ' + name + '[] = {')[1]
            return [int(v, 16) for v in re.findall(r'0x[0-9a-f]+', body.split('};')[0])]
        mask, data = words('folder_mask'), words('folder_data')
        self.snapshot('workspace_before')
        # Terminal may cover the bitmap while the left edge of its label
        # remains exposed. Locate the icon in the desktop startup capture.
        frame = (self.directory/'desktop_ready.pbm').read_bytes().split(b'\n', 2)[2]
        positions = []
        for row in range(11):
            x, y = 36, 38 + row * 66
            if all(bool(frame[(y + r) * 128 + (x + c) // 8] & (0x80 >> ((x + c) % 8))) ==
                   bool(data[r * 2 + c // 16] & (0x8000 >> (c % 16)))
                   for r in range(32) for c in range(32)
                   if mask[r * 2 + c // 16] & (0x8000 >> (c % 16))):
                positions.append((x - 12, y + 44))
        assert len(positions) == 1, 'Workspace icon missing from startup desktop'
        x, y = positions[0]
        assert self.rpc(36, struct.pack('=hh', x, y))[0] == 0
        previous = self.rpc(36, struct.pack('=hh', 300, 50))[0]
        self.event(3, x, y)
        for _ in range(2):
            self.event(10, x, y)
            self.event(11, x, y)
        self.session.pause(.5)
        self.snapshot('workspace_open')
        browser = self.rpc(36, struct.pack('=hh', 300, 50))[0]
        assert browser > 0 and browser != previous, 'Workspace double-click did not open file manager with all samples running'
        assert self.rect(browser)[:2] == (219, 40)
        for kind in (3, 10, 11):
            self.event(kind, 229, 50)
        self.session.pause(.3)
        assert self.rpc(36, struct.pack('=hh', 300, 50))[0] == previous
        self.session.state['workspace_checks_passed'] = True
        print('PASS Workspace opens and closes with all samples running', flush=True)

    def desktop_info(self):
        # The Stout closer lies directly underneath the Desk popup's first
        # row. A menu click must never reach that covered window control.
        target = self.rpc(36, struct.pack('=hh', 10, 32))[0]
        assert target > 0, 'Missing window underneath Desktop info'
        original = self.rect(target)
        witness = self.rpc(29, struct.pack('=5h', 3, 800, 100, 160, 120))[0]
        assert witness > 0
        assert self.rpc(30, struct.pack('=5h', witness, 800, 100, 160, 120))[0] == 1
        windows = {handle: self.rect(handle) for handle in range(1, 33)
                   if self.rpc(33, struct.pack('=hh', handle, 4))[0] == 1}
        before = self.snapshot('desktop_info_before')
        # A split frame is legal. Its deadline must exclude time for which
        # GEM's synchronous menu prevents the server from receiving bytes.
        request = struct.pack('=IHHIhh', 0x47454d31, 2, 33, 4, witness, 4)
        self.connection.sendall(request[:8])
        self.session.pause(.1)
        for kind, x, y in ((3, 20, 10), (10, 20, 10), (11, 20, 10),
                           (3, 10, 32), (10, 10, 32), (11, 10, 32)):
            self.event(kind, x, y)
            if kind == 11 and y == 10:
                self.session.pause(3)
        self.snapshot('desktop_info_selected')
        self.connection.sendall(request[8:])
        magic, status, size = struct.unpack('=IiI', self.receive(12))
        assert magic == 0x47454d31 and status == 1
        self.receive(size)
        self.session.pause(7)
        opened = self.snapshot('desktop_info_open')
        # The alert's right edge is over exposed background, not animation.
        def alert_edge(frame):
            return bytes(frame[row * 128 + 85] for row in range(340, 430))
        assert alert_edge(opened) != alert_edge(before), 'Desktop info did not appear'
        for handle, rect in windows.items():
            assert self.rect(handle) == rect, 'Desktop info removed or moved a sample window'
        assert self.rpc(36, struct.pack('=hh', 10, 32))[0] == target, 'Desktop info closed the underlying window'
        assert self.rect(target) == original
        for kind in (3, 10, 11):
            self.event(kind, 512, 420)
        self.session.pause(.4)
        closed = self.snapshot('desktop_info_closed')
        assert alert_edge(closed) == alert_edge(before), 'Desktop info did not close'
        for handle, rect in windows.items():
            assert self.rect(handle) == rect, 'Dismissing Desktop info removed a window'
        assert self.rpc(36, struct.pack('=hh', 10, 32))[0] == target, 'Dismissing Desktop info closed the underlying window'
        for proc in self.session.processes:
            if proc.args[0].endswith('/msa'):
                continue
            assert proc.poll() is None, f'Desktop info killed {proc.args[0]}'
        assert self.rpc(31, struct.pack('=h', witness))[0] == 1
        assert self.rpc(32, struct.pack('=h', witness))[0] == 1
        self.session.state['desktop_info_checks_passed'] = True
        print('PASS Desktop info preserves the covered window and all samples', flush=True)

    def check(self):
        viewer = next(proc for proc in self.session.processes
                      if Path(proc.args[0]).name == 'rasta')
        assert '--inverse' in viewer.args, 'F5 viewer must use GEM display polarity'
        self.connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.connection.settimeout(2)
        self.connection.connect(str(self.directory/'socket'))
        assert self.rpc(1)[0] > 0  # APPL_INIT: independent responsive client.
        self.desktop_info()
        stout = self.rpc(36, struct.pack('=hh', 200, 30))[0]
        assert stout > 0
        for index in range(3):
            self.drag(stout, 100, 80, f'stout_drag_{index + 1}')
        self.workspace()
        terminal = self.rpc(36, struct.pack('=hh', 300, 38))[0]
        assert terminal > 0 and terminal != stout
        self.drag(terminal, 120, 80, 'terminal_drag')
        self.keyboard(terminal)
        # Close the moved terminal through its closer: its event loop must
        # receive WM_CLOSED, not just leave a live but blocked process behind.
        x, y, _, _ = self.rect(terminal)
        for kind in (3, 10, 11):
            self.event(kind, x + 10, y + 10)
        proc = next(p for p in self.session.processes
                    if p.args[0].endswith('/terminal'))
        self.session.wait(lambda: proc.poll() is not None,
                          'terminal to handle WM_CLOSED', 3)
        assert proc.returncode == 0, 'Terminal failed while closing'
        self.session.state['interaction_checks_passed'] = True
        self.rpc(2)  # APPL_EXIT
        print('PASS terminal close and independent RPC responsiveness', flush=True)

    def close(self):
        if self.connection:
            self.connection.close()
        self.running = False
        self.thread.join(timeout=1)
        self.relay.close()

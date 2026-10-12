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


def mono_frame(raw, width, height):
    assert len(raw) == width * height * 4
    pitch = (width + 7) // 8
    result = bytearray(pitch * height)
    for y in range(height):
        for x in range(width):
            if raw[(y * width + x) * 4] < 128:
                result[y * pitch + x // 8] |= 0x80 >> (x % 8)
    return bytes(result)


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
        self.width = int(session.env['GEM_VDI_WIDTH'])
        self.height = int(session.env['GEM_VDI_HEIGHT'])
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
        data = mono_frame((self.directory/'framebuffer').read_bytes(),
                          self.width, self.height)
        header = f'P4\n{self.width} {self.height}\n'.encode()
        (self.directory/f'{name}.pbm').write_bytes(header + data)
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
            left = x + 8
            right = x + w - 8
            stride = self.width * 4
            return b''.join(frame[row * stride + left * 4:
                                 row * stride + right * 4]
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
        # The full session includes CEF's external message pump and every GEM
        # app; one 100 ms presentation interval is acceptable, while a stuck
        # or misrouted keyboard event still fails well below the 1 s guard.
        assert median < 150 and max(latencies) < 250, \
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
                  'src/apps/desktop/desktop_assets.c').read_text()
        def words(name):
            body = assets.split('static const UWORD ' + name + '[] = {')[1]
            return [int(v, 16) for v in re.findall(r'0x[0-9a-f]+', body.split('};')[0])]
        mask, data = words('folder_mask'), words('folder_data')
        self.snapshot('workspace_before')
        # Terminal may cover the bitmap while the left edge of its label
        # remains exposed. Locate the icon in the desktop startup capture.
        frame = (self.directory/'desktop_ready.pbm').read_bytes().split(b'\n', 2)[2]
        pitch = (self.width + 7) // 8
        positions = []
        for row in range(11):
            x, y = 36, 38 + row * 66
            if all(bool(frame[(y + r) * pitch + (x + c) // 8] & (0x80 >> ((x + c) % 8))) ==
                   bool(data[r * 2 + c // 16] & (0x8000 >> (c % 16)))
                   for r in range(32) for c in range(32)
                   if mask[r * 2 + c // 16] & (0x8000 >> (c % 16))):
                positions.append((x - 12, y + 44))
        assert len(positions) == 1, 'Workspace icon missing from startup desktop'
        x, y = positions[0]
        assert self.rpc(36, struct.pack('=hh', x, y))[0] == 0
        previous = self.rpc(36, struct.pack('=hh', 300, 50))[0]
        for kind in (3, 10, 11):
            self.event(kind, x, y)
        self.session.pause(.85)
        for kind in (3, 10, 11):
            self.event(kind, x, y)
        self.session.pause(.3)
        assert self.rpc(36, struct.pack('=hh', 300, 50))[0] == previous, \
            'A delayed click on a selected icon acted as a double-click'
        self.session.pause(.85)
        self.event(3, x, y)
        for _ in range(2):
            self.event(10, x, y)
            self.event(11, x, y)
        self.session.pause(.5)
        self.snapshot('workspace_open')
        browser = self.rpc(36, struct.pack('=hh', 300, 50))[0]
        assert browser > 0 and browser != previous, \
            'Workspace double-click did not open file manager with all applications running'
        assert self.rect(browser)[:2] == (219, 40)
        for kind in (3, 10, 11):
            self.event(kind, 229, 50)
        self.session.pause(.3)
        assert self.rpc(36, struct.pack('=hh', 300, 50))[0] == previous
        self.session.state['workspace_checks_passed'] = True
        print('PASS Workspace opens and closes with all applications running', flush=True)

    def system_alert(self):
        # Put a probe-owned closer directly underneath the system popup's
        # Shutdown row. A menu click must never reach that covered control;
        # using a fixture keeps this independent of the app inventory.
        witness = self.rpc(29, struct.pack('=5h', 3, 0, 20, 160, 120))[0]
        assert witness > 0
        assert self.rpc(30, struct.pack('=5h', witness, 0, 20, 160, 120))[0] == 1
        target = self.rpc(36, struct.pack('=hh', 10, 82))[0]
        assert target == witness, 'Fixture window is not underneath the system menu'
        original = self.rect(target)
        windows = {handle: self.rect(handle) for handle in range(1, 33)
                   if self.rpc(33, struct.pack('=hh', handle, 4))[0] == 1}
        before = self.snapshot('system_alert_before')
        # A split frame is legal. Its deadline must exclude time for which
        # GEM's synchronous menu prevents the server from receiving bytes.
        request = struct.pack('=IHHIhh', 0x47454d31, 2, 33, 4, witness, 4)
        self.connection.sendall(request[:8])
        self.session.pause(.1)
        for kind, x, y in ((3, 12, 10), (10, 12, 10),
                           (3, 10, 82), (11, 10, 82)):
            self.event(kind, x, y)
            if kind == 10 and y == 10:
                self.session.pause(3)
        self.snapshot('system_alert_selected')
        self.connection.sendall(request[8:])
        magic, status, size = struct.unpack('=IiI', self.receive(12))
        assert magic == 0x47454d31 and status == 1
        self.receive(size)
        self.session.pause(7)
        opened = self.snapshot('system_alert_open')
        # The alert's right side is over exposed background, not animation.
        def alert_edge(frame):
            pitch = (self.width + 7) // 8
            column = (self.width // 2 + 128) // 8
            top = self.height // 2 - 44
            return bytes(frame[row * pitch + column]
                         for row in range(top, top + 90))
        assert alert_edge(opened) != alert_edge(before), 'Shutdown alert did not appear'
        for handle, rect in windows.items():
            assert self.rect(handle) == rect, 'Shutdown alert removed or moved a sample window'
        assert self.rpc(36, struct.pack('=hh', 10, 82))[0] == target, 'System menu closed the underlying window'
        assert self.rect(target) == original
        for kind in (3, 10, 11):
            self.event(kind, self.width // 2 + 53, self.height // 2 + 26)
        self.session.pause(.4)
        closed = self.snapshot('system_alert_closed')
        assert alert_edge(closed) == alert_edge(before), 'Shutdown alert did not close'
        for handle, rect in windows.items():
            assert self.rect(handle) == rect, 'Dismissing Shutdown removed a window'
        assert self.rpc(36, struct.pack('=hh', 10, 82))[0] == target, 'Dismissing Shutdown closed the underlying window'
        for proc in self.session.processes:
            if proc.args[0].endswith('/msa'):
                continue
            assert proc.poll() is None, f'Shutdown alert killed {proc.args[0]}'
        assert self.rpc(31, struct.pack('=h', witness))[0] == 1
        assert self.rpc(32, struct.pack('=h', witness))[0] == 1
        self.session.state['system_alert_checks_passed'] = True
        print('PASS Shutdown alert preserves the covered window and all applications', flush=True)

    def check(self):
        viewer = next(proc for proc in self.session.processes
                      if Path(proc.args[0]).name == 'rasta')
        assert '--inverse' in viewer.args and 'off' in viewer.args, \
            'F5 viewer must use GEM display polarity'
        cursor_option = viewer.args.index('--cursor')
        assert viewer.args[cursor_option + 1] == 'off', \
            'F5 viewer must hide the Linux cursor over GEM'
        assert self.session.env['GEM_RASTA_CURSOR'] == 'off', \
            'GEM must preserve the hidden Rasta cursor on reconfiguration'
        self.connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.connection.settimeout(2)
        self.connection.connect(str(self.directory/'socket'))
        assert self.rpc(1)[0] > 0  # APPL_INIT: independent responsive client.
        self.system_alert()
        # Applications start in a deterministic order. Terminal, Gemscape,
        # Gemread, Maestro and Stout report handles 3 through 7 respectively.
        stout = 7
        assert self.rect(stout)[2] > 0
        for index in range(3):
            self.drag(stout, 100, 80, f'stout_drag_{index + 1}')
        x, y, _, _ = self.rect(stout)
        for kind in (3, 10, 11):
            self.event(kind, x + 10, y + 10)
        stout_process = next(p for p in self.session.processes
                             if p.args[0].endswith('/stout'))
        # The close below is intentional. Stop treating Stout as a required
        # long-lived child before waiting on later applications, otherwise a
        # subsequent wait can misreport this verified clean exit as a crash.
        for record in self.session.state['processes']:
            if record['name'] == 'stout':
                record['required'] = False
                break
        self.session.publish()
        self.session.wait(lambda: stout_process.poll() is not None,
                          'Stout guest to handle WM_CLOSED', 3)
        assert stout_process.returncode == 0, 'Stout failed while closing'
        # Move Maestro's title off the windows checked next. Stout's final
        # position exposes it before the guest closes normally.
        maestro = 6
        assert self.rect(maestro)[2] > 0
        self.drag(maestro, 1000, 0, 'maestro_drag')
        gemread = 5
        assert self.rect(gemread)[2] > 0
        self.drag(gemread, 0, 120, 'gemread_drag')
        gemscape = 4
        assert self.rect(gemscape)[2] > 0
        self.drag(gemscape, 0, 100, 'gemscape_drag')
        terminal = 3
        assert self.rect(terminal)[2] > 0 and terminal != stout
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

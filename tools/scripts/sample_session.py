#!/usr/bin/env python3
"""Own one F5 Rasta/gemd session and launch selected GEM applications.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import stat
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = Path(os.environ.get('GEM_RUNTIME_ROOT', str(ROOT/'bin'))).resolve()
SESSION = ROOT / 'build/f5'
STATE = SESSION / 'state.json'
BUNDLED_APPS = (
    'desktop', 'calc', 'clock', 'terminal', 'gemscape', 'gemread', 'stout'
)
SOURCE_BUNDLED_APPS = {
    'desktop', 'calculator', 'clock', 'terminal', 'gemscape', 'gemread',
    'stout'
}
SAMPLE_APPS = ('maestro',)
# Keep Stout last in the interaction run so its guest window is exposed for
# the repeated drag regression, independently of where the executable lives.
ALL_APPS = BUNDLED_APPS[:-1] + SAMPLE_APPS + ('stout',)
INITIAL_APPS = ('desktop', 'terminal')
F5_WIDTH = 1920
F5_HEIGHT = 1080


def application_path(name):
    directory = 'apps' if name in BUNDLED_APPS else 'samples'
    return RUNTIME / directory / name


def identity(pid):
    try:
        fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
        return None if fields[0] == 'Z' else fields[19]
    except (OSError, IndexError):
        return None


def alive(record):
    return record.get('birth') is not None and identity(record['pid']) == record['birth']


def terminate(record, sig=signal.SIGTERM):
    if alive(record):
        try:
            if record.get('group'):
                os.killpg(record['pid'], sig)
            else:
                os.kill(record['pid'], sig)
        except ProcessLookupError:
            pass


def read_state():
    try:
        return json.loads(STATE.read_text())
    except (OSError, ValueError):
        return {}


def remove_socket(state):
    path = SESSION/'socket'
    expected = state.get('socket_identity')
    servers = [record for record in state.get('processes', []) if record['name'] == 'gemd']
    if not expected or any(alive(record) for record in servers):
        return
    try:
        info = path.lstat()
        if stat.S_ISSOCK(info.st_mode) and [info.st_dev, info.st_ino] == expected:
            path.unlink()
    except FileNotFoundError:
        pass


def receive_exact(connection, size):
    data = b''
    while len(data) < size:
        part = connection.recv(size - len(data))
        if not part:
            raise ConnectionError('gemd closed the readiness probe')
        data += part
    return data


def gemd_responding(path):
    """Confirm gemd can service a new application after desktop startup."""
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(2)
            connection.connect(str(path))

            def call(opcode):
                connection.sendall(struct.pack('=IHHI', 0x47454d31, 2,
                                               opcode, 0))
                magic, status, size = struct.unpack(
                    '=IiI', receive_exact(connection, 12))
                if magic != 0x47454d31 or size > 65536:
                    raise ConnectionError('invalid gemd readiness reply')
                receive_exact(connection, size)
                return status

            app_id = call(1)
            return app_id > 0 and call(2) == 1
    except (ConnectionError, OSError, struct.error):
        return False


def stop():
    state = read_state()
    manager = state.get('manager', {})
    if manager and alive(manager):
        terminate(manager)
        deadline = time.monotonic() + 6
        while alive(manager) and time.monotonic() < deadline:
            time.sleep(.05)
    # Also recover children if the supervisor was interrupted during cleanup.
    children = state.get('processes', [])
    for record in reversed(children):
        terminate(record)
    time.sleep(.1)
    for record in reversed(children):
        terminate(record, signal.SIGKILL)
    remove_socket(state)


class Session:
    def __init__(self, own_server=False, duration=None, apps=INITIAL_APPS):
        self.own_server = own_server
        self.duration = duration
        self.apps = apps
        self.running = True
        self.processes = []
        self.logs = []
        self.interaction = None
        self.state = {'manager': {'pid': os.getpid(), 'birth': identity(os.getpid())},
                      'phase': 'starting', 'processes': []}
        for sig in (signal.SIGTERM, signal.SIGINT):
            signal.signal(sig, self.interrupt)

    def interrupt(self, *_):
        self.running = False

    def publish(self, phase=None):
        if phase:
            self.state['phase'] = phase
            print(phase, flush=True)
        temp = STATE.with_suffix('.tmp')
        temp.write_text(json.dumps(self.state, indent=2) + '\n')
        temp.replace(STATE)

    def wait(self, predicate, description, timeout=10):
        deadline = time.monotonic() + timeout
        while self.running and time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            for record in self.state['processes']:
                if record.get('required') and not alive(record):
                    raise RuntimeError(f"{record['name']} exited; see {SESSION}/{record['name']}.log")
            time.sleep(.05)
        if not self.running:
            raise InterruptedError('Session stopped')
        raise RuntimeError(f'Timed out while waiting for {description}')

    def start_process(self, name, command, required=True):
        log = (SESSION / f'{name}.log').open('w')
        self.logs.append(log)
        process = subprocess.Popen(command, cwd=ROOT, env=self.env,
                                   stdout=log, stderr=log, start_new_session=True)
        self.processes.append(process)
        self.state['processes'].append({'name': name, 'pid': process.pid,
            'birth': identity(process.pid), 'group': True, 'required': required})
        self.publish()
        return process

    def pause(self, seconds):
        deadline = time.monotonic() + seconds
        while self.running and time.monotonic() < deadline:
            time.sleep(.05)

    def run(self):
        bundled = {p.name for p in (ROOT/'src/apps').iterdir()
                   if p.is_dir() and (p/'CMakeLists.txt').exists()}
        samples = {p.name for p in (ROOT/'samples/src').iterdir()
                   if p.is_dir() and (p/'CMakeLists.txt').exists()}
        if bundled != SOURCE_BUNDLED_APPS or samples != {'maestro', 'msa'}:
            raise RuntimeError('Update the session launcher for the changed application inventory')
        SESSION.mkdir(parents=True, exist_ok=True)
        SESSION.chmod(0o700)
        sock = SESSION / 'socket'
        if sock.exists():
            raise RuntimeError(f'Session socket still exists: {sock}')
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserve:
            reserve.bind(('127.0.0.1', 0))
            port = reserve.getsockname()[1]
        rasta = os.environ.get('RASTA_BIN', str(RUNTIME/'tools/rasta'))
        if not Path(rasta).is_file():
            raise RuntimeError('Run make to download and build the pinned Rasta viewer')
        overrides = {'GEMD_SOCKET': str(sock), 'GEM_RASTA_FRAMEBUFFER': str(SESSION/'framebuffer'),
            'GEM_RASTA_HOST': '127.0.0.1', 'GEM_RASTA_PORT': str(port),
            'GEM_RASTA_CURSOR': 'off',
            'GEM_VDI_WIDTH': str(F5_WIDTH), 'GEM_VDI_HEIGHT': str(F5_HEIGHT),
            'GEM_RESOURCE_DIR': str(RUNTIME/'resources'),
            'GEMIX_HOME': str(RUNTIME),
            'XDG_CONFIG_HOME': str(SESSION/'config'),
            'STOUT_MAX_CYCLES': '0',
            'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
            'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
        self.env = dict(os.environ, **overrides)
        (SESSION/'session.env').write_text(''.join(f'{key}={value}\n' for key, value in overrides.items()))
        self.publish()
        viewer = self.start_process('rasta', [rasta, '--inverse', 'off', '--cursor', 'off', '--port', str(port), '--width', str(F5_WIDTH),
            '--height', str(F5_HEIGHT), '--bpp', '32', '--scale', '1', '--framebuffer', str(SESSION/'framebuffer')])
        self.pause(.4)
        if viewer.poll() is not None:
            raise RuntimeError('Rasta failed to start; see rasta.log')
        self.publish('viewer-ready')
        if self.own_server:
            sys.path.insert(0, str(ROOT/'tests/integration/session'))
            if self.apps == ALL_APPS:
                from interaction import Interaction
                self.interaction = Interaction(self, SESSION)
            self.start_process('gemd', [str(RUNTIME/'core/gemd')])
        self.wait(sock.exists, 'debugger to start gemd', 60)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.connect(str(sock))
            pid, _, _ = struct.unpack('3i', connection.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
        info = sock.stat()
        self.state['socket_identity'] = [info.st_dev, info.st_ino]
        if not self.own_server:
            self.state['processes'].append({'name': 'gemd', 'pid': pid, 'birth': identity(pid),
                                           'group': False, 'required': True})
            self.publish()
        desktop = self.start_process('desktop', [str(application_path('desktop'))])
        def rendered():
            path = SESSION/'framebuffer'
            if not path.exists():
                return False
            raw = path.read_bytes()
            return len(raw) == F5_WIDTH * F5_HEIGHT * 4 and any(
                raw[offset] == 0 for offset in range(0, len(raw), 4))
        self.wait(rendered, 'desktop to render')
        self.pause(.5)
        if desktop.poll() is not None:
            raise RuntimeError('Desktop failed to initialize; see desktop.log')
        self.wait(lambda: gemd_responding(sock),
                  'gemd to finish the initial Desktop redraw', 20)
        self.state['desktop_rpc_ready'] = True
        if self.interaction:
            self.interaction.snapshot('desktop_ready')
        self.publish('desktop-ready')
        msa = None
        program = None
        if 'stout' in self.apps:
            guest = SESSION/'guest'
            guest.mkdir(exist_ok=True)
            msa = self.start_process(
                'msa', [str(RUNTIME/'samples/msa'), 'extract',
                        str(ROOT/'samples/data/st.msa'), str(guest)],
                required=False)
            if msa.wait(timeout=10) != 0:
                raise RuntimeError('MSA extraction failed; see msa.log')
            program = next((p for p in guest.rglob('*')
                            if p.name.upper() == 'DEMO.PRG'), None)
            if program is None:
                raise RuntimeError('Bundled MSA image did not contain DEMO.PRG')
        for name in self.apps:
            if name == 'desktop':
                continue
            command = [str(application_path(name))]
            if name == 'stout':
                command.extend((str(program), str(ROOT/'samples/data/st.msa')))
            if name == 'gemread' and os.environ.get('GEMREAD_DOCUMENT'):
                command.append(os.environ['GEMREAD_DOCUMENT'])
            proc = self.start_process(name, command)
            if name == 'stout':
                def connected():
                    try:
                        return any(p.readlink().as_posix().startswith('socket:[')
                                   for p in Path(f'/proc/{proc.pid}/fd').iterdir())
                    except OSError:
                        return False
                self.wait(connected, 'Stout guest to connect to gemd', 15)
            self.pause(.5)
            if proc.poll() is not None:
                raise RuntimeError(f'{name} exited during startup; see {name}.log')
        self.pause(1)
        for proc in self.processes:
            if proc is not msa and proc.poll() is not None:
                raise RuntimeError(f'Application exited during startup: {proc.args}')
        capture_name = ('all_samples.pbm' if self.apps == ALL_APPS
                        else 'applications.pbm')
        raw = (SESSION/'framebuffer').read_bytes()
        pitch = (F5_WIDTH + 7) // 8
        mono = bytearray(pitch * F5_HEIGHT)
        for y in range(F5_HEIGHT):
            for x in range(F5_WIDTH):
                if raw[(y * F5_WIDTH + x) * 4] < 128:
                    mono[y * pitch + x // 8] |= 0x80 >> (x % 8)
        header = f'P4\n{F5_WIDTH} {F5_HEIGHT}\n'.encode()
        (SESSION/capture_name).write_bytes(header + bytes(mono))
        self.state['started_apps'] = list(self.apps)
        self.state['all_samples_started'] = self.apps == ALL_APPS
        self.publish('all-samples-ready' if self.apps == ALL_APPS
                     else 'applications-ready')
        deadline = time.monotonic() + self.duration if self.duration is not None else float('inf')
        while self.running and viewer.poll() is None and desktop.poll() is None and time.monotonic() < deadline:
            if self.duration is not None:
                for proc in self.processes:
                    if proc is not msa and proc.poll() is not None:
                        raise RuntimeError(f'Application exited during validation: {proc.args}')
            self.pause(.2)
        if self.interaction and self.running:
            self.interaction.check()

    def close(self):
        if self.interaction:
            self.interaction.close()
        for record in reversed(self.state['processes']):
            terminate(record)
        deadline = time.monotonic() + 3
        for proc in reversed(self.processes):
            try:
                proc.wait(timeout=max(.01, deadline-time.monotonic()))
            except subprocess.TimeoutExpired:
                pass
        for record in reversed(self.state['processes']):
            terminate(record, signal.SIGKILL)
        for proc in self.processes:
            proc.wait()
        for log in self.logs:
            log.close()
        for proc in self.processes:
            for record in self.state['processes']:
                if record['pid'] == proc.pid:
                    record['exit_code'] = proc.returncode
        remove_socket(self.state)
        self.publish('stopped')


def main():
    global SESSION, STATE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('start', 'stop', 'supervise', 'check'))
    parser.add_argument('--directory', type=Path, default=SESSION)
    parser.add_argument('--apps', default=None,
                        help='comma-separated applications to launch')
    args = parser.parse_args()
    SESSION = args.directory.resolve()
    STATE = SESSION/'state.json'
    SESSION.mkdir(parents=True, exist_ok=True)
    if args.action == 'stop':
        stop()
        return
    if args.action == 'start':
        stop()
        with (SESSION/'session.log').open('w') as log:
            command = [sys.executable, '-B', __file__, 'supervise',
                       '--directory', str(SESSION), '--apps',
                       ','.join(INITIAL_APPS)]
            proc = subprocess.Popen(command, stdout=log, stderr=log,
                                    start_new_session=True)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and proc.poll() is None:
            state = read_state()
            if state.get('manager', {}).get('pid') == proc.pid and state.get('phase') == 'viewer-ready':
                print('Rasta ready. F5 will start gemd, Desktop and Terminal. '
                      f'Logs: {SESSION}')
                return
            time.sleep(.05)
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=5)
        raise RuntimeError(f'Session preparation failed; see {SESSION}/session.log')
    if args.action == 'check':
        stop()
    if args.apps is None:
        apps = ALL_APPS if args.action == 'check' else INITIAL_APPS
    else:
        apps = tuple(name for name in args.apps.split(',') if name)
        unknown = set(apps) - set(ALL_APPS)
        if unknown or 'desktop' not in apps:
            parser.error('apps must include desktop and use known application names')
    session = Session(own_server=args.action == 'check',
                      duration=3 if args.action == 'check' else None,
                      apps=apps)
    try:
        session.run()
    except InterruptedError:
        pass
    except Exception as error:
        session.state['error'] = str(error)
        raise
    finally:
        session.close()


if __name__ == '__main__':
    main()

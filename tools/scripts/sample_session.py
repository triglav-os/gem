#!/usr/bin/env python3
"""Own one F5 Rasta/gemd session and launch every sample, desktop first.

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
APPS = ('desktop', 'calc', 'clock', 'gemscape', 'maestro', 'terminal', 'stout')


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
    def __init__(self, own_server=False, duration=None):
        self.own_server = own_server
        self.duration = duration
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
        discovered = {p.name for p in (ROOT/'samples/src').iterdir()
                      if p.is_dir() and (p/'CMakeLists.txt').exists()}
        if discovered != set(APPS) | {'msa'}:
            raise RuntimeError('Update the session launcher for the changed samples inventory')
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
            'GEM_VDI_WIDTH': '1024', 'GEM_VDI_HEIGHT': '768',
            'GEM_RESOURCE_DIR': str(RUNTIME/'resources'),
            'STOUT_MAX_CYCLES': '0',
            'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
            'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
        self.env = dict(os.environ, **overrides)
        (SESSION/'session.env').write_text(''.join(f'{key}={value}\n' for key, value in overrides.items()))
        self.publish()
        viewer = self.start_process('rasta', [rasta, '--inverse', '--port', str(port), '--width', '1024',
            '--height', '768', '--bpp', '1', '--scale', '1', '--framebuffer', str(SESSION/'framebuffer')])
        self.pause(.4)
        if viewer.poll() is not None:
            raise RuntimeError('Rasta failed to start; see rasta.log')
        self.publish('viewer-ready')
        if self.own_server:
            sys.path.insert(0, str(ROOT/'tests/integration/session'))
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
        desktop = self.start_process('desktop', [str(RUNTIME/'samples/desktop')])
        def rendered():
            path = SESSION/'framebuffer'
            return path.exists() and len(set(path.read_bytes())) > 8
        self.wait(rendered, 'desktop to render')
        self.pause(.5)
        if desktop.poll() is not None:
            raise RuntimeError('Desktop failed to initialize; see desktop.log')
        if self.interaction:
            self.interaction.snapshot('desktop_ready')
        self.publish('desktop-ready')
        guest = SESSION/'guest'
        guest.mkdir(exist_ok=True)
        msa = self.start_process('msa', [str(RUNTIME/'samples/msa'), 'extract',
                                str(ROOT/'samples/data/st.msa'), str(guest)], required=False)
        if msa.wait(timeout=10) != 0:
            raise RuntimeError('MSA extraction failed; see msa.log')
        program = next((p for p in guest.rglob('*') if p.name.upper() == 'DEMO.PRG'), None)
        if program is None:
            raise RuntimeError('Bundled MSA image did not contain DEMO.PRG')
        for name in APPS[1:]:
            command = [str(RUNTIME/'samples'/name)]
            if name == 'stout':
                command.append(str(program))
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
                raise RuntimeError(f'Sample exited during startup: {proc.args}')
        (SESSION/'all_samples.pbm').write_bytes(b'P4\n1024 768\n' + (SESSION/'framebuffer').read_bytes())
        self.state['all_samples_started'] = True
        self.publish('all-samples-ready')
        deadline = time.monotonic() + self.duration if self.duration is not None else float('inf')
        while self.running and viewer.poll() is None and desktop.poll() is None and time.monotonic() < deadline:
            if self.duration is not None:
                for proc in self.processes:
                    if proc is not msa and proc.poll() is not None:
                        raise RuntimeError(f'Sample exited during validation: {proc.args}')
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
            proc = subprocess.Popen([sys.executable, '-B', __file__, 'supervise', '--directory', str(SESSION)],
                stdout=log, stderr=log, start_new_session=True)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and proc.poll() is None:
            state = read_state()
            if state.get('manager', {}).get('pid') == proc.pid and state.get('phase') == 'viewer-ready':
                print(f'Rasta ready. F5 will start gemd, desktop, then all samples. Logs: {SESSION}')
                return
            time.sleep(.05)
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=5)
        raise RuntimeError(f'Session preparation failed; see {SESSION}/session.log')
    if args.action == 'check':
        stop()
    session = Session(own_server=args.action == 'check', duration=3 if args.action == 'check' else None)
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

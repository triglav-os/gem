#!/usr/bin/env python3
"""Check F5 with the host IDE adapter; GEM processes still run in Docker.

Optional host-side IDE test, separate from CTest inside the build container.
SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adapter', type=Path, required=True)
    args = parser.parse_args()
    adapter = args.adapter.resolve()
    if not adapter.is_file():
        parser.error('Supply the C++ extension debugAdapters/bin/OpenDebugAD7')
    name = 'gem-runtime-' + hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
    name += '-' + str(os.getuid()) + '-f5-test'

    def container_exists():
        return subprocess.run(['docker', 'inspect', name], stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL).returncode == 0

    if container_exists():
        parser.error('An isolated F5 test container already exists: ' + name)
    config = json.loads((ROOT / '.vscode/launch.json').read_text())['configurations'][0]
    config = json.loads(json.dumps(config).replace('${workspaceFolder}', str(ROOT)))
    tasks = {task['label']: task for task in
             json.loads((ROOT / '.vscode/tasks.json').read_text())['tasks']}

    def task_command(key):
        task = tasks[config.pop(key)]
        return [task['command'], *task.get('args', [])]

    start, stop = task_command('preLaunchTask'), task_command('postDebugTask')
    config['logging'] = {'engineLogging': True}
    evidence = ROOT / 'build/f5-adapter'
    evidence.mkdir(parents=True, exist_ok=True)
    test_env = dict(os.environ, GEM_RUNTIME_CONTAINER=name, SDL_VIDEODRIVER='dummy',
                    XDG_CONFIG_HOME=str(evidence/'config'),
                    XDG_DATA_HOME=str(evidence/'data'),
                    XDG_CACHE_HOME=str(evidence/'cache'),
                    XDG_STATE_HOME=str(evidence/'state'))
    process = None
    with (evidence / 'tasks.log').open('w') as task_log, \
            (evidence / 'adapter.stderr').open('w') as errors, \
            (evidence / 'protocol.jsonl').open('w') as protocol:
        try:
            subprocess.run(start, cwd=ROOT, env=test_env,
                           stdout=task_log, stderr=task_log, check=True, timeout=600)
            process = subprocess.Popen([str(adapter)], cwd=ROOT, env=test_env, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=errors)
            messages = queue.Queue()

            def read_messages():
                while True:
                    headers = {}
                    while True:
                        line = process.stdout.readline()
                        if not line:
                            messages.put(None)
                            return
                        if line == b'\r\n':
                            break
                        key, value = line.decode().split(':', 1)
                        headers[key.lower()] = value.strip()
                    messages.put(json.loads(process.stdout.read(int(headers['content-length']))))

            threading.Thread(target=read_messages, daemon=True).start()
            sequence = 0

            def request(command, arguments=None):
                nonlocal sequence
                sequence += 1
                data = json.dumps({'seq': sequence, 'type': 'request',
                                   'command': command, 'arguments': arguments or {}}).encode()
                process.stdin.write(f'Content-Length: {len(data)}\r\n\r\n'.encode() + data)
                process.stdin.flush()

            request('initialize', {'clientID': 'vscode', 'adapterID': 'cppdbg',
                                  'pathFormat': 'path', 'linesStartAt1': True,
                                  'columnsStartAt1': True, 'supportsRunInTerminalRequest': True})
            breakpoint_hit = False
            ready_since = None
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                try:
                    message = messages.get(timeout=.1)
                except queue.Empty:
                    message = {}
                if message is None or process.poll() is not None:
                    raise RuntimeError('Adapter exited before session validation')
                if message:
                    protocol.write(json.dumps(message) + '\n')
                    protocol.flush()
                if message.get('type') == 'request':
                    raise RuntimeError('Unexpected adapter request: ' + message['command'] +
                                       '; Docker debugging must not allocate a host terminal')
                if message.get('type') == 'response':
                    if not message.get('success'):
                        raise RuntimeError(str(message))
                    if message['command'] == 'initialize':
                        request('launch', config)
                    elif message['command'] == 'setFunctionBreakpoints':
                        assert message['body']['breakpoints'][0]['verified'], message
                        request('configurationDone')
                event = message.get('event')
                if event == 'initialized':
                    request('setFunctionBreakpoints', {'breakpoints': [{'name': 'main'}]})
                elif event == 'stopped':
                    assert message['body']['reason'] == 'breakpoint', message
                    breakpoint_hit = True
                    request('continue', {'threadId': message['body']['threadId']})
                elif event in ('terminated', 'exited'):
                    raise RuntimeError('Debuggee exited before session validation')
                directory = '/run/user/' + str(os.getuid()) + '/gem/session-' + hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
                state = json.loads(subprocess.check_output(
                    ['docker', 'exec', name, 'cat', directory + '/state.json'], text=True))
                if breakpoint_hit and state.get('phase') == 'applications-ready':
                    assert state['started_apps'] == ['desktop'], state
                    if ready_since is None:
                        ready_since = time.monotonic()
                    elif time.monotonic() - ready_since >= 10:
                        break
                elif ready_since is not None:
                    raise RuntimeError('Session stopped after becoming ready')
            else:
                raise RuntimeError('Debugger session did not become ready; see protocol.jsonl')
            permissions = subprocess.check_output(
                ['docker', 'exec', name, 'stat', '-c', '%a', directory,
                 str(Path(directory).parent)], text=True).splitlines()
            assert permissions == ['700', '700'], permissions
            logs = evidence/'state/gem/sessions'/hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
            assert (logs/'desktop.log').is_file(), 'Runtime logs did not reach XDG state storage'
            request('disconnect', {'terminateDebuggee': True})
            process.wait(timeout=10)
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            subprocess.run(stop, cwd=ROOT, env=test_env, stdout=task_log, stderr=task_log,
                           check=True, timeout=20)
    assert not container_exists(), 'Post-debug task left the session container running'
    print('PASS actual F5 adapter, breakpoint/continue, stable applications and make stop')


if __name__ == '__main__':
    main()

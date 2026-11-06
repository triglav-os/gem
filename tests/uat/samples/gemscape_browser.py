#!/usr/bin/env python3
"""Test real CEF form events, settings, scrolling and persistent browser data.

SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools/scripts'))
from docker_context import ensure_container
ensure_container()
import argparse
import configparser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import shutil
import threading
from urllib.parse import parse_qs, urlsplit
from gemscape import Session, crop, field, type_text
from dialog_checks import check_standard_border


class BrowserSession(Session):
    def key(self, scan):
        self.event(1, scan)
        self.event(2, scan)
        self.pause(.02)


class Site(BaseHTTPRequestHandler):
    events = []
    requests = []
    slow_started = threading.Event()
    slow_release = threading.Event()
    page = (Path(__file__).resolve().parents[2] / 'data/uat/gemscape-browser.html').read_bytes()

    def log_message(self, *_):
        pass

    def do_GET(self):
        path = urlsplit(self.path)
        self.requests.append(path.path)
        if path.path == '/slow':
            self.slow_started.set()
            self.slow_release.wait(15)
        if path.path == '/event':
            query = parse_qs(path.query, keep_blank_values=True)
            self.events.append((query['name'][0], query['value'][0]))
            body = b'ok'
        elif path.path == '/cacheable':
            body = b'cached response'
        elif path.path == '/pixel':
            body = b'<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20"><rect width="20" height="20"/></svg>'
        else:
            body = self.page
        self.send_response(200)
        self.send_header('Content-Type', 'image/svg+xml' if path.path == '/pixel' else
                         ('text/html' if path.path not in ('/event', '/cacheable') else 'text/plain'))
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'max-age=3600' if path.path == '/cacheable' else 'no-store')
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass  # Stop intentionally cancels the slow request.


def event(session, name, expected=None, after=0):
    def match():
        return next((value for key, value in reversed(Site.events[after:])
                     if key == name and (expected is None or value == expected)), None)
    return session.wait(match, f'page event {name}={expected}', 12)


def chord(session, modifier, scan):
    session.event(1, modifier)
    session.key(scan)
    session.event(2, modifier)


def navigate(session, url):
    mark = len(Site.events)
    chord(session, 224, 15)  # Ctrl+L
    type_text(session, url)
    session.key(40)
    event(session, 'loaded', urlsplit(url).path + ('?' + urlsplit(url).query if '?' in url else ''), mark)


def settings(session):
    session.click(615, 58)
    handle = session.wait(lambda: field(session, 0, 10)[0] if field(session, 0, 10)[0] != 1 else 0,
                          'settings window')
    x, y, _, _ = field(session, handle, 5)
    return handle, x, y


def close_browser(session):
    x, y, _, _ = field(session, 1, 4)
    session.click(x + 9, y + 10)
    session.wait(lambda: session.demo.poll() is not None, 'clean browser shutdown', 12)
    assert session.demo.returncode == 0, f'Browser exited {session.demo.returncode}'


def clear_data(session, which):
    _, x, y = settings(session)
    session.click(x + 240, y + 20)
    session.click(x + 80 + which * 164, y + 157)
    session.frame('clear_confirmation')
    session.key(31)  # Explicit second confirmation button.
    session.pause(.6)
    session.key(41)  # Escape discards the unchanged settings draft.


def controls(session, base):
    event(session, 'loaded', '/start')
    session.wait(lambda: '/pixel' in Site.requests, 'images enabled by default')
    session.click(120, 95)
    type_text(session, 'abcdef')
    session.key(80)
    session.key(42)
    session.key(40)
    event(session, 'submit', 'abcdf')
    session.click(95, 142)
    event(session, 'check', 'true')
    session.click(195, 145)
    session.frame('select_open')
    session.key(81)
    session.key(40)
    event(session, 'choice', 'Second')
    session.click(335, 240)
    dialog_frame = session.frame('website_confirmation')
    assert sum(crop(dialog_frame, 72, 151, 460, 20)) > 350, 'Website dialog was hidden behind CEF pixels'
    session.key(40)  # Accept the parent-centered website confirmation.
    event(session, 'confirm', 'true')
    mark = len(Site.events)
    session.click(335, 240)
    session.key(41)
    event(session, 'confirm', 'false', mark)
    session.click(500, 240)
    type_text(session, 'answer')
    session.key(40)
    event(session, 'prompt', 'answer')
    session.event(3, 90, 187)
    session.event(10, 90, 187)
    session.event(3, 250, 187)
    session.event(11, 250, 187)
    assert int(event(session, 'range')) > 70, 'Page drag did not reach the slider'
    session.click(115, 232)
    event(session, 'loaded', '/second')
    mark = len(Site.events)
    session.click(82, 58)
    event(session, 'loaded', '/start', mark)
    mark = len(Site.events)
    session.click(102, 58)
    event(session, 'loaded', '/second', mark)
    mark = len(Site.events)
    session.click(124, 58)
    event(session, 'loaded', '/second', mark)
    # A deliberately pending HTTP response gives Reload/Stop a real load to cancel.
    chord(session, 224, 15)
    type_text(session, base + '/slow')
    session.key(40)
    session.wait(Site.slow_started.is_set, 'pending browser load')
    session.click(124, 58)
    Site.slow_release.set()
    session.pause(.4)
    assert ('loaded', '/slow') not in Site.events, 'Stop allowed the pending document to load'
    # In-place edit with Home/End and Shift+Left; Enter must use the edited URL.
    navigate(session, base + '/editx')
    chord(session, 224, 15)
    session.key(77)
    chord(session, 225, 80)
    type_text(session, 'y')
    session.key(40)
    event(session, 'loaded', '/edity')
    # A pointer click inside an active field positions the insertion caret.
    chord(session, 224, 15)
    session.key(74)
    session.pause(.5)
    path_x = 138 + len(base + '/') * 8
    session.click(path_x, 58)
    type_text(session, 'z')
    session.key(40)
    event(session, 'loaded', '/zedity')
    chord(session, 224, 15)
    session.key(74)
    session.event(3, path_x, 58)
    session.event(10, path_x, 58)
    session.event(3, path_x + 6 * 8, 58)
    session.event(11, path_x + 6 * 8, 58)
    type_text(session, 'mouse')
    session.key(40)
    event(session, 'loaded', '/mouse')
    chord(session, 224, 15)
    chord(session, 224, 6)  # Copy address, then paste into a web field.
    session.click(120, 95)
    chord(session, 224, 25)
    session.key(40)
    event(session, 'submit', base + '/mouse')
    chord(session, 224, 4)
    type_text(session, base + '/paste')
    chord(session, 224, 4)
    chord(session, 224, 6)
    chord(session, 224, 15)
    chord(session, 224, 25)
    session.key(40)
    event(session, 'loaded', '/paste')
    navigate(session, base + '/start')

    session.wait(lambda: field(session, 1, 16)[0] < 1000, 'vertical metrics')
    session.click(636, 300)
    scroll = json.loads(event(session, 'scroll'))
    session.wait(lambda: abs(field(session, 1, 9)[0] - round(scroll['y'] * 1000 / (scroll['total'] - scroll['h']))) <= 2,
                 'GEM thumb matching CSS scroll position')
    # Drag actual native thumbs to their endpoints and compare page coordinates.
    wx, wy, ww, wh = field(session, 1, 5)
    span = wh - 36
    thumb = max(18, span * field(session, 1, 16)[0] // 1000)
    center = wy + 18 + (span - thumb) * field(session, 1, 9)[0] // 1000 + thumb // 2
    session.event(3, wx + ww + 8, center)
    session.event(10, wx + ww + 8, center)
    session.event(3, wx + ww + 8, wy + wh - 19)
    session.event(11, wx + ww + 8, wy + wh - 19)
    session.wait(lambda: field(session, 1, 9)[0] == 1000, 'vertical thumb endpoint')
    scroll = json.loads(event(session, 'scroll'))
    assert scroll['y'] == scroll['total'] - scroll['h'], scroll
    span = ww - 36
    thumb = max(18, span * field(session, 1, 15)[0] // 1000)
    center = wx + 18 + thumb // 2
    session.event(3, center, wy + wh + 8)
    session.event(10, center, wy + wh + 8)
    session.event(3, wx + ww - 19, wy + wh + 8)
    session.event(11, wx + ww - 19, wy + wh + 8)
    session.wait(lambda: field(session, 1, 8)[0] == 1000, 'horizontal thumb endpoint')
    scroll = json.loads(event(session, 'scroll'))
    assert scroll['x'] == 1200 - scroll['w'], scroll
    session.frame('scrolled')
    navigate(session, base + '/start?reset')
    before = field(session, 1, 16)[0]
    session.click(335, 280)
    event(session, 'grown', '1')
    session.wait(lambda: field(session, 1, 16)[0] < before, 'dynamic document height')

    navigate(session, base + '/start?animate')
    handle, x, y = settings(session)
    parent = field(session, 1, 4)
    outer = field(session, handle, 4)
    assert abs(2*outer[0]+outer[2] - (2*parent[0]+parent[2])) <= 1
    assert abs(2*outer[1]+outer[3] - (2*parent[1]+parent[3])) <= 1
    before = session.frame('settings_animation_before')
    expected = crop(before, x, y, 520, 250)
    check_standard_border(before, (x, y, 520, 250), outer)
    sx, sy = outer[0]+100, outer[1]+10
    session.event(3, sx, sy)
    session.event(10, sx, sy)
    session.event(3, sx-25, sy+28)
    session.event(11, sx-25, sy+28)
    session.wait(lambda: field(session, handle, 4)[:2] != outer[:2], 'settings drag')
    x, y, _, _ = field(session, handle, 5)
    session.event(3, 639, 399)  # Keep the GEM cursor outside the compared dialog.
    for index in range(4):
        session.pause(.16)
        moved = session.frame(f'settings_animation_moved_{index}')
        assert crop(moved, x, y, 520, 250) == expected, 'Animated page corrupted moved dialog'
    session.key(41)
    navigate(session, base + '/start')
    handle, x, y = settings(session)
    session.frame('settings_general')
    session.click(x + 420, y + 20)
    session.frame('settings_advanced')
    session.click(x + 350, y + 117)
    session.click(x + 80, y + 20)
    session.click(x + 150, y + 77)
    type_text(session, base + '/home')
    session.click(x + 350, y + 117)
    session.click(x + 320, y + 224)
    config = Path(session.env['XDG_CONFIG_HOME']) / 'gem/gemscape.cfg'
    session.wait(config.exists, 'saved preferences')
    assert 'zoom=110' in config.read_text().replace(' ', ''), config.read_text()
    assert 'frame_rate=60' in config.read_text().replace(' ', ''), config.read_text()
    assert base + '/home' in config.read_text()
    handle, x, y = settings(session)
    session.click(x + 350, y + 117)
    session.click(x + 440, y + 224)
    assert 'zoom=110' in config.read_text().replace(' ', ''), 'Cancel saved draft'
    handle, x, y = settings(session)
    session.click(x + 220, y + 117)  # restore 100% for page hit coordinates
    session.click(x + 320, y + 224)
    chord(session, 226, 74)  # Alt+Home uses the saved home page.
    event(session, 'loaded', '/home')
    session.click(150, 280)
    data = json.loads(event(session, 'data'))
    assert 'probe=kept' in data['cookie'] and data['local'] == 'kept', data
    session.frame('controls_complete')
    close_browser(session)


def memory_profile(session, base):
    event(session, 'loaded')
    data = json.loads(event(session, 'data'))
    assert data['cookie'] == '' and data['local'] is None, data
    mark = len(Site.events)
    session.click(150, 280)
    data = json.loads(event(session, 'data', after=mark))
    assert 'probe=kept' in data['cookie'] and data['local'] == 'kept', data
    close_browser(session)


def blocked_cookies(session, base):
    event(session, 'loaded')
    mark = len(Site.events)
    session.click(150, 280)
    data = json.loads(event(session, 'data', after=mark))
    assert data['cookie'] == '' and data['local'] == 'kept', data
    close_browser(session)


def saved_session(session, base):
    event(session, 'loaded')
    if urlsplit(session.env['GEMSCAPE_URL']).path == '/session-store':
        session.click(150, 280)
        assert 'session=kept' in json.loads(event(session, 'data'))['cookie']
    else:
        assert 'session=kept' in json.loads(event(session, 'data'))['cookie']
    close_browser(session)


def advanced(session, base):
    path = urlsplit(session.env['GEMSCAPE_URL']).path
    session.wait(lambda: path in Site.requests, 'advanced-options document')
    session.pause(.6)
    if path == '/no-script':
        assert Site.events == [], 'JavaScript ran despite being disabled'
    else:
        event(session, 'loaded', path)
    assert '/pixel' not in Site.requests[session.requests_mark:], 'Images loaded despite being disabled'
    close_browser(session)


def privacy(session, base):
    event(session, 'loaded', '/restart')
    data = json.loads(event(session, 'data'))
    assert 'probe=kept' in data['cookie'] and data['local'] == 'kept', data
    assert 'session=kept' not in data['cookie'], 'Session cookies persisted without consent'
    cache_requests = Site.requests.count('/cacheable')
    clear_data(session, 0)
    mark = len(Site.events)
    session.click(470, 280)
    data = json.loads(event(session, 'data', after=mark))
    assert data['cookie'] == '' and data['local'] == 'kept', data
    clear_data(session, 1)
    navigate(session, base + '/cache-check')
    session.wait(lambda: Site.requests.count('/cacheable') > cache_requests, 'cache invalidation')
    other = base.replace('127.0.0.1', 'localhost')
    navigate(session, other + '/other-origin')
    mark = len(Site.events)
    session.click(150, 280)
    assert json.loads(event(session, 'data', after=mark))['local'] == 'kept'
    navigate(session, base + '/clear-origin')
    clear_data(session, 2)
    mark = len(Site.events)
    session.click(470, 280)
    data = json.loads(event(session, 'data', after=mark))
    assert data['local'] is None, data
    mark = len(Site.events)
    navigate(session, other + '/other-origin-check')
    data = json.loads(event(session, 'data', after=mark))
    assert data['local'] == 'kept' and 'probe=kept' in data['cookie'], 'Clearing one site affected another origin'
    session.frame('privacy_complete')
    close_browser(session)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    args.mode, args.demo, args.capture = 'proxy', 92, True
    root = Path(args.artifacts).resolve() / 'gemscape_browser'
    root.mkdir(parents=True, exist_ok=True)
    # Only discard this test's own fixtures; real browser profiles are untouched.
    for name in ('profile', 'config'):
        shutil.rmtree(root / name, ignore_errors=True)
    server = ThreadingHTTPServer(('127.0.0.1', 0), Site)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:' + str(server.server_port)
    result = {'passed': False}
    try:
        stages = [('controls', '/start', controls), ('privacy', '/restart', privacy),
                  ('memory', '/memory', memory_profile), ('memory_restart', '/memory-restart', memory_profile),
                  ('blocked', '/blocked', blocked_cookies), ('session_store', '/session-store', saved_session),
                  ('session_restore', '/session-restore', saved_session),
                  ('no_images', '/no-images', advanced), ('no_script', '/no-script', advanced)]
        for stage, page, check in stages:
            if stage not in ('controls', 'privacy'):
                config = configparser.ConfigParser()
                config.read(root / 'config/gem/gemscape.cfg')
                config['privacy']['persistent'] = 'true' if stage.startswith('session_') else 'false'
                config['privacy']['session_cookies'] = 'true' if stage.startswith('session_') else 'false'
                config['privacy']['cookies'] = 'false' if stage == 'blocked' else 'true'
                config['advanced']['images'] = 'false' if stage.startswith('no_') else 'true'
                config['advanced']['javascript'] = 'false' if stage == 'no_script' else 'true'
                with (root / 'config/gem/gemscape.cfg').open('w') as output:
                    config.write(output)
            Site.events.clear()
            session = BrowserSession(args)
            session.requests_mark = len(Site.requests)
            session.path = root / stage
            session.path.mkdir(exist_ok=True)
            session.env.update(GEMSCAPE_URL=base + page, GEMSCAPE_PROFILE=str(root / 'profile'),
                               XDG_CONFIG_HOME=str(root / 'config'), GEMD_SOCKET=str(session.path / 'socket'),
                               GEM_RASTA_FRAMEBUFFER=str(session.path / 'framebuffer'))
            try:
                session.run()
                check(session, base)
            finally:
                session.frame('last_frame')
                (session.path / 'events.json').write_text(json.dumps(Site.events, indent=2))
                session.close()
            print('PASS Gemscape ' + stage, flush=True)
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        server.shutdown()
        (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()

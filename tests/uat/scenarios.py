"""Acceptance actions and reviewed framebuffer checkpoints for every demo.

MIT License (see LICENSE). Copyright (C) 2026 tomaz stih.
"""
from pathlib import Path
import time

REFERENCES = Path(__file__).resolve().parents[1] / 'data' / 'uat'


def normalized(data):
    data = bytearray(data)
    # The software pointer is asynchronous and is not part of the scene.
    for y in range(24):
        data[y*80:y*80+3] = b'\0'*3
    for y in range(376,400):
        data[y*80+77:y*80+80] = b'\0'*3
    return bytes(data)


def checkpoint(session, name, data=None):
    data = normalized(session.frame(name) if data is None else data)
    reference = REFERENCES / f'demo{session.args.demo}_{name}.pbm'
    if session.args.record:
        assert session.args.mode == 'direct', 'Record references from direct execution only'
        reference.parent.mkdir(parents=True, exist_ok=True)
        reference.write_bytes(b'P4\n640 400\n'+data)
    else:
        expected = reference.read_bytes().split(b'\n',2)[2]
        difference = sum((a ^ b).bit_count() for a,b in zip(data,expected))
        assert len(data)==len(expected) and difference==0, (
            f'{name}: {difference} pixels differ from reviewed reference {reference}')
    return data


def changed(session, name, before):
    after = session.frame(name)
    difference = sum((a ^ b).bit_count() for a,b in zip(normalized(before),normalized(after)))
    assert difference > 8, f'{name}: input produced no visible response'
    checkpoint(session,name,after)
    return after


def exited(session):
    deadline = time.monotonic()+5
    while session.demo.poll() is None and time.monotonic()<deadline:
        session.pause(.05)
    assert session.demo.poll()==0, f'Demo did not exit cleanly: {session.demo.poll()}'


def type_text(session, text):
    for char in text:
        if char.islower(): scan=ord(char)-ord('a')+4
        elif char in '1234567890': scan='1234567890'.index(char)+30
        elif char==' ': scan=44
        elif char=='\n': scan=40
        elif char=='>':
            session.event(1,225); session.key(55); session.event(2,225)
            continue
        else: raise AssertionError(f'No scancode for {char!r}')
        session.key(scan)


def check(session, initial):
    n=session.args.demo
    if n in (19,31):
        # Prove HID -> AES -> PTY -> real shell, with an independently read result.
        path=session.path/'output'
        path.unlink(missing_ok=True)
        type_text(session,'echo uat123 > output\n')
        deadline=time.monotonic()+5
        while not path.exists() and time.monotonic()<deadline: session.pause(.05)
        assert path.read_text()=='uat123\n', 'Terminal did not execute the typed command'
        type_text(session,'cat output\n')
        after=session.frame('shell_output')
        assert normalized(after)!=normalized(initial), 'Shell output was not displayed'
        type_text(session,'exit\n'); session.pause(.5)
        session.key(41); exited(session)
        return
    checkpoint(session,'initial',initial)
    if n<=16:
        # The scene is deliberately static, including its VDI inquiry output.
        checkpoint(session,'initial',session.frame('stable'))
        return
    if n==17:
        session.event(3,150,20); session.event(10,150,20)
        session.event(3,200,55); session.event(11,200,55)
        session.event(3,639,399)
        changed(session,'moved',initial)
        session.click(83,56); exited(session)
    elif n==18:
        session.event(3,35,10)
        menu=changed(session,'menu_open',initial)
        session.key(41)
        if session.args.mode == 'proxy':
            second=session.start('demo_b',[session.args.binary,'B'])
            session.pause(.8)
            session.event(3,639,399)
            b=session.frame('second_menu')
            session.click(80,30)
            a=session.frame('first_menu')
            assert normalized(a)[:1600]==normalized(initial)[:1600], 'Topping A did not restore its menu'
            assert normalized(a)[:1600]!=normalized(b)[:1600], 'Client menus did not switch'
            session.click(380,50)
            again=session.frame('second_menu_again')
            assert normalized(again)[:1600]==normalized(b)[:1600], 'Topping B did not restore its menu'
            session.click(289,50)
            deadline=time.monotonic()+5
            while second.poll() is None and time.monotonic()<deadline: session.pause(.05)
            assert second.poll()==0 and session.demo.poll() is None, 'Closing B affected A'
        session.event(3,35,10)
        session.click(35,32); exited(session)
    elif n==20:
        session.click(230,160)
        changed(session,'second_closed',initial)
        session.click(70,70); exited(session)
    elif n==21:
        session.click(160,176)
        toggled=changed(session,'checkbox',initial)
        session.click(300,230)
        changed(session,'radio',toggled)
        session.click(130,100); exited(session)
    elif n in (22,23):
        session.event(3,25,10)
        opened=changed(session,'menu_open',initial)
        session.key(41)
        session.click(30,30); exited(session)
    elif n==24:
        session.click(210,177)
        toggle=changed(session,'checkbox',initial)
        session.click(330,204)
        changed(session,'radio',toggle)
        session.key(41); exited(session)
    elif n==25:
        session.click(145,123); session.key(4)
        edited=changed(session,'edited',initial)
        session.click(330,222)
        changed(session,'cleared',edited)
        session.key(41); exited(session)
    elif n==26:
        session.event(3,75,10)
        changed(session,'windows_menu',initial)
        session.key(41)
        session.click(100,80)
        changed(session,'topped',initial)
        session.click(54,80); exited(session)
    elif n==27:
        session.key(41); exited(session)
    elif n==28:
        session.click(122,155)
        changed(session,'image_selected',initial)
        session.key(41); exited(session)
    elif n==29:
        session.click(305,110)
        changed(session,'userdef_updated',initial)
        session.key(41); exited(session)
    elif n==30:
        session.click(190,220)
        changed(session,'form_keybd',initial)
        session.key(41); exited(session)
    elif n==32:
        session.key(30)
        alert=changed(session,'alert',initial)
        session.key(40)
        changed(session,'alert_result',alert)
        session.key(41); exited(session)
    elif n==33:
        session.key(4); session.key(43); session.key(5)
        changed(session,'edited',initial)
        session.key(40)
        changed(session,'saved_alert',initial)
        session.key(40); exited(session)

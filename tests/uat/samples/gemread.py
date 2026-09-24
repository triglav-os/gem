#!/usr/bin/env python3
"""Check Gemread's empty page and File/Open menu through real GEM input.

AGPL-3.0-or-later License (see ../../../src/apps/gemread/LICENSE).
Copyright (C) 2026 tomaz stih.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
import sys
import time
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import Session


def crop(frame, x, y, w, h):
    return bytes((frame[row * 80 + col // 8] >> (7 - col % 8)) & 1
                 for row in range(y, y + h) for col in range(x, x + w))


def field(session, handle, number):
    status, data = session.rpc(33, struct.pack('=hh', handle, number))
    assert status == 1
    return struct.unpack('=4h', data)


def type_text(session, text):
    plain = {'-': 45, '.': 55}
    for char in text:
        if 'a' <= char <= 'z':
            scan = ord(char) - ord('a') + 4
        elif char in plain:
            scan = plain[char]
        else:
            raise AssertionError(f'Unsupported Gemread UAT character {char!r}')
        session.key(scan)


def create_epub(path):
    container = '''<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
 <rootfiles><rootfile full-path="OEBPS/content.opf"
  media-type="application/oebps-package+xml"/></rootfiles>
</container>'''
    package = '''<?xml version="1.0" encoding="UTF-8"?>
<package version="2.0" unique-identifier="book-id"
 xmlns="http://www.idpf.org/2007/opf">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:title>Gemread UAT</dc:title><dc:language>en</dc:language>
  <dc:identifier id="book-id">gemread-uat</dc:identifier>
 </metadata>
 <manifest><item id="chapter" href="chapter.xhtml"
  media-type="application/xhtml+xml"/></manifest>
 <spine><itemref idref="chapter"/></spine>
</package>'''
    paragraphs = ''.join(
        f'<p>Gemread reflowable EPUB line {number}: layout and scrolling.</p>'
        for number in range(80))
    chapter = ('<?xml version="1.0" encoding="UTF-8"?>'
               '<html xmlns="http://www.w3.org/1999/xhtml"><head>'
               '<title>Gemread UAT</title></head><body><h1>Gemread EPUB</h1>'
               + paragraphs + '</body></html>')
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr('mimetype', 'application/epub+zip',
                         compress_type=zipfile.ZIP_STORED)
        archive.writestr('META-INF/container.xml', container)
        archive.writestr('OEBPS/content.opf', package)
        archive.writestr('OEBPS/chapter.xhtml', chapter)


def create_color_pdf(path):
    """Create a dependency-free three-page PDF with distinct page colors."""
    streams = [
        b'0.85 0.12 0.12 rg 0 0 300 400 re f\n1 1 1 rg 50 150 200 100 re f\n',
        b'0.12 0.75 0.20 rg 0 0 300 400 re f\n1 1 1 rg 80 100 140 200 re f\n',
        b'0.10 0.25 0.90 rg 0 0 300 400 re f\n1 1 1 rg 25 175 250 50 re f\n',
    ]
    objects = [
        b'<< /Type /Catalog /Pages 2 0 R >>',
        b'<< /Type /Pages /Kids [3 0 R 5 0 R 7 0 R] /Count 3 >>',
        b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] '
        b'/Resources << >> /Contents 4 0 R >>',
        b'<< /Length %d >>\nstream\n%s' % (len(streams[0]), streams[0]) +
        b'endstream',
        b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] '
        b'/Resources << >> /Contents 6 0 R >>',
        b'<< /Length %d >>\nstream\n%s' % (len(streams[1]), streams[1]) +
        b'endstream',
        b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] '
        b'/Resources << >> /Contents 8 0 R >>',
        b'<< /Length %d >>\nstream\n%s' % (len(streams[2]), streams[2]) +
        b'endstream',
    ]
    pdf = bytearray(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
    offsets = [0]
    for number, obj in enumerate(objects, 1):
        offsets.append(len(pdf))
        pdf.extend(f'{number} 0 obj\n'.encode())
        pdf.extend(obj)
        pdf.extend(b'\nendobj\n')
    xref = len(pdf)
    pdf.extend(f'xref\n0 {len(objects) + 1}\n'.encode())
    pdf.extend(b'0000000000 65535 f \n')
    for offset in offsets[1:]:
        pdf.extend(f'{offset:010d} 00000 n \n'.encode())
    pdf.extend((f'trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\n'
                f'startxref\n{xref}\n%%EOF\n').encode())
    path.write_bytes(pdf)


def raw_frame(session):
    session.pause(.25)
    raw = (session.path/'framebuffer').read_bytes()
    assert len(raw) == 640 * 400 * 4
    return raw


def dominant_color(raw, x, y, w, h):
    totals = [0, 0, 0]
    colorful = 0
    for row in range(y, min(y + h, 400)):
        for column in range(x, min(x + w, 640)):
            offset = (row * 640 + column) * 4
            blue, green, red = raw[offset:offset + 3]
            if max(red, green, blue) - min(red, green, blue) > 35:
                totals[0] += red
                totals[1] += green
                totals[2] += blue
                colorful += 1
    assert colorful > 1000, 'Gemread did not publish a color BGRA page'
    return totals.index(max(totals)), colorful


def check_empty(session, selector_document):
    initial = session.frame('gemread_empty')
    assert 0 < sum(crop(initial, 20, 0, 70, 22)) < 1540, \
        'Gemread File menu title is missing'
    page = crop(initial, 150, 80, 430, 250)
    assert sum(page) < 64, 'Gemread empty document viewport is not white'
    assert sum(crop(initial, 22, 53, 45, 20)) > 3, \
        'Gemread zoom toolbar icons are missing'
    assert sum(crop(initial, 22, 370, 300, 18)) > 10, \
        'Gemread status bar is missing'

    session.event(3, 45, 10)
    session.event(10, 45, 10)
    opened = session.frame('gemread_file_menu')
    assert crop(opened, 20, 22, 130, 45) != crop(initial, 20, 22, 130, 45), \
        'File menu did not open'
    session.event(3, 45, 38)
    session.event(11, 45, 38)
    chooser = session.frame('gemread_open_chooser')
    assert crop(chooser, 80, 60, 480, 280) != crop(initial, 80, 60, 480, 280), \
        'File/Open did not display the document chooser'
    session.key(41)
    session.pause(.3)
    session.event(3, 639, 399)
    restored = session.frame('gemread_chooser_closed')
    assert crop(restored, 150, 80, 430, 250) == page, \
        'Canceling File/Open did not restore the empty document page'

    session.event(3, 45, 10)
    session.event(10, 45, 10)
    session.event(3, 45, 38)
    session.event(11, 45, 38)
    session.pause(.2)
    session.click(200, 365)
    type_text(session, selector_document.name)
    session.click(475, 365)
    deadline = time.monotonic() + 10
    rendered = None
    while time.monotonic() < deadline:
        rendered = session.frame('gemread_selector_document')
        if sum(crop(rendered, 22, 52, 600, 325)) > 500:
            break
        session.pause(.1)
    assert rendered is not None and \
        sum(crop(rendered, 22, 52, 600, 325)) > 500, \
        'Gemread did not open the PDF selected through File/Open'

    session.click(29, 40)
    session.wait(lambda: session.demo.poll() is not None,
                 'Gemread to close cleanly')
    assert session.demo.returncode == 0, 'Gemread returned an error on close'
    print('PASS Gemread empty page, File menu, Open chooser and cancel')


def resize_window(session):
    before = field(session, 1, 4)
    sx = min(635, before[0] + before[2] - 5)
    sy = min(395, before[1] + before[3] - 5)
    session.event(3, sx, sy)
    session.event(10, sx, sy)
    session.event(3, sx - 80, sy - 60)
    session.event(11, sx - 80, sy - 60)
    session.wait(lambda: field(session, 1, 4)[2:] != before[2:],
                 'Gemread to resize through its standard GEM sizer')


def check_document(session, document_kind, color_fixture=False):
    initial = session.frame(f'gemread_{document_kind}_initial')
    outer = field(session, 1, 4)
    work = field(session, 1, 5)
    assert sum(crop(initial, 22, 52, 600, 325)) > 500, \
        f'Gemread did not render the {document_kind.upper()} page'
    assert field(session, 1, 1)[0] & 0x0020, \
        'Gemread window does not expose the standard GEM resize handle'
    assert sum(crop(initial, work[0], work[1], work[2], 20)) > 5, \
        'Gemread toolbar did not draw'
    assert sum(crop(initial, work[0], work[1] + work[3] - 20,
                    min(350, work[2]), 20)) > 10, \
        'Gemread status bar did not draw'

    if color_fixture:
        first_raw = raw_frame(session)
        first_color, first_count = dominant_color(
            first_raw, work[0] + 120, work[1] + 20,
            work[2] - 120, work[3] - 40)
        assert first_color == 0, 'First PDF thumbnail/page is not red'
        first_page = crop(initial, work[0] + 120, work[1] + 20,
                          work[2] - 120, work[3] - 40)
        session.click(work[0] + 55, work[1] + 20 + 112 + 45)
        second = session.frame('gemread_pdf_thumbnail_page_2')
        second_color, _ = dominant_color(
            raw_frame(session), work[0] + 120, work[1] + 20,
            work[2] - 120, work[3] - 40)
        assert second_color == 1, \
            'Clicking the second page thumbnail did not navigate to page 2'
        assert crop(second, work[0] + 120, work[1] + 20,
                    work[2] - 120, work[3] - 40) != first_page, \
            'Multipage thumbnail navigation did not change the page'
        session.click(work[0] + 55, work[1] + 20 + 112 + 45)
        third = session.frame('gemread_pdf_thumbnail_page_3')
        third_color, _ = dominant_color(
            raw_frame(session), work[0] + 120, work[1] + 20,
            work[2] - 120, work[3] - 40)
        assert third_color == 2 and third != second, \
            'Thumbnail rail did not advance to and open page 3'

    # Zoom in through the shared toolbar, then exercise native scrollbars.
    session.click(work[0] + 30, work[1] + 10)
    session.wait(lambda: field(session, 1, 16)[0] < 1000,
                 f'{document_kind.upper()} zoom to resize the GEM slider')
    session.click(work[0] + work[2] + 9,
                  work[1] + work[3] - 10)
    session.wait(lambda: field(session, 1, 9)[0] > 0,
                 f'{document_kind.upper()} scroll position to move the slider')
    scrolled = session.frame(f'gemread_{document_kind}_scrolled')
    assert crop(scrolled, 22, 52, 600, 325) != \
        crop(initial, 22, 52, 600, 325), \
        f'{document_kind.upper()} page did not scroll'

    before_resize_raw = raw_frame(session) if color_fixture else None
    resize_window(session)
    resized = session.frame(f'gemread_{document_kind}_resized')
    assert resized != scrolled, \
        f'{document_kind.upper()} page did not rerender after resize'
    if color_fixture:
        new_work = field(session, 1, 5)
        _, resized_count = dominant_color(
            raw_frame(session), new_work[0] + 120, new_work[1] + 20,
            max(1, new_work[2] - 120), max(1, new_work[3] - 40))
        assert resized_count != first_count and before_resize_raw != raw_frame(session), \
            'Gemread color output did not adapt to the resized viewport'
    session.click(29, 40)
    session.wait(lambda: session.demo.poll() is not None,
                 'Gemread to close cleanly')
    assert session.demo.returncode == 0, 'Gemread returned an error on close'
    print(f'PASS Gemread {document_kind.upper()} render and standard GEM '
          'vertical scrollbar')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('direct', 'proxy'), required=True)
    for name in ('binary', 'gemd', 'artifacts', 'resources'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--document')
    parser.add_argument('--epub-fixture', action='store_true')
    parser.add_argument('--pdf-fixture', action='store_true')
    args = parser.parse_args()
    args.demo, args.capture = 90, True
    session = Session(args)
    if args.epub_fixture:
        scenario = 'epub'
    elif args.pdf_fixture:
        scenario = 'pdf'
    elif args.document:
        scenario = Path(args.document).suffix.lstrip('.').lower()
    else:
        scenario = 'empty'
    session.path = (Path(args.artifacts).resolve() /
                    f'gemread_{scenario}_{args.mode}')
    session.path.mkdir(parents=True, exist_ok=True)
    session.env['GEM_RASTA_FRAMEBUFFER'] = str(session.path/'framebuffer')
    session.env['GEMD_SOCKET'] = str(session.path/'socket')
    assert sum(bool(value) for value in
               (args.document, args.epub_fixture, args.pdf_fixture)) <= 1, \
        'Choose one document fixture'
    if args.epub_fixture:
        document = session.path/'gemread.epub'
        create_epub(document)
        document_kind = 'epub'
    elif args.pdf_fixture:
        document = session.path/'gemread-color-pages.pdf'
        create_color_pdf(document)
        document_kind = 'pdf'
    elif args.document:
        document = Path(args.document).resolve()
        document_kind = document.suffix.lstrip('.').lower()
    else:
        document = None
        document_kind = None
    selector_document = None
    if document is None:
        selector_document = session.path/'y-reader.pdf'
        source = (Path(__file__).resolve().parents[3]/'docs'/'manuals'/
                  'The_Atari_Compendium.pdf')
        shutil.copyfile(source, selector_document)
        (session.path/'.hidden-system.pdf').write_bytes(b'hidden fixture')
    args.binary_args = [str(document)] if document else []
    result = {'passed': False}
    try:
        try:
            session.run()
            if document:
                assert args.mode == 'proxy', \
                    'Document UAT needs RPC slider checks'
                check_document(session, document_kind, args.pdf_fixture)
            else:
                check_empty(session, selector_document)
        finally:
            session.close()
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (session.path/'result.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()

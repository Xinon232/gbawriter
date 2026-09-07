#!/usr/bin/env python3
"""Compile the real editor-header capacity and renderer under ASan/UBSan."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
match = re.search(r'char\s+buffer\[([^\]]+)\]', main)
assert match, 'Editor buffer declaration not found; update integration test'
capacity = match.group(1)
# Keep the test tied to the actual formatting and clipping path, not a shadow API.
compact = re.sub(r'\s+', '', main)
assert 'writer::format(buffer,sizeof(buffer),"%s%s",app.text().dirty()?"*":"",storage.current_name());line(px,8,writer::STATUS_Y,buffer,128);' in compact
assert 'if(app.status_visible()){' in compact
assert 'line(px,144,writer::STATUS_Y,app.active_group(),32);' in compact, 'active group must be between filename and Shift/Caps'
assert 'line(px,184,writer::STATUS_Y,"Shift",48);' in compact
assert 'line(px,184,writer::STATUS_Y,"Caps",48);' in compact
assert 'app.viewport()+app.view_rows()' in compact
assert 'y=writer::TEXT_Y+(row-app.viewport())*writer::TEXT_PITCH' in compact
assert 'y=writer::TEXT_Y+(caret.row-app.viewport())*writer::TEXT_PITCH' in compact
assert 'app.save_feedback(snapshot)' in compact
assert 'UP/DOWN: FIELD  LEFT/RIGHT: +/-' in main
flags = ['-O1', '-g', '-fsanitize=address,undefined',
         '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie']
includes = ['-I' + str(root / 'include'),
            '-I' + str(root / 'references/superfw/src'),
            '-I' + str(root / 'references/superfw/src/fonts')]
with tempfile.TemporaryDirectory(prefix='gbawriter-header-') as directory:
    out = Path(directory)
    # Compile the actual editor branch and bus-safe line/caret primitives, not
    # a reimplementation. Only Butano's page allocation/flip is outside this test.
    primitives = main.split('void line(', 1)[1].split('void title(', 1)[0]
    editor = main.split('case Scene::EDITOR:{', 1)[1].split('break;}', 1)[0]
    (out / 'editor_render.inc').write_text(
        'int glyph_width(const char* s){return int(font_width(s));}\n'
        'void line(' + primitives +
        '\nvoid render_editor(uint8_t* px, writer::Application& app, writer::Storage& storage){\n'
        'char buffer[WRITER_HEADER_CAPACITY];\n' + editor + '\n}\n')
    subprocess.run(['gcc', '-std=c11', *flags, *includes,
                    '-Wno-discarded-qualifiers', '-c',
                    str(root / 'src/superfw_font.c'), '-o', str(out / 'font.o')],
                   check=True)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    *flags, '-no-pie', *includes, '-I' + str(out),
                    '-DWRITER_HEADER_CAPACITY=(' + capacity + ')',
                    str(root / 'tests/test_writer_header.cpp'),
                    *[str(root / ('src/' + name + '.cpp')) for name in
                      ('writer_format', 'writer_core', 'writer_layout', 'writer_app', 'writer_storage')],
                    str(out / 'font.o'),
                    '-o', str(out / 'header')], check=True)
    subprocess.run([str(out / 'header'),
                    str(root / 'references/superfw/res/fonts.pack'),
                    str(root / 'references/superfw/res/reader-symbols.pack')],
                   check=True)

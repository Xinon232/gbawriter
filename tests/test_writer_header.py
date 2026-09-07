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
assert 'writer::format(buffer,sizeof(buffer),"%s%s",app.text().dirty()?"*":"",storage.current_name());line(px,8,0,buffer,164);' in compact
flags = ['-O1', '-g', '-fsanitize=address,undefined',
         '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie']
includes = ['-I' + str(root / 'include'),
            '-I' + str(root / 'references/superfw/src'),
            '-I' + str(root / 'references/superfw/src/fonts')]
with tempfile.TemporaryDirectory(prefix='gbawriter-header-') as directory:
    out = Path(directory)
    subprocess.run(['gcc', '-std=c11', *flags, *includes,
                    '-Wno-discarded-qualifiers', '-c',
                    str(root / 'src/superfw_font.c'), '-o', str(out / 'font.o')],
                   check=True)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    *flags, '-no-pie', *includes,
                    '-DWRITER_HEADER_CAPACITY=(' + capacity + ')',
                    str(root / 'tests/test_writer_header.cpp'),
                    str(root / 'src/writer_format.cpp'), str(out / 'font.o'),
                    '-o', str(out / 'header')], check=True)
    subprocess.run([str(out / 'header'),
                    str(root / 'references/superfw/res/fonts.pack'),
                    str(root / 'references/superfw/res/reader-symbols.pack')],
                   check=True)

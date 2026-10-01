#!/usr/bin/env python3
# V4.0: Controls and Credits pages (src/writer_help.cpp). Body lines go to the
# SuperFW glyph/width check; "#" subheadings use the 5x7 UI font (ASCII, 6 px).
import ast,re,sys
from pathlib import Path
source=(Path(__file__).resolve().parents[1]/'src/writer_help.cpp').read_text()
pages=re.findall(r'\{"((?:[^"\\]|\\.)*)",\s*\{((?:"(?:[^"\\]|\\.)*",?\s*){8})\}\}',source)
assert len(pages)>=15, 'help pages not found'
body=[]
for title,block in pages:
    lines=[ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)]
    assert len(lines)==8, title
    assert all(32<=ord(c)<127 for c in title) and len(title)*6<=200, title
    for line in lines:
        if line.startswith('#'):
            assert all(32<=ord(c)<127 for c in line[1:]) and len(line[1:])*6<=224, line
        elif line:
            body.append(line)
Path(sys.argv[1]).write_text('\n'.join(body)+'\n')

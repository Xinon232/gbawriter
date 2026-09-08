#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.cpp').read_text()
app=(root/'include/writer_app.h').read_text()
assert 'R cycle: normal, Shift, CAPS' in main
assert 'No timing window' in main
assert 'START+SELECT: bar / full screen' in main
assert 'Bottom: file, group, Shift/Caps' in main
assert 'Date UP/DOWN: choose field' in main
assert 'Date LEFT/RIGHT: change value' in main
assert 'HELP_PAGES = 19' in app
assert 'quickly' not in main
for path in ['README.md','RELEASE_NOTES.md']:
    text=(root/path).read_text()
    assert 'v1.0.0' in text and 'prerelease' in text
    assert 'whole-word' in text and 'Start: Credits' in text
    assert 'START+SELECT' in text and 'no timing window' in text
    assert 'filename → active letter group → Shift/Caps' in text
    assert '24 frames' in text and '5 frames' in text
    assert 'A alone' in text and 'B alone' in text
assert 'name: gbawriter-v1.0.0' in (root/'.github/workflows/build-rom.yml').read_text()
print('PASS: v1.0.0 controls/help and hardware-unverified prerelease documentation')

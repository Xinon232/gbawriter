#!/usr/bin/env python3
# V4.0 controls (src/writer_help.cpp) and hardware-unverified documentation.
from pathlib import Path
root=Path(__file__).resolve().parents[1]
helps=(root/'src/writer_help.cpp').read_text()
for line in ('Short R alone: Shift.','Hold R alone (0.8 s): Caps.','R alone again: back to normal.',
             'Hold A or B alone: repeat.','Start alone: new line.','Start+A: save.','Start+B: Home without saving.',
             'Select: helper line on / off.','Hold A + Up/Down: move a file.','Start: Import TXT files.',
             'Up/Down: day, month or year.','Left/Right: change it.','There is no autosave.'):
    assert f'"{line}"' in helps, line
assert 'quickly' not in helps
readme=(root/'README.md').read_text()
assert 'V4.0' in readme and 'hardware' in readme
assert 'whole-word' in readme and 'Start on Home: Import' in readme
assert 'START+SELECT' in readme and '48 frames' in readme
assert 'filename → active letter group → Shift/Caps' in readme
assert '24 frames' in readme and '5 frames' in readme
assert 'A alone' in readme and 'B alone' in readme
assert 'B: Resume active file' in readme and 'Save / Discard / Cancel' in readme
notes=(root/'RELEASE_NOTES.md').read_text()
assert notes.startswith('# gbawriter V4.1') and 'hardware' in notes
assert (root/'docs/release-v4.1.0.md').read_text().startswith('# gbawriter V4.1')
assert 'Helper line: On / Off' in readme
assert 'name: gbawriter-rom' in (root/'.github/workflows/build-rom.yml').read_text()
print('PASS: V4.0 controls pages and hardware-unverified documentation')

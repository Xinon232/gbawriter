#!/usr/bin/env python3
"""Contract for both documented press orders; real rendering is checked separately."""
import ast
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
block = main.split('const char* const help[', 1)[1].split('};', 1)[0]
lines = [ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', block)]
for required in (
    'Hold SELECT, then type a chord',
    'Or type a letter; keep held:',
    'Exact direction, L if used,',
    'and its producing B/A/R button',
    'Then press SELECT: same letter',
    'First accent; keeps its case',
    'No extra period or letter',
    'No time limit; no extra keys',
    'Release/change chord: ineligible',
    'No variants? Letter unchanged',
    'Release/repress final B/A/R',
    'Release SELECT to keep result',
    'L+UP+A held, then SELECT: é',
    'Cancels new provisional only',
    'Converted existing letter stays',
):
    assert required in lines, f'Missing help instruction: {required}'
assert 'Press SELECT: inserts . now' not in lines
assert 'Cancels live SELECT character' not in lines
readme = (root / 'README.md').read_text()
for required in ('Accents work in either order', 'exact single D-pad direction', 'L if used',
                 'producing B/A/R button', 'no time limit', 'release and press',
                 'keeping its case', 'without adding a period or another letter',
                 'converted existing letter stays', 'L+UP+A', 'SELECT'):
    assert required in readme, f'Missing README instruction: {required}'
assert 'Press SELECT to insert **`.` immediately**.' not in readme
pages = int(re.search(r'HELP_PAGES\s*=\s*(\d+)', (root/'include/writer_app.h').read_text()).group(1))
assert len(lines) == pages * 6
assert f'opens {pages} control pages' in readme
for required in ('UP: ABC    RIGHT: HIJ', 'DOWN: NOP  LEFT: TUW',
                 'L+UP: DEF   L+RIGHT: KLM', 'L+DOWN: QRS L+LEFT: XYZ',
                 'Keep RIGHT held: R,R = g', 'Keep LEFT held: R,R = v'):
    assert required in lines, f'Missing V1.2 mapping: {required}'
for row in ('| Up | a / b / c | d / e / f |', '| Right | h / i / j | k / l / m |',
            '| Down | n / o / p | q / r / s |', '| Left | t / u / w | x / y / z |'):
    assert row in readme
assert 'gbawriter V1.2' in main
assert readme.startswith('# gbawriter')
print(f'PASS: both-order Select help/README contract; {pages} complete pages')

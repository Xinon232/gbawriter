#!/usr/bin/env python3
"""Contract for both documented accent press orders (README and V4.0 pages)."""
import ast
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
helps = (root / 'src/writer_help.cpp').read_text()
lines = [ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', helps)]
for required in (
    'Hold Select, then type a letter:', 'it gets its first accent.',
    'Or type a letter, keep its keys', 'held and press Select.',
    'Keep Select held and press', 'B / A / R again.',
    'a: á ä à â ã å æ', 'o: ó ö ô ò õ ø œ', 'Shift and Caps work here too.',
    'Up: abc    Right: hij', 'Down: nop  Left: tuw', 'Up: def    Right: klm', 'Down: qrs  Left: xyz',
    'Hold Right, press R twice: g.', 'Hold Left, press R twice: v.',
):
    assert required in lines, f'Missing help instruction: {required}'
readme = (root / 'README.md').read_text()
for required in ('Accents work in either order', 'exact single D-pad direction', 'L if used',
                 'producing B/A/R button', 'no time limit', 'release and press',
                 'keeping its case', 'without adding a period or another letter',
                 'converted existing letter stays', 'L+UP+A', 'SELECT'):
    assert required in readme, f'Missing README instruction: {required}'
assert 'Press SELECT to insert **`.` immediately**.' not in readme
for row in ('| Up | a / b / c | d / e / f |', '| Right | h / i / j | k / l / m |',
            '| Down | n / o / p | q / r / s |', '| Left | t / u / w | x / y / z |'):
    assert row in readme
assert readme.startswith('# gbawriter')
print('PASS: both-order Select help/README contract')

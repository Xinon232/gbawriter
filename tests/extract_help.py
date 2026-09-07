#!/usr/bin/env python3
import ast,re,sys
from pathlib import Path
main=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
block=main.split('const char* const help[',1)[1].split('};',1)[0]
lines=[ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)]
header=(Path(__file__).resolve().parents[1]/'include/writer_app.h').read_text()
match=re.search(r'HELP_PAGES\s*=\s*(\d+)',header)
assert match, 'HELP_PAGES declaration not found'
pages=int(match.group(1))
assert len(lines)==pages*6, 'Each declared help page must contain six lines'
Path(sys.argv[1]).write_text('\n'.join(lines)+'\n')

#!/usr/bin/env python3
import ast,re,sys
from pathlib import Path
main=(Path(__file__).resolve().parents[1]/'src/main.cpp').read_text()
block=main.split('const char* const help[',1)[1].split('};',1)[0]
lines=[ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)]
assert len(lines)==72
Path(sys.argv[1]).write_text('\n'.join(lines)+'\n')

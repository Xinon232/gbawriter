#!/usr/bin/env python3
import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('gate',Path(__file__).with_name('check_memory.py'))
assert spec and spec.loader
gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
valid={'ewram_end':0x020220cc,'iwram_end':0x030015ec,'stack_top':0x03007f00,'app':0x020000e0,'storage':0x02012180}
assert gate.validate(valid,1640)==[]
for key,value in [('app',0x03000000),('storage',0x03000000),('ewram_end',0x0203ffff),('iwram_end',0x03007000)]:
    broken=dict(valid);broken[key]=value
    assert gate.validate(broken,1640),key
assert gate.validate(valid,24000)
print('PASS: memory/stack gate rejects misplaced buffers, RAM exhaustion and oversized frames')

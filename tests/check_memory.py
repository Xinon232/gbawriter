#!/usr/bin/env python3
"""Post-link RAM placement and static frame budget (not a stack-depth proof)."""
import argparse
import os
from pathlib import Path
import subprocess


def validate(symbols, largest_frame):
    failures=[]
    for key in ('app','storage'):
        if not 0x02000000 <= symbols[key] < 0x02040000:
            failures.append(key+' must be in EWRAM')
    if 0x02040000-symbols['ewram_end'] < 64*1024:
        failures.append('less than 64 KiB EWRAM headroom')
    if symbols['stack_top']-symbols['iwram_end'] < 20*1024:
        failures.append('less than 20 KiB IWRAM user stack headroom')
    if largest_frame > 2048:
        failures.append('writer/storage/render static frame exceeds 2 KiB')
    return failures


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf',nargs='?',default='gbawriter.elf')
    parser.add_argument('--build',default='build')
    args=parser.parse_args()
    nm=Path(os.environ.get('DEVKITARM','/opt/devkitpro/devkitARM'))/'bin/arm-none-eabi-nm'
    output=subprocess.check_output([str(nm),'-C',args.elf],text=True)
    names={'ewram_end':'__end__','iwram_end':'__fini_array_end','stack_top':'__sp_usr',
           'app':'(anonymous namespace)::app','storage':'(anonymous namespace)::storage'}
    symbols={}
    for line in output.splitlines():
        fields=line.split(maxsplit=2)
        if len(fields)!=3:continue
        for key,name in names.items():
            if fields[2]==name:symbols[key]=int(fields[0],16)
    assert set(symbols)==set(names),'missing RAM/linker symbols'
    frames=[]
    required=['main','writer_app','writer_core','writer_layout','writer_storage','writer_format',
              'ff','ffsystem','ffunicode','supercard_driver','diskio','superfw_font']
    for name in required:
        path=Path(args.build)/(name+'.su')
        assert path.exists(),f'missing {path}: clean-build with -fstack-usage'
        for line in path.read_text().splitlines():
            fields=line.split('\t')
            if len(fields)==3:
                assert fields[2] in ('static','dynamic,bounded'),line
                frames.append((int(fields[1]),fields[0]))
    largest=max(frames)
    failures=validate(symbols,largest[0])
    print(f"EWRAM used={symbols['ewram_end']-0x02000000} free={0x02040000-symbols['ewram_end']}")
    print(f"IWRAM static end=0x{symbols['iwram_end']:08x}; user-stack headroom={symbols['stack_top']-symbols['iwram_end']}")
    print(f'Largest runtime-source static frame={largest[0]}: {largest[1]}')
    print('Budget gate only: indirect calls, IRQ nesting and total stack depth require runtime/hardware QA.')
    if failures:raise SystemExit('; '.join(failures))
    print('PASS: RAM placement and static frame budgets')

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Real production FatFS on a sector-backed disposable FAT16 image.
Requires gcc, g++, mkfs.fat, fsck.fat. Never accesses a device or the user's SD.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile

repo=Path(__file__).resolve().parents[1]
for tool in ('gcc','g++','mkfs.fat','fsck.fat'):
    if not shutil.which(tool):raise SystemExit(f'Missing {tool}; install host compilers and dosfstools')
with tempfile.TemporaryDirectory(prefix='gbawriter-fatfs-') as directory:
    root=Path(directory)
    sources=['ff.c','ffunicode.c','ffsystem.c','writer_core.cpp','writer_storage.cpp','writer_format.cpp']
    headers=['writer_storage.h','writer_core.h','writer_format.h','ff.h','ffconf.h','diskio.h','gbahw.h','supercard_driver.h']
    hashes={}
    for p in ['src/'+s for s in sources]+['include/'+s for s in headers]+['tests/test_writer_fatfs.cpp']:
        data=(repo/p).read_bytes();(root/Path(p).name).write_bytes(data)
        hashes[p]=hashlib.sha256(data).hexdigest()
    print('SOURCE_SHA256 '+json.dumps(hashes,sort_keys=True),flush=True)
    def run(args):
        subprocess.run([str(a) for a in args],cwd=root,check=True)
    flags=['-O1','-g','-ffunction-sections','-fdata-sections','-I'+str(root),'-D__DEVKITARM__']
    objects=[]
    for source in sources:
        obj=root/(source+'.o');objects.append(obj)
        run(['gcc' if source.endswith('.c') else 'g++','-std=c11' if source.endswith('.c') else '-std=c++17',*flags,'-c',root/source,'-o',obj])
    run(['g++','-std=c++17',*flags,root/'test_writer_fatfs.cpp',*objects,'-Wl,--gc-sections','-o',root/'harness'])
    image=root/'fat16.img'
    with image.open('wb') as file:file.truncate(32768*512)
    run(['mkfs.fat','-F','16','-S','512','-s','1',image])
    run([root/'harness',image])
    run(['fsck.fat','-n',image])
print('PASS: portable production FatFS integration; disposable FAT16 image clean')

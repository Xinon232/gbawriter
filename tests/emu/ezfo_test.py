#!/usr/bin/env python3
"""Exact-ROM differential test: SuperCard model vs EZ-Flash Omega and EverDrive GBA models.

Runs tests/emu/ezfo_script.txt (import with a used name, open, edit, save)
with writer_runner on the SuperCard model and on the Omega model in several
variants (other NOR IDs, slow card, the kernel's in-game IRQ hook, retried
reads and writes; on the EverDrive model (edgba_model.h): SDHC, SDSC, v1
cards and slow or retried reads). Every screenshot and every file on the card must be the
same as on the SuperCard, and fsck.vfat must find the card clean. Also checks
the driver with gbasd/tests/check_ram_code.py. Modeled carts, not hardware proof.
Needs libmgba-dev, mtools, dosfstools.
Usage: tests/emu/ezfo_test.py gbawriter.gba
"""
import os, re, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def card_files(img, out):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    out.mkdir()
    subprocess.run(['mcopy', '-s', '-n', '-i', str(img), '::/', str(out)], env=env, check=True, capture_output=True)
    return {str(p.relative_to(out)): p.read_bytes() for p in sorted(out.rglob('*')) if p.is_file()}


def run(runner, rom, work, name, env_extra, script):
    out = work / name
    out.mkdir()
    img = out / 'card.img'
    subprocess.run(['sh', str(HERE / 'make_card.sh'), str(img)], check=True)
    env = dict(os.environ, SD_IMAGE=str(img), **env_extra)
    p = subprocess.run([str(runner), str(rom), str(script), str(out)], env=env,
                       capture_output=True, text=True)
    if p.returncode:
        sys.exit(f'FAIL {name}: runner exit {p.returncode}\n{p.stderr[-2000:]}')
    m = re.search(r'(?:EZFO|EDGBA) (.*)', p.stdout)
    rep = m.group(1) if m else ''
    shots = {q.name: q.read_bytes() for q in sorted(out.glob('*.ppm'))}
    if subprocess.run(['fsck.vfat', '-n', str(img)], capture_output=True).returncode:
        sys.exit(f'FAIL {name}: filesystem not clean')
    return shots, card_files(img, out / 'files'), dict(re.findall(r'(\w+)=(\w+)', rep))


def main():
    rom = Path(sys.argv[1]).resolve()
    elf = rom.with_suffix('.elf')
    if elf.exists():
        subprocess.run([sys.executable, str(HERE.parent.parent / 'gbasd' / 'tests' / 'check_ram_code.py'), str(elf),
                        'ezfo_read', 'ezfo_write', 'ezfo_detect'], check=True)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        sc, ez, ed = work / 'runner_sc', work / 'runner_ezfo', work / 'runner_edgba'
        subprocess.run(['cc', '-O2', str(HERE / 'writer_runner.c'), '-lmgba', '-o', str(sc)], check=True)
        subprocess.run(['cc', '-O2', '-DEZFO_MODEL', str(HERE / 'writer_runner.c'), '-lmgba', '-o', str(ez)], check=True)
        subprocess.run(['cc', '-O2', '-DEDGBA_MODEL', str(HERE / 'writer_runner.c'), '-lmgba', '-o', str(ed)], check=True)
        # A timed-out command waits about a second (the kernel's timeout), so the
        # fault variants use a copy of the script with long waits after each key.
        fast = HERE / 'ezfo_script.txt'
        slow = work / 'ezfo_script_slow.txt'
        slow.write_text(''.join(l + ('wait 150\n' if l.split()[:1] in (['tap'], ['hold']) else '')
                                for l in fast.read_text().splitlines(keepends=True)))
        refs = {}
        for script in (fast, slow):
            shots, files, _ = run(sc, rom, work, 'supercard-' + script.stem, {}, script)
            if 'gbawriter/Notes (2).txt' not in files:
                sys.exit(f'FAIL supercard {script.name}: the import did not happen; the script no longer matches the app')
            refs[script] = shots, files
            print(f'PASS supercard reference ({script.name}): {len(shots)} screens, {len(files)} files')
        for name, env, script in [('ezfo', {}, fast), ('ezfo-nor-id-227e', {'EZFO_NOR_ID': '0x227E'}, fast),
                                  ('ezfo-slow-card', {'EZFO_DELAY_POLLS': '4000'}, fast),
                                  ('ezfo-kernel-irq-hook', {'EZFO_IRQ_HOOK': '1'}, fast),
                                  ('ezfo-read-retry', {'EZFO_FAIL_READ': '20'}, slow),
                                  ('ezfo-write-retry', {'EZFO_FAIL_WRITE': '2'}, slow),
                                  ('edgba-sdhc', {'SDHC': '1'}, fast), ('edgba-sdsc', {}, fast),
                                  ('edgba-sd-v1', {'SDV1': '1'}, fast),
                                  ('edgba-slow-block', {'EDGBA_FAIL_READ': '20', 'EDGBA_FAIL_COUNT': '3000'}, fast),
                                  ('edgba-read-retry', {'SDHC': '1', 'EDGBA_FAIL_READ': '20', 'EDGBA_FAIL_COUNT': '70000'}, slow)]:
            ref_shots, ref_files = refs[script]
            shots, files, rep = run(ed if name.startswith('edgba') else ez, rom, work, name, env, script)
            bad = [k for k in ref_shots if shots.get(k) != ref_shots[k]]
            if bad:
                sys.exit(f'FAIL {name}: screens differ from the SuperCard run: {bad}')
            if files != ref_files:
                sys.exit(f'FAIL {name}: card files differ: {sorted(set(files) ^ set(ref_files)) or "contents"}')
            if name.startswith('edgba'):
                if rep.get('cfg') != '0003' or rep.get('eeprom_window_writes') != '0' or rep.get('sectors_written', '0') == '0':
                    sys.exit(f'FAIL {name}: model report {rep}')
                print(f'PASS {name}: same screens and files as the SuperCard; {rep["read_cmds"]} read commands, '
                      f'{rep["sectors_written"]} sectors written')
                continue
            if rep.get('page') != '0200' or rep.get('irq_in_kernel') != '0' or rep.get('nor_left_in_id') != '0':
                sys.exit(f'FAIL {name}: model report {rep}')
            print(f'PASS {name}: same screens and files as the SuperCard; {rep["read_cmds"]} read / '
                  f'{rep["write_cmds"]} write commands')
    print('EZFO/EDGBA DIFFERENTIAL TEST PASS (modeled carts, not hardware proof)')


if __name__ == '__main__':
    main()

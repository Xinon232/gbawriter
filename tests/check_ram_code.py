#!/usr/bin/env python3
"""Check that code which runs while the EZ-Flash Omega kernel is mapped over
the game ROM never reaches ROM.

usage: check_ram_code.py ELF ROOT [ROOT ...]

Starting from the ROOT functions, follows every direct branch (b, bl, blx)
and reads every literal-pool word. Fails if a reached function is not in RAM
(EWRAM 02xxxxxx or IWRAM 03xxxxxx), or if any literal points into the game
ROM (08000000-08FFFFFF) other than the Omega's unlock addresses and the ROM
header, which detection compares on purpose. Indirect calls through registers
are allowed only via such literals, so they are covered by the same rule.
(C) 2026 Halim Jarrar, MIT."""
import os, re, subprocess, sys

ALLOWED_ROM = {0x08000000, 0x08020000, 0x08040000}
HEADER = range(0x080000A0, 0x080000C0)


def tool(name):
    base = os.environ.get('DEVKITARM', '/opt/devkitpro/devkitARM')
    return os.path.join(base, 'bin', 'arm-none-eabi-' + name)


def main():
    elf, roots = sys.argv[1], sys.argv[2:]
    if not roots:
        sys.exit(__doc__)
    syms = {}
    for line in subprocess.check_output([tool('nm'), '-S', '-n', elf], text=True).splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2].lower() == 't':
            syms[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    by_addr = {a & ~1: n for n, (a, s) in syms.items()}
    dis = subprocess.check_output([tool('objdump'), '-d', elf], text=True).splitlines()
    insn = {}
    for line in dis:
        m = re.match(r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{4,8}\s+)+(.*)', line)
        if m:
            insn[int(m.group(1), 16)] = m.group(2)
    errors, seen, todo = [], set(), list(roots)
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        if name not in syms:
            errors.append(f'{name}: no such function'); continue
        start, size = syms[name]
        start &= ~1
        if not 0x02000000 <= start < 0x04000000:
            errors.append(f'{name} at {start:08x} is not in RAM'); continue
        for addr in range(start, start + size, 2):
            text = insn.get(addr)
            if text is None:
                continue
            words = re.findall(r'\.word\s+0x([0-9a-f]+)', text)
            for w in words:
                v = int(w, 16)
                if 0x08000000 <= v < 0x09000000 and v not in ALLOWED_ROM and v not in HEADER:
                    errors.append(f'{name}+{addr - start:#x}: literal {v:08x} points into the game ROM')
                elif (v & ~1) in by_addr and 0x02000000 <= v < 0x04000000:
                    todo.append(by_addr[v & ~1])
            m = re.match(r'(b|bl|blx|b\w\w)(?:\.n|\.w)?\s+([0-9a-f]+)\s', text + ' ')
            if m:
                target = int(m.group(2), 16)
                if start <= target < start + size:
                    continue
                if not 0x02000000 <= target < 0x04000000:
                    errors.append(f'{name}+{addr - start:#x}: branch to ROM {target:08x}')
                elif target in by_addr:
                    todo.append(by_addr[target])
                else:
                    errors.append(f'{name}+{addr - start:#x}: branch to unknown {target:08x}')
    for e in errors:
        print('RAM CODE CHECK:', e)
    print(f'RAM code check: {len(seen)} functions from {", ".join(roots)}:',
          'FAIL' if errors else 'PASS')
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()

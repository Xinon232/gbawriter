#!/usr/bin/env python3
"""Check that code which runs while the EZ-Flash Omega kernel is mapped over
the game ROM never reaches ROM.

usage: check_ram_code.py ELF [--guarded NAME ...] ROOT [ROOT ...]

Starting from the ROOT functions, follows every direct branch (b, bl, blx)
and reads every literal-pool word. Fails if a reached function is not in RAM
(EWRAM 02xxxxxx or IWRAM 03xxxxxx), or if any literal points into the game
ROM (08000000-08FFFFFF) other than the Omega's unlock addresses and the ROM
header, which detection compares on purpose. Indirect calls through registers
are allowed only via such literals, so they are covered by the same rule.
--guarded NAME accepts references to the ROM function NAME: for a call the
code makes only while the ROM is mapped (for example behind a check of
ezfo_busy); the reason belongs next to the call.
(C) 2026 Halim Jarrar, MIT."""
import os, re, subprocess, sys

ALLOWED_ROM = {0x08000000, 0x08020000, 0x08040000}
HEADER = range(0x080000A0, 0x080000C1)  # the header and its end (a loop bound)


def tool(name):
    base = os.environ.get('DEVKITARM', '/opt/devkitpro/devkitARM')
    return os.path.join(base, 'bin', 'arm-none-eabi-' + name)


def main():
    elf, args, roots, guarded = sys.argv[1], sys.argv[2:], [], set()
    while args:
        a = args.pop(0)
        if a == '--guarded':
            guarded.add(args.pop(0))
        else:
            roots.append(a)
    if not roots:
        sys.exit(__doc__)
    funcs, names = {}, {}   # address -> (name, size); name -> addresses (static names repeat)
    for line in subprocess.check_output([tool('nm'), '-S', '-n', elf], text=True).splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2].lower() == 't':
            a = int(parts[0], 16) & ~1
            funcs[a] = (parts[3], int(parts[1], 16))
            names.setdefault(parts[3], []).append(a)
    allowed = {a for n in guarded for a in names.get(n, [])}
    missing = sorted(n for n in guarded if n not in names) + sorted(n for n in roots if n not in names)
    if missing:
        sys.exit(f'unknown function: {missing}')
    dis = subprocess.check_output([tool('objdump'), '-d', elf], text=True).splitlines()
    insn = {}
    for line in dis:
        m = re.match(r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{4,8}\s+)+(.*)', line)
        if m:
            insn[int(m.group(1), 16)] = m.group(2)
    errors, seen = [], set()
    todo = [a for n in roots for a in names[n] if 0x02000000 <= a < 0x04000000] or [names[n][0] for n in roots]
    for n in roots:  # a root must be in RAM
        if not any(0x02000000 <= a < 0x04000000 for a in names[n]):
            errors.append(f'{n} at {names[n][0]:08x} is not in RAM')
    while todo:
        start = todo.pop()
        if start in seen:
            continue
        seen.add(start)
        name, size = funcs[start]
        if not 0x02000000 <= start < 0x04000000:
            continue
        for addr in range(start, start + size, 2):
            text = insn.get(addr)
            if text is None:
                continue
            for w in re.findall(r'\.word\s+0x([0-9a-f]+)', text):
                v = int(w, 16)
                if (v & ~1) in allowed:
                    continue
                if 0x08000000 <= v < 0x09000000 and v not in ALLOWED_ROM and v not in HEADER:
                    errors.append(f'{name}+{addr - start:#x}: literal {v:08x} points into the game ROM')
                elif (v & ~1) in funcs and 0x02000000 <= v < 0x04000000:
                    todo.append(v & ~1)
            m = re.match(r'(b|bl|blx|b\w\w)(?:\.n|\.w)?\s+([0-9a-f]+)\s', text + ' ')
            if m:
                target = int(m.group(2), 16)
                if start <= target < start + size or target in allowed:
                    continue
                if not 0x02000000 <= target < 0x04000000:
                    errors.append(f'{name}+{addr - start:#x}: branch to ROM {target:08x}')
                elif target in funcs:
                    todo.append(target)
                else:
                    errors.append(f'{name}+{addr - start:#x}: branch to unknown {target:08x}')
    for e in errors:
        print('RAM CODE CHECK:', e)
    note = f' (guarded ROM calls: {", ".join(sorted(guarded))})' if guarded else ''
    print(f'RAM code check: {len(seen)} functions from {", ".join(roots)}{note}:',
          'FAIL' if errors else 'PASS')
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()

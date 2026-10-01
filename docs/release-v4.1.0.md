# gbawriter V4.1

Based on gbawriter V4.0. **Physical GBA/Supercard SD saving remains unverified**: keep backups and use disposable documents first.

Files in this release:

- `gbawriter.gba`: the Game Boy Advance ROM.
- `gbawriter-full-controls.pdf`: the manual.
- `SHA256SUMS`.

## New: Status bar setting

- **Select > Status bar: On / Off** decides whether the editor's bottom bar (file name, letter group, Shift/Caps) is shown when a file opens. Default: On. The choice is saved in `/gbawriter/GBAWRITER.SYS` and kept after switching off.
- **Start+Select** still hides or shows the bar while writing; the next file you open follows the setting again.
- Controls > Caret and saving has a new page, Status bar.

## Notes

- Settings files from V4.0 are read as before (Status bar On).
- Everything else is unchanged from V4.0. Not tested on real hardware.

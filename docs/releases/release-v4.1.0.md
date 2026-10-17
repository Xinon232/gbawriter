# gbawriter V4.1

Based on gbawriter V4.0. **Physical GBA/Supercard SD saving remains unverified**: keep backups and use disposable documents first.

Files in this release:

- `gbawriter.gba`: the Game Boy Advance ROM.
- `gbawriter-full-controls.pdf`: the manual.
- `SHA256SUMS`.

## New: Helper line setting

- **Select > Helper line: On / Off** decides whether the helper line at the bottom of the editor (file name, letter group, Shift/Caps) is shown when a file opens. Default: On. The choice is saved in `/gbawriter/GBAWRITER.SYS` and kept after switching off.
- **Start+Select** still hides or shows the helper line while writing; the next file you open follows the setting again.
- The Select menu now has a line under Helper line, like the line under New File on Home: the settings are above it, the instructions (Controls topics and Credits) below.
- Controls > Caret and saving has a new page, Helper line.

## Notes

- Settings files from V4.0 are read as before (Helper line On).
- Everything else is unchanged from V4.0. Not tested on real hardware.

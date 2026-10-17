# gbawriter V4.0

Based on gbawriter V3.0. The editor and all typing controls are unchanged, except Start+B. **Physical GBA/Supercard SD saving remains unverified**: keep backups and use disposable documents first.

Files in this release:

- `gbawriter.gba`: the Game Boy Advance ROM.
- `gbawriter-full-controls.pdf`: the manual.
- `SHA256SUMS`.

## Home is your file list

- The screens now look like gbamp3: white background, a small title, eight rows, a light-blue bar on the selected row, long names scrolling sideways, no instructions on screen.
- **New File** is the first row, with a page icon and a line under it; then every TXT file in `/gbawriter` without `.txt`. At start the cursor is always on New File.
- **Hold A + Up / Down** moves a file. The order is saved on the card (`/gbawriter/GBAWRITER.SYS`, hidden); files the order does not know yet (new, imported, copied on a PC) appear at the top. A acts when it is released. Up to 1,000 files are listed.

## Start: Import

- `Import to /gbawriter` browses the whole SD card: folders first, then TXT files. A opens a folder or imports a file (`Import into /gbawriter?`, No first), B goes up and, at the root, back to Home.
- A name already used becomes a numbered copy (`Diary (2).txt`, after asking). Nothing is replaced; a failed copy leaves no partial file. The new file is selected at the top of Home.

## Select: the menu

- On a file: **Rename** (typed with the gbawriter letters; Start+A saves, Start+B cancels; `.txt` is added) and **Delete** (after turning it on in Secret Settings; `Sure?` first).
- **File names**: the date format of New File — DDMMYYYY (default, as before), DD.MM.YYYY, MMDDYYYY, MM.DD.YYYY or YYYY-MM-DD. The date picker shows its fields in the same order.
- **Controls** in seven short topics (Files and menus, Letters, L layer, Spaces and case, Symbols, Accents, Caret and saving) and **Credits** as its own section: your page first (gbawriter V4.0, Halim Jarrar, halimj.itch.io, gba@halim-jarrar.de), then License, Text and fonts, SD card and files, Engine.
- **Secret Settings** (last Credits page, A): Reorder list (A+DPAD) On / Off, Delete files Off / On, Delete configuration (`Sure?`).

## Start+B: leave without saving

- Start+A still saves. **Start+B now goes to Home without saving**; the text stays open. Its row has the play mark ▶ and `B: Resume active file` takes you back exactly where you were.
- Opening another file or New File with unsaved changes asks **Save / Discard / Cancel**.
- The open file cannot be renamed or deleted ("This file is open. Open another file first.").
- There is no autosave: switching off loses text that is not saved.

## Notes

- The card is read once at start to show Home (`Checking SD...`); without a card Home says `No SD card` after the driver gives up.
- Verification: host suites (also with AddressSanitizer/UBSan), FatFS image fault injection, and the exact ROM on gbamp3's modeled Supercard SD in mGBA (Home, reorder and restart, rename, import with numbered copies, file name formats, New File, Start+B / Resume / Save-Discard-Cancel, Delete, Secret Settings, Controls and Credits). Not tested on real hardware.

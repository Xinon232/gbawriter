# gbawriter V4.0 — plan (not implemented yet)

Status: implemented in V4.0 (see RELEASE_NOTES.md). Base: gbawriter **V3.0** (`release/v3.0.0`, `1f9e45c`).
References: gbamp3 **v1.8**, gbavocab **V4.3**, gbareader **V3.1.1** (gbareader-dev).

Goal: gbawriter gets the gbamp3 look and the gbavocab / gbareader menu model.
**Writing itself (the editor and all typing controls) does not change**, except
that Start+B no longer saves (see 4). The editor's bottom bar stays as it is,
including how it clips long file names.

---

## 1. Look

- Every screen except the editor looks like gbamp3 v1.8 / gbavocab V4.3:
  white background, small centred 5x7 header, eight 17-pixel rows, light-blue
  bar on the selected row, long names scroll sideways on the selected row,
  ▶ play mark at the right end of a row.
- **No instruction lines on screen.** The only fixed hint is the grey bottom
  line `B: Resume active file` while a file is open (see 4).
- Source: port gbavocab's Butano list code (same engine as gbawriter):
  `ui_canvas.h`, `entries_nav.h`, `entries_screen.h/.cpp` (`run_entries`,
  `run_menu`, `run_question`, `draw_message`). Its icons (book, play) and the
  underline under the first row come with it. Keep gbawriter's existing 5x7 UI
  font and SuperFW body font.
- The blue Butano `>` cursor goes away (the light-blue bar replaces it).
- The New File date picker and the error / "Please note" screens get the same
  look; their keys do not change.

## 2. Home = the file list

```
            gbawriter
 New File                     [icon]
 ───────────────────────────────────    <- line + gap, as under "Dictionaries" in gbavocab
 Holiday notes
 02.10.2026                       ▶     <- the open file
 01.10.2026
 ...
 B: Resume active file                   <- grey, only while a file is open
```

- Title `gbawriter`. First row **New File** with an icon and the same gap and
  line that gbavocab draws under *Dictionaries*. Then every `.txt` in
  `/gbawriter`, **without** `.txt`.
- **At power-on the cursor is always on New File** (diary use; the last file
  is not remembered).
- No TXT files: one grey row `No TXT files in /gbawriter`; no card: `No SD card`.
- Up / Down move (hold to repeat; a fresh press at either end wraps), Left /
  Right one page. No L + Up/Down letter jumps (the order is the user's own).
- Up to **1,000 files** are listed (all names must be in RAM to be reordered);
  the README states the limit.

### Keys on Home

| Key | Action |
|---|---|
| A | New File: date picker. A file: open it (▶ file: resume it). A acts on **release** (so A+Up/Down never opens). |
| B | Resume the open file (only while one is open). |
| A held + Up / Down | Move the selected file one row (Secret Settings, default On). New File always stays first. |
| Start | **Import** (section 5). |
| Select | **Menu** (section 6). |

## 3. List order (saved)

- Starting order (first run, or after Delete configuration): as today —
  valid diary dates newest first, then other names A to Z.
- After that the saved order is used. A+Up/Down changes it and it is saved at
  once (no Start needed), like gbamp3 playlists.
- Files the order does not know yet (new from New File, Import, or copied on a
  PC) appear **at the top**, directly under New File, newest first.
- Rename keeps the file's place; Delete removes it; files missing from the
  card are dropped from the order.
- Stored in `GBAWRITER.SYS` (section 8).

## 4. Leaving the editor without saving (gbavocab model)

| In the editor | V3.0 | V4.0 |
|---|---|---|
| Start + A | save | save (unchanged) |
| Start + B | save, then menu (only on success) | **back to Home without saving** |

- The text stays in RAM; the file is the **active file**: ▶ on its row and
  `B: Resume active file` at the bottom of Home. B (or A on the ▶ row) goes
  back into the editor with caret, view and Shift/Caps as they were.
- Opening another file or New File while the active file has unsaved changes
  asks (gbavocab wording): `Unsaved file` / `Save changes before switching?` —
  **A: Save, B: Discard, Select: Cancel**. A failed save keeps the active file
  and its text.
- Opening another file replaces the active file. There is no separate Close.
- Unsaved text is lost if the GBA is switched off — said in Controls and the
  manual (as in gbavocab: no autosave).
- Help pages and the manual drop "START+B: save, then main menu".

## 5. Import (Start on Home) — as gbareader V3.1.1 / gbavocab V4.1

- Title `Import to /gbawriter`. Starts at the SD-card root. Folders first (folder
  icon), then `.txt` files (case-insensitive extension), each in card order or
  A to Z as gbareader does. Hidden / system entries and `/gbawriter` itself
  are not shown.
- A on a folder opens it; B goes up; B at the root returns Home.
- A on a TXT asks `Import into /gbawriter?` (No selected first). Yes copies it
  with a percent display; a failed copy leaves no partial file.
- **Name already used**: ask to import a numbered copy, e.g. `Diary (2).txt`
  (gbavocab `import_target_name`: 2 to 99, nothing is replaced). Too long or
  no free number: "Not imported" with the reason.
- Afterwards Home shows with the new file at the top (section 3) and selected.
- Code: gbareader-dev `reader_browse.cpp` (browse, copy) + gbavocab
  `import_files.cpp` (numbered names).
- Importing while a file is active is allowed; the active file stays open.

## 6. Select menu (gbavocab Help list style)

A gbamp3 list. Its header is the selected file's name (or `gbawriter` on New File).

1. **Rename** — selected file only (not on New File).
2. **Delete** — selected file only, and only when Secret Settings > Delete
   files is On.
3. **File names: 01.10.2026** — the date format for new files (section 7).
4. Controls topics (section 9), each opens its pages.
5. **Credits** — last item; Secret Settings on its last page (section 10).

B returns Home.

### Rename

- gbamp3 *Rename* screen: a text field with the current name **without
  `.txt`**, typed with the normal gbawriter letter system; Start+A saves,
  Start+B cancels. `.txt` is added automatically.
- Refused with a message, nothing changed:
  - the file is the active file: `This file is open. Open another file first.`
  - empty name, `/ \ : * ? " < > |`, name too long (250 bytes incl. `.txt`);
  - name already used, also case-only (as New File);
  - recovery copies (`.gwt` / `.gwb` / `.gwi`) exist for the file:
    `RECOVERY: CHECK SD ON PC`.

### Delete

- The row turns into `Sure?`; A again deletes, B or moving the cursor cancels
  (gbamp3 Secret Settings style).
- Same refusals as Rename for the active file and for recovery copies.
- Only the `.txt` is removed; the order entry is dropped.

## 7. File name format for New File

| Setting | Example | Date picker order |
|---|---|---|
| DDMMYYYY (default, = V3.0) | `01102026.txt` | Day, Month, Year |
| DD.MM.YYYY | `01.10.2026.txt` | Day, Month, Year |
| MMDDYYYY | `10012026.txt` | Month, Day, Year |
| MM.DD.YYYY | `10.01.2026.txt` | Month, Day, Year |
| YYYY-MM-DD | `2026-10-01.txt` | Year, Month, Day |

- A on the menu row cycles the formats; the row shows the proposed date in
  that format. Saved in `GBAWRITER.SYS`.
- Changes only the names of **new** files. The list always shows real names.
- The date picker shows its fields in the format's order; keys unchanged
  (Up/Down field, Left/Right value, A create, B back). No format switch in the
  date picker.
- Proposed date = the day after the newest diary file **in the chosen format**
  (DDMMYYYY and MMDDYYYY cannot be told apart, so only the chosen one counts).
  None found: the last date chosen, else 10 July 2026.
- The starting order (section 3) recognises all five formats as dates for
  "newest first"; a name valid in both DDMMYYYY and MMDDYYYY is read in the
  chosen format.
- Collision rule unchanged: never overwrite, also case-only.

## 8. Settings file `/gbawriter/GBAWRITER.SYS`

- A folder, as gbamp3 `GBAMP3.SYS` / gbavocab `GBAVOCAB.SYS` (does not show
  in the file list or confuse users on a PC).
- Two slots `STATE0.DAT` / `STATE1.DAT`, magic + generation + FNV-1a checksum;
  the newer valid one is read, writes go to the other (gbamp3 `state.c`).
- Holds: file name format, last chosen date, Reorder list switch, Delete files
  switch, the list order (file names, up to 1,000).
- Missing or damaged: defaults; the app keeps working. Write failures show a
  short message; the documents are never touched by these writes.
- The card is read once at start (Home is the file list), with `Checking SD...`.

## 9. Controls (Select menu topics)

Rewritten in plain words, gbavocab V4.3 / gbareader page layout: title with a
grey page number on the right, one line per row, grey `#` subheadings, no
instruction line, Left / Right pages, B back to the menu. Topics:

1. Files and menus — Home, New File, Import, Select menu, A+Up/Down, B: Resume.
2. Letters — D-pad + B/A/R; g and v.
3. L layer.
4. Shift / Caps.
5. Symbols — Select cycles (digits, punctuation, signs).
6. Accents — both orders, cycling.
7. Caret and saving — Start + D-pad / L / R, Start+A save, Start+B leave
   without saving, Start+Select status bar, no autosave.

Text reused from gbavocab's Typing pages where identical, checked against the
gbawriter input code. Every line is checked for glyph coverage and screen width
(existing `extract_help.py` / glyph tests).

## 10. Credits and Secret Settings

gbavocab / gbamp3 / gbareader order, one part per page:

1. `gbawriter V4.0`, Made by Halim Jarrar, (C) 2026, halimj.itch.io, gba@halim-jarrar.de
2. License: GPL 3.0 or later, source github.com/Xinon232/gbawriter
3. Text and fonts: SuperFW text renderer (David Guillen Fandos, GPL 3.0+);
   UNSCII (Viznut), GNU Unifont, Unifont-derived Hangul; UI 5x7 font from gbamp3
4. SD card and files: SuperFW SD driver (GPL 3.0+); FatFs by ChaN (BSD style)
5. Engine: Butano by Gustavo Valiente (zlib); devkitPro / devkitARM
6. Secret Settings: `Press A to open Secret Settings.`

Secret Settings (gbamp3 list):

- `Reorder list (A+DPAD): On` (default On)
- `Delete files: Off` (default Off)
- `Delete configuration` — `Sure?`, A again: removes `GBAWRITER.SYS` (order and
  settings back to defaults). Never touches `.txt` files.

The README and manual credits follow the same order.

## 11. Documents and version

- Version **V4.0** in Credits page 1, README, release notes, PDF.
- README rewritten for the new Home / Import / menu / Start+B / Secret Settings,
  limits (1,000 files) and the unchanged hardware warning.
- `gbawriter-full-controls.pdf` regenerated (`tools/build_controls_pdf.py`).
- `RELEASE_NOTES.md`: V4.0 section on top.

## 12. Tests

- Host: list order (new files on top, move, rename, delete, unknown/missing
  files), state file (two slots, damaged slot, defaults), date formats
  (format/parse/propose for all five, ambiguous names), import names
  (numbered copies), rename/delete refusals (active file, recovery copies,
  case-only collisions), Start+B leaves without saving and Resume restores the
  text, Save/Discard/Cancel question.
- FatFS image tests extended for rename, delete, import copy and the state
  slots with fault injection.
- Existing editor/input suites must pass unchanged (writing is not changed).
- Memory gate (`make memory-check`) keeps its limits; 1,000 names must fit.
- Emulator run of the new screens; physical hardware stays unverified.

## 13. Work order

1. Port the gbavocab list widget and draw Home (read-only list, A opens).
2. Active file + Start+B leave + B: Resume + Save/Discard/Cancel.
3. `GBAWRITER.SYS`, list order, A+Up/Down.
4. Select menu: Rename, Delete, File names format, date picker order.
5. Import.
6. Controls and Credits pages, Secret Settings.
7. Restyle date picker and messages.
8. Docs, PDF, tests, memory gate, release notes.

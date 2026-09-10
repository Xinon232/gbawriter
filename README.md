# gbawriter — V1.2

Create and edit TXT files on your Game Boy Advance. Save your writing directly to the SD card, or open an existing text file to keep working. Put your TXT files in `/gbawriter` at the root of the SD card. Requires a compatible Supercard SD.

Based on [GBAReader v0.5.0](https://github.com/Xinon232/gbareader), using Butano menus, the SuperFW bitmap text renderer and fonts, and Supercard SD/FatFS storage. There is no QWERTY keyboard, network service, or AI component.

> **Physical hardware remains unverified.** Host tests and software filesystem fault injection pass; these are **not proof that saving is safe on a real GBA + Supercard SD**. Use disposable documents and a backed-up SD card until the hardware checklist below is completed. Do not entrust the only copy of a diary to this release. V1.2 is a prerelease candidate; this does not certify physical-hardware safety.

## Hardware and installation

- A GBA-compatible system and **Supercard SD** using the inherited SuperFW-compatible SD driver. Other flashcarts and generic emulator save files are not supported storage backends.
- Copy `gbawriter.gba` to your flashcart and launch it through its usual ROM loader. The source build produces the ROM; published builds belong under this repository's [Releases](https://github.com/Xinon232/gbawriter/releases).
- Documents live only in **`/gbawriter/`** on the SD card. The app attempts to create the directory when New/Load is first selected. An SD/directory error is displayed instead of discarding text.
- Menu and control instructions do not probe SD at boot. Choosing New/Load shows `CHECKING SD...`; an absent/incompatible card can take a substantial driver timeout to report an error. Emulator menu/help operation does not imply SD access works.
- Back up the SD card before testing. Never remove the card or power off while `SAVING` is displayed.

## Diary and file workflow

**New File is selected at startup.** Press A to choose a date, then A to create and edit. The preset is exactly one calendar day after the chronologically latest valid diary filename, including month/year/leap-year transitions. Filenames use **`DDMMYYYY.txt`**; raw alphabetical order is not used to choose the latest date.

If no dated documents exist, the manual starting date is **10 July 2026**. This is an editable preset, not a real-time clock. In the date picker, Up/Down selects Day/Month/Year; Left decrements and Right increments the field; A creates; B returns. Invalid day combinations are clamped. Supported years are 1–9999. At the upper calendar boundary there is no representable next day: the picker retains the latest date, and creation still refuses a collision.

**New File never overwrites an existing name**, including case-only `.TXT` differences. A collision shows `FILE ALREADY EXISTS` and returns to the unchanged proposed date after acknowledgment. It does not add suffixes or truncate the original.

**Load File** lists `.txt` files in `/gbawriter/`: valid diary dates newest first, then other names alphabetically (ASCII case-insensitive). Up/Down selects; A opens; B returns. The display shows six entries at a time; the storage index rescans in bounded 32-entry batches, so the directory is not limited to 32 files. Longer directories take longer to rescan. There are no delete, rename, duplicate, or overwrite-from-menu actions.

## Controller layout

Main-menu **SELECT** opens controls and **START** opens credits; **B** returns from either. The adjacent bottom hints read exactly `Select: Controls` and `Start: Credits`. Credits always open on the personal first page: `Made by Halim Jarrar`, `(C) 2026`, `halim-jarrar.de`, and `monday@halim-jarrar.de`, with no third-party credit mixed into that page. Left/Right cycles the two Credits pages; B returns from either. The second page credits the SuperFW software font renderer, UNSCII fonts (`viznut.fi/unscii`, inherited source marked GPL), and Unifont-derived Hangul blocks. Font notices remain in the source.

The suite home screen reads `gbawriter V1.2` and `files: /gbawriter`, with NEW FILE first/default and LOAD FILE second. The UI label is independent of the release tag.

Menu interface text, help section headings and navigation hints use the existing blue UI font. Numeric date values, text filenames, saving indications, controls body text and credits content retain the SuperFW-based writing font. Editor typography and status positions are unchanged.

The [full-controls PDF](gbawriter-full-controls.pdf) includes the description, file placement and every control below, with author credit Halim Jarrar. Regenerate it with `python3 tools/build_controls_pdf.py` in an environment with ReportLab and DejaVu Sans installed.

Hold a direction, then press **B / A / R** to choose its **first / second / third** letter. Lowercase is the default.

| Direction | Normal: B / A / R | Hold L: B / A / R |
|---|---|---|
| Up | a / b / c | d / e / f |
| Right | h / i / j | k / l / m |
| Down | n / o / p | q / r / s |
| Left | t / u / w | x / y / z |

**L is held, never toggled.** Diagonals are not letter groups.

- Keep Right held and press R twice: the first `j` becomes **`g`**.
- Keep Left held and press R twice: the first `w` becomes **`v`**.
- Release the group between presses to type literal `jj` or `ww`. These transformations depend on the same continuous hold, **not timing**, and never substitute unrelated text.
- **A alone:** space. **B alone:** backspace one UTF-8 codepoint; no effect at the beginning. Hold either button alone to repeat: one immediate edit, first repeat after **24 frames**, then every **5 frames**, exactly matching START navigation. Adding any other button cancels edit repeat; releasing a chord's modifier does not start it. Release A/B and press it alone again to rearm. Letter/SELECT/save chords remain press-only.
- **START released alone:** newline. Holding it does not insert immediately.

### Shift and Caps

From normal case, **release a short isolated R press to arm one-shot Shift**. Hold **R alone continuously for 48 frames** (twice the initial A-repeat delay; about 0.8 seconds at 60 Hz) to enable **Caps while still held**, once per hold. Any other button before the threshold cancels that hold; releasing the companion cannot restart it. Release R and start a fresh solo hold.

With **Shift or Caps already active**, another isolated R press—short or long—**clears the mode on release**. It cannot arm Shift or Caps again during the same hold. Shift applies to the next successfully inserted alphabetic character; digits, spaces and punctuation do not consume it.

**With Caps on, one isolated R press/release turns Caps off without arming Shift.** R inside a direction group remains the third-letter key and cannot toggle Caps. Shift/Caps applies to the international letters below; `ß` stays `ß`, never two letters. Rejected capacity-limited edits do not consume Shift. A failed save preserves Shift/Caps as well as the document.

The bottom status bar reads **filename → active letter group → Shift/Caps**. The filename starts at x=8 (128-pixel slot), the group at x=144 (32-pixel slot), and Shift/Caps at x=184 (48-pixel slot): hold Up to see `abc`, or `ABC` with Shift/Caps; all normal and held-L groups follow the same rule. Release the direction to clear it; diagonals and START navigation show no letter group. `*` marks unsaved changes. Transient messages occupy only the filename slot, leaving capitalization and group visible.

### START commands

Hold START, then:

| Button | Action |
|---|---|
| Left / Right | Move caret one UTF-8 codepoint |
| Up / Down | Move to the closest horizontal position in the adjacent visual row |
| L / R | Previous / next viewport page, maintaining a valid caret |
| A | Save the current file |
| B | Save, then return to the main menu **only on success** |
| SELECT | Toggle the bottom status bar / full-screen editor |

Navigation repeats while held. Releasing START after a recognized command **does not add a newline**, even when saving fails. A save failure opens a clear error message and returns to the editor with text retained after acknowledgment. There is no discard shortcut or autosave.

### SELECT: one provisional character

Press SELECT alone to insert **`.` immediately**. Keep SELECT held to change **that same character**; release to keep it. Exception: SELECT after an eligible continuously held letter chord converts the existing letter instead (see International letters). Cycling never appends additional characters.

| While SELECT is held | Cycle |
|---|---|
| Up | `. → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 0 → 1 …` |
| Down | `. → 0 → 9 → 8 → 7 → 6 → 5 → 4 → 3 → 2 → 1 → 0 …` |
| R, without a direction | `. → , → ' → " → : → ! → ? → .` |
| L, without a completed letter chord | Exact reverse common-punctuation cycle |
| Right | `. → ( → ) → / → ; → @ → # → % → & → _ → + → = → - → .` |
| Left | Exact reverse additional-symbol cycle |

A completed letter chord takes priority over intermediate directional/punctuation changes. For example SELECT, then Up, then L, then A changes the same provisional `.` to `1` and then to `é`, not three characters. A complete L-layer chord similarly overrides the temporary SELECT+L punctuation step. Unsupported accent families leave the provisional glyph unchanged. **START+SELECT toggles the status bar in the editor only**, in either press order. It cancels only a newly inserted SELECT provisional character, restoring the original caret and dirty state. A converted existing letter stays accented; it is not deleted. The chord never types, saves, or adds a newline. It toggles once per hold and suppresses commands until both START and SELECT are released; other held buttons do not prevent rearming.

### International letters

**Accents work in either order:** hold SELECT, then enter the normal letter combination; or enter a letter and keep its **exact single D-pad direction, L if used, and producing B/A/R button continuously held**, then press SELECT. That same letter changes to its first accented variant, keeping its case, without adding a period or another letter. There is no time limit, but releasing or changing the combination ends eligibility. Other extra buttons disqualify this letter-first gesture; diagonals are not letter groups. A letter with no accent variants stays unchanged in the letter-first order.

**Example:** hold L+UP and press A to type `e`; keep L+UP+A held and press SELECT to turn that same `e` into `é`. In the other order, hold SELECT, then L+UP and press A to produce `é` as one character. The held-L groups work in both orders too.

While SELECT and the same group remain held, **release and press the final B/A/R selector again** to cycle in the exact order below. Keep the direction and L layer (if used) held during cycling. Release SELECT to keep the current result. The letter-first result keeps the original letter's lowercase/Shift/Caps case, even after one-shot Shift was consumed.

| Family | Lowercase cycle | Uppercase equivalents |
|---|---|---|
| A | á ä à â ã å æ | Á Ä À Â Ã Å Æ |
| C | ç č ć | Ç Č Ć |
| E | é è ë ê | É È Ë Ê |
| I | í ï ì î | Í Ï Ì Î |
| N | ñ ń | Ñ Ń |
| O | ó ö ô ò õ ø œ | Ó Ö Ô Ò Õ Ø Œ |
| S | ß š ś | ß Š Ś |
| U | ü ú ù û | Ü Ú Ù Û |
| Y | ý ÿ | Ý Ÿ |
| Z | ž ź ż | Ž Ź Ż |

E/S/Y/Z use the held L layer; N/O/U are unmodified groups. Cycles wrap back to the first alternate. All 77 distinct required lowercase/uppercase glyphs are verified against the real font packs and renderer, including even/odd pixel addressing. Main-menu **SELECT** opens 20 control pages; Left/Right changes page; B returns. Help strings are checked for glyph coverage and screen width.

## Text, memory and limits

- Maximum document content: **24 KiB (24,576 UTF-8 bytes)**, not characters. Oversize files, malformed UTF-8, and embedded NUL are rejected without changing the current buffer. Required precomposed Latin letters render; other valid Unicode bytes are preserved but glyph coverage is not guaranteed.
- A fixed contiguous editable buffer is the source of truth, not the rendered page. Edits move bounded memory and reflow a fixed visual-row index; this implementation is **not a gap buffer**, despite some inherited internal member names. No unbounded document allocation is used.
- Caret movement/backspace is codepoint-based, not grapheme-cluster-based. Combining marks can therefore be navigated separately. No normalization or encoding conversion occurs.
- Soft wrapping is display-only **whole-word** wrapping: words that fit the viewport move intact to the next row; only oversized words split across rows. Whitespace remains in the visual row index (including trailing spaces), never trimmed or rewritten. Inter-word spaces/tabs carried onto an automatic continuation row have zero display advance, so the next word starts at the left margin. Deliberate indentation at document start or after explicit newlines stays visible even when it spans rows. Hidden separators remain individually reachable with Left/Right and backspace; vertical navigation chooses the earliest byte when several caret positions share x=0. Reflow looks ahead once per word and never saves additional line breaks. Explicit Enter inserts LF. Existing CRLF and BOM bytes are preserved; CR/BOM have zero display width, and tabs occupy a fixed 24 pixels rather than tab stops. Mixed line endings are possible after editing CRLF documents.
- Editor text begins at y=0 with unchanged 16-pixel glyph height and 18-pixel line pitch. The bar occupies y=144–159; a six-pixel gutter is reserved above it. Complete rows only: **7 rows with the bar, 9 full-screen**. Page navigation uses the current row count. Bar visibility defaults on at boot and remains a session preference across saves, errors and documents; it does not alter stored files.
- The vertical caret is a drawn graphic primitive, not a text character, with GBA-safe halfword writes. It stays visible on typing/navigation and blinks after about one second idle (36-frame phases).
- Loaded filenames must fit 250 bytes to leave room for transient recovery suffixes. Longer names can be listed but are refused when opened. Long visible names are clipped by pixel width; the status buffer retains complete UTF-8 names, including the dirty marker.
- Measured clean devkitARM build: **139,484 bytes EWRAM occupied, 122,660 bytes remaining**; IWRAM user-stack headroom **26,900 bytes**; largest checked runtime-source static stack frame **1,640 bytes**. App and storage objects are explicitly in EWRAM. The memory gate enforces at least 64 KiB EWRAM headroom, 20 KiB IWRAM stack headroom and a 2 KiB individual-frame ceiling. This is a budget check, **not a proof of total call-stack/IRQ depth**.
- No EPUB editor, undo/redo, clipboard, search, autosave, RTC dependency, or general file manager.

## Saving and interrupted-save recovery

Save updates **only the currently opened file** as plain UTF-8 bytes. It never appends the GBAReader bookmark footer or creates a permanent `.sav` sidecar.

The implementation writes 512-byte chunks to a same-directory `filename.txt.gwt`, syncs/closes and reopens it to compare bytes, writes a transient `filename.txt.gwi` size/hash manifest, renames the original to `filename.txt.gwb`, renames staging to the original name, verifies the result, then removes recovery artifacts. Success is shown only after required filesystem operations succeed. This reduces risk; **FAT rename is not atomic**, and SD/controller power-loss behavior remains unverified.

Automatic recovery restores or validates copies only in states it can safely interpret. **Canonical file + `.gwt` is deliberately left untouched**: a failed FatFS rename can make two names share a cluster chain, and deleting either alias can destroy the canonical data. Ambiguous states report `RECOVERY: CHECK SD ON PC` and can block New/Load scanning until resolved. A failed save leaves in-memory text dirty.

If this happens:

1. Do not delete `.gwt`, `.gwb` or `.gwi` on the original SD to make the warning disappear.
2. Preserve the card with a full image/backup; work on a copy. Power loss still loses unsaved RAM edits, so keep the device powered while considering recovery where practical.
3. Inspect/export all readable versions from the copy using filesystem-recovery tools. Compare original, staging and backup text. Cross-linked names need filesystem-aware repair; ordinary unlink is unsafe.
4. Only after recovering an independent verified document and repairing a working copy should artifacts be cleared or a fresh backed-up card be used.

The app is conservative rather than promising automatic recovery from every interruption. Full disks, torn sectors, repeated faults, FAT32 fault sweeps, and physical Supercard persistence remain outside the verified fault model.

## Build and test

Requirements: GNU Make, Python 3, host GCC/G++, devkitPro/devkitARM, and Butano pinned to **`77dcbcb3d8783596a9f333c64eedbccec77b05dc`**. CI uses `devkitpro/devkitarm:20260610`; local verification used the installed devkitARM toolchain. Clone Butano separately and check out that commit.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
export LIBBUTANO=/absolute/path/to/butano/butano
./tests/run_host_tests.sh
make clean
make -j2
make memory-check
```

Outputs: `gbawriter.gba`, `gbawriter.elf`, `build/gbawriter.map`, and compiler `.su` stack reports. `LIBBUTANO` must point to Butano's **inner** `butano` directory. Clean rebuilds are required when changing compile flags. The default Supercard mirror optimization remains disabled (`SC_FAST_ROM_MIRROR=0`).

Additional verification:

```sh
EXTRA_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' ./tests/run_host_tests.sh
# Requires dosfstools; uses a temporary regular image file, never a device:
python3 tests/run_fatfs_tests.py
```

The normal runner includes writer core/app/layout/storage/frame/format suites, a 100,000-operation deterministic text-model differential test, real glyph/help checks, memory-gate rejection tests, and preserved GBAReader regression suites. Portable FatFS QA compiles the production `__DEVKITARM__` filesystem branch with actual FatFS, substituting only sector-backed host disk I/O and bypassing hardware initialization. Verified: **795 checks, 137 injected disk-request boundaries, zero failures; 62 conservative manual-recovery outcomes**. Host operation-level storage tests separately inject 34 save failures. These tests are not physical hardware validation.

`make test` is only an optional short mGBA immediate opcode/header rejection check, not boot or storage proof. CI builds on main/PR/manual dispatch with read-only repository permission and uploads ROM/ELF artifacts; it does not publish releases automatically. The clean build retains warnings in inherited reader/vendor sources; new writer sources build without warnings.

## Real-hardware checklist — not yet completed

Use a real GBA and target Supercard SD, a backed-up card and disposable documents:

- [ ] Discover/create `/gbawriter/`; visible directory/card failure handling.
- [ ] A,A diary creation; manual date picker; chronological date preset across leap/month/year boundaries.
- [ ] Duplicate/case-only filename refusal; original bytes unchanged on a computer.
- [ ] Directory sorting, mixed-case `.TXT`, long names and more than 32 files.
- [ ] Load, edit, save, shorter/empty replacement, repeated saves and reboot persistence; no truncation or permanent artifacts.
- [ ] Long sessions up to the byte limit; insert/delete at wrap boundaries, navigation repeat, pages, caret placement and blinking.
- [ ] Every normal/L-layer chord, same-held G/V versus separate jj/ww, Shift/Caps and Caps-off override.
- [ ] START newline/save/save-menu/navigation; no extra newline after commands; failed save retains buffer and capitalization.
- [ ] SELECT one-character replacement, exact digit/punctuation/symbol cycles and all international lower/uppercase glyphs.
- [ ] Open resulting UTF-8 files on Windows/Linux/macOS and compare exact bytes, including accents, BOM/CRLF and empty files.
- [ ] Safely induced write failures and interrupted recovery on disposable card copies, plus full-disk behavior. Never experiment with valuable diary data.

## License and attribution

The repository carries **GNU GPL v3** in [LICENSE](LICENSE). This modified application preserves GBAReader's source/assets and their notices. Credit to GBAReader, SuperFW's Supercard driver and software font renderer, Butano, ChaN's FatFS, the font authors, and miniz. See original notices in `references/superfw/`, `src/ff.c`, `src/ffunicode.c`, and `third_party/miniz/`; Butano carries its own licenses in the pinned checkout. Reader-only sources/tests remain for provenance and regressions, not as advertised Writer features. Do not remove upstream or font notices when redistributing.

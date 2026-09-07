# GBA Writer v0.3.1 — hardware-unverified prerelease

## Changes from v0.3.0

- Suppress display advance for inter-word spaces/tabs carried onto a soft-wrapped row. Original bytes remain unchanged; deliberate indentation at document start and after explicit LF/CRLF remains visible, including long indentation.
- Rendering, caret positioning and vertical navigation share the same display start. Hidden separator bytes remain reachable by horizontal movement and backspace; vertical x=0 ties choose the earliest byte in the row.
- Added exact real-font framebuffer, UTF-8/BOM/CRLF, long separator/indentation, trailing whitespace and maximum-capacity regressions. No input, storage, font or unrelated UI changes.

## Retained v0.3.0 changes (from v0.2.1)

- Main-menu START opens credits; B returns without accessing SD. The writing-font body reads `Made by Halim Jarrar`, `(C) 2026`, `halim-jarrar.de`, `monday@halim-jarrar.de`.
- The adjacent bottom prompts are exactly `Select: Controls` and `Start: Credits`, in the existing blue UI font. Menu labels, date labels, load markers, error explanations and navigation hints use that font; numeric date values and text filenames retain the writing font. Controls/credits content and saving indications also retain the writing font.
- Display-only **whole-word** wrapping keeps fitting words intact, splitting only words wider than the viewport. One lookahead per word keeps reflow linear. Original UTF-8 bytes, whitespace, BOM, CRLF and explicit newlines remain unchanged; row indexing and caret navigation follow the same display boundaries.
- Editor body/status fonts and positions, controls, storage/save/recovery behavior and font assets are unchanged.
- Added sanitizer-backed production menu font-routing and bounds checks plus word-wrap/UTF-8/whitespace/navigation/capacity/linear-work regressions. Updated controls PDF is included as a release asset.

## Retained v0.2.1 changes (from v0.2.0)

- Hold **A alone** to repeat spaces or **B alone** to repeat UTF-8 codepoint backspace. The existing START-navigation timer is shared: immediate initial edit, first repeat after **24 frames**, then every **5 frames**.
- Repeat requires a fresh isolated press. Any additional button cancels it; chord/modifier release tails cannot rearm it. Letter, SELECT, save and status-toggle chords keep their existing press/release behavior.
- Status labels now read **Shift** and **Caps**, retaining x=184 / y=144 and all filename/group/body positions.
- Added exact-timing, release/rearm, direct-switch, chord-exclusion, capacity and multibyte-backspace regressions; real-font full-framebuffer tests cover the titlecase labels at unchanged coordinates.
- Release includes **GBA-Writer-Controls.pdf**, the complete controller reference, credited to **Halim Jarrar**, **(C) 2026**. `SHA256SUMS.txt` covers both ROM and PDF.
- Storage, fonts, help pages and unrelated behavior are unchanged.

## Retained v0.2.0 changes

- Bottom status bar in **filename → active letter group → Shift/Caps** order, at x=8 / 144 / 184 respectively. All normal/L-layer groups show lowercase or uppercase with Shift/Caps, including while a transient save message is displayed.
- Body text starts at y=0, retaining 16-pixel glyphs, 18-pixel line pitch and the six-pixel reserved gutter above the y=144 bar: seven complete rows with the bar, nine in full-screen mode.
- **START+SELECT** toggles the bar in the editor only. Either press order works; a live SELECT provisional is rolled back without changing prior text, caret or dirty state. One toggle per chord, with no typing, newline, save, or misleading save feedback; rearm after both chord keys release.
- Isolated R taps cycle **normal → Shift → Caps → normal**, with **no timing window**. One-shot Shift, rejected-edit safety, international letters and all grouped R/G/V behavior remain unchanged.
- Date picker: Up/Down chooses the field, Left decrements, Right increments.
- Thirteen menu-SELECT help pages, updated controls and sanitizer-backed status/filename and input regressions.
- Storage format, save/recovery implementation, fonts, colors, text pitch and all unrelated functionality are unchanged.

## Project

GBA Writer is a controller-native plain UTF-8 diary/editor for Game Boy Advance, derived from GBAReader v0.5.0's Butano/SuperFW/Supercard SD foundation. This is a Writer update, not a GBAReader v0.5.0 update.

**Use backed-up cards and disposable documents. Real GBA + Supercard SD saving has not been verified. Do not use this prerelease as the only copy of diary data.** Software fault injection and emulator execution cannot establish real-card power-loss safety.

## Included

- Runnable `gbawriter.gba` built from the normal Makefile; app/storage explicitly placed in EWRAM rather than crowding IWRAM stack.
- `/gbawriter/` document directory; date-aware `DDMMYYYY.txt` creation and newest-first diary loading with bounded, rescanned file-list batches.
- No-overwrite New File, editable initial manual date **10 July 2026**, leap/calendar arithmetic, and visible errors.
- All normal/held-L letter groups; same-hold JJ→G and WW→V; UTF-8 insertion/backspace; drawn caret, wrapped-row navigation, pages and held-navigation repeat.
- Isolated R normal/Shift/Caps cycle; **one isolated R disables active Caps without arming Shift** (approved control override). Failed saves/rejected edits preserve capitalization.
- Deferred START newline; START+A save; START+B save/menu only on success.
- One provisional SELECT character with exact digit, punctuation, symbol and international-letter cycles. All required 77 distinct lower/uppercase glyphs are present; `ß` stays `ß`.
- Thirteen in-app instruction pages checked for actual glyph coverage and width.
- Plain-text replacement saves with checked buffered I/O, staging/backup/manifest recovery and conservative refusal of ambiguous FatFS rename states. No reader bookmark footers or permanent document sidecars.

## Verification

- Full host runner passes, including all writer suites and preserved reader regressions.
- C++ suites pass under AddressSanitizer + UndefinedBehaviorSanitizer. A separate instrumented real-renderer regression covers **1,026 clean/dirty filename headers**, including multibyte boundaries and maximum supported lengths. It also compiles the actual editor render branch and compares complete framebuffers for all group/case positions, top-origin text/caret, the reserved gutter, seven/nine rows, bar toggles and transient messages.
- Deterministic text model differential test: **100,000 operations**.
- Actual font renderer: **77 required glyphs** rendered nonblank with odd/even-address bounds checks; all 78 help lines have glyph coverage and fit the width budget.
- Portable production FatFS/FAT16 integration: **795 checks, zero failures, 137 injected disk-request boundaries**, including **62 preserved manual-recovery states**. Separately, 34 host save-operation failures are exercised.
- Clean devkitARM build and memory budget gate pass. EWRAM: **139,468 bytes occupied / 122,676 free**. IWRAM user-stack headroom: **26,900 bytes**. Largest checked runtime-source static frame: **1,640 bytes**. These are static budgets, not a total call-stack/IRQ proof.
- CI is main/PR/manual build-and-test only, with read-only repository permission, a dedicated actual-FatFS job, and ROM/ELF artifact upload. No automatic release publication.

Independent emulator verification is reported with release QA artifacts; do not interpret a ROM smoke check as hardware or SD verification.

## Limits and recovery warning

- **24 KiB maximum UTF-8 content**, fixed contiguous buffer and bounded visual-row index. Oversize, malformed UTF-8 and embedded NUL input are rejected.
- Only required font coverage is guaranteed. Editing is codepoint-based, not grapheme-based. Wrapping is display-only whole-word wrapping, splitting only oversized words; tabs have fixed display width; Enter inserts LF and existing whitespace/CRLF/BOM bytes are preserved.
- Supercard SD only; no generic emulator-save, other-flashcart, EPUB, undo, autosave or file-manager functionality.
- SD probing is lazy; missing-card errors can take a long driver timeout after selecting New/Load.
- FAT rename is **not atomic**. Canonical + `.gwt` may share clusters after a failed rename. Ambiguous states are kept intact and require PC-side recovery; never blindly delete recovery artifacts on the original card. Preserve a full card image and recover independent copies first.
- Real hardware, full-disk/torn-sector/persistent-fault cases, FAT32 fault sweeps and sustained target-device performance remain unverified. Existing reader/vendor build warnings remain documented; new Writer sources build without warnings.

See [README](README.md) for complete controls, build commands, recovery precautions, attribution and the unchecked physical-hardware acceptance checklist. Preserve all upstream licensing notices.

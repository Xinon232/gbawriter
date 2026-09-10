# gbawriter V1.2 — input-layout prerelease candidate

- Based on the verified GitHub v1.1.0 release, on a separate release branch; not published yet.
- Exact original groups relocated: Up abc, L+Up def; Right hij, L+Right klm; Down nop, L+Down qrs; Left tuw, L+Left xyz. B/A/R remains first/second/third.
- Hold unmodified Right and press R twice for j → g; hold unmodified Left and press R twice for w → v. Continuous-group sessions, not timing. Release the direction between presses for jj/ww; g/v have no accents.
- All accent families follow their base letters, in unchanged cycling order. Select-before and Select-after a continuously held letter chord retain case, replacement and host acceptance/cancellation behavior.
- Home title is `gbawriter V1.2`; active-group displays, all 20 in-app Controls pages and the complete instructional PDF match the new layout. Author: Halim Jarrar, before third-party font attribution.
- Added the explicitly requested second Credits page for SuperFW, UNSCII and Unifont-derived Hangul, with Left/Right paging. The first page preserves only the existing Halim Jarrar personal block and always opens first. PDF credits and navigation match.
- All other controls, Shift/Caps timing, persistence, fonts and UI positions remain unchanged. Physical hardware has not been tested.
- This candidate stops before independent review, commit/push and publication. Current artifact hashes and actual verification results are in the parent handoff, not the historical notes below.

## Historical v1.1 notes (unchanged)

# gbawriter v1.1 — final release (prepared locally)

- Short isolated R release arms Shift from normal case. An uninterrupted R-only hold enables Caps at 48 frames, twice the initial A repeat delay, while held.
- Any companion before the threshold cancels eligibility until R is released and freshly pressed alone. Shift/Caps clears on the next isolated R release, short or long; that hold cannot rearm.
- R-containing letters, repeated g/v, both Select accent orders, START navigation, whole-word wrapping and persistence are retained. No index or save-validation changes.
- Complete in-app Controls and PDF updated; author Halim Jarrar retained. Physical hardware and SD power-loss safety remain unverified.
- Fixed R-API-1: public press/release events now synchronize frame edges, preserving canceled R sessions and the 48-elapsed-frame boundary when mixing APIs. Caps timing requires an actually isolated R snapshot. User controls and PDFs are unchanged by this follow-up.
- Rebuilt `gbawriter.gba` SHA256: `dacbff8365fe4ec5db5785de9805e09f9b34396ee9855e4d9867be6e04d83b81`. Current frozen artifacts and verification: `input-fix-handoff.md` / `input-fix-freeze.json` in the suite v1.1 evidence directory; these supersede the earlier input handoff binaries.
- Prepared locally for parent review; not committed, pushed or published. Historical hashes below are unchanged.

## Historical v1.0.0 notes

# gbawriter v1.0.0 — hardware-unverified prerelease

Create and edit TXT files on your Game Boy Advance.

## Changes since v0.3.1

- Lowercase `gbawriter V1.0` home title, `/gbawriter` folder label, and clearer menu/help typography.
- Accents now work in either press order: hold Select then type, or keep the exact producing letter chord held and press Select to change that same letter. Case is retained; no duplicate letter or period is inserted. Start+Select cancels only a newly inserted provisional character, not an existing letter converted to an accent.
- Complete 19-page in-app help and four-page `gbawriter-full-controls.pdf`, including both accent orders, examples, typing, navigation, saving and recovery cautions.
- Portable input and production-host regression matrices integrated into the full host runner.

## Release assets and verification

- `gbawriter.gba` SHA256: `00327229f91ecc433245f86b8ab4e28fea85237da6464a4a5ee977cd3b7d7a63`
- `gbawriter-full-controls.pdf` SHA256: `9c7a0eac792202b4c9e3ec972cda3c2e001a1cde090ab7119ba71993190a0335`
- Frozen source/artifact manifests match the reviewed build. Full host and ASan/UBSan suites passed; production FatFS/FAT16 integration passed 795 checks with 137 injected disk-request boundaries and zero failures.
- Exact-ROM emulator checks passed all 19 help pages, credits and repeated menu round trips. All four PDF pages were visually inspected.
- Limited debugger-seeded empty-editor keypad evidence demonstrated letter-first conversion and cycling only; the optional full emulator typing suite did not complete. Broader input semantics are covered by production-host tests, not claimed as full emulator verification.
- **Physical GBA/Supercard hardware and save safety remain unverified.** Use backed-up cards and disposable documents; never keep the only copy of your writing here.
- Release tag is `v1.0.0`; the verified app/PDF display `V1.0`. The inherited internal ROM header title remains `GBA WRITER`; it was not changed or rebuilt solely for publication metadata.
- CI results must be read from the exact release commit; local verification is not a claim that GitHub CI is green. CI has read-only permissions and no release-upload automation.

## Historical release notes (unchanged; not current feature counts)

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

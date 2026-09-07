# GBA Writer v0.1.0 — initial hardware-unverified prerelease

GBA Writer is a controller-native plain UTF-8 diary/editor for Game Boy Advance, derived from GBAReader v0.5.0's Butano/SuperFW/Supercard SD foundation. This is the first Writer release, not a GBAReader v0.5.0 update.

**Use backed-up cards and disposable documents. Real GBA + Supercard SD saving has not been verified. Do not use this prerelease as the only copy of diary data.** Software fault injection and emulator execution cannot establish real-card power-loss safety.

## Included

- Runnable `gbawriter.gba` built from the normal Makefile; app/storage explicitly placed in EWRAM rather than crowding IWRAM stack.
- `/gbawriter/` document directory; date-aware `DDMMYYYY.txt` creation and newest-first diary loading with bounded, rescanned file-list batches.
- No-overwrite New File, editable initial manual date **10 July 2026**, leap/calendar arithmetic, and visible errors.
- All normal/held-L letter groups; same-hold JJ→G and WW→V; UTF-8 insertion/backspace; drawn caret, wrapped-row navigation, pages and held-navigation repeat.
- Isolated R Shift/double-R Caps; **one isolated R disables active Caps without arming Shift** (approved control override). Failed saves/rejected edits preserve capitalization.
- Deferred START newline; START+A save; START+B save/menu only on success.
- One provisional SELECT character with exact digit, punctuation, symbol and international-letter cycles. All required 77 distinct lower/uppercase glyphs are present; `ß` stays `ß`.
- Twelve in-app instruction pages checked for actual glyph coverage and width.
- Plain-text replacement saves with checked buffered I/O, staging/backup/manifest recovery and conservative refusal of ambiguous FatFS rename states. No reader bookmark footers or permanent document sidecars.

## Verification

- Full host runner passes, including all writer suites and preserved reader regressions.
- C++ suites pass under AddressSanitizer + UndefinedBehaviorSanitizer. A separate instrumented real-renderer regression covers **1,026 clean/dirty filename headers**, including multibyte boundaries and maximum supported lengths.
- Deterministic text model differential test: **100,000 operations**.
- Actual font renderer: **77 required glyphs** rendered nonblank with odd/even-address bounds checks; all 72 help lines have glyph coverage and fit the width budget.
- Portable production FatFS/FAT16 integration: **795 checks, zero failures, 137 injected disk-request boundaries**, including **62 preserved manual-recovery states**. Separately, 34 host save-operation failures are exercised.
- Clean devkitARM build and memory budget gate pass. EWRAM: **139,468 bytes occupied / 122,676 free**. IWRAM user-stack headroom: **26,900 bytes**. Largest checked runtime-source static frame: **1,640 bytes**. These are static budgets, not a total call-stack/IRQ proof.
- CI is main/PR/manual build-and-test only, with read-only repository permission, a dedicated actual-FatFS job, and ROM/ELF artifact upload. No automatic release publication.

Independent emulator verification is reported with release QA artifacts; do not interpret a ROM smoke check as hardware or SD verification.

## Limits and recovery warning

- **24 KiB maximum UTF-8 content**, fixed contiguous buffer and bounded visual-row index. Oversize, malformed UTF-8 and embedded NUL input are rejected.
- Only required font coverage is guaranteed. Editing is codepoint-based, not grapheme-based. Wrapping is character-based; tabs have fixed display width; Enter inserts LF and existing CRLF/BOM bytes are preserved.
- Supercard SD only; no generic emulator-save, other-flashcart, EPUB, undo, autosave or file-manager functionality.
- SD probing is lazy; missing-card errors can take a long driver timeout after selecting New/Load.
- FAT rename is **not atomic**. Canonical + `.gwt` may share clusters after a failed rename. Ambiguous states are kept intact and require PC-side recovery; never blindly delete recovery artifacts on the original card. Preserve a full card image and recover independent copies first.
- Real hardware, full-disk/torn-sector/persistent-fault cases, FAT32 fault sweeps and sustained target-device performance remain unverified. Existing reader/vendor build warnings remain documented; new Writer sources build without warnings.

See [README](README.md) for complete controls, build commands, recovery precautions, attribution and the unchecked physical-hardware acceptance checklist. Preserve all upstream licensing notices.

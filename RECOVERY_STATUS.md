# GBA Writer implementation / recovery checkpoint

## Release candidate

- Initial **v0.1.0 hardware-unverified prerelease**. Runtime implementation is integrated, not the old prototype.
- Full 2530-line specification read; approved overrides retained: initial manual date **10 July 2026**; one isolated R turns active Caps off without arming Shift.
- ROM: `gbawriter.gba`, 839,764 bytes, SHA-256 `d514021f589301d1dc647c34a9f9a4f94d316a6ae7522b0716a4115a7370bf5b`.
- Matching final clean-build ELF SHA-256: `ab8322fc8055522291203dc9965efb656fe1d6b7fe683050c0d9e6ea12692f76`.
- Pinned Butano `77dcbcb3d8783596a9f333c64eedbccec77b05dc` at `/home/halim/.local/share/butano/butano` in this environment.

## Implemented and verified in software

- Host-testable core, layout, storage and application controller; actual Butano/SuperFW UI adapter with twelve complete help pages, status strip and GBA-safe graphical caret.
- UTF-8/capacity protection, all letter/SELECT cycles and uppercase counterparts, continuous-hold G/V, whole-frame input precedence, deferred START commands, held navigation, capitalization redraw/retention and rejected-edit rollback.
- Fixed 24 KiB contiguous editable buffer and bounded visual-row index. Large app/storage objects explicitly in `.sbss` EWRAM.
- Safe New File no-clobber, chronological dates, 32-entry rescanned load batches without a 32-file directory cap, exact UTF-8 load/save, checked transient staging/backup/manifest and conservative ambiguous recovery.
- Allocation-free bounded Writer formatting/strict manifest parser fixes actual normal-build linker failure caused by unavailable libc snprintf/sscanf.
- Runner now executes writer app/layout/storage/frames/format as well as core, differential 100,000-operation test, actual 77-glyph rendering, all 72 help-line coverage/width checks, memory-gate rejection tests, and preserved reader suites.
- Host and C++ ASan+UBSan runs pass. Final review added a reproduced filename-header regression: complete UTF-8 filenames now fit before pixel clipping; 1,026 clean/dirty header cases pass through the real renderer under ASan/UBSan.
- Portable production FatFS runner: **795 checks, 0 failures, 137 injected disk-request boundaries, 62 preserved manual-recovery states**. Separate native storage suite verifies 34 operation-level failure points.
- Final clean normal build passes. EWRAM occupied/free **139468/122676** bytes; user-stack headroom **26900** bytes; largest checked runtime-source static frame **1640** bytes. Budget gate passes; not a proof of cumulative stack/IRQ depth.
- No new Writer compile warnings. Inherited reader/FatFS/Supercard/font/miniz warnings remain; see external final-clean-build.log.
- README and release notes now document v0.1.0, complete controls, limits, recovery precautions, unchecked hardware checklist and attribution. CI is main/PR/manual read-only build + artifact upload and separate dosfstools-based actual-FatFS job; no automatic publication. Python generated caches ignored.

## Critical filesystem finding retained

Failed FatFS renames can leave canonical + `.gwt` cross-linked. Never unlink either to clear a warning: doing so can free the canonical's cluster chain. Production recovery preserves ambiguous files and returns RECOVERY_NEEDED; actual-FatFS regressions enforce no disk mutation in manual-recovery states. PC recovery must begin with a whole-card backup/image and independent copies.

## Remaining verification / ownership

- **Physical GBA + Supercard SD verification remains undone.** Thus the real-hardware safety acceptance criterion is not satisfied. Do not present software QA as proof of safe real diary storage.
- Final source is frozen for hash-bound emulator verification, independent review, and GitHub CI/publication. Consult GitHub for current publication state; this source checkpoint is not proof that a release has been published.
- Emulator editor fixture QA is debugger-seeded and explicitly not a storage workflow. Parent maintains those evidence reports separately.
- Known limitations: 24 KiB, codepoints not graphemes, character wrapping, fixed-width tabs, LF new input/retained CRLF+BOM, unknown glyphs not guaranteed, long SD timeout, conservative manual recovery, no full-disk/torn-sector/persistent-fault/FAT32 fault sweep or sustained hardware performance proof.

## Reproduce / external handoff

From this checkout:

```sh
./tests/run_host_tests.sh
EXTRA_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' ./tests/run_host_tests.sh
python3 tests/run_fatfs_tests.py
make LIBBUTANO=/home/halim/.local/share/butano/butano clean
make LIBBUTANO=/home/halim/.local/share/butano/butano -j2
make LIBBUTANO=/home/halim/.local/share/butano/butano memory-check
git diff --check
```

Environment: DEVKITPRO=/opt/devkitpro, DEVKITARM=/opt/devkitpro/devkitARM. Handoff and logs: `/home/halim/.cache/gbawriter-release-qa/implementation-handoff.md`; ROM-ready checkpoint: `/home/halim/.cache/gbawriter-release-qa/ROM_READY`.

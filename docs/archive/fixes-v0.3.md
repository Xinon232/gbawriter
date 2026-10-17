# GBA Writer — things still to fix

Tracking only. Do not implement these items until explicitly requested.

- [x] **Unwanted indentation at the start of some automatically wrapped lines** — fixed for v0.3.1
  - Report: Sometimes the first word on a new visual line does not start at the normal left margin.
  - Likely explanation, supported by current `src/writer_layout.cpp`: when a line is full, the space separating two words can itself wrap onto the next row. The renderer retains that space's display width, so the following word starts slightly indented. This also applies to multiple spaces or tabs that cross a wrap boundary. The user's exact text has not yet been reproduced.
  - Desired outcome: Ordinary inter-word spacing carried across an automatic wrap should not visibly indent the next word.
  - Constraints for a future fix: Keep original document bytes unchanged. Distinguish automatic-wrap separators from intentional indentation after explicit newlines. Keep UTF-8 caret movement, vertical navigation, editing and row indexing consistent with the display.
  - Resolution: Display-only suppression of inter-word spaces/tabs carried onto a soft row. Document bytes stay unchanged; deliberate document-start/explicit-newline indentation remains visible, including indentation spanning rows. Rendering, caret position and vertical navigation share the same display start; hidden bytes remain reachable by Left/Right and backspace.
  - Verified: RED/GREEN layout and real-font framebuffer regressions; full host ASan/UBSan; clean ROM build and unchanged memory budgets; exact-ROM emulator with real authored text, wrap navigation, hidden-space backspace, explicit indentation and UTF-8. All 41 authored input deliveries passed on their first attempt. Emulator steady pacing measured 57.8–58.6 FPS; an overly narrow 59–61 FPS-only harness check failed and its trace is preserved, with no dropped inputs or text retries. Physical hardware remains unverified. Evidence: `/home/halim/.cache/gbawriter-wrap-indent-qa/`. Commit/publication awaits parent review.

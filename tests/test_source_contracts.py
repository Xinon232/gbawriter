#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text()
core = (root / "src/writer_core.cpp").read_text()
header = (root / "include/writer_core.h").read_text()
make = (root / "Makefile").read_text()
app = (root / "src/writer_app.cpp").read_text()
helps = (root / "src/writer_help.cpp").read_text()
# V4.0: Home is the file list with New File first; Credits name V4.0.
assert '"New File"' in app and '"gbawriter"' in app and '"B: Resume active file"' in app
assert '"gbawriter V4.1"' in helps and 'halimj.itch.io' in helps and 'gba@halim-jarrar.de' in helps
assert 'based on gbareader' not in helps.lower()
assert '"gbawriter V3.0"' not in main and "LOAD FILE" not in main
assert "draw_text_idx8_bus16_range" in main
assert "bn::sprite_font ui_font(" in main
assert 'writer::Application app' in main
assert 'section(".sbss")' in main  # integrated app + storage must be in EWRAM
assert 'volatile uint16_t' in main  # caret must use GBA-safe halfword writes
assert '"Checking SD..."' in app
assert 'HELP_TOPICS' in (root / "include/writer_help.h").read_text()
# Files stream from SD; only unsaved typed text is capped in RAM.
assert "TEXT_CAPACITY = 64 * 1024" in header and "class TextSource" in header
assert "MAX_ROWS" in (root / "include/writer_layout.h").read_text()
assert "parse_diary_name" in core and "next_day" in core
assert "alternate_letter" in core and "SELECT" in core
assert "TARGET      :=  gbawriter" in make
runner = (root / "tests/run_host_tests.sh").read_text()
for suite in ("app", "layout", "storage", "frames", "v4"):
    assert f'test_writer_{suite}' in runner, f"missing writer {suite} suite"
print("PASS: writer source contracts")

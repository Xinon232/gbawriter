#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text()
core = (root / "src/writer_core.cpp").read_text()
header = (root / "include/writer_core.h").read_text()
make = (root / "Makefile").read_text()
assert "gbawriter V1.1" in main and "files: /gbawriter" in main and "NEW FILE" in main and "LOAD FILE" in main
assert "draw_text_idx8_bus16_range" in main
assert "bn::sprite_font ui_font(" in main
assert 'writer::Application app' in main
assert 'section(".sbss")' in main  # integrated app + storage must be in EWRAM
assert 'volatile uint16_t' in main  # caret must use GBA-safe halfword writes
assert 'CHECKING SD' in main
assert 'HELP_PAGES' in main
assert "TEXT_CAPACITY = 24 * 1024" in header
assert "parse_diary_name" in core and "next_day" in core
assert "alternate_letter" in core and "SELECT" in core
assert "TARGET      :=  gbawriter" in make
runner = (root / "tests/run_host_tests.sh").read_text()
for suite in ("app", "layout", "storage", "frames"):
    assert f'test_writer_{suite}' in runner, f"missing writer {suite} suite"
print("PASS: writer source contracts")

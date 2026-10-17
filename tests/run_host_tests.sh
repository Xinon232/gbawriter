#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/gbawriter-tests-XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT/include")
if [[ -n "${EXTRA_CXXFLAGS:-}" ]]; then
    read -r -a EXTRA_CXXFLAGS_ARRAY <<< "$EXTRA_CXXFLAGS"
    CXXFLAGS+=("${EXTRA_CXXFLAGS_ARRAY[@]}")
fi

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_writer_core.cpp" "$ROOT/src/writer_core.cpp" \
    -o "$OUT/test_writer_core"
"$OUT/test_writer_core"

for suite in test_input_layout_v12 test_solo_r test_writer_v020 test_writer_v4 test_writer_app test_writer_layout test_writer_storage test_writer_frames test_writer_format test_text_model_differential test_writer_large; do
    g++ "${CXXFLAGS[@]}" "$ROOT/tests/$suite.cpp" \
        "$ROOT/src/writer_core.cpp" "$ROOT/src/writer_layout.cpp" \
        "$ROOT/src/writer_storage.cpp" "$ROOT/src/writer_app.cpp" "$ROOT/src/writer_format.cpp" "$ROOT/src/writer_help.cpp" -o "$OUT/$suite"
    "$OUT/$suite"
done

# Font coverage of a TXT/EPUB glyph corpus (the fonts are shared with gbareader).
python3 "$ROOT/tests/generate_epub_fixtures.py" "$OUT/fixtures"
python3 "$ROOT/tests/test_glyph_coverage.py" "$OUT/fixtures"
python3 "$ROOT/references/superfw/res/fonts/generator.py" \
    --font-files "$ROOT/references/superfw/res/fonts/unscii-16-full.hex" \
    --font-blocks reader-punctuation,reader-currency,reader-letterlike \
    --output "$OUT/reader-symbols.pack" >/dev/null
cmp "$ROOT/references/superfw/res/reader-symbols.pack" "$OUT/reader-symbols.pack"

gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-old-style-declaration -Wno-discarded-qualifiers -I"$ROOT/references/superfw/src" -I"$ROOT/references/superfw/src/fonts" \
    "$ROOT/tests/test_writer_glyph_runtime.c" -o "$OUT/test_writer_glyph_runtime"
python3 "$ROOT/tests/extract_help.py" "$OUT/help.txt"
(cd "$OUT" && ./test_writer_glyph_runtime "$ROOT/references/superfw/res/fonts.pack" "$ROOT/references/superfw/res/reader-symbols.pack" "$OUT/help.txt")
python3 "$ROOT/tests/test_source_contracts.py"
python3 "$ROOT/tests/test_memory_gate.py"
python3 "$ROOT/tests/test_writer_header.py"
python3 "$ROOT/tests/test_writer_menu.py"
python3 "$ROOT/tests/test_writer_v020_docs.py"
python3 "$ROOT/tests/test_select_accent_docs.py"
bash "$ROOT/tests/run_select_accent_tests.sh"

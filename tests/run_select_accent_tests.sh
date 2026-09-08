#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
flags=(-std=c++17 -Wall -Wextra -Werror -Iinclude)
if [[ -n ${EXTRA_CXXFLAGS:-} ]]; then read -r -a extra <<< "$EXTRA_CXXFLAGS"; flags+=("${extra[@]}"); fi
g++ "${flags[@]}" src/writer_core.cpp tests/test_select_accent.cpp -o "$OUT/core"
"$OUT/core"
if [[ -f include/entry_editor.h ]]; then
 g++ "${flags[@]}" -DSELECT_ACCENT_VOCAB src/writer_core.cpp src/writer_layout.cpp src/entry_editor.cpp src/vocab.cpp tests/test_select_accent_host.cpp -o "$OUT/host"
else
 g++ "${flags[@]}" src/writer_core.cpp src/writer_layout.cpp src/writer_app.cpp src/writer_storage.cpp src/writer_format.cpp tests/test_select_accent_host.cpp -o "$OUT/host"
fi
"$OUT/host"

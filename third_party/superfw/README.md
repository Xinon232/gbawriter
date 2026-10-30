# SuperFW parts used by gbawriter

From SuperFW by David Guillen Fandos (https://github.com/davidgfnet/superfw),
GPL-3.0-or-later (see the notice in each file and `LICENSE` at the top of this
repository). Taken through GBAReader v0.5.0 (Halim Jarrar), which added a
second font pack for reader symbols (`reader_font_base_addr`) and the
`reader-*` blocks of the generator.

- `src/utf_util.c`, `src/utf_util.h`: UTF-8 helpers.
- `src/fonts/font_render.c`, `font_render.h`, `font_embed.h`, `hangul.h`:
  the bitmap text renderer. Built through `src/superfw_font.c`.
- `res/fonts.pack`, `res/reader-symbols.pack`: font packs, included in the
  ROM by `src/superfw_font_pack.s`. Both are made by `res/fonts/generator.py`
  (GBAReader added the `reader-*` symbol blocks) from `res/fonts/*.hex`; the
  commands are in `NOTICES.md` and `res/fonts/README.md`.
- `res/fonts/unscii-16-full.hex`, `res/fonts/hangul-blocks.hex`: UNSCII by
  Viznut and the Hangul blocks of GNU Unifont, GPL-2.0-or-later with the font
  embedding exception, see `res/fonts/LICENSE-Unscii.txt`.

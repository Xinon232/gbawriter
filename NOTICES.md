# gbawriter 10.0 — notices and credits

gbawriter, Copyright (c) 2026 Halim Jarrar.

gbawriter is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version (GPL-3.0-or-later). The full text is in [LICENSE](LICENSE).

gbawriter is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

Source code: https://github.com/Xinon232/gbawriter

The application code (`src/`, `include/writer_*.h`,
`include/ffconf.h` settings, `tools/`) is by Halim Jarrar and
started from GBAReader v0.5.0 (also by Halim Jarrar), whose notices are kept.

## Parts made by others

Each file keeps its own copyright and license notice.

- **SuperFW** by David Guillen Fandos, Copyright (C) 2024-2025,
  GPL-3.0-or-later (https://github.com/davidgfnet/superfw):
  - text renderer and UTF-8 helpers: `third_party/superfw/src/utf_util.c`,
    `utf_util.h`, `third_party/superfw/src/fonts/font_render.c`,
    `font_render.h`, `font_embed.h`, `hangul.h` (built through
    `src/superfw_font.c`);
  - `include/gbahw.h`, `include/compiler.h`;
  - the font pack generator `third_party/superfw/res/fonts/generator.py`
    (with GBAReader's `reader-*` symbol blocks) and the packs it makes,
    `third_party/superfw/res/fonts.pack` and `reader-symbols.pack` (both
    rebuilt byte for byte by the commands in
    `third_party/superfw/res/fonts/README.md` and below);
  - the SuperCard SD driver in gbaSD (`gbasd/src/supercard_driver.c` and the
    files listed in `gbasd/CREDITS.md`).
- **Fonts** in the SuperFW packs: UNSCII by Viznut (http://viznut.fi/unscii/)
  and the Hangul blocks of GNU Unifont, `third_party/superfw/res/fonts/*.hex`:
  GPL-2.0-or-later with the GNU font embedding exception, see
  `third_party/superfw/res/fonts/LICENSE-Unscii.txt`.
- **UI font**: the 5x7 small font of gbamp3 by Halim Jarrar (MIT),
  `tools/gbamp3_small_font/small_font.c`, turned into
  `graphics/ui_small_font.bmp` and `include/ui_small_font.h` by
  `tools/make_ui_small_font.py`.
- **gbaSD** (git submodule `gbasd`, https://github.com/Xinon232/gbaSD),
  GPL-3.0-or-later, Halim Jarrar, with:
  - EZ-Flash Omega driver by Halim Jarrar, written after the EZ-Flash Omega
    and Omega DE kernels and ezfo-disc_io (Apache-2.0; no file of those
    projects is included);
  - EverDrive GBA SD code: port of edio-gba by krikzz
    (https://github.com/krikzz/gba-ed-pub), Copyright (c) 2024 krikzz, MIT
    (notice kept in `gbasd/src/edgba_driver.c`);
  - FatFs R0.15 by ChaN (http://elm-chan.org/fsw/ff), Copyright (C) 2022 ChaN,
    FatFs license (BSD style; notice kept in each file). `include/ffconf.h` is
    FatFs's configuration file with this app's settings.
  - see `gbasd/CREDITS.md` and `gbasd/LICENSE`.
- **Butano** by Gustavo Valiente (https://github.com/GValiente/butano),
  zlib license, see `third_party/butano/LICENSE`. Butano is not part of this
  repository (check it out at the commit named in the README);
  `include/common_variable_8x16_sprite_font.h` and
  `graphics/ui_variable_8x16_font.*` come from its `common` folder.
- Built with **devkitPro / devkitARM**.

## Font packs

```sh
cd third_party/superfw/res/fonts
./generator.py --font-files unscii-16-full.hex hangul-blocks.hex \
  --font-blocks cjk-sym,latin,latin-a,latin-b,greek,cyrilic,hiragana,katakana,cjk-uni,hangul-part \
  --output ../fonts.pack
./generator.py --font-files unscii-16-full.hex \
  --font-blocks reader-punctuation,reader-currency,reader-letterlike \
  --output ../reader-symbols.pack
```

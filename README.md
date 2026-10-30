# gbawriter 10.0

Create and edit TXT files on your Game Boy Advance. Save your writing directly to the SD card, or open an existing text file to keep working. Put your TXT files in `/gbawriter` at the root of the SD card, or import them from anywhere on the card. Runs on the **SuperCard SD**, the **EZ-Flash Omega** (also the Definitive Edition; start the ROM from the SD card, not from NOR) and the **EverDrive GBA**.

This repository holds the complete source code of gbawriter 10.0. Every control is in the manual, [gbawriter-full-controls.pdf](gbawriter-full-controls.pdf).

> Physical hardware is not verified for this release: host tests and emulator runs on modeled SD cards pass, but they are not proof that saving is safe on a real cart. Back up your SD card and try it with disposable documents first.

## In short

- **Home** is your file list: New File (a date-named diary file), then every TXT file in `/gbawriter`. Hold A + Up/Down to reorder.
- **Typing without a keyboard:** hold a direction, then B / A / R for its first / second / third letter; hold L for more letters; Select for digits, punctuation, symbols and accents (á ä ç é ñ ö ß ü …). Short R: Shift, held R: Caps.
- **Start+A** saves, **Start+B** goes back to Home without saving (the text stays open). There is no autosave.
- **Start on Home:** import a TXT file from anywhere on the card. **Select on Home:** Rename, Delete, file name formats, the Controls pages and Credits.
- **L+R on Home:** sleep; Start held 1 s wakes it.
- No file size limit: an opened file stays on the SD card, only your changes use RAM (up to 64 KiB of unsaved typed text). Saves go through a staging copy that is checked before it replaces the file.

## Build

The ROM is built from this source with:

- devkitPro **devkitARM r68** (arm-none-eabi-gcc 16.1.0, binutils 2.46.0, newlib 4.6.0), GNU Make and Python 3;
- **Butano** at commit `77dcbcb3d8783596a9f333c64eedbccec77b05dc` (https://github.com/GValiente/butano);
- the **gbaSD** submodule (`gbasd`, https://github.com/Xinon232/gbaSD) at the commit recorded in this repository.

```sh
git clone --recursive https://github.com/Xinon232/gbawriter.git
cd gbawriter
# (already cloned without --recursive? then: git submodule update --init)
git clone https://github.com/GValiente/butano.git ../butano
git -C ../butano checkout 77dcbcb3d8783596a9f333c64eedbccec77b05dc
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
make -j2 LIBBUTANO="$PWD/../butano/butano"
```

Output: **`gbawriter.gba`** in the top folder (plus `gbawriter.elf` and the `build/` folder). `LIBBUTANO` must point to Butano's inner `butano` folder. With the toolchain above the build is byte-identical to the released ROM:

```
5ebf7c4dde862a3313d239ea053f0ce211ab8a7a0b0c260f5de42ae32814b242  gbawriter.gba
```

`make clean LIBBUTANO=...` removes the build.

PC-side tools in `tools/` (not needed for the build; their outputs are in the repository): `make_ui_small_font.py` regenerates `graphics/ui_small_font.bmp` and `include/ui_small_font.h` from gbamp3's 5x7 font (`tools/gbamp3_small_font/small_font.c`, needs a host C compiler); `build_controls_pdf.py` is the generator of the PDF manual (ReportLab and DejaVu Sans; it reads the sections of the full manual README, which this repository's short README does not have). The font packs are made by `third_party/superfw/res/fonts/generator.py`, see [NOTICES.md](NOTICES.md).

## License

gbawriter 10.0, Copyright (c) 2026 Halim Jarrar (halimj.itch.io, gba@halim-jarrar.de).

gbawriter is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version (**GPL-3.0-or-later**, see [LICENSE](LICENSE)).

gbawriter is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Credits

Parts made by others, each with its own notice in the files ([NOTICES.md](NOTICES.md) has the details):

- **SuperFW** by David Guillen Fandos — text renderer, UTF-8 helpers and font packs (`third_party/superfw/`) and the SuperCard SD driver (in gbaSD): GPL-3.0-or-later.
- **Fonts:** UNSCII by Viznut and GNU Unifont (Hangul blocks): GPL-2.0-or-later with the font embedding exception. UI font: gbamp3's 5x7 font by Halim Jarrar (MIT).
- **gbaSD** by Halim Jarrar (GPL-3.0-or-later), with the EverDrive GBA SD code from edio-gba by krikzz (MIT), an EZ-Flash Omega driver written after the Omega kernel (Apache-2.0) and **FatFs** by ChaN (FatFs license, BSD style).
- **Butano** by Gustavo Valiente: zlib ([third_party/butano/LICENSE](third_party/butano/LICENSE)).
- Built with **devkitPro / devkitARM**.

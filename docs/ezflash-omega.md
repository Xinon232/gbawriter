# EZ-Flash Omega support (test build)

The same document is in gbareader, gbawriter, gbavocab and gbamp3: the four
apps share one Omega driver.

## Status

- **Test build, not yet tried on a real EZ-Flash Omega.** Everything below
  was checked on an emulator with a model of the Omega (see *How it was
  tested*). Back up the SD card before trying it, and keep the SuperCard
  release for daily use until the hardware checklist below is done.
- **EZ-Flash Omega:** the target. **Omega Definitive Edition (DE):** expected
  to work (its kernel uses the same SD routines and pages), not tested.
- **Start the ROM from the SD card (PSRAM).** Writing it to the Omega's NOR
  flash is not supported: the app then stops on a red screen at start.
- The SuperCard keeps working as before: one ROM for both carts.

## How it works

- **Which cart?** Checked once at every start, in a few milliseconds:
  1. SuperCard: SDRAM writes are switched off first, so the Omega's unlock
     writes below cannot change the ROM in a SuperCard.
  2. The Omega kernel is mapped for a moment: the ROM header must disappear
     and come back afterwards. The NOR flash ID is read and kept for the
     diagnostics, but it does not decide (boards with other NOR chips work).
  3. Sector 0 of the SD card must answer with 55 AA.
  Otherwise the SuperCard driver starts as before.
  **Hold L+R while the app starts** to skip the Omega check (SuperCard forced).
- **SD access.** The Omega maps its kernel over the game ROM while the SD
  card is used, so the driver runs from RAM, maps the kernel for one command
  (up to 4 sectors) at a time and maps the game back after each command.
  Registers, order and timeouts are those of the Omega kernel
  (`Read_SD_sectors` / `Write_SD_sectors`). A failed read is reported, not
  copied. A write that times out twice makes later writes fail at once
  (reads go on), so a card that takes no writes cannot be written halfway.
- **Interrupts.** gbareader, gbawriter and gbavocab turn interrupts off for
  each command. gbamp3 keeps its sound interrupt running (the music must not
  stop): every handler that can run meanwhile is in RAM, and the screen
  switch the sound interrupt does in the vertical blank waits for the next
  interrupt while the card is in use. `tests/check_ram_code.py` checks the
  linked ROM for that.
- **In-game menu / reset / sleep patches of the Omega kernel.** They hook the
  game's interrupt vector with code in the ROM. While the kernel is mapped,
  the driver points the vector straight at the app's own handler. Turning
  these options off for the four apps is still recommended.
- **gbamp3 memory.** IWRAM and EWRAM are nearly full, so the SD driver code
  that must run from RAM shares one IWRAM slot with the SuperCard driver's
  RAM code (only one cart is used per boot; the right one is copied in at
  start). Memory limits and the main-stack gate are unchanged.

## How it was tested (emulator, not hardware)

- `tests/emu/ezfo_model.h` (gbamp3: `tests/ezfo_model.h`): a register-level
  Omega model for the exact-ROM mGBA runners. While the kernel is mapped, any
  instruction fetch or data read from the game ROM stops the run. It can
  fail reads and writes, slow the card down, report other NOR IDs, have no
  card, refuse all writes, and apply the kernel's in-game IRQ patch.
- gbareader: `tests/emu/ezfo_test.py` (book import and reading in all those
  variants; the SuperCard screenshots are unchanged).
- gbawriter and gbavocab: `tests/emu/ezfo_test.py` runs one script on the
  SuperCard model and on the Omega model; every screen and every file on the
  card must be identical.
- gbamp3: the full exact-ROM acceptance suite (`tests/run_acceptance.py`,
  with `EZFO=1 python3 -B tests/build_runner.py`) passes on the Omega model,
  including the 15 audio stress runs with no underruns, and still passes on
  the SuperCard model.
- `tools/ezfo_diag` in gbareader: a diagnostics ROM for testers.

## Hardware checklist

Use a backed-up card. Note the Omega model (Omega / DE) and the kernel
version. Name the test ROMs `*-ezfo-test1.gba` so they are not mixed up with
releases (the version shown in the apps is unchanged).

1. **ezfo_diag** (from gbareader `tools/ezfo_diag`): start it, check that it
   says *EZ-Flash Omega*, *Header gone yes, back yes*, *SD sector 0 ok*,
   *FAT mounted* and *ROM image unchanged*; press A, check *Read back same*
   and *ROM still unchanged*. Send a photo or `/ezfo-diag.txt`.
2. **gbareader:** the book list appears; open a TXT, an EPUB and a comic;
   turn pages; Start saves the place; restart and check the place; Import a
   book from another folder.
3. **gbawriter:** New File, type, Start+A saves; open it again; Import;
   Rename; restart and check the file on a PC.
4. **gbavocab:** open a list, answer, Start saves; Entry editor: add an
   entry; open a dictionary and search; Import a list.
5. **gbamp3:** Update Library with covers; play MP3 and GAC for several
   minutes in Songs and Now Playing (listen for gaps); scroll fast while
   playing; seek; lock (L+R) and unlock; a playlist; convert one MP3 to GAC.
6. **Every app:** hold L+R while starting: the app must report that there
   is no SD card (gbamp3: *SD not ready*), because the SuperCard driver
   finds no SuperCard; nothing else may happen.
7. Optional: the same with the Omega's in-game menu / reset / sleep options
   switched on.

## Credits

The driver (`ezfo_driver.c`, `cart_io.c`) is by Halim Jarrar, MIT licence,
written after the EZ-Flash Omega and Omega DE kernels
(github.com/ez-flash/omega-kernel, omega-de-kernel; Apache License 2.0) and
ezfo-disc_io by Felix Jones and ApacheThunder (Apache License 2.0).

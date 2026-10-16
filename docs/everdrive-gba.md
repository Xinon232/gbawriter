# EverDrive GBA support (test build)

The same document is in gbareader, gbawriter, gbavocab and gbamp3: the four
apps share one EverDrive driver (and one Omega driver, see
[ezflash-omega.md](ezflash-omega.md)).

## Status

- **Test build, not yet tried on a real EverDrive.** Everything below was
  checked on an emulator with a model of the EverDrive (see *How it was
  tested*). Back up the SD card before trying it.
- **EverDrive GBA Mini:** the target (krikzz's published SD code for it is
  the base). **EverDrive GBA X5 and X3:** expected to work, not tested
  (krikzz's published code is for the Mini; the X series is believed to use
  the same registers). Other EverDrive GBA models are not covered.
- Start the app from the EverDrive menu like any game. The save type does not
  matter: the apps keep their files on the SD card.
- The SuperCard and the EZ-Flash Omega keep working as before: one ROM for
  all three carts.

## How it works

- **Which cart?** Checked once at every start, in a few milliseconds:
  1. SuperCard: SDRAM writes are switched off first, so the writes below
     cannot change the ROM in a SuperCard.
  2. EverDrive: the register key is written and the EverDrive's register
     window is switched off and on; the status, version and SD registers
     must read differently in the two states (on another cart both reads see
     the same ROM bytes). The game ROM always stays mapped and writable PSRAM
     is never switched on.
  3. If that found no EverDrive: the Omega check
     ([ezflash-omega.md](ezflash-omega.md)), then the SuperCard driver.
  The EverDrive is checked first because its configuration register sits
  where the Omega check writes its lock value.
  **Hold L+R while the app starts** to skip both checks (SuperCard forced).
- **SD access.** The GBA drives the SD card itself through the EverDrive's
  registers, as krikzz's edio-gba code does: SD commands with their CRC,
  4-bit data reads (CMD18) and writes (CMD25) with the cart's CRC buffer,
  SDSC and SDHC/SDXC cards, v1 and v2 cards. The game ROM stays mapped, so
  the driver runs from ROM and interrupts (gbamp3's sound) keep running. A
  failed read is closed and retried once; a failed write is reported.
  Every wait has a limit, so a dead card cannot hang the app.
- **Changes from krikzz's code:** plain CPU copies instead of DMA, any
  buffer alignment, limited busy waits, the write CRC computed without a
  512-byte stack buffer, and the ROM wait state set to the GBA's default
  (4/2, what krikzz's sample runs at) during each access and restored
  afterwards.
- **gbamp3 memory:** the driver is in ROM (Thumb); it needs 12 bytes of
  EWRAM. Memory limits and the main-stack gate are unchanged.

## How it was tested (emulator, not hardware)

- `tests/emu/edgba_model.h` (gbamp3: `tests/edgba_model.h`): a register-level
  EverDrive model for the exact-ROM mGBA runners. The SD command and data
  lines are modeled bit by bit; every command CRC and the CRC of every
  written block on each of the four data lines is checked. It can have an
  SDHC, SDSC or v1 card, no card, a write-protected card, and slow or failing
  reads. A configuration write that would unmap the game ROM or turn on ROM
  writes stops the run.
- gbareader: `tests/emu/edgba_test.py` (book import and reading in all those
  variants, no card, write-protected card, L+R).
- gbawriter and gbavocab: `tests/emu/ezfo_test.py` runs one script on the
  SuperCard model, the Omega model and the EverDrive model; every screen and
  every file on the card must be identical.
- gbamp3: the exact-ROM acceptance suite (`tests/run_acceptance.py`, with
  `EDGBA=1 python3 -B tests/build_runner.py`) on the EverDrive model.
- `tools/ezfo_diag` in gbareader: the diagnostics ROM also shows the
  EverDrive's FPGA version, card type and card start error.

## Hardware checklist

Use a backed-up card. Note the EverDrive model (Mini, X5, X3) and the OS
version.

1. **ezfo_diag:** start it, check that it says *EverDrive GBA*, *Card start
   error 0*, *FAT mounted* and *ROM image unchanged*; press A, check *Read
   back same* and *ROM still unchanged*. Send a photo or `/ezfo-diag.txt`.
2. **gbareader, gbawriter, gbavocab, gbamp3:** the same steps as items 2 to
   5 of the Omega checklist in [ezflash-omega.md](ezflash-omega.md).
3. **Every app:** hold L+R while starting: the app must report that there
   is no SD card (gbamp3: *SD not ready*); nothing else may happen.

## Credits

The driver (`edgba_driver.c`) is a port of krikzz's edio-gba SD code
(edio-gba everdrive.c and disk.c; github.com/krikzz/gba-ed-pub, MIT
licence, Copyright (c) 2024 krikzz), changed by Halim Jarrar (MIT). The licence text is at the top
of `edgba_driver.c`.

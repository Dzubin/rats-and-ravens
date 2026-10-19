# Changelog

All notable changes to Rats and Ravens (the PicoCalc platformer).
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `rats_and_ravens_config.h`.

## [Unreleased]

- Updated the vendored `picocalc-text-starter` to Blair Leduc's code of 2026-10-18 (commit
  `85a002e`) and moved to Pico SDK 2.3.1. The search-hint, deleted-entry and busy-card fixes this
  copy carried are in his code now; four small local fixes remain (README, Third-party). The audio
  driver in it now sounds 20 to 20000 Hz, where it used to be silent outside 100 to 2000 Hz.
- Vendored starter: `fat32.c` `fat32_write()` no longer leaks a cluster each time a write starts on a
  cluster boundary. The program itself is unchanged; not rebuilt.

- Vendored starter: `fat32.c` skips deleted directory entries before long-name parts (an
  out-of-bounds stack write) and `sdcard.c` waits for a busy card by the clock and before
  every command (failed reads and writes on a card that was still programming). The
  program itself is unchanged; not rebuilt.

## [V1.01B] - 2026-10-02

### Changed (desktop build)
- Build outputs are named without the `picocalc-` prefix, since the chip or system
  at the end of the name already says what they are for: `rats-and-ravens-RP2040.uf2`, `rats-and-ravens-RP2350.uf2`, `rats-and-ravens-Windows.exe` and `rats-and-ravens-Linux`.
- Vendored `drivers/fat32.c`: `get_next_free_cluster()` now advances its search
  hint past the cluster it hands out (a local fix, not in upstream), so a long
  sequential write to the SD card no longer re-scans every cluster already given
  out and looks hung.
- The build logic repeated in `desktop/CMakeLists.txt` now lives in
  `desktop/shim_desktop.cmake`, and the shim files were refreshed from a shared
  copy (this replaces the project's own `shim_audio.c`, which behaves the same
  for the calls the game makes). The numeric keypad now works in the desktop
  build. No gameplay change.

### Changed (gameplay)
- Rat_raven count now starts at 6 (levels 1-2) and doubles every 2 levels
  (12 on levels 3-4, 24 on 5-6, 40 on 7-8 and every level from there on),
  instead of starting at 8 and doubling every level. The empty level (10)
  is unchanged.

### Changed (controls), V1.01A
- Q, q and ESC now leave the game and return to the PicoCalc UF2 Loader's
  menu, the same on the RP2040 and the RP2350. They work on the splash
  screen, in the game and on the death screen. In the game Q still asks
  "QUIT?" first (Y to confirm); ESC leaves at once. Before, they only went
  back to the splash screen, and did nothing on the splash screen.
  The game asks the loader for its menu through the chip's watchdog scratch
  registers and reboots; if it was flashed without the loader it just
  restarts. The desktop build is unchanged (ESC asks first, then closes the
  program); Q or ESC on its splash screen now closes it too.
- The death screen's message line now says `PRESS Q OR ESC TO QUIT`, and
  ESC quits from it as well as Q.
- Version V1.01A.

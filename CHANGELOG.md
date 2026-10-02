# Changelog

All notable changes to Rats and Ravens (the PicoCalc platformer).
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `platformer_config.h`.

## [Unreleased]

### Changed (desktop build)
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

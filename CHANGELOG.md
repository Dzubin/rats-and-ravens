# Changelog

All notable changes to Rats and Ravens (the PicoCalc platformer).
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `platformer_config.h`.

## [Unreleased]

### Changed (gameplay)
- Rat_raven count now starts at 6 (levels 1-2) and doubles every 2 levels
  (12 on levels 3-4, 24 on 5-6, 40 on 7-8 and every level from there on),
  instead of starting at 8 and doubling every level. The empty level (10)
  is unchanged.

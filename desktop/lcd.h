/*
 * lcd.h - desktop-only shadow of the vendored lcd.h (by Blair Leduc,
 * picocalc-text-starter): pulls in the real declarations via
 * #include_next, then redefines WIDTH/HEIGHT for the desktop build's own,
 * bigger 640x480 game area (2026-09-23, Thomas: "the code should be able
 * to scale the gameplay area so that it runs on a 640x480 screen").
 *
 * The real lcd.h is vendored and read-only, and always says 320x320 --
 * that's correct for PicoCalc hardware, so it is never touched. This
 * file is found FIRST only when compiling the desktop target (this
 * folder is first on that build's include path -- see
 * desktop/CMakeLists.txt); the firmware build never sees it, so it keeps
 * the real, unmodified 320x320. Everything else lcd.h declares (RGB(),
 * GLYPH_HEIGHT, lcd_*() functions, ROWS/MAX_ROW -- which derive from
 * HEIGHT, so they pick up 480 automatically) passes through unchanged.
 * platformer_config.h and platformer.c both use WIDTH/HEIGHT directly
 * (WIDTH*10 style scaling via SCALE_X()/SCALE_Y() in platformer_config.h),
 * so redefining just these two macros here is enough to scale the whole
 * game for the desktop build -- see platformer_config.h's "Scale" section.
 *
 * Author: Thomas Dzubin
 */
#include_next "lcd.h"

#undef WIDTH
#undef HEIGHT
#define WIDTH   640
#define HEIGHT  480

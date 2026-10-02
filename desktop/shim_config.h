/*
 * shim_config.h - tunables for the Windows (SDL2) PicoCalc shim.
 *
 * Author: Thomas Dzubin
 *
 * Every constant the shim uses lives here so the .c files hold only code.
 */
#ifndef SHIM_CONFIG_H
#define SHIM_CONFIG_H

/* Window title; the CMake file overrides this per project. */
#ifndef SHIM_WINDOW_TITLE
#define SHIM_WINDOW_TITLE       "PicoCalc"
#endif

/* The window opens at (LCD size * this); it is resizable, and SDL keeps
 * the picture at the LCD's aspect ratio. 2026-09-24: dropped from 2 to 1
 * for this project now that its desktop LCD size is 640x480 (see
 * platformer_config.h's "Scale" section); at 2x that would be a
 * 1280x960 window, taller than many screens' usable height, and Thomas
 * hit exactly that: "the top (status lines) and the bottom (ground) are
 * not visible" (centred vertically, with the excess height hanging off
 * both the top and bottom of the screen). 640x480 at 1x fits comfortably
 * anywhere and is still resizable if he wants it bigger. */
#define SHIM_WINDOW_SCALE       1

/* Longest gap between screen refreshes while the program is busy. */
#define SHIM_PRESENT_INTERVAL_US 16000

/* Keyboard: size of the key-event FIFO and of the keyboard_get_key() buffer. */
#define SHIM_KEY_QUEUE_SIZE     64

/* Audio: stereo signed 16-bit square waves, like the PicoCalc's PWM output. */
#define SHIM_AUDIO_RATE         44100
#define SHIM_AUDIO_SAMPLES      512
#define SHIM_AUDIO_AMPLITUDE    5000

#endif /* SHIM_CONFIG_H */

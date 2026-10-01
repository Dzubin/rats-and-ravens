/*
 *  PicoCalc Platformer: compile-time configuration
 *  ================================================
 *  All #define constants live here (see platformer.c for the design notes
 *  and the game itself).  Author: Thomas Dzubin.
 */
#pragma once

/* shown on the splash screen */
#define GAME_TITLE "RATS AND RAVENS"
#define VERSION "V1.01A"

/* ===================================================================== */
/*  Screen                                                                */
/* ===================================================================== */

/* Text grid: rows/columns of the built-in 8x10 font. WIDTH/HEIGHT come
 * from lcd.h (320x320 on PicoCalc, 640x480 on desktop, see the "Scale"
 * section below). put_row() asks the driver for the real column count
 * (lcd_get_columns(), which is WIDTH/glyph-width and so is correct on
 * both builds), so there is no separate column-count constant here.     */

/* main loop idle sleep, so we don't peg the CPU polling I2C for nothing */
#define IDLE_SLEEP_MS 4

/* ===================================================================== */
/*  Scale: the PicoCalc hardware stays fixed at 320x320 (WIDTH/HEIGHT,
 *  from lcd.h); the desktop SDL2 build instead renders a bigger 640x480
 *  game area. WIDTH/HEIGHT are already available here since platformer.c
 *  includes lcd.h before this header (see its #include block), and are
 *  still 320x320 for the firmware build; only the desktop build sees
 *  them as 640x480, via desktop/lcd.h, a small shadow header that
 *  #include_nexts the real (vendored, untouched) one and redefines just
 *  those two macros for that build alone. See its own header comment.
 *
 *  Scaling is split in two:
 *
 *  - SCALE_X() scales HORIZONTAL LAYOUT only: camera dead zone,
 *    level-safe margin, gap/platform/tower/foggy-zone widths and
 *    spacing, rat_raven spawn distance, player start X. This lets the
 *    wider 640-pixel viewport show a richer, more spread-out world.
 *  - Every vertical PHYSICS quantity (gravity, all three jump launch
 *    velocities, max fall speed, foggy climb/step speed) and every
 *    obstacle HEIGHT (tower/platform/foggy-zone, and the height above
 *    the ground of the floating-fog and flying-rat_raven bands) stays at
 *    its exact original, hardware-confirmed absolute
 *    value on BOTH builds, unaffected by SCALE_Y. This keeps every
 *    vertical-reach relationship (the double jump clears every tower
 *    without a power-up) byte-for-byte identical to the firmware feel,
 *    with zero risk of a desktop-only softlock.
 *  - GROUND_TOP is the one exception, scaled by SCALE_Y after all:
 *    unscaled, it leaves a ground band that eats too much of the taller
 *    desktop screen. Scaling it only moves WHERE the ground sits on
 *    screen; it does not change any height or velocity above, and
 *    everything else that cares about the ground (PLATFORM_Y_MIN/MAX,
 *    FOGGY_FLOAT_Y_MIN/MAX, FLYING_RAT_RAVEN_Y_MIN/MAX, RAT_RAVEN_Y,
 *    PLAYER_START_Y, open-air pickups) measures itself relative to
 *    GROUND_TOP, so the reach relationships stay intact. GROUND_HEIGHT
 *    (below) is whatever's left of HEIGHT under it, so the ground
 *    band's THICKNESS scales too, not just its top edge.
 *  - MOVE_SPEED/RAT_RAVEN_SPEED/FOGGY_MOVE_SPEED (horizontal RATES, not
 *    layout) are also unscaled, for the same reason: speed is tied to
 *    the character, which did not get bigger, not to the wider world.
 *
 *  Deliberately NOT scaled: PLAYER_W/H, RAT_RAVEN_SIZE, PICKUP_SIZE. The
 *  player and the rat_ravens may grow in a later pass, but not yet.      */
/* ===================================================================== */
#define SCALE_X(v)  (((v) * WIDTH)  / 320)
#define SCALE_Y(v)  (((v) * HEIGHT) / 320)   /* only GROUND_TOP uses this; see below */

/* Fixed reference "screen" width for level-content DENSITY formulas
 * (TOWERS_PER_SCREEN_X10 and friends, in generate_level()'s gen_*()
 * functions). Always 320, regardless of the real WIDTH, so a wider
 * desktop viewport shows proportionally MORE content at once instead of
 * the same content spread thinner across a wider world. world_width
 * itself is unaffected (still LEVEL_SCREENS * WIDTH; a level is always
 * LEVEL_SCREENS VIEWPORTS wide on either build, see below). A
 * "screen" just means more actual world pixels on desktop, so the same
 * per-screen density produces proportionally more objects.              */
#define REF_SCREEN_W  320

/* ===================================================================== */
/*  Camera / world: every level is randomly generated (see
 *  generate_level() in platformer.c). world_width is a runtime variable,
 *  always LEVEL_SCREENS * WIDTH, so the same code serves both builds'
 *  screen widths. WIDTH (from
 *  lcd.h; 320 on PicoCalc, 640 on desktop, see the "Scale" section
 *  above) is what the camera looks through, and a level is still
 *  LEVEL_SCREENS of that many pixels wide either way. See
 *  update_camera() and render_frame().                                  */
/* ===================================================================== */

#define LEVEL_SCREENS      10   /* every level is exactly this many screens wide */

/* How close (in screen pixels) the player can get to either edge of the
 * screen before the camera starts scrolling to keep up. The band
 * between these two is the "dead zone" the player can move around in
 * freely without the camera moving at all. An instant snap, not a
 * smoothed or lerped follow, matching this project's simplest-feel-
 * first approach used elsewhere (see MOVE_SPEED below).                 */
#define CAMERA_DEADZONE_LEFT   SCALE_X(120)
#define CAMERA_DEADZONE_RIGHT  SCALE_X(200)

/* Falling into a gap has no ground underneath. Once the character has
 * fallen this far below the visible screen, it is treated the same as
 * any other death (see TIMER_DURATION_MS below): the death screen
 * shows, then the current level restarts (not a new one; only winning
 * generates a fresh level, see win_level() in platformer.c). HEIGHT is
 * used directly (already the real screen height on either build); the
 * 60px margin below it is unscaled, per the "Scale" section. It just
 * needs to be definitely below the visible screen, which 60px
 * comfortably is on either build.                                      */
#define FALL_RESPAWN_Y  (HEIGHT + 60)

/* Every level has a 2-minute time limit: a tick-based countdown
 * (decremented once per physics tick, so it pauses along with
 * everything else, the same as the jewel's timer),
 * shown on screen as MM:SS. Reaching 0 is a death, same as falling into
 * a gap: the death screen shows, then the SAME level restarts. See
 * timer_remaining_ms and player_died in platformer.c.                   */
#define TIMER_DURATION_MS  (2 * 60 * 1000)

/* ===================================================================== */
/*  Level geometry: every level is randomly generated (see
 *  generate_level() in platformer.c) from the ranges/densities below,
 *  rather than one hand-placed layout. Every obstacle (including the
 *  ground) is a fully solid rectangle: the player cannot pass through
 *  any side of it, not just land on top. See the SOLIDS[] array near
 *  the top of platformer.c.                                             */
/* ===================================================================== */

/* GROUND_TOP is scaled by SCALE_Y so the ground band keeps the same
 * proportion of the screen on both builds: left unscaled at a flat
 * 280, GROUND_HEIGHT (below) would grow to fill the desktop's extra
 * height, eating far more of the taller screen than the PicoCalc
 * shows. Scaling GROUND_TOP only moves WHERE the ground surface sits
 * on screen; it does NOT touch TOWER_H_MIN/MAX, PLATFORM_Y_MIN/MAX,
 * gravity, or any jump velocity, which all stay at their exact
 * unscaled hardware values. Everything that cares about the ground
 * (PLATFORM_Y_MIN/MAX below, RAT_RAVEN_Y, PLAYER_START_Y) already
 * measures itself relative to GROUND_TOP, so the double-jump-clears-
 * every-tower relationship stays exactly as hardware-confirmed
 * regardless of where the ground line falls; only the amount of empty
 * sky above it changes.                                                 */
/* The ground band is SCALE_Y(300): 20px thick on the PicoCalc. Every
 * height band in the level (platforms, the floating foggy band, the
 * flying rat_raven band, open-air pickups) is defined as a distance above
 * GROUND_TOP, never as an absolute screen Y, so it sits at the same
 * height above the ground on both builds even though GROUND_TOP itself
 * is 300 on the PicoCalc and 450 on the desktop.                         */
#define GROUND_TOP    SCALE_Y(300)  /* top edge of the ground, in pixels  */
#define GROUND_HEIGHT (HEIGHT - GROUND_TOP)

/* Fixed-capacity arrays; see platformer.c's memory comment on
 * generate_level(). Generous headroom over what the density formulas
 * below actually produce at LEVEL_SCREENS, at ~10-12 bytes per
 * entry: trivial next to RP2040's 264KB RAM, no reason to trim further. */
#define MAX_SOLIDS         64   /* ground segments + towers + platforms  */
#define MAX_FOGGY_ZONES   24
#define MAX_PICKUPS        72
#define MAX_GAPS            8   /* generation-time scratch space only    */
#define MAX_RAT_RAVENS        96

/* No gaps or obstacle placement within this many pixels of either end
 * of the level. Guarantees solid ground under the player's start
 * (screen 1) and under the win-trigger at the far right edge (see
 * player_won in platformer.c).                                         */
#define LEVEL_SAFE_MARGIN  SCALE_X(200)

/* Gaps: no jump/double-jump/power-up gating, just a plain running jump.
 * MOVE_SPEED and the ground jump's airtime clear GAP_W_MAX with room to
 * spare (~144px max reach vs. 110px worst case here). Miss one and fall
 * clear through to FALL_RESPAWN_Y.                                      */
#define GAP_W_MIN        SCALE_X(70)
#define GAP_W_MAX        SCALE_X(110)
#define GAP_SPACING_MIN  SCALE_X(80)    /* minimum clear ground between two gaps */
#define GAPS_PER_SCREEN_X10  3     /* i.e. 0.3 gaps/screen                */
#define MIN_GAPS             1

/* Floating platforms, their tops 90-130px above the ground. The plain
 * ground jump (~99px peak) only reaches the lowest of them; the rest
 * need the double jump (~141px combo reach, see AIR_JUMP_VELOCITY), so
 * every platform is reachable without a power-up, with an 11px margin at
 * the top of the band. Every platform also keeps at least PLAYER_H of
 * clear space under it (see gen_platforms() in platformer.c), so it can
 * never sit so low over a tower that the tower's top becomes a
 * crawlspace the player cannot fit into.
 * Width and height are multiples of 8 so they can be tiled with an 8x8
 * texture on hardware that supports it; position (PLATFORM_Y_MIN/MAX)
 * is not grid-aligned.                                                  */
#define PLATFORM_W_MIN   SCALE_X(72)
#define PLATFORM_W_MAX   SCALE_X(112)
#define PLATFORM_H       8
#define PLATFORM_Y_MIN   (GROUND_TOP - 130)
#define PLATFORM_Y_MAX   (GROUND_TOP - 90)
#define PLATFORMS_PER_SCREEN_X10  5
#define MIN_PLATFORMS             1

/* Towers, sitting flush on the ground: a genuine wall, not just an
 * optional climb. A tower is SOLID across its full width, so the only
 * way past one is over the top; there is no walking around it. That
 * makes "power-ups are optional" a per-tower requirement here, not
 * just a whole-level one: EVERY tower must be clearable by a
 * well-timed double jump alone, or a randomly generated one could
 * softlock a run that has not found a power-up yet. TOWER_H_MAX (120)
 * keeps a safety margin under the double jump's reach; do not raise it
 * without re-checking that reach. The power-up still makes every tower
 * easier (a single powered-up jump clears any of them solo, no combo
 * needed), it is just never the only way up, for any individual tower.
 * Width and height are multiples of 8 so they can be tiled with an 8x8
 * texture; position is not grid-aligned.                                */
#define TOWER_W_MIN      SCALE_X(32)
#define TOWER_W_MAX      SCALE_X(48)
/* Unscaled; see the "Scale" section above. Heights stay exactly what
 * the double jump's reach is tuned against; scaling them up on desktop
 * without also scaling the jump would make towers uncrossable without
 * a power-up.                                                           */
#define TOWER_H_MIN      72
#define TOWER_H_MAX      120
/* More towers per level than the other obstacle types, to make speed
 * running through a level more difficult.                               */
#define TOWERS_PER_SCREEN_X10  6
#define MIN_TOWERS             2

/* Foggy zones: half ground-flush, half floating (picked randomly per
 * zone at generation time). See FOGGY_ZONES[] and player_in_foggy()
 * in platformer.c for what they do. A zone's X/Y are free to overlap a
 * solid's on purpose (see the design note in platformer.c's comment on
 * generate_level()); overlap is only avoided against OTHER foggy
 * zones, purely for visual clarity (two overlapping zone rectangles
 * would be indistinguishable), not because overlap would be broken.
 * Width and height are multiples of 8 so they can be tiled with an 8x8
 * texture; position (FOGGY_FLOAT_Y_MIN/MAX) is not grid-aligned.       */
#define FOGGY_W_MIN         SCALE_X(40)
#define FOGGY_W_MAX         SCALE_X(72)
/* Heights and Y-bands unscaled; see the "Scale" section above.          */
#define FOGGY_GROUND_H_MIN  48   /* ground-flush: height above the ground */
#define FOGGY_GROUND_H_MAX  88
/* Early-level ramp: random ground zones are FOGGY_GROUND_H_START tall on
 * level 1 and reach the full H_MIN..H_MAX range on level
 * FOGGY_GROUND_H_FULL_LEVEL (must be at least 2). The level-wide fog layer
 * is not ramped.                                                          */
#define FOGGY_GROUND_H_START       16
#define FOGGY_GROUND_H_FULL_LEVEL  8
#define FOGGY_FLOAT_H_MIN   40   /* floating: its own height              */
#define FOGGY_FLOAT_H_MAX   72
#define FOGGY_FLOAT_Y_MIN   (GROUND_TOP - 180)  /* floating band: top edge, measured from the ground */
#define FOGGY_FLOAT_Y_MAX   (GROUND_TOP - 60)
#define FOGGY_PER_SCREEN_X10  5
#define MIN_FOGGY_ZONES        1

/* Fog levels: every FOG_LEVEL_INTERVAL-th level gets one extra ground
 * foggy zone spanning the whole level width, as tall as a ground foggy
 * zone can ever be (FOGGY_GROUND_H_MAX), built by the same helper as the
 * regular ground zones.                                                   */
#define FOG_LEVEL_INTERVAL     4

/* Foggy zones stay put on levels below this one; from this level up they
 * drift sideways (see FOGGY_SPEED_DIV).                                   */
#define FOG_DRIFT_START_LEVEL  10

/* Slow sinking: standing still (no left/right/up/down/jump input) in a
 * foggy zone, or on top of one, for FOGGY_SINK_DELAY_TICKS (~3 seconds)
 * starts a sink of one pixel every FOGGY_SINK_INTERVAL_TICKS ticks. Any
 * input resets the wait and stops the sink, so pressing UP counters it.
 * Not applied while the jewel is active.                         */
#define FOGGY_SINK_DELAY_TICKS     (3000 / MS_PER_TICK)
#define FOGGY_SINK_INTERVAL_TICKS  4

#define FOGGY_FLOAT_MAX_GROUND_REACH    120   /* vertical jump reach, unscaled */
#define FOGGY_FLOAT_PLATFORM_MAX_DX     SCALE_X(140)   /* horizontal; scales like GAP_W_MAX */

/* Pickups: every pickup, the jewel and gold coin alike, shares one
 * size and one generic mechanism (PICKUPS[] in platformer.c: {x, y,
 * type, taken}, drawn/collected/erased the same way regardless of
 * type). Any pickup, of any type, is free to sit in open air, on a
 * platform or tower, or inside a foggy zone; collection is a plain
 * position overlap test, unrelated to where it is placed. Pickups
 * avoid overlapping each other, again just for visual clarity.          */
#define PICKUP_SIZE          8
#define PICKUP_SPACING_MIN   SCALE_X(20)
/* Open-air pickups: the pickup's bottom edge sits this many pixels above
 * the ground, within reach of a plain ground jump (the player's head
 * reaches ~123px above the ground at the peak). Unscaled, like every
 * other height.                                                         */
#define PICKUP_AIR_H_MIN     60
#define PICKUP_AIR_H_MAX     110

/* Gold coin: a FIXED count per level, not density-scaled like everything
 * else here. Score comes from gold coin pickups, stomping a rat_raven, and the
 * win screen's time bonus; see `score` in platformer.c.                 */
#define COIN_VALUE          20
#define STOMP_VALUE          25   /* points for stomping a rat_raven            */
#define TIME_BONUS_PER_SECOND 3   /* win screen: points per second left       */
/* Win screen: points taken off per gold coin pickup left uncollected, after
 * the time bonus; the score stops at 0. See show_win_screen().           */
#define MISSED_COIN_PENALTY  5
#define COL_COIN            RGB(255, 200,  50)   /* gold/orange */
#define COIN_PER_LEVEL      30

/* Randomized, but at least two jewels per level. Taking one
 * never adds score; only gold coin does. See step_physics().                */
#define JEWELS_PER_SCREEN_X10  3
#define MIN_JEWELS             2

/* Rat_ravens: a patrol hazard. Two kinds, both in the same RAT_RAVENS[]
 * array (rat_raven_t's `flying` flag). GROUND ones are placed on solid
 * ground the same way towers are (never in a gap, never inside a tower),
 * patrol left and right at RAT_RAVEN_SPEED, and reverse on hitting a
 * tower, either end of the level, or the edge of a gap, so they never
 * cross one (see rat_raven_stay_on_ground() in platformer.c). FLYING
 * ones patrol anywhere across the level width
 * at a random height instead, ignoring the ground and gaps entirely,
 * at FLYING_RAT_RAVEN_SPEED, and reverse off a PLATFORM too since they
 * fly at head height. Neither kind spawns within RAT_RAVEN_MIN_SPAWN_DIST
 * of the player's own start position, so one can never hit before the
 * player has a chance to react. Either kind's speed is halved while it
 * is inside a foggy zone; see rat_raven_in_foggy() in platformer.c.
 * Drawn as a rat or a raven sprite (RAT_RAVEN_USE_SPRITES below), or,
 * with sprites off, as a black square outlined in white, since a solid
 * black fill would be invisible against the black background.
 * RAT_RAVEN_SIZE is aliased to PLAYER_W rather than a separate literal
 * so the two can never drift apart. Rat_raven COUNT is level-number-
 * based, not density-scaled by level width: RAT_RAVENS_START (6) on
 * levels 1-2, doubling every RAT_RAVENS_DOUBLE_EVERY (2) levels after
 * that (12 on levels 3-4, 24 on 5-6, 40 on 7-8 and every level from
 * there on, clamped at RAT_RAVENS_LEVEL_CAP; see gen_rat_ravens()). Each
 * one has a FLYING_RAT_RAVEN_CHANCE_PCT chance of being flying, but at
 * least one flying rat_raven is guaranteed per level regardless of how
 * that percentage happens to roll.                                      */
#define RAT_RAVEN_SIZE       PLAYER_W
/* 1 = draw the rat_ravens as a rat (ground) and a raven (flying), from
 * rat_raven_sprites.h, with the same 16x16 collision box; 0 = the original
 * white-outlined black squares (that code is still in draw_rat_raven_shape()). */
#define RAT_RAVEN_USE_SPRITES  1
/* 1 = draw the player from player_sprite.h (16x24, with a face); 0 = the
 * original plain coloured rectangle (still in draw_player_shape()).       */
#define PLAYER_USE_SPRITE    1
/* 1 = draw the pickups from pickup_sprites.h (a coin for gold coin, a jewel for
 * the jewel, 8x8); 0 = plain gold / cyan squares.                */
#define PICKUP_USE_SPRITES   1
#define RAT_RAVEN_Y          (GROUND_TOP - RAT_RAVEN_SIZE)   /* sits flush on the ground */
/* Unscaled; see the "Scale" section above.                              */
#define RAT_RAVEN_SPEED      SUBPIXELS(1)
#define RAT_RAVENS_START          6    /* base count on levels 1-2 */
#define RAT_RAVENS_DOUBLE_EVERY   2    /* base count doubles every this many levels */
#define RAT_RAVENS_LEVEL_CAP      40
/* No rat_raven may spawn within this many pixels of PLAYER_START_X,
 * either side. See gen_rat_ravens().                                     */
#define RAT_RAVEN_MIN_SPAWN_DIST   SCALE_X(180)
#define FLYING_RAT_RAVEN_SPEED      (RAT_RAVEN_SPEED * 2)
/* Flying band: a flying rat_raven's top edge, measured from the ground
 * (110-220px above it on both builds; see GROUND_TOP's comment).        */
#define FLYING_RAT_RAVEN_Y_MIN      (GROUND_TOP - 220)
#define FLYING_RAT_RAVEN_Y_MAX      (GROUND_TOP - 110)
/* Flying rat_ravens bob: every RAT_RAVEN_ALT_INTERVAL_TICKS (half a second)
 * each one moves RAT_RAVEN_ALT_STEP (half its height) up or down, picked at
 * random each time; a step that would leave FLYING_RAT_RAVEN_Y_MIN/MAX goes
 * the other way instead.                                                   */
#define RAT_RAVEN_ALT_STEP            (RAT_RAVEN_SIZE / 2)
#define RAT_RAVEN_ALT_INTERVAL_TICKS  (500 / MS_PER_TICK)
#define FLYING_RAT_RAVEN_CHANCE_PCT 25   /* out of 100; see gen_rat_ravens() */

/* Colliding with a rat_raven while normal is getting "sick" (see `sick`
 * in platformer.c): the character turns black, with the same outline-
 * in-white treatment as rat_ravens, since it is the identical
 * visibility problem. It is cured either instantly, by a jewel
 * pickup (gold coin does NOT cure sickness), or by SICK_CURE_TICKS of
 * survival (30 seconds, tick-based so it pauses with everything else,
 * like the level timer), back to "well" (plain green again, needing
 * two fresh rat_raven hits before the next death). Colliding with a rat_raven again while already sick is a death. If the jewel is
 * active at the moment of a hit, it acts as a one-time shield instead:
 * the character survives without becoming sick, but the power-up
 * itself is used up immediately (ends early, reverts to a plain green
 * block). Either way, the rat_raven that hit the player is used up too
 * (removed from play), same as a stomp-kill. RAT_RAVEN_HIT_INVULN_TICKS
 * is a brief grace period started by any rat_raven touch of any kind:
 * without it, standing in the same overlap for a couple of extra
 * ticks right after a hit looked like a second, separate touch, and
 * could kill or shield again off the very same contact.                 */
#define RAT_RAVEN_HIT_INVULN_TICKS   40
#define SICK_CURE_TICKS   (30000 / MS_PER_TICK)   /* 30 seconds */

/* 16x24, a multiple of 8 so it can be tiled with an 8x8 texture.
 * PLAYER_START_Y stays derived from GROUND_TOP, which is already a
 * multiple of 8, so it lands on the 8px grid automatically.             */
#define PLAYER_W      16
#define PLAYER_H      24
#define PLAYER_START_X SCALE_X(40)
#define PLAYER_START_Y (GROUND_TOP - PLAYER_H)

/* ===================================================================== */
/*  Physics: fixed-point (no float), fixed-timestep.
 *
 *  Position and velocity are stored in subpixels: SUBPIXEL_SHIFT bits of
 *  fraction below the integer pixel, e.g. a shift of 4 means 16 subpixels
 *  per pixel. This keeps the feel identical on RP2040 (no hardware FPU)
 *  and RP2350, and identical on the desktop build too, since it is all
 *  plain integer arithmetic. See step_physics() in platformer.c.        */
/* ===================================================================== */

#define SUBPIXEL_SHIFT   4
#define SUBPIXELS(px)    ((px) << SUBPIXEL_SHIFT)

/* One physics step covers this many milliseconds of simulated time; the
 * main loop measures real elapsed time and runs however many whole
 * steps that covers (capped by MAX_TICKS_PER_FRAME so a stall cannot
 * make the game leap forward when it resumes).                         */
#define MS_PER_TICK          16
#define MAX_TICKS_PER_FRAME  5

#define GRAVITY_ACCEL     24

/* Downward speed is clamped here so falling never outruns collision
 * checks, and so it does not feel like an elevator on a long drop.      */
#define MAX_FALL_SPEED    SUBPIXELS(18)

/* Three fixed launch velocities (negative = up), each an instant impulse
 * applied at liftoff. There is no way to hold a key to go higher; the
 * *only* thing that changes a jump's height is which of these three
 * fires:
 *
 *   JUMP_VELOCITY (ground jump): peaks at ~99px, clearing a platform
 *     (PLATFORM_Y_MIN..MAX needs ~90-130px of rise) with a little room.
 *     The shortest towers (TOWER_H_MIN, 72px) can be cleared by this jump
 *     alone; the taller ones need the double jump, see below.
 *
 *   AIR_JUMP_VELOCITY (the double jump, MAX_AIR_JUMPS below): weaker
 *     than the ground jump, but strong enough that a double jump
 *     chained right at the peak of the ground jump (the tallest a
 *     combo can realistically get) reaches ~141px, enough to clear
 *     EVERY generated tower (TOWER_H_MIN..TOWER_H_MAX, 72..120px) on
 *     skill alone, with no power-up needed. That matters more here
 *     than a normal "power-ups are optional" rule: a tower is a
 *     full-width solid wall, not an optional climb, so if even one
 *     could exceed the double jump's reach, a run without a power-up
 *     yet could hit a genuine softlock. A plain ground jump by itself
 *     still cannot clear any of them.
 *
 *   POWERUP_JUMP_VELOCITY: the jewel (POWERUP_DURATION_MS
 *     below). This is a genuinely stronger launch, not a gravity
 *     change; while the power-up is active it replaces JUMP_VELOCITY
 *     and AIR_JUMP_VELOCITY both, so every jump, ground or air, goes
 *     out faster. On its own (no double jump needed) it peaks at
 *     ~150px, comfortably clearing even the tallest generated tower
 *     (TOWER_H_MAX, 120px) with room to spare: the *easy* way, not the
 *     *only* way, since every tower is also clearable by double-jump
 *     skill alone (see AIR_JUMP_VELOCITY above).                        */
#define JUMP_VELOCITY         (-SUBPIXELS(18))
#define AIR_JUMP_VELOCITY     (-SUBPIXELS(12))
#define POWERUP_JUMP_VELOCITY (-SUBPIXELS(22))

/* Inside a foggy zone, gravity is switched off entirely: vertical
 * movement only ever comes from a deliberate UP or DOWN press, and each
 * press is a bounded, self-contained step, not an infinite climb or
 * sink once started. Pressing UP (or SPACE) queues FOGGY_STEP_PX more
 * pixels of upward progress; DOWN queues the same downward; the queued
 * distance is worked off gradually at FOGGY_CLIMB_SPEED subpixels per
 * tick, so each press reads as a small, slow, deliberate nudge rather
 * than a jump or a teleport. A zone's height (FOGGY_GROUND_H_* or
 * FOGGY_FLOAT_H_* above) plus a full player-height above its top is
 * the climb distance to fully clear one; at 18px per press that is a
 * handful of separate UP presses for a typical zone, not one. There is
 * no decay while the player is active: with nothing queued, vertical
 * speed is zero, until the idle sink (FOGGY_SINK_* below) kicks in.
 * Horizontal movement is also slowed (FOGGY_MOVE_SPEED, replacing
 * MOVE_SPEED). The power-up always overrides all of this: a jump still
 * launches at full power-up strength, with normal gravity, even inside
 * one, pushing straight through the resistance.                        */
/* Unscaled; see the "Scale" section above.                              */
#define FOGGY_STEP_PX        18   /* pixels queued per UP/DOWN press */
#define FOGGY_CLIMB_SPEED    SUBPIXELS(1)  /* how fast queued distance is worked off, subpixels/tick */
#define FOGGY_MOVE_SPEED     SUBPIXELS(2)

/* A DOWN press is buffered for this many ticks (~96ms) rather than
 * consumed or dropped on the exact tick it arrives. Landing on a
 * foggy zone's ledge (on_foggy_ledge) and the key press are two
 * separate, independently-timed events, and a press arriving a tick
 * or two before landing completes would otherwise be silently
 * dropped. See down_buffer_ticks in platformer.c.                       */
#define DOWN_BUFFER_TICKS  6

/* Double jump: how many extra jumps are allowed while airborne, on top
 * of the one from the ground, before landing refills it. 1 means a
 * double jump, using AIR_JUMP_VELOCITY above.                           */
#define MAX_AIR_JUMPS  1

/* Jewel: while active (POWERUP_DURATION_MS after pickup), every
 * jump, ground or air, launches at POWERUP_JUMP_VELOCITY instead of its
 * usual velocity. A real energy boost to the character, not a gravity
 * change, so it stacks with nothing else and just makes every jump
 * taken during the window stronger. It wears off on its own; there is
 * no way to cancel it early (other than spending it as a shield). The
 * countdown is in physics ticks, like the level timer, so pausing the
 * game pauses it too.                                                   */
#define POWERUP_DURATION_MS     7500
#define POWERUP_DURATION_TICKS  (POWERUP_DURATION_MS / MS_PER_TICK)

/* Horizontal speed while LEFT/RIGHT is held; no acceleration or
 * friction yet, deliberately the simplest possible feel. Unscaled on
 * desktop too (see the "Scale" section above), since it is tied to the
 * character, which did not get bigger, not to the wider world.          */
#define MOVE_SPEED        SUBPIXELS(6)

/* ===================================================================== */
/*  Colours (RGB565 via the driver's RGB() macro)                        */
/* ===================================================================== */

#define COL_BG       RGB(  0,   0,   0)
#define COL_WHITE    RGB(235, 235, 235)
/* Secondary/label text: fully saturated, not dim grey, which is hard
 * to read on the real LCD.                                              */
#define COL_LABEL    RGB(255, 160,  40)
#define COL_CYAN     RGB(120, 225, 255)
#define COL_YELLOW   RGB(255, 225,  70)

#define COL_PLAYER   RGB( 90, 235, 130)
/* The character's colour while a double jump is still in effect this
 * trip, until landing: a small, deliberately minor visible cue. See
 * current_player_colour() in platformer.c.                              */
#define COL_PLAYER_AIRJUMP RGB(120, 225, 255)   /* == COL_CYAN */
#define PLAYER_POWERUP_BORDER_PX  3
#define COL_GROUND   RGB(150, 100,  60)
/* Surface detail on the solids (see draw_solid_detail() in platformer.c):
 * grass and darker soil on the ground, stone with mortar lines and lit or
 * dark windows on the towers, wood with plank gaps on the platforms, and a
 * light / dark bevel edge on all of them. 1 = draw it, 0 = plain
 * rectangles. Everything is worked out from the solid's world position,
 * so it stays put as the screen scrolls.                                  */
#define SOLID_TEXTURES      1
#define COL_GROUND_DARK     RGB(105,  70,  40)
#define COL_GRASS           RGB( 70, 170,  60)
#define COL_GRASS_LIGHT     RGB(120, 215,  90)
#define COL_TOWER           RGB(120, 130, 165)   /* stone */
#define COL_TOWER_LIGHT     RGB(170, 180, 210)
#define COL_TOWER_DARK      RGB( 70,  75, 100)
#define COL_MORTAR          RGB( 85,  90, 115)
#define COL_WINDOW_DARK     RGB( 30,  35,  55)
#define COL_WINDOW_LIT      RGB(255, 245, 190)
#define COL_PLATFORM_LIGHT  RGB(235, 190, 125)
#define COL_PLATFORM_DARK   RGB(130,  90,  50)
#define COL_PLANK_GAP       RGB(105,  70,  40)
#define GRASS_H             4     /* height of the grass band on the ground */

/* Foggy zones drift sideways: each one moves at 1/FOGGY_SPEED_DIV of the
 * ground rat_raven's speed (so half by default, and it follows the rat_raven
 * speed ramp), turning around at the edge of the level or when it would
 * bump into another foggy zone. A zone as wide as the whole level (the
 * fog layer) does not move.                                              */
#define FOGGY_SPEED_DIV     2

/* Fog texture: soft lighter and darker horizontal streaks scattered over
 * each foggy zone, one candidate per FOG_CELL_W x FOG_CELL_H cell, laid out
 * from the zone's own top-left corner so the pattern drifts along with it.
 * 1 = draw it, 0 = flat grey.                                             */
#define FOG_TEXTURE         1
#define FOG_CELL_W          32
#define FOG_CELL_H          10
#define COL_FOG_LIGHT       RGB(195, 195, 200)
#define COL_FOG_DARK        RGB(130, 130, 136)
/* Platforms are wood, towers (COL_TOWER above) are stone; both are solid
 * to the player, but the two colour families make them easy to tell apart. */
#define COL_PLATFORM RGB(200, 150,  90)   /* wood */
/* Jewel pickup icon colour only (see the comment on COL_PLAYER
 * above): blue/cyan, reusing COL_CYAN's value so it reads as the same
 * "jump" colour family as COL_PLAYER_AIRJUMP.                           */
#define COL_JEWEL RGB(120, 225, 255)   /* == COL_CYAN */
#define COL_FOGGY   RGB(160, 160, 160)   /* grey like fog */

/* ===================================================================== */
/*  Sound effects                                                        */
/* ===================================================================== */

/* A two-tone "happy" jingle plays on a gold coin pickup, a quick rising
 * three-tone one on a jewel pickup, and a single lower "sad" tone plays for becoming sick or
 * dying. Played through the non-blocking audio_play_sound() (see
 * audio.h) plus a small tick-driven state machine
 * (sfx_play_happy()/sfx_play_sad()/sfx_update() in platformer.c) rather
 * than audio_play_sound_blocking(), which sleeps for its whole
 * duration and would freeze physics for that long, unacceptable for a
 * sound that can fire on every pickup. Durations are in physics ticks
 * (MS_PER_TICK each), not milliseconds, so they pause along with
 * everything else, same as every other tick-based timer in this
 * project. SFX_HAPPY_FREQ1/2 are an ascending major third (C5 to E5, a
 * classic "coin" interval); SFX_SAD_FREQ is deliberately well below
 * both of those and held longer, to read as sad rather than as a
 * second, shorter "happy" beep.                                         */
#define SFX_HAPPY_FREQ1   PITCH_C5
#define SFX_HAPPY_TICKS1  5    /* ~80ms  */
#define SFX_HAPPY_FREQ2   PITCH_E5
#define SFX_HAPPY_TICKS2  8    /* ~128ms */
#define SFX_JEWEL_FREQ1   PITCH_G5   /* quick rising three tones for a jewel pickup */
#define SFX_JEWEL_TICKS1  4    /* ~64ms  */
#define SFX_JEWEL_FREQ2   PITCH_B5
#define SFX_JEWEL_TICKS2  4    /* ~64ms  */
#define SFX_JEWEL_FREQ3   PITCH_D6
#define SFX_JEWEL_TICKS3  10   /* ~160ms */
#define SFX_SAD_FREQ      PITCH_A3
#define SFX_SAD_TICKS     20   /* ~320ms */

/* A sharp high-to-low "pow" for a rat_raven collision or a stomp-kill.
 * Only used where no other sound is already playing at that exact
 * moment, the shield outcome and every stomp-kill; sickness and death
 * keep the existing sad tone as the dominant cue instead of layering
 * this on top of it. See sfx_play_impact() in platformer.c.             */
#define SFX_IMPACT_FREQ1   PITCH_C6
#define SFX_IMPACT_TICKS1  3    /* ~48ms, sharp high crack */
#define SFX_IMPACT_FREQ2   PITCH_C3
#define SFX_IMPACT_TICKS2  10   /* ~160ms, low thud tail */

/* Win screen's score and seconds countdown; see show_win_screen() in
 * platformer.c. This one is not tick/sfx_update()-driven like the
 * others above: the countdown loop calls audio_play_sound_blocking()
 * directly for the beep, since it is already sleeping every other
 * iteration anyway via SCORE_COUNTDOWN_TICK_MS, so there is no physics
 * to avoid freezing here the way sfx_play_*() avoids it elsewhere.
 * SCORE_COUNTDOWN_TICK_MS is real milliseconds, not ticks, since this
 * runs after the level is already over, with no physics loop driving
 * it.                                                                    */
#define SCORE_COUNTDOWN_TICK_MS   40
#define SCORE_COUNTDOWN_BEEP_EVERY 5
#define SFX_COUNTDOWN_FREQ        PITCH_A5
#define SFX_COUNTDOWN_MS          100   /* a quick tenth-of-a-second beep */

/* Short happy song played on the win screen once the final score is
 * showing (after the time bonus). Note list only; the array itself is
 * built in platformer.c (WIN_SONG). Each entry is a pitch and a length in
 * milliseconds; SILENCE is a rest. About 4.6 seconds in total including
 * the driver's 20ms gap after each sounded note, so it stays under the
 * 5 second limit.                                                        */
#define WIN_NOTE(p, ms)  { (p), (p), (ms) }
#define WIN_SONG_NOTES     WIN_NOTE(PITCH_C5, 150), WIN_NOTE(PITCH_E5, 150), WIN_NOTE(PITCH_G5, 150),     WIN_NOTE(PITCH_C6, 300), WIN_NOTE(PITCH_G5, 150), WIN_NOTE(PITCH_C6, 450),     WIN_NOTE(SILENCE, 100),     WIN_NOTE(PITCH_D5, 150), WIN_NOTE(PITCH_F5, 150), WIN_NOTE(PITCH_A5, 150),     WIN_NOTE(PITCH_D6, 300), WIN_NOTE(PITCH_A5, 150), WIN_NOTE(PITCH_D6, 450),     WIN_NOTE(SILENCE, 100),     WIN_NOTE(PITCH_E5, 150), WIN_NOTE(PITCH_G5, 150), WIN_NOTE(PITCH_C6, 150),     WIN_NOTE(PITCH_E6, 300), WIN_NOTE(PITCH_C6, 600)

/* element count of a fixed-size array */
#define NELEMS(a) ((int)(sizeof (a) / sizeof (a)[0]))

/* Shows a one-line position/velocity/grounded readout while tuning the
 * jump feel on real hardware. Off by default; left as a flag, not
 * deleted, in case tuning needs it again later.                         */
#define ENABLE_DEBUG_READOUT 0

/* Status line layout (see draw_status_lines() in platformer.c): three
 * lines. LEVEL:/TIME: on the first and SCORE: on the second, both
 * drawn bigger than the driver's normal 8x10 text via a hand-rolled
 * scaled-glyph renderer (draw_big_text()), scale 2 (16x20px per
 * glyph); LEVEL HIGH SCORE: on the third, at normal size (scale 1),
 * tracking the best score this specific level layout has ever been
 * won with (see level_high_score in platformer.c). Positioned just
 * below the debug readout's row 0. The 10 below is GLYPH_HEIGHT (the
 * driver's normal font row height, from font.h), spelled out as a
 * literal here since lcd.h and font.h are not included by the time
 * this constant is worked out (unlike GROUND_HEIGHT above, which uses
 * HEIGHT directly since lcd.h is already included by then; see the
 * "Scale" section's comment).                                          */
#define STATUS_SCALE          2
#define STATUS_LEVEL_TIME_Y   (10 + 2)
#define STATUS_SCORE_Y        (STATUS_LEVEL_TIME_Y + 10 * STATUS_SCALE + 2)
#define STATUS_HIGHSCORE_Y    (STATUS_SCORE_Y + 10 * STATUS_SCALE + 2)

/* A fourth line, same normal size as the high score line right above it,
 * showing a short per-level hint (see level_message() in platformer.c). */
#define STATUS_MESSAGE_Y      (STATUS_HIGHSCORE_Y + 10 + 2)
/* Nothing in the level (solids, fog, pickups, rat_ravens) is ever above this
 * line, only the status lines are, so a scroll repaint can leave the band
 * above it alone instead of blanking and redrawing the status text every
 * time the camera moves (which made it flash). The one exception is the
 * player at the top of a very high jewel jump (PicoCalc only); any erase
 * that reaches up here marks the status lines it touched for a redraw.
 * See render_frame() and redraw_region().                                */
#define PLAYFIELD_TOP_Y      (STATUS_MESSAGE_Y + 10 + 2)
#define MSG_LEVEL_1           "ARROWS TO MOVE.  SPACE TO JUMP"
#define MSG_LEVEL_2           "WATCH OUT FOR THE RATS AND RAVENS"
#define MSG_FOG_LEVEL         "THE FOG IS THICK"
#define MSG_LOTS_COIN        "LOTS TO COLLECT"
#define MSG_MANY_RAT_RAVENS          "MANY RATS AND RAVENS"
#define MSG_LOTS_JEWEL        "LOTS OF JEWELS"
#define MSG_NOTHING_BAD       "NOTHING BAD ON 10"

/* Special levels: every LEVEL_LOTS_COIN_EVERY-th level has twice the gold coin
 * pickups, every LEVEL_MANY_RAT_RAVENS_EVERY-th twice the rat_ravens, every
 * LEVEL_LOTS_JEWEL_EVERY-th twice the jewels; level
 * LEVEL_EMPTY has no rat_ravens and no gaps.                              */
#define LEVEL_LOTS_COIN_EVERY  7
#define LEVEL_MANY_RAT_RAVENS_EVERY    13
#define LEVEL_LOTS_JEWEL_EVERY  9
#define LEVEL_EMPTY             10

/* Rat_raven speed ramp: above level RAT_RAVEN_SPEED_RAMP_START, both kinds of rat_raven get RAT_RAVEN_SPEED_RAMP_PCT percent faster per level, up to
 * RAT_RAVEN_SPEED_MAX_PCT percent of their normal speed (100 = unchanged).      */
#define RAT_RAVEN_SPEED_RAMP_START    20
#define RAT_RAVEN_SPEED_RAMP_PCT      5
#define RAT_RAVEN_SPEED_MAX_PCT       200
#define MSG_DEATH             "PRESS Q OR ESC TO QUIT"
#define MSG_SICK              "YOU FEEL SICK"
#define MSG_JEWEL             "YOU FEEL GREAT!"
#define MSG_IN_FOG            "YOU ARE IN THICK FOG"
#define COL_MESSAGE           COL_COIN   /* every message uses the gold coin pickup colour */

/* The death screen's one-line reason, one per way to die (death_reason
 * in platformer.c).                                                      */
#define MSG_DEATH_TIME        "YOU RAN OUT OF TIME"
#define MSG_DEATH_FALL        "YOU FELL INTO A GAP"
#define MSG_DEATH_RAT         "A RAT KILLED YOU"     /* ground rat_raven */
#define MSG_DEATH_RAVEN       "A RAVEN KILLED YOU"   /* flying rat_raven */

/* ===================================================================== */
/*  Level generation retries                                             */
/* ===================================================================== */

/* Retry-based placement (see generate_level() in platformer.c): each
 * gen_*() loop gives up after this many attempts per object it wants,
 * and each "pick a spot" helper after GEN_SPOT_TRIES attempts.          */
#define GEN_TRIES_PER_OBJECT  20
#define GEN_SPOT_TRIES        20

/* ===================================================================== */
/*  Screens, prompts and timing outside gameplay                         */
/* ===================================================================== */

/* Text rows (the driver's 8x10 grid) used by the end screens and prompts. */
#define ROW_END_TITLE         10   /* "YOU DIED" / "YOU WIN!"          */
#define ROW_END_REASON        11   /* death_reason                     */
#define ROW_END_PROMPT        20   /* "PRESS ANY KEY TO CONTINUE"      */
#define ROW_END_PROMPT2       21   /* "PRESS ENTER TO RETRY LEVEL"     */
#define ROW_CONFIRM           14   /* "QUIT?"                          */
#define ROW_CONFIRM_HINT      16
#define ROW_PAUSED            16
#define ROW_BOOTSEL           15
#define ROW_CHOOSE_LEVEL      10

/* Win screen: pixel rows and text scale of the big score readout and of
 * the smaller TIME BONUS / MISSED COINS line under it.                   */
#define WIN_SCORE_Y           130
#define WIN_SCORE_SCALE       3
#define WIN_LINE_Y            170
#define WIN_LINE_SCALE        2

/* Minimum pause on an end screen before a key is accepted (keys drained
 * throughout), the shorter pause between win-screen steps, and how often
 * those pauses poll the keyboard.                                        */
#define END_SCREEN_PAUSE_MS   2000
#define WIN_STEP_PAUSE_MS     1000
#define KEY_DRAIN_POLL_MS     20
/* Poll interval while waiting for a key press (wait_any_key(), choose_level()). */
#define KEY_WAIT_POLL_MS      3
/* Silence after each sounded note of the win song.                      */
#define WIN_NOTE_GAP_MS       20
/* How long the "REBOOTING TO BOOTSEL..." message shows before the reboot. */
#define BOOTSEL_MSG_MS        300

/* Leaving the game for the PicoCalc UF2 Loader (pelrun/uf2loader), on the
 * PicoCalc builds only. The loader has no call for an app to use, but its own
 * menu hands commands to its start-up code through the chip's watchdog scratch
 * registers, which survive a watchdog reboot: scratch 0 holds a magic number,
 * 1 the boot mode, 2 an argument. Asking for boot mode "SD" then rebooting
 * makes the loader show its menu again. The same values are used by
 * PicoCalc-SD-Drive.                                                       */
#define LOADER_COMMAND_MAGIC      0xE98CC638u /* PICOCALC_BL_MAGIC in the loader's proginfo.h */
#define LOADER_BOOT_MODE_SD       1           /* BOOT_SD: load the menu from the SD card */
#define LOADER_SCRATCH_MAGIC      0
#define LOADER_SCRATCH_MODE       1
#define LOADER_SCRATCH_ARGUMENT   2
/* How long the "BACK TO THE LOADER..." message shows before the reboot.     */
#define LOADER_EXIT_MSG_MS        300
/* The watchdog reboot happens this many ms after it is requested.          */
#define LOADER_REBOOT_DELAY_MS    10

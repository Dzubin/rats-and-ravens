/*
 *  Rats and Ravens (PicoCalc Platformer)
 *  ======================================
 *
 *  A movement / jump / camera-scroll / scoring / randomly generated level
 *  game for the ClockworkPi PicoCalc (and other hardware). An original 2D
 *  platformer, not based on or inspired by any specific existing game.
 *  Author: Thomas Dzubin.
 *
 *  Built on the Raspberry Pi Pico SDK and the LCD / south-bridge
 *  keyboard drivers from "picocalc-text-starter" by Blair Leduc. The full
 *  game description (level generation, rat_ravens, sickness, scoring,
 *  jumping, foggy zones, rendering, controls) is in GAME_DESCRIPTION.md.
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"      /* reset_usb_boot() - BOOTSEL reboot ('~') */
#ifndef PLATFORM_DESKTOP
#include "hardware/watchdog.h" /* watchdog_hw, watchdog_reboot() - exit to the UF2 Loader */
#endif

#include "lcd.h"          /* WIDTH, HEIGHT, GLYPH_HEIGHT, RGB(), lcd_*   */
#include "southbridge.h"  /* sb_init(), sb_read_keyboard()               */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values             */
#include "audio.h"        /* PITCH_*, audio_init(), audio_play_sound()   */
#include "platformer_config.h"
#include "rat_raven_sprites.h"
#include "player_sprite.h"
#include "pickup_sprites.h"

#include "platformer_functions.h"   /* static function prototypes */
#include "platformer_variables.c"   /* global types and variables */

/* ===================================================================== */
/*  main                                                                  */
/* ===================================================================== */
int main(void)
{
    stdio_init_all();

    sb_init();                /* keyboard / south-bridge I2C (no bg poll) */
    lcd_init();                /* ST7365P LCD */
    lcd_enable_cursor(false);  /* we don't want the blinking text cursor */
    audio_init();              /* stereo PWM tones; see sfx_play_*()      */

    for (;;) {
        splash_screen();
        if (is_quit_key(wait_any_key()))
            exit_to_loader();             /* does not return */
        run_demo();
    }
}

static void splash_screen(void)
{
    lcd_clear_screen();
    draw_big_text((WIDTH - (int)strlen(GAME_TITLE) * font_8x10.width * STATUS_SCALE) / 2,
                  3 * GLYPH_HEIGHT, GAME_TITLE, STATUS_SCALE, COL_YELLOW);
    put_row(6,  "A 2D SIDE-SCROLLING PLATFORMER",           COL_CYAN);
    put_row(7,  VERSION,                                    COL_CYAN);
    put_row(8,  "BY THOMAS DZUBIN",                         COL_CYAN);
    put_row(13, "YOU ARE A HAPPY",                          COL_PLAYER);
    put_row(14, "GREEN GLOB WHO",                           COL_PLAYER);
    put_row(15, "LIKES TO COLLECT",                         COL_PLAYER);
    put_row(16, "GOLD COINS AND JEWELS",                        COL_PLAYER);
    put_row(17, "AND JUMP OVER THINGS",                     COL_PLAYER);
    /* row 18 deliberately left blank */
    put_row(19, "BUT WATCH OUT FOR THE",                    COL_PLAYER);
    put_row(20, "RATS AND RAVENS",                        COL_PLAYER);
    put_row(MAX_ROW - 4, "ARROWS move, SPACE/UP jump",      COL_LABEL);
    put_row(MAX_ROW - 3, "ENTER pause, ESC or Q quit",      COL_LABEL);
    put_row(MAX_ROW - 2, "PRESS ANY KEY TO START",          COL_WHITE);
}

static void run_demo(void)
{
    level_num = 1;
    generate_level();
    start_run();

    uint32_t last_ms = to_ms_since_boot(get_absolute_time());
    uint32_t accum_ms = 0;

    for (;;) {
        uint16_t e = key_event();
        while (e) {
            uint8_t code = ev_code(e);
            uint8_t st   = ev_state(e);
            uint8_t c    = (code >= 'A' && code <= 'Z') ? (uint8_t)(code + 0x20) : code;

            /* A/S/D/F work as left/up/down/right, for a laptop keyboard. */
            if (c == 'a')      code = KEY_LEFT;
            else if (c == 's') code = KEY_UP;
            else if (c == 'd') code = KEY_DOWN;
            else if (c == 'f') code = KEY_RIGHT;

            if (c == '~' && st == KEY_STATE_PRESSED)
                go_bootsel();               /* does not return */

            if (code == KEY_ESC && st == KEY_STATE_PRESSED) {
#ifdef PLATFORM_DESKTOP
                if (confirm_quit())
                    exit(0);
                /* Declined: same clean-up as Q's, below. */
                left_held = right_held = false;
                drawn_camera_x = -1;
                render_frame();
                last_ms = to_ms_since_boot(get_absolute_time());
                accum_ms = 0;
                break;
#else
                exit_to_loader();           /* does not return */
#endif
            }

            if (c == 'q' && st == KEY_STATE_PRESSED) {
                if (confirm_quit())
                    exit_to_loader();       /* does not return */
                /* Declined. The prompt drained the key buffer, so any
                 * LEFT/RIGHT release during it was lost: forget both, or
                 * the character would keep running on its own. The prompt
                 * also overwrote two rows, so force a full repaint, and
                 * don't count the time spent in it as game time.          */
                left_held = right_held = false;
                drawn_camera_x = -1;
                render_frame();
                last_ms = to_ms_since_boot(get_absolute_time());
                accum_ms = 0;
                break;
            }

            if (c == 'l' && st == KEY_STATE_PRESSED) {
                int chosen = choose_level();
                if (chosen > 0) {
                    level_num = chosen;
                    generate_level();
                    paused = false;
                    left_held = right_held = false;
                    start_run();
                } else {
                    left_held = right_held = false;   /* releases lost; see Q above */
                    drawn_camera_x = -1;
                    render_frame();
                }
                last_ms = to_ms_since_boot(get_absolute_time());
                accum_ms = 0;
                break;
            }

            /* Enter or P pauses the game, showing a "PAUSED" message on
             * screen. Any sound effect is cut off (nothing ends it while
             * physics is stopped); the jewel and level timers are
             * tick-based, so they simply stop along with physics.        */
            if ((code == KEY_ENTER || code == KEY_RETURN || c == 'p') &&
                st == KEY_STATE_PRESSED) {
                paused = !paused;
                if (paused) {
                    sfx_silence();
                    put_row(ROW_PAUSED, "PAUSED", COL_YELLOW);
                } else {
                    drawn_camera_x = -1;
                    render_frame();
                }
            }

            if (code == KEY_LEFT || code == KEY_RIGHT) {
                /* A release always counts, even while paused, so letting
                 * go of a key during a pause cannot leave it stuck "held"
                 * afterwards (the character would run on by itself).
                 * Presses only count while playing.                       */
                bool *held = (code == KEY_LEFT) ? &left_held : &right_held;
                if (st == KEY_STATE_RELEASED)
                    *held = false;
                else if (!paused && (st == KEY_STATE_PRESSED || st == KEY_STATE_HOLD))
                    *held = true;
            } else if (!paused) {
                if ((code == KEY_SPACE || code == KEY_UP) && st == KEY_STATE_PRESSED)
                    jump_requested = true;
                else if (code == KEY_DOWN && st == KEY_STATE_PRESSED)
                    down_buffer_ticks = DOWN_BUFFER_TICKS;
            }

            e = key_event();
        }

        if (!paused) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            accum_ms += (now - last_ms);
            last_ms = now;

            int ticks = 0;
            while (accum_ms >= MS_PER_TICK && ticks < MAX_TICKS_PER_FRAME &&
                   !player_died && !player_won) {
                step_physics();
                accum_ms -= MS_PER_TICK;
                ticks++;
            }
            if (ticks == MAX_TICKS_PER_FRAME)
                accum_ms = 0;   /* dropped time after a stall; don't binge-simulate */

            if (player_died) {
                render_frame();        /* show the moment of death first */
                sfx_play_sad();         /* same sad sound as becoming sick */
                if (show_death_screen())
                    exit_to_loader();       /* does not return */
                start_run();           /* retry the SAME level */
                last_ms = to_ms_since_boot(get_absolute_time());
                accum_ms = 0;
                continue;
            }

            if (player_won) {
                render_frame();        /* show the moment of victory first */
                bool retry_level = show_win_screen();
                if (retry_level)
                    start_run();        /* replay the SAME level, keep level_high_score */
                else
                    win_level();        /* a genuinely NEW level; score resets to 0 */
                last_ms = to_ms_since_boot(get_absolute_time());
                accum_ms = 0;
                continue;
            }

            render_frame();
        } else {
            last_ms = to_ms_since_boot(get_absolute_time());  /* don't accumulate while paused */
        }

        sleep_ms(IDLE_SLEEP_MS);
    }
}

/* ===================================================================== */
/*  Level data: every level is randomly generated; see generate_level()
 *  at the end of this section.                                          */
/* ===================================================================== */

/* does the player's bounding box (top-left px,py) overlap any foggy zone? */
static bool player_in_foggy(int px, int py)
{
    for (int i = 0; i < foggy_count; i++) {
        const solid_t *z = &FOGGY_ZONES[i];
        if (px + PLAYER_W > z->x && px < z->x + z->w &&
            py + PLAYER_H > z->y && py < z->y + z->h)
            return true;
    }
    return false;
}

/* Same idea as player_in_foggy() above, but for a rat_raven's own bounding
 * box (top-left bx,by, always RAT_RAVEN_SIZE square): used to halve a rat_raven's speed for as long as it is inside one. Parametrized rather than
 * shared with player_in_foggy() since a rat_raven's box is a different,
 * fixed size.                                                            */
static bool rat_raven_in_foggy(int bx, int by)
{
    for (int i = 0; i < foggy_count; i++) {
        const solid_t *z = &FOGGY_ZONES[i];
        if (bx + RAT_RAVEN_SIZE > z->x && bx < z->x + z->w &&
            by + RAT_RAVEN_SIZE > z->y && by < z->y + z->h)
            return true;
    }
    return false;
}

#if !PICKUP_USE_SPRITES
static uint16_t pickup_colour(pickup_type_t type)
{
    return type == PICK_COIN ? COL_COIN : COL_JEWEL;
}
#endif

/* ===================================================================== */
/*  Random level generation                                              */
/*                                                                        */
/*  Memory: SOLIDS[]/FOGGY_ZONES[]/PICKUPS[] are fixed-capacity arrays,
 *  not dynamically allocated (no malloc anywhere in this project). At
 *  ~10-12 bytes/entry and generous caps (MAX_SOLIDS/MAX_FOGGY_ZONES/
 *  MAX_PICKUPS in platformer_config.h) that is under 1.5KB total,
 *  trivial next to RP2040's 264KB RAM even though real generated levels
 *  use far fewer entries than the caps allow.                            */
/* ===================================================================== */

/* Tiny xorshift32 PRNG: plain ANSI C, no platform dependency (unlike the
 * Pico SDK's hardware RNG, which would need its own stub added to
 * desktop/ to keep the Windows/Linux build compiling), so it behaves
 * identically on RP2040, RP2350, and the desktop build. Seeded once from
 * wall-clock time the first time a level is generated.                   */

static uint32_t rand_u32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static int rand_below(int n) { return (int)(rand_u32() % (uint32_t)n); }
static int rand_range(int lo, int hi) { return lo + rand_below(hi - lo + 1); }

/* Strict AABB overlap (touching edges do not count, e.g. a tower resting
 * exactly on the ground is NOT an overlap with it). See the design note
 * below on why solids avoid overlapping each other but foggy zones and
 * pickups only avoid overlapping their own kind.                        */
static bool overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

/* Where several special levels coincide (level 63 is a multiple of both 7
 * and 9), every effect applies, but the message follows this order.      */
static void pick_level_rules(void)
{
    rules.coin_mult = (level_num % LEVEL_LOTS_COIN_EVERY == 0) ? 2 : 1;
    rules.jewel_mult = (level_num % LEVEL_LOTS_JEWEL_EVERY == 0) ? 2 : 1;
    rules.rat_raven_mult   = (level_num % LEVEL_MANY_RAT_RAVENS_EVERY == 0) ? 2 : 1;
    rules.rat_raven_speed_pct = 100;
    if (level_num > RAT_RAVEN_SPEED_RAMP_START) {
        rules.rat_raven_speed_pct += (level_num - RAT_RAVEN_SPEED_RAMP_START) * RAT_RAVEN_SPEED_RAMP_PCT;
        if (rules.rat_raven_speed_pct > RAT_RAVEN_SPEED_MAX_PCT)
            rules.rat_raven_speed_pct = RAT_RAVEN_SPEED_MAX_PCT;
    }
    rules.no_gaps = rules.no_rat_ravens = (level_num == LEVEL_EMPTY);
    rules.fog = (level_num % FOG_LEVEL_INTERVAL == 0);

    if (level_num == LEVEL_EMPTY)                        rules.message = MSG_NOTHING_BAD;
    else if (rules.rat_raven_mult > 1)                         rules.message = MSG_MANY_RAT_RAVENS;
    else if (rules.jewel_mult > 1)                       rules.message = MSG_LOTS_JEWEL;
    else if (rules.coin_mult > 1)                       rules.message = MSG_LOTS_COIN;
    else if (rules.fog)                                  rules.message = MSG_FOG_LEVEL;
    else if (level_num == 1)                             rules.message = MSG_LEVEL_1;
    else if (level_num == 2)                             rules.message = MSG_LEVEL_2;
    else                                                 rules.message = "";
}

static void gen_gaps(void)
{
    if (rules.no_gaps) {
        gaps_n = 0;
        return;
    }

    int screens = world_width / REF_SCREEN_W;
    int want = screens * GAPS_PER_SCREEN_X10 / 10;
    if (want < MIN_GAPS) want = MIN_GAPS;
    if (want > MAX_GAPS) want = MAX_GAPS;

    gaps_n = 0;
    for (int tries = 0; gaps_n < want && tries < want * GEN_TRIES_PER_OBJECT; tries++) {
        int w = rand_range(GAP_W_MIN, GAP_W_MAX);
        int lo = LEVEL_SAFE_MARGIN;
        int hi = world_width - LEVEL_SAFE_MARGIN - w;
        if (hi <= lo)
            break;
        int x = rand_range(lo, hi);

        bool clash = false;
        for (int i = 0; i < gaps_n; i++) {
            if (x < gap_x[i] + gap_w[i] + GAP_SPACING_MIN &&
                x + w + GAP_SPACING_MIN > gap_x[i]) {
                clash = true;
                break;
            }
        }
        if (!clash) {
            gap_x[gaps_n] = (int16_t)x;
            gap_w[gaps_n] = (int16_t)w;
            gaps_n++;
        }
    }

    /* Sort by x; gaps_n is tiny (a handful at most), so a plain insertion
     * sort is plenty.                                                    */
    for (int i = 1; i < gaps_n; i++) {
        int16_t xx = gap_x[i], ww = gap_w[i];
        int j = i - 1;
        while (j >= 0 && gap_x[j] > xx) {
            gap_x[j + 1] = gap_x[j];
            gap_w[j + 1] = gap_w[j];
            j--;
        }
        gap_x[j + 1] = xx;
        gap_w[j + 1] = ww;
    }
}

static void add_solid(int x, int y, int w, int h, uint16_t colour)
{
    if (solids_count >= MAX_SOLIDS)
        return;
    SOLIDS[solids_count].x = (int16_t)x;
    SOLIDS[solids_count].y = (int16_t)y;
    SOLIDS[solids_count].w = (int16_t)w;
    SOLIDS[solids_count].h = (int16_t)h;
    SOLIDS[solids_count].colour = colour;
    solids_count++;
}

/* The ground is the complement of the sorted gap list: one segment
 * before each gap, one after the last one.                               */
static void gen_ground(void)
{
    int prev_end = 0;
    for (int i = 0; i < gaps_n; i++) {
        add_solid(prev_end, GROUND_TOP, gap_x[i] - prev_end, GROUND_HEIGHT, COL_GROUND);
        prev_end = gap_x[i] + gap_w[i];
    }
    add_solid(prev_end, GROUND_TOP, world_width - prev_end, GROUND_HEIGHT, COL_GROUND);
    ground_count = solids_count;
}

/* Generic "does this candidate rectangle overlap any solid placed so
 * far" check. Ground segments are included, but touching (not
 * overlapping) a ground segment is fine (see overlaps()'s strict
 * comparison), which is exactly how a tower resting flush on the
 * ground already behaves.                                               */
static bool solid_overlaps_any(int x, int y, int w, int h)
{
    for (int i = 0; i < solids_count; i++) {
        const solid_t *s = &SOLIDS[i];
        if (overlaps(x, y, w, h, s->x, s->y, s->w, s->h))
            return true;
    }
    return false;
}

/* A random x within a random ground segment wide enough for `w`; used to
 * place towers (which must stand on solid ground, never over a gap).    */
static bool pick_ground_span(int w, int *out_x)
{
    for (int tries = 0; tries < GEN_SPOT_TRIES; tries++) {
        const solid_t *s = &SOLIDS[rand_below(ground_count)];
        if (s->w < w)
            continue;
        *out_x = s->x + rand_below(s->w - w + 1);
        return true;
    }
    return false;
}

/* Towers: stand on solid ground, height varies across a band that
 * straddles the double jump's reach (see TOWER_H_MIN/MAX's comment in
 * platformer_config.h). Avoids overlapping any other solid (see the
 * design note above generate_level() on why solids specifically need
 * this, unlike foggy zones or pickups).                                 */
static void gen_towers(void)
{
    int screens = world_width / REF_SCREEN_W;
    int want = screens * TOWERS_PER_SCREEN_X10 / 10;
    if (want < MIN_TOWERS) want = MIN_TOWERS;

    towers_begin = solids_count;
    for (int tries = 0; towers_n < want && tries < want * GEN_TRIES_PER_OBJECT; tries++) {
        int w = rand_range(TOWER_W_MIN, TOWER_W_MAX);
        int h = rand_range(TOWER_H_MIN, TOWER_H_MAX);
        int x;
        if (!pick_ground_span(w, &x))
            break;
        int y = GROUND_TOP - h;
        if (solid_overlaps_any(x, y, w, h))
            continue;
        add_solid(x, y, w, h, COL_TOWER);
        towers_n++;
    }
}

/* Platforms: float anywhere (including over a gap), at a height a jump or
 * double jump can always reach (see PLATFORM_Y_MIN/MAX's comment). The
 * overlap test covers the platform PLUS a player-height of space both
 * under it and above it (above catches a platform placed EARLIER sitting
 * just over this one), so every platform keeps room for the player
 * beneath it: a platform can never sit so low over a tower or another
 * platform that its top (and any pickup on it) becomes a crawlspace
 * nothing can fit into, and the two can never merge into one taller
 * wall.                                                                 */
static void gen_platforms(void)
{
    int screens = world_width / REF_SCREEN_W;
    int want = screens * PLATFORMS_PER_SCREEN_X10 / 10;
    if (want < MIN_PLATFORMS) want = MIN_PLATFORMS;

    platforms_begin = solids_count;
    for (int tries = 0; platforms_n < want && tries < want * GEN_TRIES_PER_OBJECT; tries++) {
        int w = rand_range(PLATFORM_W_MIN, PLATFORM_W_MAX);
        int x = rand_range(0, world_width - w);
        int y = rand_range(PLATFORM_Y_MIN, PLATFORM_Y_MAX);
        if (solid_overlaps_any(x, y - PLAYER_H, w, PLAYER_H + PLATFORM_H + PLAYER_H))
            continue;
        add_solid(x, y, w, PLATFORM_H, COL_PLATFORM);
        platforms_n++;
    }
}

static void add_foggy(int x, int y, int w, int h)
{
    if (foggy_count >= MAX_FOGGY_ZONES)
        return;
    foggy_x_sub[foggy_count] = SUBPIXELS(x);
    /* Still until FOG_DRIFT_START_LEVEL; the rand_below() call is kept
     * either way so a level's layout doesn't depend on this rule.         */
    int8_t drift_dir = rand_below(2) ? 1 : -1;
    foggy_dir[foggy_count] = (w >= world_width || level_num < FOG_DRIFT_START_LEVEL) ? 0 : drift_dir;
    foggy_drawn_x[foggy_count] = (int16_t)x;
    foggy_spawn_x[foggy_count] = (int16_t)x;
    foggy_spawn_dir[foggy_count] = foggy_dir[foggy_count];
    FOGGY_ZONES[foggy_count].x = (int16_t)x;
    FOGGY_ZONES[foggy_count].y = (int16_t)y;
    FOGGY_ZONES[foggy_count].w = (int16_t)w;
    FOGGY_ZONES[foggy_count].h = (int16_t)h;
    FOGGY_ZONES[foggy_count].colour = COL_FOGGY;
    foggy_count++;
}

/* Foggy zones are free to overlap a solid on purpose (a zone partly
 * blocked by a platform is a valid, intended obstacle; see the design
 * note in platformer_config.h's FOGGY_* comment). Only checked against
 * OTHER foggy zones here, purely so two overlapping zone rectangles
 * do not draw as one indistinguishable blob.                             */
/* A ground-flush foggy zone of height h; shared by the random ground
 * zones and the level-wide fog.                                          */
static void add_ground_foggy(int x, int w, int h)
{
    add_foggy(x, GROUND_TOP - h, w, h);
}

static bool foggy_overlaps_any(int x, int y, int w, int h)
{
    for (int i = 0; i < foggy_count; i++) {
        const solid_t *z = &FOGGY_ZONES[i];
        if (overlaps(x, y, w, h, z->x, z->y, z->w, z->h))
            return true;
    }
    return false;
}

/* True if some already-placed platform (gen_platforms() runs before
 * gen_foggy_zones() in generate_level(), so platforms_begin/platforms_n
 * are already final) is close enough, horizontally within
 * FOGGY_FLOAT_PLATFORM_MAX_DX, and vertically within a double jump's
 * reach of the candidate zone's underside, that a player standing on it
 * could jump up into the zone. Landing on any solid refills the double
 * jump the same way landing on the ground does, so the same
 * FOGGY_FLOAT_MAX_GROUND_REACH applies here too. Used by
 * gen_foggy_zones() when a floating zone floats higher than that reach
 * allows directly from the ground; see its own comment in
 * platformer_config.h.                                                   */
static bool platform_near_foggy_float(int x, int y, int w, int h)
{
    for (int i = 0; i < platforms_n; i++) {
        const solid_t *p = &SOLIDS[platforms_begin + i];
        int dx = 0;

        if (p->x + p->w < x)
            dx = x - (p->x + p->w);
        else if (x + w < p->x)
            dx = p->x - (x + w);

        int dy = p->y - (y + h);   /* platform top vs. the zone's underside */
        if (dx <= FOGGY_FLOAT_PLATFORM_MAX_DX && dy >= 0 && dy <= FOGGY_FLOAT_MAX_GROUND_REACH)
            return true;
    }
    return false;
}

/* Half ground-flush, half floating, picked randomly per zone. A
 * ground-flush zone always sits flush on the ground and is reachable by
 * just walking up to it, no jump needed. Only a FLOATING zone needs a
 * reachability check (see platform_near_foggy_float()'s comment and
 * FOGGY_FLOAT_MAX_GROUND_REACH's in platformer_config.h).               */
static void gen_foggy_zones(void)
{
    int screens = world_width / REF_SCREEN_W;
    int want = screens * FOGGY_PER_SCREEN_X10 / 10;
    if (want < MIN_FOGGY_ZONES) want = MIN_FOGGY_ZONES;

    /* Ground zones start short and grow to their full random height range
     * by FOGGY_GROUND_H_FULL_LEVEL: both ends of the range ramp linearly
     * from FOGGY_GROUND_H_START (level 1) to FOGGY_GROUND_H_MIN/_MAX.     */
    int ramp_span = FOGGY_GROUND_H_FULL_LEVEL - 1;
    int ramp_lvl = (level_num < FOGGY_GROUND_H_FULL_LEVEL ? level_num : FOGGY_GROUND_H_FULL_LEVEL) - 1;
    if (ramp_lvl < 0) ramp_lvl = 0;
    int ground_h_min = FOGGY_GROUND_H_START + (FOGGY_GROUND_H_MIN - FOGGY_GROUND_H_START) * ramp_lvl / ramp_span;
    int ground_h_max = FOGGY_GROUND_H_START + (FOGGY_GROUND_H_MAX - FOGGY_GROUND_H_START) * ramp_lvl / ramp_span;

    for (int tries = 0; foggy_count < want && tries < want * GEN_TRIES_PER_OBJECT; tries++) {
        int w = rand_range(FOGGY_W_MIN, FOGGY_W_MAX);
        int x = rand_range(0, world_width - w);
        int y, h;
        if (rand_below(2) == 0) {
            h = rand_range(ground_h_min, ground_h_max);
            y = GROUND_TOP - h;   /* same placement as add_ground_foggy() */
        } else {
            h = rand_range(FOGGY_FLOAT_H_MIN, FOGGY_FLOAT_H_MAX);
            y = rand_range(FOGGY_FLOAT_Y_MIN, FOGGY_FLOAT_Y_MAX);
            if (GROUND_TOP - (y + h) > FOGGY_FLOAT_MAX_GROUND_REACH &&
                !platform_near_foggy_float(x, y, w, h))
                continue;
        }
        if (foggy_overlaps_any(x, y, w, h))
            continue;
        add_foggy(x, y, w, h);
    }
}

/* On a fog level, one more foggy zone covers the whole level width down
 * on the ground. It is added after the random zones so it never causes
 * them to be rejected for overlap, and takes the last slot if the list
 * is already full so it can't be dropped.                                */
static void gen_fog(void)
{
    if (!rules.fog)
        return;
    if (foggy_count >= MAX_FOGGY_ZONES)
        foggy_count = MAX_FOGGY_ZONES - 1;
    add_ground_foggy(0, world_width, FOGGY_GROUND_H_MAX);
}

static void add_pickup(int x, int y, pickup_type_t type, int fog)
{
    if (pickups_count >= MAX_PICKUPS)
        return;
    PICKUPS[pickups_count].x = (int16_t)x;
    PICKUPS[pickups_count].y = (int16_t)y;
    PICKUPS[pickups_count].type = type;
    PICKUPS[pickups_count].taken = false;
    PICKUPS[pickups_count].fog = (int8_t)fog;
    PICKUPS[pickups_count].spawn_x = (int16_t)x;
    pickups_count++;
}

/* Pickups avoid overlapping EACH OTHER (again, purely visual, since two
 * overlapping icons would draw on top of one another), padded by
 * PICKUP_SPACING_MIN so they read as clearly separate collectibles.      */
static bool pickup_overlaps_any(int x, int y)
{
    for (int i = 0; i < pickups_count; i++) {
        const pickup_t *p = &PICKUPS[i];
        if (overlaps(x - PICKUP_SPACING_MIN, y - PICKUP_SPACING_MIN,
                     PICKUP_SIZE + 2 * PICKUP_SPACING_MIN, PICKUP_SIZE + 2 * PICKUP_SPACING_MIN,
                     p->x, p->y, PICKUP_SIZE, PICKUP_SIZE))
            return true;
    }
    return false;
}

/* One mechanism places every pickup, regardless of type: on top of a
 * random tower, on top of a random platform, floating in open air at a
 * jump-reachable height, or inside a random foggy zone. See the design
 * note on PICKUPS[] above for why this needs to stay generic. Whatever
 * the mode, a spot overlapping any solid is rejected: the open-air and
 * foggy-zone modes know nothing about towers or platforms, and a pickup
 * buried inside one could never be collected. (Resting exactly on top of
 * a tower or platform is touching, not overlapping; see overlaps().)
 * *out_fog is the foggy zone the pickup was placed inside, or -1.        */
static bool pick_pickup_spot(int *out_x, int *out_y, int *out_fog)
{
    for (int tries = 0; tries < GEN_SPOT_TRIES; tries++) {
        int mode = rand_below(4);
        int x, y, fog = -1;

        if (mode == 0 && towers_n > 0) {
            const solid_t *s = &SOLIDS[towers_begin + rand_below(towers_n)];
            x = s->x + (s->w - PICKUP_SIZE) / 2;
            y = s->y - PICKUP_SIZE;
        } else if (mode == 1 && platforms_n > 0) {
            const solid_t *s = &SOLIDS[platforms_begin + rand_below(platforms_n)];
            x = s->x + (s->w - PICKUP_SIZE) / 2;
            y = s->y - PICKUP_SIZE;
        } else if (mode == 2 && foggy_count > 0) {
            fog = rand_below(foggy_count);
            const solid_t *z = &FOGGY_ZONES[fog];
            if (z->w <= PICKUP_SIZE || z->h <= PICKUP_SIZE)
                continue;
            x = z->x + rand_below(z->w - PICKUP_SIZE);
            y = z->y + rand_below(z->h - PICKUP_SIZE);
        } else {
            /* Floating in open air, anywhere across the level: the same
             * jump-reachable height band a plain jump from either side
             * of a gap would reach, regardless of what is directly
             * below (ground or a gap).                                  */
            x = rand_range(0, world_width - PICKUP_SIZE);
            y = GROUND_TOP - rand_range(PICKUP_AIR_H_MIN, PICKUP_AIR_H_MAX) - PICKUP_SIZE;
        }

        if (!pickup_overlaps_any(x, y) &&
            !solid_overlaps_any(x, y, PICKUP_SIZE, PICKUP_SIZE)) {
            *out_x = x;
            *out_y = y;
            *out_fog = fog;
            return true;
        }
    }
    return false;
}

/* Ground rat_ravens spawn on solid ground, same span-picking as towers
 * (never inside a gap to start); flying ones spawn anywhere across the
 * level width instead, since they do not need anything solid underneath,
 * at a random height between FLYING_RAT_RAVEN_Y_MIN/MAX. Either kind:
 * never within RAT_RAVEN_MIN_SPAWN_DIST of the player's own start
 * position. Direction is random; everything else about their behaviour
 * is runtime (movement/collision, in step_physics()). Count is level-
 * based, not density-scaled by level width; see RAT_RAVENS_PER_LEVEL and
 * RAT_RAVENS_LEVEL_CAP. Each one has a FLYING_RAT_RAVEN_CHANCE_PCT percent
 * chance of being flying, but at least one flying rat_raven is guaranteed
 * per level regardless, enforced after the loop rather than by rigging
 * the roll, so it does not matter how the percentage draws happened to
 * land this time. Neither kind ever spawns overlapping a solid (a tower,
 * or for a flying one a platform too): it would otherwise pop out to
 * the solid's side on its first tick, possibly off its piece of ground.
 * Standing on the ground is touching, not overlapping; see overlaps().   */
static bool rat_raven_spot_ok(int x, int y)
{
    if (x + RAT_RAVEN_SIZE + RAT_RAVEN_MIN_SPAWN_DIST > PLAYER_START_X &&
        x < PLAYER_START_X + PLAYER_W + RAT_RAVEN_MIN_SPAWN_DIST)
        return false;
    return !solid_overlaps_any(x, y, RAT_RAVEN_SIZE, RAT_RAVEN_SIZE);
}

static void gen_rat_ravens(void)
{
    /* RAT_RAVENS_START on levels 1-2, doubling every RAT_RAVENS_DOUBLE_EVERY
     * levels after that, clamped at RAT_RAVENS_LEVEL_CAP. A loop (rather
     * than a shift by a computed amount) avoids ever shifting by more than
     * fits in an int, however high level_num climbs.                       */
    int want = RAT_RAVENS_START;
    int tier = (level_num - 1) / RAT_RAVENS_DOUBLE_EVERY;
    for (int t = 0; t < tier && want < RAT_RAVENS_LEVEL_CAP; t++)
        want *= 2;
    if (want > RAT_RAVENS_LEVEL_CAP) want = RAT_RAVENS_LEVEL_CAP;
    want *= rules.rat_raven_mult;
    if (rules.no_rat_ravens) want = 0;

    rat_ravens_count = 0;
    int flying_count = 0;
    for (int tries = 0; rat_ravens_count < want && tries < want * GEN_TRIES_PER_OBJECT; tries++) {
        bool flying = rand_below(100) < FLYING_RAT_RAVEN_CHANCE_PCT;
        int x, y;
        if (flying) {
            x = rand_range(0, world_width - RAT_RAVEN_SIZE);
            y = rand_range(FLYING_RAT_RAVEN_Y_MIN, FLYING_RAT_RAVEN_Y_MAX);
        } else {
            if (!pick_ground_span(RAT_RAVEN_SIZE, &x))
                break;
            y = RAT_RAVEN_Y;
        }
        if (!rat_raven_spot_ok(x, y))
            continue;

        rat_raven_t *bb = &RAT_RAVENS[rat_ravens_count];
        bb->y = (int16_t)y;
        bb->flying = flying;
        bb->spawn_x_sub = SUBPIXELS(x);
        bb->spawn_dir = rand_below(2) ? 1 : -1;
        bb->x_sub = bb->spawn_x_sub;
        bb->dir = bb->spawn_dir;
        bb->alive = true;
        bb->drawn_visible = false;
        rat_ravens_count++;
        if (flying) flying_count++;
    }

    /* Turn one ground rat_raven into a flyer at a fresh spot. Only if a
     * good spot turns up; with the spawn-distance and solid checks each
     * failing well under half the time, GEN_SPOT_TRIES misses in a row
     * does not happen in practice.                                       */
    if (flying_count == 0 && rat_ravens_count > 0) {
        for (int tries = 0; tries < GEN_SPOT_TRIES; tries++) {
            int x = rand_range(0, world_width - RAT_RAVEN_SIZE);
            int y = rand_range(FLYING_RAT_RAVEN_Y_MIN, FLYING_RAT_RAVEN_Y_MAX);
            if (!rat_raven_spot_ok(x, y))
                continue;
            rat_raven_t *bb = &RAT_RAVENS[rand_below(rat_ravens_count)];
            bb->flying = true;
            bb->y = (int16_t)y;
            bb->spawn_x_sub = SUBPIXELS(x);
            bb->x_sub = bb->spawn_x_sub;
            break;
        }
    }

    for (int i = 0; i < rat_ravens_count; i++) {
        rat_raven_t *bb = &RAT_RAVENS[i];
        bb->spawn_y = bb->y;
        bb->drawn_y = bb->y;
    }
}

/* Puts every foggy zone back where the level generated it.               */
static void reset_foggy(void)
{
    for (int i = 0; i < foggy_count; i++) {
        foggy_x_sub[i] = SUBPIXELS(foggy_spawn_x[i]);
        foggy_dir[i] = foggy_spawn_dir[i];
        FOGGY_ZONES[i].x = foggy_spawn_x[i];
    }
}

/* Restores every rat_raven to exactly where/which-way generate_level() first
 * placed it; used on a death-retry of the SAME level (start_run()), the
 * same role PICKUPS[]'s `taken` reset plays for pickups. Does NOT
 * re-roll positions (that would be a different level's layout).          */
static void reset_rat_ravens(void)
{
    rat_raven_alt_ticks = 0;
    for (int i = 0; i < rat_ravens_count; i++) {
        rat_raven_t *bb = &RAT_RAVENS[i];
        bb->y = bb->spawn_y;
        bb->x_sub = bb->spawn_x_sub;
        bb->dir = bb->spawn_dir;
        bb->alive = true;
    }
}

static void gen_pickups(void)
{
    int screens = world_width / REF_SCREEN_W;

    int boosts = screens * JEWELS_PER_SCREEN_X10 / 10;
    if (boosts < MIN_JEWELS) boosts = MIN_JEWELS;
    boosts *= rules.jewel_mult;

    /* Fixed count, not density-scaled; see COIN_PER_LEVEL's comment. */
    int coins = COIN_PER_LEVEL;
    coins *= rules.coin_mult;

    for (int i = 0; i < boosts; i++) {
        int x, y, fog;
        if (pick_pickup_spot(&x, &y, &fog))
            add_pickup(x, y, PICK_JEWEL, fog);
    }
    for (int i = 0; i < coins; i++) {
        int x, y, fog;
        if (pick_pickup_spot(&x, &y, &fog))
            add_pickup(x, y, PICK_COIN, fog);
    }
}

/* The single entry point: picks a fresh world_width and rebuilds
 * SOLIDS[]/FOGGY_ZONES[]/PICKUPS[]/RAT_RAVENS[] from scratch, in
 * dependency order (gaps before ground, ground before towers/platforms/
 * rat_ravens, since all three need solid ground to stand on, before
 * pickups so pick_pickup_spot() has towers/platforms/foggy zones to
 * place things on). Called to start a genuinely NEW level: once at the
 * very first boot, and once after every win (see win_level()). Dying
 * does NOT call this; it retries the SAME level layout instead (see
 * start_run()), only resetting the player/score/timer/which pickups
 * are taken/where rat_ravens are (see reset_rat_ravens()).                 */
static void generate_level(void)
{
    if (rng_state == 0)
        rng_state = (uint32_t)to_ms_since_boot(get_absolute_time()) | 1;  /* xorshift needs a nonzero seed */

    world_width = LEVEL_SCREENS * WIDTH;
    pick_level_rules();

    solids_count = 0;
    foggy_count = 0;
    pickups_count = 0;
    rat_ravens_count = 0;
    towers_n = 0;
    platforms_n = 0;
    level_high_score = 0;

    gen_gaps();
    gen_ground();
    gen_towers();
    gen_platforms();
    gen_rat_ravens();
    gen_foggy_zones();
    gen_fog();
    gen_pickups();
}

/* ===================================================================== */
/*  Player state: resetting it (the variables are in                     */
/*  platformer_variables.c)                                              */
/* ===================================================================== */

static void reset_player(void)
{
    player_facing = 1;
    player_x_sub = SUBPIXELS(PLAYER_START_X);
    player_y_sub = SUBPIXELS(PLAYER_START_Y);
    player_vy = 0;
    grounded = false;
    left_held = right_held = false;
    jump_requested = false;
    down_buffer_ticks = 0;
    air_jumps_left = MAX_AIR_JUMPS;
    used_air_jump = false;
    foggy_move_remaining = 0;
    on_foggy_ledge = false;
    foggy_idle_ticks = 0;
    paused = false;
    powerup_boost_active = false;
    powerup_ticks_remaining = 0;
    sick = false;
    sick_ticks_remaining = 0;
    rat_raven_invuln_ticks = 0;
    just_taken_count = 0;
    score = 0;
    timer_remaining_ms = TIMER_DURATION_MS;
    player_died = false;
    death_reason = NULL;
    player_won = false;
    camera_x = 0;

    for (int i = 0; i < pickups_count; i++) {
        PICKUPS[i].taken = false;
        PICKUPS[i].x = PICKUPS[i].spawn_x;   /* undo riding a drifting foggy zone */
    }
    reset_rat_ravens();
    reset_foggy();
}

/* reset_player() plus the redraw needed to actually show a fresh run;
 * used to restart the SAME level after a death (score resets to 0, same
 * as everything else). Assumes a level already exists; run_demo() is
 * responsible for calling generate_level() first, at boot.               */
static void start_run(void)
{
    reset_player();
    drawn_camera_x = -1;   /* force render_frame()'s next call to do a full draw */
    render_frame();
}

/* Generates a genuinely NEW level and starts a fresh run on it. Score
 * resets to 0, same as everything else reset_player() zeroes; it never
 * carries between levels (level_high_score, updated in show_win_screen()
 * before this runs, is the only thing that persists a level's result).   */
static void win_level(void)
{
    level_num++;
    generate_level();
    reset_player();
    drawn_camera_x = -1;
    render_frame();
}

/* A ground rat_raven stays on the piece of ground it is walking on: it
 * turns around at the edge of a gap (or a level end) instead of crossing
 * it. Finds the ground segment the rat_raven is standing on now (its
 * position before this tick's step) and, if the step would take it past
 * either end of that segment, puts it at that end and reverses it.
 * Returns true if it did.                                                */
static bool rat_raven_stay_on_ground(rat_raven_t *bb, int *sx)
{
    int ox = (int)(bb->x_sub >> SUBPIXEL_SHIFT);
    for (int t = 0; t < ground_count; t++) {
        const solid_t *g = &SOLIDS[t];
        if (ox < g->x || ox + RAT_RAVEN_SIZE > g->x + g->w)
            continue;                    /* not this segment */
        if (*sx < g->x) {
            *sx = g->x;
            bb->dir = 1;
            return true;
        }
        if (*sx + RAT_RAVEN_SIZE > g->x + g->w) {
            *sx = g->x + g->w - RAT_RAVEN_SIZE;
            bb->dir = -1;
            return true;
        }
        return false;
    }
    return false;
}

/* If the SOLIDS[] range [begin, begin+n) blocks `bb`'s path at its own
 * height (bb->y) and candidate x (*sx), reverses `bb`'s direction, clamps
 * *sx to sit flush against whichever side it hit, and returns true.
 * Shared by both rat_raven kinds' tower-bounce: a ground rat_raven only
 * ever checks the tower range; a flying one checks towers, then (if it
 * did not already bounce) platforms too, since it patrols at head
 * height where platforms float. See the call sites in step_physics().    */
static bool rat_raven_bounce_off_range(rat_raven_t *bb, int *sx, int begin, int n)
{
    for (int t = 0; t < n; t++) {
        const solid_t *s = &SOLIDS[begin + t];
        if (*sx + RAT_RAVEN_SIZE > s->x && *sx < s->x + s->w &&
            bb->y + RAT_RAVEN_SIZE > s->y && bb->y < s->y + s->h) {
            *sx = (bb->dir > 0) ? (s->x - RAT_RAVEN_SIZE) : (s->x + s->w);
            bb->dir = -bb->dir;
            return true;
        }
    }
    return false;
}

/* Sound effects: a tiny three-step, tick-driven state machine, so a sound
 * effect never blocks gameplay the way audio_play_sound_blocking() would
 * (it sleeps for its whole duration). sfx_play_*() starts the first tone
 * immediately (audio_play_sound() itself is non-blocking, it just sets
 * the PWM frequency and returns), then sfx_update() (called once per
 * physics tick below, and ALSO pumped by end_screen_pause_and_wait()'s
 * pause loop, since physics stops ticking during that pause but a
 * sad-death tone still needs to stop on time) counts down and either
 * switches to the second tone or falls silent. A "sad" sound has no
 * second tone (sfx_ticks2 left at 0, freq2 unused).                       */

static void sfx_play(uint16_t freq1, int ticks1, uint16_t freq2, int ticks2,
                     uint16_t freq3, int ticks3)
{
    audio_play_sound(freq1, freq1);
    sfx_ticks_remaining = ticks1;
    sfx_freq2 = freq2;
    sfx_ticks2 = ticks2;
    sfx_freq3 = freq3;
    sfx_ticks3 = ticks3;
}

static void sfx_play_happy(void)
{
    sfx_play(SFX_HAPPY_FREQ1, SFX_HAPPY_TICKS1, SFX_HAPPY_FREQ2, SFX_HAPPY_TICKS2, SILENCE, 0);
}

static void sfx_play_jewel(void)
{
    sfx_play(SFX_JEWEL_FREQ1, SFX_JEWEL_TICKS1, SFX_JEWEL_FREQ2, SFX_JEWEL_TICKS2,
             SFX_JEWEL_FREQ3, SFX_JEWEL_TICKS3);
}

static void sfx_play_sad(void)
{
    sfx_play(SFX_SAD_FREQ, SFX_SAD_TICKS, SILENCE, 0, SILENCE, 0);
}

static void sfx_play_impact(void)
{
    sfx_play(SFX_IMPACT_FREQ1, SFX_IMPACT_TICKS1, SFX_IMPACT_FREQ2, SFX_IMPACT_TICKS2, SILENCE, 0);
}

static void sfx_update(void)
{
    if (sfx_ticks_remaining <= 0 || --sfx_ticks_remaining > 0)
        return;
    if (sfx_freq2 != SILENCE) {
        audio_play_sound(sfx_freq2, sfx_freq2);
        sfx_ticks_remaining = sfx_ticks2;
        sfx_freq2 = sfx_freq3;      /* shift the next tone (if any) up */
        sfx_ticks2 = sfx_ticks3;
        sfx_freq3 = SILENCE;
    } else {
        audio_stop();
    }
}

/* Cuts off whatever sound effect is playing, right now. Needed anywhere
 * the game stops ticking physics for a while (pause, the quit prompt,
 * level select, the win screen): sfx_update() is what ends a tone, and
 * nothing calls it there, so a tone started just before would otherwise
 * keep sounding the whole time.                                          */
static void sfx_silence(void)
{
    audio_stop();
    sfx_ticks_remaining = 0;
}

/* One fixed 16ms simulation step: horizontal move against SOLIDS[], then
 * gravity plus jump, then vertical move against SOLIDS[]. Axis-separated
 * (horizontal fully resolved before vertical looks at it), which is
 * enough for a handful of slow-moving obstacles like this milestone's.   */
static void step_physics(void)
{
    /* Level timer: ticks down once per physics tick, so it pauses along
     * with everything else (the jewel's timer does the same). Reaching 0
     * is an instant death; nothing else this tick matters once that
     * happens.                                                           */
    timer_remaining_ms -= MS_PER_TICK;
    if (timer_remaining_ms <= 0) {
        timer_remaining_ms = 0;
        player_died = true;
        death_reason = MSG_DEATH_TIME;
        return;
    }

    /* Foggy zones: checked once, at the start of the tick (before any
     * movement this tick happens), and used below to slow horizontal
     * movement and switch gravity off for the whole tick. See
     * FOGGY_ZONES[] and the FOGGY_* constants in platformer_config.h. The
     * power-up always overrides stickiness.                               */
    bool in_foggy = player_in_foggy((int)(player_x_sub >> SUBPIXEL_SHIFT),
                                      (int)(player_y_sub >> SUBPIXEL_SHIFT));

    /* Jump is a plain edge trigger (one press = one event): consumed here
     * regardless of which branch below ends up using it, so a press can
     * never leak into a later tick or a different zone. DOWN is buffered
     * instead (down_buffer_ticks counts down every tick, see below);
     * whichever branch actually uses it clears the buffer immediately so
     * it cannot fire a second time.                                       */
    bool jump_press = jump_requested;
    jump_requested = false;
    bool down_press = down_buffer_ticks > 0;
    if (down_buffer_ticks > 0)
        down_buffer_ticks--;

    /* Foggy zones drift sideways at a fraction of the ground rat_raven's
     * speed, turning around at the level's edges or when they would touch
     * another moving foggy zone. Any pickup placed inside a zone (its
     * `fog`) moves the same whole-pixel distance, so it stays inside.   */
    int32_t fog_speed = (RAT_RAVEN_SPEED * rules.rat_raven_speed_pct / 100) / FOGGY_SPEED_DIV;
    for (int i = 0; i < foggy_count; i++) {
        foggy_dx[i] = 0;
        if (foggy_dir[i] == 0)
            continue;
        solid_t *z = &FOGGY_ZONES[i];
        int32_t new_sub = foggy_x_sub[i] + foggy_dir[i] * fog_speed;
        int nx = (int)(new_sub >> SUBPIXEL_SHIFT);
        bool blocked = (new_sub < 0 || nx + z->w > world_width);
        for (int j = 0; j < foggy_count && !blocked; j++) {
            if (j != i && foggy_dir[j] != 0 &&
                overlaps(nx, z->y, z->w, z->h,
                         FOGGY_ZONES[j].x, FOGGY_ZONES[j].y, FOGGY_ZONES[j].w, FOGGY_ZONES[j].h))
                blocked = true;
        }
        if (blocked) {
            foggy_dir[i] = (int8_t)-foggy_dir[i];
        } else {
            int moved_px = nx - z->x;
            foggy_dx[i] = new_sub - foggy_x_sub[i];
            foggy_x_sub[i] = new_sub;
            z->x = (int16_t)nx;
            if (moved_px != 0) {
                for (int k = 0; k < pickups_count; k++)
                    if (PICKUPS[k].fog == i)
                        PICKUPS[k].x = (int16_t)(PICKUPS[k].x + moved_px);
            }
        }
    }

    /* Rat_ravens: patrol, independent of the player. Moves every tick
     * regardless of input; reverses off a tower or either end of the
     * level (world edge), and a GROUND one also at the edge of a gap
     * (rat_raven_stay_on_ground()), so it never crosses one. A FLYING
     * rat_raven (`flying`, see gen_rat_ravens()) moves at
     * FLYING_RAT_RAVEN_SPEED (twice RAT_RAVEN_SPEED), ignores gaps, bobs
     * up and down (just below), and also reverses off a PLATFORM, not
     * just a tower, since it patrols at head height where platforms
     * float; see rat_raven_bounce_off_range() above. Either kind's speed
     * is halved for as long as its current position is inside a foggy
     * zone (rat_raven_in_foggy(), checked against where it already is
     * this tick, same as the player's own in_foggy check above).         */
    if (++rat_raven_alt_ticks >= RAT_RAVEN_ALT_INTERVAL_TICKS) {
        rat_raven_alt_ticks = 0;
        for (int i = 0; i < rat_ravens_count; i++) {
            rat_raven_t *bb = &RAT_RAVENS[i];
            if (!bb->flying)
                continue;
            int step = rand_below(2) ? RAT_RAVEN_ALT_STEP : -RAT_RAVEN_ALT_STEP;
            int ny = bb->y + step;
            if (ny > FLYING_RAT_RAVEN_Y_MAX || ny < FLYING_RAT_RAVEN_Y_MIN)
                ny = bb->y - step;   /* would leave the band: go the other way */
            bb->y = (int16_t)ny;
        }
    }

    for (int i = 0; i < rat_ravens_count; i++) {
        rat_raven_t *bb = &RAT_RAVENS[i];
        if (!bb->alive)
            continue;

        int32_t speed = bb->flying ? FLYING_RAT_RAVEN_SPEED : RAT_RAVEN_SPEED;
        speed = (speed * rules.rat_raven_speed_pct + 50) / 100;   /* level ramp, rounded */
        if (rat_raven_in_foggy((int)(bb->x_sub >> SUBPIXEL_SHIFT), bb->y))
            speed /= 2;

        /* Keep the true subpixel sum (new_x_sub) around, not just its
         * truncated pixel value (sx); see the `repositioned` comment
         * below for why.                                                  */
        int32_t new_x_sub = bb->x_sub + bb->dir * speed;
        int sx = (int)(new_x_sub >> SUBPIXEL_SHIFT);
        bool repositioned = false;

        if (sx < 0) {
            sx = 0;
            bb->dir = 1;
            repositioned = true;
        } else if (sx > world_width - RAT_RAVEN_SIZE) {
            sx = world_width - RAT_RAVEN_SIZE;
            bb->dir = -1;
            repositioned = true;
        }

        if (!bb->flying && rat_raven_stay_on_ground(bb, &sx))
            repositioned = true;

        if (rat_raven_bounce_off_range(bb, &sx, towers_begin, towers_n))
            repositioned = true;
        else if (bb->flying && rat_raven_bounce_off_range(bb, &sx, platforms_begin, platforms_n))
            repositioned = true;

        bb->x_sub = repositioned ? SUBPIXELS(sx) : new_x_sub;
    }

    /* Horizontal: direct speed while held (no inertia), blocked by any
     * solid it would end up overlapping. Uses the raw overlap test, not
     * foggy_active (computed below after the power-up's expiry is
     * resolved for this tick); close enough for one tick either way, and
     * keeps this section independent of that ordering.                   */
    int32_t move_speed = in_foggy ? FOGGY_MOVE_SPEED : MOVE_SPEED;
    int32_t dx = 0;
    if (right_held) dx += move_speed;
    if (left_held)  dx -= move_speed;
    if (dx > 0)      player_facing = 1;
    else if (dx < 0) player_facing = -1;

    /* A drifting foggy zone carries the player along sideways, as long as
     * the player is not standing on real ground (ground, tower or
     * platform): while inside the zone, or resting on top of it. Added to
     * the player's own movement, so the solid collision below still stops
     * a carried player at a wall, and once the zone has moved on the player
     * is simply out of it and drops. The magic power-up does not stop it.  */
    if (!(grounded && !on_foggy_ledge)) {
        int cpx = (int)(player_x_sub >> SUBPIXEL_SHIFT);
        int cpy = (int)(player_y_sub >> SUBPIXEL_SHIFT);
        for (int i = 0; i < foggy_count; i++) {
            const solid_t *z = &FOGGY_ZONES[i];
            if (foggy_dx[i] == 0 || cpx + PLAYER_W <= z->x || cpx >= z->x + z->w)
                continue;
            bool inside = cpy + PLAYER_H > z->y && cpy < z->y + z->h;
            bool on_top = on_foggy_ledge && cpy + PLAYER_H == z->y;
            if (inside || on_top) {
                dx += foggy_dx[i];
                break;
            }
        }
    }

    int32_t new_x = player_x_sub + dx;
    if (new_x < 0) new_x = 0;
    if (new_x > SUBPIXELS(world_width - PLAYER_W)) new_x = SUBPIXELS(world_width - PLAYER_W);

    int px = (int)(new_x >> SUBPIXEL_SHIFT);
    int py = (int)(player_y_sub >> SUBPIXEL_SHIFT);  /* y not moved yet this tick */
    for (int i = 0; i < solids_count; i++) {
        const solid_t *s = &SOLIDS[i];
        if (px + PLAYER_W > s->x && px < s->x + s->w &&
            py + PLAYER_H > s->y && py < s->y + s->h) {
            if (dx > 0)      px = s->x - PLAYER_W;   /* moving right: stop at its left face */
            else if (dx < 0) px = s->x + s->w;       /* moving left: stop at its right face */
            new_x = SUBPIXELS(px);
        }
    }
    player_x_sub = new_x;

    /* Win: reaching the far right edge of the level. Checked here, right
     * after horizontal movement resolves, rather than waiting for the
     * vertical pass; it does not matter whether the player is grounded
     * or mid-jump when they hit it.                                       */
    if (px >= world_width - PLAYER_W) {
        player_won = true;
        return;
    }

    /* Power-up: expires on its own after POWERUP_DURATION_TICKS,
     * regardless of what the player is doing. Tick-based like the level
     * timer, so a pause does not eat into it.                            */
    if (powerup_boost_active && --powerup_ticks_remaining <= 0)
        powerup_boost_active = false;

    /* Sickness auto-cures after SICK_CURE_TICKS of survival, tick-based
     * so it pauses along with everything else (like the level timer): a
     * sick player who survives for 30 seconds becomes well again. A
     * pickup cures it instantly instead (see the pickup-collection block
     * below); either path also zeroes sick_ticks_remaining so a stale
     * partial countdown can never carry into the NEXT time the player
     * gets sick.                                                          */
    if (sick) {
        if (sick_ticks_remaining > 0)
            sick_ticks_remaining--;
        if (sick_ticks_remaining == 0)
            sick = false;
    }

    /* Pressing DOWN while resting on a foggy zone's landing ledge (see
     * on_foggy_ledge and the FOGGY_ZONES loop in the vertical resolve
     * below) steps the character back down into the zone instead of
     * doing nothing; the only other option there would otherwise be
     * jumping away. Treated as foggy-controlled for this whole tick,
     * same as actually overlapping the zone. Excludes the power-up (it
     * always overrides stickiness, giving a real jump instead of a weak
     * climb step) EXCEPT for actually escaping the ledge; see the
     * `on_foggy_ledge && down_press` exception on the FOGGY_ZONES[]
     * landing-ledge loop below, which lets DOWN step off the ledge even
     * while jewel-powered-up.                                              */
    /* Idle sink: with no input at all while in or on top of a foggy zone
     * (and not powered up), count idle ticks; past FOGGY_SINK_DELAY_TICKS
     * every FOGGY_SINK_INTERVAL_TICKS-th tick is a sink tick, which moves
     * the character down exactly one pixel. Any input, leaving the zone,
     * or the power-up resets the count. From the ledge a sink tick is
     * treated like a DOWN press so the character actually enters the zone. */
    bool idle = !left_held && !right_held && !jump_press && !down_press;
    if (idle && (in_foggy || on_foggy_ledge) && !powerup_boost_active)
        foggy_idle_ticks++;
    else
        foggy_idle_ticks = 0;
    bool sink_tick = foggy_idle_ticks >= FOGGY_SINK_DELAY_TICKS &&
                     (foggy_idle_ticks - FOGGY_SINK_DELAY_TICKS) % FOGGY_SINK_INTERVAL_TICKS == 0;

    bool foggy_active = (in_foggy || (on_foggy_ledge && (down_press || sink_tick))) && !powerup_boost_active;

    /* Foggy zone (unless boosted): no gravity at all. Vertical movement
     * only comes from UP/DOWN presses, each queuing a bounded
     * FOGGY_STEP_PX of distance (negative = climb, positive = descend)
     * that is worked off gradually at FOGGY_CLIMB_SPEED per tick below,
     * so a press reads as one small, slow, deliberate step rather than a
     * leap. With nothing queued, vertical speed is simply zero: no
     * sinking back. Landing on solid ground, exiting the zone, or the
     * power-up kicking in all clear the queue and return to normal
     * physics.                                                            */
    if (foggy_active) {
        if (jump_press)
            foggy_move_remaining -= SUBPIXELS(FOGGY_STEP_PX);
        if (down_press) {
            foggy_move_remaining += SUBPIXELS(FOGGY_STEP_PX);
            down_buffer_ticks = 0;   /* consumed; do not fire again over the rest of its buffer window */
        }

        if (foggy_move_remaining < 0) {
            int32_t step = (-foggy_move_remaining < FOGGY_CLIMB_SPEED)
                                ? -foggy_move_remaining : FOGGY_CLIMB_SPEED;
            player_vy = -step;
            foggy_move_remaining += step;
        } else if (foggy_move_remaining > 0) {
            int32_t step = (foggy_move_remaining < FOGGY_CLIMB_SPEED)
                                ? foggy_move_remaining : FOGGY_CLIMB_SPEED;
            player_vy = step;
            foggy_move_remaining -= step;
        } else {
            player_vy = sink_tick ? SUBPIXELS(1) : 0;
        }
    } else {
        foggy_move_remaining = 0;

        /* Jump liftoff: a ground jump if we were grounded at the start
         * of this tick, otherwise one of the MAX_AIR_JUMPS double-jump
         * charges if any are left; consumed here regardless so a jump
         * press with nothing left to spend is simply dropped, not
         * queued. See platformer_config.h for the launch velocities and
         * why the power-up replaces whichever one would otherwise fire.   */
        if (jump_press) {
            if (grounded) {
                player_vy = powerup_boost_active ? POWERUP_JUMP_VELOCITY : JUMP_VELOCITY;
                grounded = false;
            } else if (air_jumps_left > 0) {
                player_vy = powerup_boost_active ? POWERUP_JUMP_VELOCITY : AIR_JUMP_VELOCITY;
                used_air_jump = true;
                air_jumps_left--;
            }
        }

        /* Gravity: the same every tick, no "held" variant. Height comes
         * entirely from which launch velocity a jump started with, above. */
        player_vy += GRAVITY_ACCEL;
        if (player_vy > MAX_FALL_SPEED)
            player_vy = MAX_FALL_SPEED;
    }

    /* Vertical: move, then resolve against every solid the player
     * overlaps horizontally, landing on top of one or bonking the
     * underside of one from below. Side collision was already handled
     * above, in the horizontal pass. The screen's top edge is a hard
     * ceiling regardless of source: no jump or combination of jumps
     * (double jump, power-up, or both together) can send the character
     * above it.                                                          */
    int32_t old_y = player_y_sub;
    int32_t new_y = old_y + player_vy;
    if (new_y < 0) {
        new_y = 0;
        if (player_vy < 0)
            player_vy = 0;
    }

    int old_top = (int)(old_y >> SUBPIXEL_SHIFT);
    int old_bottom = old_top + PLAYER_H;
    px = (int)(player_x_sub >> SUBPIXEL_SHIFT);  /* x already resolved above */

    /* Rat_raven stomp: falling down through a rat_raven's top edge this tick
     * kills it (dropping off a ledge, or on the way down out of a jump,
     * either way just player_vy >= 0) and leaves the player untouched,
     * same idea as landing on a solid but for a hazard instead. Checked
     * against `new_y` before the SOLIDS[]/foggy-ledge landing clamps
     * below can shorten the fall, since the real ground sits right under
     * a ground-patrol rat_raven and would otherwise mask the crossing.
     * Each rat_raven's own `y` is used, not a shared constant, since flying
     * rat_ravens patrol at their own height, `y` between
     * FLYING_RAT_RAVEN_Y_MIN/MAX, rather than always RAT_RAVEN_Y. Every
     * stomp-kill also triggers the distinct impact sound.                 */
    if (player_vy >= 0) {
        int raw_new_bottom = (int)(new_y >> SUBPIXEL_SHIFT) + PLAYER_H;
        for (int i = 0; i < rat_ravens_count; i++) {
            rat_raven_t *bb = &RAT_RAVENS[i];
            if (!bb->alive)
                continue;
            if (!(old_bottom <= bb->y && raw_new_bottom >= bb->y))
                continue;
            int sx = (int)(bb->x_sub >> SUBPIXEL_SHIFT);
            if (px + PLAYER_W > sx && px < sx + RAT_RAVEN_SIZE) {
                bb->alive = false;
                score += STOMP_VALUE;
                sfx_play_impact();
            }
        }
    }

    grounded = false;
    on_foggy_ledge = false;

    for (int i = 0; i < solids_count; i++) {
        const solid_t *s = &SOLIDS[i];
        if (px + PLAYER_W <= s->x || px >= s->x + s->w)
            continue;   /* no horizontal overlap with this solid at all */

        int new_top = (int)(new_y >> SUBPIXEL_SHIFT);
        int new_bottom = new_top + PLAYER_H;

        if (player_vy >= 0 && old_bottom <= s->y && new_bottom >= s->y) {
            new_y = SUBPIXELS(s->y - PLAYER_H);   /* land on top */
            player_vy = 0;
            grounded = true;
            air_jumps_left = MAX_AIR_JUMPS;
            used_air_jump = false;
        } else if (player_vy < 0 && old_top >= s->y + s->h && new_top <= s->y + s->h) {
            new_y = SUBPIXELS(s->y + s->h);       /* bonk its underside */
            player_vy = 0;
        }
    }

    /* Foggy zone top edges: a one-way landing ledge, generic across
     * every entry in FOGGY_ZONES[] regardless of where it is placed
     * (not just this level's one instance). Only the "land on top" half
     * of the SOLIDS[] check above applies; there is deliberately no
     * "bonk the underside" case, so climbing up through a zone from
     * below never catches on its own top edge. It just glides through
     * and, once clear, immediately falls the short distance back onto it
     * under gravity (one tick, imperceptible) and is caught here exactly
     * like landing on real ground: grounded, double jump refilled, so a
     * regular jump (or another double jump) works right away. Walking
     * sideways off the zone's X range drops the character off the ledge
     * normally, same as walking off any platform's edge. Skipped while
     * foggy_active (this tick's vertical move is deliberate
     * foggy-controlled movement, not a normal fall; see on_foggy_ledge
     * above) OR while a DOWN press is stepping off the ledge even with
     * foggy_active false. This second case covers standing on top of a
     * foggy area while jewel-powered-up, which forces foggy_active
     * false; without it, the character rests exactly ON z->y, so
     * ordinary gravity's tiny per-tick move re-triggers this same "land
     * on top" crossing every tick and the character can never fall past
     * it.                                                                 */
    if (!foggy_active && !(on_foggy_ledge && down_press)) {
        for (int i = 0; i < foggy_count; i++) {
            const solid_t *z = &FOGGY_ZONES[i];
            if (px + PLAYER_W <= z->x || px >= z->x + z->w)
                continue;

            int new_bottom = (int)(new_y >> SUBPIXEL_SHIFT) + PLAYER_H;

            if (player_vy >= 0 && old_bottom <= z->y && new_bottom >= z->y) {
                new_y = SUBPIXELS(z->y - PLAYER_H);   /* land on the zone's top edge */
                player_vy = 0;
                grounded = true;
                on_foggy_ledge = true;
                air_jumps_left = MAX_AIR_JUMPS;
                used_air_jump = false;
            }
        }
    }

    player_y_sub = new_y;

    /* Jewel: walk across a gap instead of falling into it. A gap
     * is just an absence of any ground SOLIDS[] entry there, so the
     * collision loop above never catches a fall through one. While the
     * power-up is active, treat GROUND_TOP as a hard floor regardless:
     * if nothing already caught the fall this tick (grounded is still
     * false) and the player has sunk to or past ground level, snap to
     * stand on it exactly like a real landing (refills the double jump
     * too). Real ground still wins normally (the loop above runs first
     * and already sets grounded=true there), so this only ever fires
     * where there really was nothing solid underneath.                   */
    if (!grounded && powerup_boost_active) {
        int bottom = (int)(player_y_sub >> SUBPIXEL_SHIFT) + PLAYER_H;
        if (bottom >= GROUND_TOP) {
            player_y_sub = SUBPIXELS(GROUND_TOP - PLAYER_H);
            player_vy = 0;
            grounded = true;
            air_jumps_left = MAX_AIR_JUMPS;
            used_air_jump = false;
        }
    }

    /* Fell into a gap (or off the world some other way): there is no
     * ground under a gap, so a miss just keeps falling with nothing to
     * catch it. Once far enough below the visible screen to be an
     * unambiguous miss (not just a big drop), it is a death, same as the
     * timer running out. See player_died and show_death_screen().        */
    if ((int)(player_y_sub >> SUBPIXEL_SHIFT) > FALL_RESPAWN_Y) {
        player_died = true;
        death_reason = MSG_DEATH_FALL;
        return;
    }

    /* Pickups: a plain overlap test against every untaken PICKUPS[]
     * entry, not part of SOLIDS[] since none of them block movement.
     * Works identically wherever a pickup happens to sit, open air, on
     * a solid, or inside a foggy zone, since this does not care about
     * stickiness at all, just position.                                  */
    {
        int py2 = (int)(player_y_sub >> SUBPIXEL_SHIFT);
        for (int i = 0; i < pickups_count; i++) {
            pickup_t *p = &PICKUPS[i];
            if (p->taken)
                continue;
            if (px + PLAYER_W > p->x && px < p->x + PICKUP_SIZE &&
                py2 + PLAYER_H > p->y && py2 < p->y + PICKUP_SIZE) {
                p->taken = true;
                if (just_taken_count < MAX_PICKUPS)
                    just_taken[just_taken_count++] = (int16_t)i;

                if (p->type == PICK_COIN) {
                    /* A source of score; see `score`'s comment.
                     * Does not cure sickness.                              */
                    score += COIN_VALUE;
                    sfx_play_happy();
                } else {
                    /* Jewel boost: the ONLY pickup that cures sickness
                     * instantly (the 30-second auto-cure, SICK_CURE_TICKS,
                     * is the only other way). Also zero the auto-cure
                     * countdown so a stale partial one cannot carry into
                     * a LATER sickness episode.                            */
                    sick = false;
                    sick_ticks_remaining = 0;
                    powerup_boost_active = true;
                    powerup_ticks_remaining = POWERUP_DURATION_TICKS;
                    sfx_play_jewel();
                }
            }
        }
    }

    /* Rat_ravens vs player: a plain overlap test against every alive rat_raven (their own movement/collision already ran earlier this tick,
     * and any stomp-kill above already happened). Three outcomes,
     * checked in this order, and EVERY one of them uses up (removes) the
     * rat_raven that caused it, same as a stomp-kill:
     *   1. jewel active: a one-time shield. The character
     *      survives without getting sick, but the power-up itself ends
     *      immediately and the player is returned to just being a plain
     *      green block.
     *   2. already sick: a death (two hits, total, end a normal run).
     *   3. otherwise: the first hit, becomes sick (see `sick` above).
     * All three are gated by RAT_RAVEN_HIT_INVULN_TICKS since the last rat_raven touch of ANY kind, so an adjacent rat_raven occupying the same
     * tick or tile cannot also count as a separate, second touch. Only
     * the shield outcome also plays the distinct impact sound
     * (sfx_play_impact()); sickness and death keep the sad tone as the
     * dominant audio cue instead of layering another sound on top of it. */
    if (rat_raven_invuln_ticks > 0)
        rat_raven_invuln_ticks--;

    if (rat_raven_invuln_ticks == 0) {
        int py3 = (int)(player_y_sub >> SUBPIXEL_SHIFT);
        for (int i = 0; i < rat_ravens_count; i++) {
            rat_raven_t *bb = &RAT_RAVENS[i];
            if (!bb->alive)
                continue;
            int sx = (int)(bb->x_sub >> SUBPIXEL_SHIFT);
            if (px + PLAYER_W > sx && px < sx + RAT_RAVEN_SIZE &&
                py3 + PLAYER_H > bb->y && py3 < bb->y + RAT_RAVEN_SIZE) {
                if (powerup_boost_active) {
                    powerup_boost_active = false;
                    powerup_ticks_remaining = 0;
                    sfx_play_impact();
                } else if (sick) {
                    player_died = true;
                    death_reason = bb->flying ? MSG_DEATH_RAVEN : MSG_DEATH_RAT;
                    return;
                } else {
                    sick = true;
                    sick_ticks_remaining = SICK_CURE_TICKS;
                    sfx_play_sad();
                }
                bb->alive = false;
                rat_raven_invuln_ticks = RAT_RAVEN_HIT_INVULN_TICKS;
                break;
            }
        }
    }

    /* Sfx: advances the tick-driven sound-effect state machine once per
     * tick, same rate as everything else; see sfx_update()'s own
     * comment. Skipped on a tick that just returned early above (a death
     * or the win check earlier in this function), which is fine: nothing
     * needs one more tick of countdown on the exact frame the run ends.   */
    sfx_update();
}

/* ===================================================================== */
/*  Drawing                                                               */
/* ===================================================================== */

static void fill(uint16_t colour, int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH)  w = WIDTH  - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w > 0 && h > 0)
        lcd_solid_rectangle(colour, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

/* fill() takes SCREEN coordinates; everything else in this file (player
 * position, SOLIDS[]/FOGGY_ZONES[], the power-up) is in WORLD
 * coordinates and goes through this instead, translating by the camera.
 * Only X scrolls, since this is a horizontal-only camera, so Y passes
 * through unchanged. See camera_x and update_camera() below.             */
static void fill_world(uint16_t colour, int wx, int y, int w, int h)
{
    fill(colour, wx - camera_x, y, w, h);
}

/* Clear a text row and print a string centred on it. Centres against the
 * driver's actual column count (lcd_get_columns(), WIDTH/glyph-width),
 * which matters since the desktop build's WIDTH is 640, twice as many
 * columns as the PicoCalc's 320.                                        */
static void put_row(int row, const char *str, uint16_t fg)
{
    lcd_solid_rectangle(COL_BG, 0, (uint16_t)(row * GLYPH_HEIGHT), WIDTH, GLYPH_HEIGHT);
    int len = (int)strlen(str);
    int col = (lcd_get_columns() - len) / 2;
    if (col < 0) col = 0;
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* The driver's font.h only ships one size (the built-in 8x10 glyphs), and
 * there is no native way to draw bigger text, so this reads the same
 * font_8x10 bitmap the driver itself uses (it is already linked in and
 * exported by font.h; see picocalc-text-starter-main/drivers/font.h,
 * `.glyphs[]` is one 8-bit row per pixel row, MSB = leftmost column, 10
 * rows per glyph, indexed by ASCII code) and draws each "on" pixel as its
 * own `scale`x`scale` block via fill() (SCREEN coordinates, like
 * put_row(), since this is status-line/overlay text, not a world
 * object). Used for the level/time/score status lines and the bigger
 * death/win score display; see their own comments for why the call
 * frequency of each is fine performance-wise.                            */
static void draw_big_text(int x, int y, const char *str, int scale, uint16_t colour)
{
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        const uint8_t *glyph = &font_8x10.glyphs[(*p) * GLYPH_HEIGHT];
        for (int row = 0; row < GLYPH_HEIGHT; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < font_8x10.width; col++) {
                if (bits & (0x80 >> col))
                    fill(colour, x + col * scale, y + row * scale, scale, scale);
            }
        }
        x += font_8x10.width * scale;
    }
}

/* Axis-aligned rectangle intersection; false (outputs left untouched) if
 * they do not overlap at all.                                            */
static bool rect_intersect(int ax, int ay, int aw, int ah,
                           int bx, int by, int bw, int bh,
                           int *ox, int *oy, int *ow, int *oh)
{
    int x0 = ax > bx ? ax : bx;
    int y0 = ay > by ? ay : by;
    int x1 = (ax + aw) < (bx + bw) ? (ax + aw) : (bx + bw);
    int y1 = (ay + ah) < (by + bh) ? (ay + ah) : (by + bh);

    if (x1 <= x0 || y1 <= y0)
        return false;
    *ox = x0; *oy = y0; *ow = x1 - x0; *oh = y1 - y0;
    return true;
}

#if PICKUP_USE_SPRITES
static uint16_t pickup_pixel_colour(pickup_type_t type, char ch, uint16_t behind)
{
    bool coin = (type == PICK_COIN);
    switch (ch) {
    case 'o': return PICKUP_COL_COIN_EDGE;
    case 'y': return PICKUP_COL_COIN_BODY;
    case 'd': return PICKUP_COL_JEWEL_EDGE;
    case 'c': return PICKUP_COL_JEWEL_BODY;
    case 'w': return coin ? PICKUP_COL_COIN_SHINE : PICKUP_COL_JEWEL_SHINE;
    default:  return behind;
    }
}
#endif

/* Draws the part of pickup p that lies in the already-cleared rectangle
 * (ix, iy, iw, ih), given in world coordinates. As a picture, the pixels
 * that are not part of it are filled with what is behind it: the fog
 * colour if its centre is inside a foggy zone, else the background.      */
static void draw_pickup(const pickup_t *p, int ix, int iy, int iw, int ih)
{
#if PICKUP_USE_SPRITES
    static uint16_t tmp[PICKUP_ART_SIZE * PICKUP_ART_SIZE];
    const char *const *art = (p->type == PICK_COIN) ? COIN_ART : JEWEL_ART;
    uint16_t behind = COL_BG;
    int cx = p->x + PICKUP_SIZE / 2, cy = p->y + PICKUP_SIZE / 2;

    for (int i = 0; i < foggy_count; i++) {
        const solid_t *z = &FOGGY_ZONES[i];
        if (cx >= z->x && cx < z->x + z->w && cy >= z->y && cy < z->y + z->h) {
            behind = z->colour;
            break;
        }
    }
    for (int row = 0; row < ih; row++)
        for (int col = 0; col < iw; col++)
            tmp[row * iw + col] = pickup_pixel_colour(p->type,
                                                      art[iy - p->y + row][ix - p->x + col], behind);
    blit_clipped(tmp, ix, iy, iw, ih);
#else
    fill_world(pickup_colour(p->type), ix, iy, iw, ih);
#endif
}

/* Fills a world rectangle, clipped to the dirty rectangle (cx, cy, cw, ch)
 * being redrawn.                                                         */
static void fill_in(uint16_t colour, int wx, int y, int w, int h, int cx, int cy, int cw, int ch)
{
    int ox, oy, ow, oh;
    if (rect_intersect(wx, y, w, h, cx, cy, cw, ch, &ox, &oy, &ow, &oh))
        fill_world(colour, ox, oy, ow, oh);
}

#if FOG_TEXTURE
static uint32_t fog_hash(int cx, int cy, int seed)
{
    uint32_t h = (uint32_t)cx * 73856093u ^ (uint32_t)cy * 19349663u ^ (uint32_t)seed * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

/* Light and dark streaks over foggy zone `z` (number `zi`), clipped to
 * the rectangle (cx, cy, cw, ch) that has just been painted grey. Each
 * cell of the zone's own grid may hold one 2 px tall streak, its length,
 * offset and shade taken from a hash of the cell, so it is the same every
 * time it is redrawn.                                                    */
static void draw_fog_texture(const solid_t *z, int zi, int cx, int cy, int cw, int ch)
{
    int lx0 = (cx - z->x) / FOG_CELL_W - 1;
    int ly0 = (cy - z->y) / FOG_CELL_H - 1;
    if (lx0 < 0) lx0 = 0;
    if (ly0 < 0) ly0 = 0;
    int lx1 = (cx + cw - z->x) / FOG_CELL_W;
    int ly1 = (cy + ch - z->y) / FOG_CELL_H;

    for (int gy = ly0; gy <= ly1; gy++) {
        for (int gx = lx0; gx <= lx1; gx++) {
            uint32_t hv = fog_hash(gx, gy, zi);
            if (((hv >> 8) & 3) >= 2)
                continue;                               /* no streak in this cell */
            int sx = gx * FOG_CELL_W + (int)((hv >> 14) & 15);
            int sy = gy * FOG_CELL_H + (int)((hv >> 18) & 7);
            int sw = 8 + (int)((hv >> 10) & 15);
            int sh = 2;
            if (sx + sw > z->w) sw = z->w - sx;
            if (sy + sh > z->h) sh = z->h - sy;
            if (sw <= 0 || sh <= 0)
                continue;
            fill_in((hv & 1) ? COL_FOG_LIGHT : COL_FOG_DARK, z->x + sx, z->y + sy, sw, sh,
                    cx, cy, cw, ch);
        }
    }
}
#endif

#if SOLID_TEXTURES

/* Surface detail drawn over a solid's plain fill, only inside the dirty
 * rectangle (cx, cy, cw, ch). The patterns are anchored to the solid's own
 * position, not the screen, so they stay put when the camera moves.       */
static void draw_solid_detail(const solid_t *s, int kind, int cx, int cy, int cw, int ch)
{
    uint16_t light, dark;

    if (kind == SOLID_GROUND) {
        fill_in(COL_GRASS, s->x, s->y, s->w, GRASS_H, cx, cy, cw, ch);
        fill_in(COL_GRASS_LIGHT, s->x, s->y, s->w, 1, cx, cy, cw, ch);
        fill_in(COL_GROUND_DARK, s->x, s->y + s->h - 4, s->w, 4, cx, cy, cw, ch);
        return;
    }

    if (kind == SOLID_TOWER) {
        light = COL_TOWER_LIGHT;
        dark  = COL_TOWER_DARK;
        /* stone courses: a mortar line every 8 px down from the top */
        for (int yy = s->y + 8; yy < s->y + s->h - 2; yy += 8)
            fill_in(COL_MORTAR, s->x, yy, s->w, 1, cx, cy, cw, ch);
        /* windows, lit or dark by a fixed pattern of the tower's position */
        for (int j = 0, wy = s->y + 14; wy + 5 < s->y + s->h - 8; j++, wy += 20)
            for (int k = 0, wx = s->x + 6; wx + 3 < s->x + s->w - 5; k++, wx += 10)
                fill_in(((k * 7 + j * 13 + s->x / 8) % 3 == 0) ? COL_WINDOW_LIT : COL_WINDOW_DARK,
                        wx, wy, 3, 5, cx, cy, cw, ch);
        /* a cap with notches, like battlements */
        fill_in(COL_TOWER_LIGHT, s->x, s->y, s->w, 3, cx, cy, cw, ch);
        for (int nx = s->x + 4; nx + 1 < s->x + s->w; nx += 8)
            fill_in(COL_MORTAR, nx, s->y + 1, 1, 2, cx, cy, cw, ch);
    } else {
        light = COL_PLATFORM_LIGHT;
        dark  = COL_PLATFORM_DARK;
        /* plank gaps every 16 px */
        for (int px = s->x + 16; px < s->x + s->w; px += 16)
            fill_in(COL_PLANK_GAP, px, s->y, 1, s->h, cx, cy, cw, ch);
    }

    /* bevel: light on the top and left, dark on the bottom and right */
    fill_in(light, s->x, s->y, s->w, 1, cx, cy, cw, ch);
    fill_in(light, s->x, s->y, 1, s->h, cx, cy, cw, ch);
    fill_in(dark, s->x, s->y + s->h - 1, s->w, 1, cx, cy, cw, ch);
    fill_in(dark, s->x + s->w - 1, s->y, 1, s->h, cx, cy, cw, ch);
}
#endif

/* Repaint a WORLD-space rectangle against the known static background:
 * background colour everywhere, then whichever parts of it fall inside
 * any SOLIDS[]/FOGGY_ZONES[]/PICKUPS[] entry get that entry's colour
 * painted back on top, all via fill_world(), so this works the same
 * whether it is a small patch (erasing a sprite's old footprint) or the
 * whole visible viewport (a camera move, or the very first frame; see
 * render_frame()). Sprites (rat_ravens, the player) are not part of the
 * background and are drawn separately, afterwards. If the rectangle
 * reaches up into the status-line band, it notes which rows it painted
 * over (status_dirty_y0/y1) so those lines get redrawn.                   */
static void redraw_region(int x, int y, int w, int h)
{
    int ix, iy, iw, ih;

    fill_world(COL_BG, x, y, w, h);

    if (y < PLAYFIELD_TOP_Y) {
        if (y < status_dirty_y0) status_dirty_y0 = y;
        if (y + h > status_dirty_y1) status_dirty_y1 = y + h;
    }

    for (int i = 0; i < solids_count; i++) {
        const solid_t *s = &SOLIDS[i];
        if (rect_intersect(x, y, w, h, s->x, s->y, s->w, s->h, &ix, &iy, &iw, &ih)) {
            fill_world(s->colour, ix, iy, iw, ih);
#if SOLID_TEXTURES
            int kind = (i >= towers_begin && i < towers_begin + towers_n) ? SOLID_TOWER
                     : (i >= platforms_begin && i < platforms_begin + platforms_n) ? SOLID_PLATFORM
                     : SOLID_GROUND;
            draw_solid_detail(s, kind, ix, iy, iw, ih);
#endif
        }
    }
    for (int i = 0; i < foggy_count; i++) {
        const solid_t *z = &FOGGY_ZONES[i];
        if (rect_intersect(x, y, w, h, z->x, z->y, z->w, z->h, &ix, &iy, &iw, &ih)) {
            fill_world(z->colour, ix, iy, iw, ih);
#if FOG_TEXTURE
            draw_fog_texture(z, i, ix, iy, iw, ih);
#endif
        }
    }
    for (int i = 0; i < pickups_count; i++) {
        const pickup_t *p = &PICKUPS[i];
        if (!p->taken &&
            rect_intersect(x, y, w, h, p->x, p->y, PICKUP_SIZE, PICKUP_SIZE, &ix, &iy, &iw, &ih))
            draw_pickup(p, ix, iy, iw, ih);
    }
}

/* Scales an RGB565 colour from black (num = 0) up to itself (num = den),
 * one channel at a time.                                                 */
static uint16_t fade_up_colour(uint16_t c, int32_t num, int32_t den)
{
    int32_t r = ((c >> 11) & 0x1F) * num / den;
    int32_t g = ((c >> 5) & 0x3F) * num / den;
    int32_t b = (c & 0x1F) * num / den;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* The player's FILL colour is a minor visible cue for two states: sick
 * from a rat_raven hit (highest priority, a real hazard, not cosmetic),
 * and a double jump still being "spent" for this trip (until landing).
 * The jewel does not recolour the character at all; when it is
 * active, the character is outlined in white instead. See
 * draw_player_shape(), which adds that outline as a SEPARATE layer on
 * top of whichever fill colour this returns.
 * While sick the fill starts black and fades up to whatever the normal
 * colour would be, in step with the cure timer (sick_ticks_remaining
 * counting down from SICK_CURE_TICKS), so it is fully back to normal at
 * the moment sickness ends; changing SICK_CURE_TICKS changes the fade
 * time with it. The white outline (draw_player_shape()) stays.           */
static uint16_t current_player_colour(void)
{
    uint16_t normal = used_air_jump ? COL_PLAYER_AIRJUMP : COL_PLAYER;
    if (!sick)
        return normal;
    int32_t elapsed = SICK_CURE_TICKS - sick_ticks_remaining;
    if (elapsed < 0) elapsed = 0;
    if (elapsed > SICK_CURE_TICKS) elapsed = SICK_CURE_TICKS;
    return fade_up_colour(normal, elapsed, SICK_CURE_TICKS);
}

/* Blits a w x h block of RGB565 pixels whose top-left is at WORLD
 * position (wx, y), clipped to the screen (one row at a time when it is
 * only partly on screen).                                                */
static void blit_clipped(const uint16_t *pix, int wx, int y, int w, int h)
{
    int sx = wx - camera_x;
    if (sx + w <= 0 || sx >= WIDTH || y + h <= 0 || y >= HEIGHT)
        return;   /* nothing of it is on screen */

    if (sx >= 0 && sx + w <= WIDTH && y >= 0 && y + h <= HEIGHT) {
        lcd_blit(pix, (uint16_t)sx, (uint16_t)y, (uint16_t)w, (uint16_t)h);
        return;
    }

    int skip = sx < 0 ? -sx : 0;
    int cw = w - skip;
    if (sx + w > WIDTH)
        cw = WIDTH - sx - skip;
    for (int row = 0; row < h; row++) {
        int ry = y + row;
        if (ry >= 0 && ry < HEIGHT && cw > 0)
            lcd_blit(&pix[row * w + skip], (uint16_t)(sx + skip), (uint16_t)ry, (uint16_t)cw, 1);
    }
}

#if PLAYER_USE_SPRITE

static void draw_player_sprite(int x, int y, uint16_t body)
{
    uint16_t shade = fade_up_colour(body, 3, 5);
    int border = powerup_boost_active ? PLAYER_POWERUP_BORDER_PX : (sick ? 1 : 0);

    for (int row = 0; row < PLAYER_H; row++) {
        for (int col = 0; col < PLAYER_W; col++) {
            int sc = player_facing < 0 ? PLAYER_W - 1 - col : col;
            uint16_t c;
            switch (PLAYER_ART[row][sc]) {
            case 'D': c = shade;    break;
            case 'w': c = COL_WHITE; break;
            case 'k': c = COL_BG;   break;
            default:  c = body;     break;
            }
            if (col < border || col >= PLAYER_W - border ||
                row < border || row >= PLAYER_H - border)
                c = COL_WHITE;
            player_pix[row * PLAYER_W + col] = c;
        }
    }
    blit_clipped(player_pix, x, y, PLAYER_W, PLAYER_H);
}
#endif

/* Draws the player as a plain fill, or, while sick or while the jewel
 * power-up is active, outlined in white first (same reason both times:
 * sick's fill is solid black, COL_BG, so a bare fill would be invisible
 * against the background; the power-up's outline is purely a cue, not a
 * visibility fix, since its fill colour is never black). The two
 * outlines are different thicknesses (PLAYER_POWERUP_BORDER_PX), but are
 * mutually exclusive in practice anyway (a rat_raven hit while powered up
 * shields instead of causing sick, see step_physics()), so which check
 * comes first does not matter here. Either way every pixel of the
 * PLAYER_W x PLAYER_H box is painted (no transparency), which is what
 * lets erase_player_if_moved() erase only the OLD footprint.            */
static void draw_player_shape(int x, int y, uint16_t colour)
{
#if PLAYER_USE_SPRITE
    draw_player_sprite(x, y, colour);
#else
    if (powerup_boost_active) {
        fill_world(COL_WHITE, x, y, PLAYER_W, PLAYER_H);
        fill_world(colour, x + PLAYER_POWERUP_BORDER_PX, y + PLAYER_POWERUP_BORDER_PX,
                   PLAYER_W - 2 * PLAYER_POWERUP_BORDER_PX, PLAYER_H - 2 * PLAYER_POWERUP_BORDER_PX);
    } else if (sick) {
        fill_world(COL_WHITE, x, y, PLAYER_W, PLAYER_H);
        fill_world(colour, x + 1, y + 1, PLAYER_W - 2, PLAYER_H - 2);
    } else {
        fill_world(colour, x, y, PLAYER_W, PLAYER_H);
    }
#endif
}

/* Erase pass (see render_frame()): if the player moved since it was last
 * painted, repaint its OLD footprint against the background. Only valid
 * while the camera has not moved (so WORLD-to-screen mapping is stable);
 * render_frame() repaints the whole viewport instead when it has.       */
static void erase_player_if_moved(void)
{
    int nx = (int)(player_x_sub >> SUBPIXEL_SHIFT);
    int ny = (int)(player_y_sub >> SUBPIXEL_SHIFT);

    if (nx != drawn_x || ny != drawn_y)
        redraw_region(drawn_x, drawn_y, PLAYER_W, PLAYER_H);
}

/* Draw pass: paints the player at its current WORLD position and colour,
 * every frame, whether or not it moved, since an erase earlier in the
 * same frame (a rat_raven's, a drifting foggy zone's, a collected
 * pickup's) may have painted over part of it. Always the last thing
 * drawn, so it is always on top.                                         */
static void draw_player(void)
{
    drawn_x = (int)(player_x_sub >> SUBPIXEL_SHIFT);
    drawn_y = (int)(player_y_sub >> SUBPIXEL_SHIFT);
    drawn_colour = current_player_colour();
    draw_player_shape(drawn_x, drawn_y, drawn_colour);
}

#if RAT_RAVEN_USE_SPRITES

static uint16_t sprite_colour(char ch)
{
    switch (ch) {
    case 'g': return SPRITE_COL_RAT_BODY;
    case 'd': return SPRITE_COL_RAT_DARK;
    case 'p': return SPRITE_COL_PINK;
    case 'e': return SPRITE_COL_EYE;
    case 'b': return SPRITE_COL_RAVEN_BODY;
    case 'n': return SPRITE_COL_RAVEN_WING;
    case 'w': return SPRITE_COL_RAVEN_BELLY;
    case 'k': return SPRITE_COL_BEAK;
    default:  return COL_BG;
    }
}

static void build_sprites(void)
{
    for (int row = 0; row < SPRITE_SIZE; row++) {
        for (int col = 0; col < SPRITE_SIZE; col++) {
            int i = row * SPRITE_SIZE + col;
            int mirrored = row * SPRITE_SIZE + (SPRITE_SIZE - 1 - col);
            rat_pix[0][i] = sprite_colour(RAT_ART[row][col]);
            rat_pix[1][mirrored] = rat_pix[0][i];
            raven_pix[0][0][i] = sprite_colour(RAVEN_ART_A[row][col]);
            raven_pix[1][0][i] = sprite_colour(RAVEN_ART_B[row][col]);
            raven_pix[0][1][mirrored] = raven_pix[0][0][i];
            raven_pix[1][1][mirrored] = raven_pix[1][0][i];
        }
    }
    sprites_ready = true;
}

static void draw_rat_raven_sprite(int x, int y, const rat_raven_t *bb)
{
    if (!sprites_ready)
        build_sprites();

    /* the raven's wing frame changes as it travels; both face where they go */
    const uint16_t *pix = bb->flying ? raven_pix[(x >> 3) & 1][bb->dir < 0 ? 1 : 0]
                                     : rat_pix[bb->dir < 0 ? 1 : 0];
    blit_clipped(pix, x, y, SPRITE_SIZE, SPRITE_SIZE);
}
#endif

static void draw_rat_raven_shape(int x, int y, const rat_raven_t *bb)
{
#if RAT_RAVEN_USE_SPRITES
    draw_rat_raven_sprite(x, y, bb);
#else
    (void)bb;
    fill_world(COL_WHITE, x, y, RAT_RAVEN_SIZE, RAT_RAVEN_SIZE);
    fill_world(COL_BG, x + 1, y + 1, RAT_RAVEN_SIZE - 2, RAT_RAVEN_SIZE - 2);
#endif
}

/* Erase pass (see render_frame()): repaints the old footprint of every
 * rat_raven that moved or died since it was last painted. Runs for ALL
 * rat_ravens before draw_rat_ravens() draws ANY of them, so one
 * rat_raven's erase can never bite into another already drawn this frame
 * (two crossing each other used to flicker that way). A flying
 * rat_raven's y can change, so its last-drawn y (`drawn_y`) is tracked
 * like `drawn_x`.                                                        */
static void erase_rat_ravens_if_moved(void)
{
    for (int i = 0; i < rat_ravens_count; i++) {
        const rat_raven_t *bb = &RAT_RAVENS[i];
        int sx = (int)(bb->x_sub >> SUBPIXEL_SHIFT);

        if (bb->drawn_visible && (!bb->alive || sx != bb->drawn_x || bb->y != bb->drawn_y))
            redraw_region(bb->drawn_x, bb->drawn_y, RAT_RAVEN_SIZE, RAT_RAVEN_SIZE);
    }
}

/* Draw pass: paints every currently-alive rat_raven, every frame, moved
 * or not (an erase earlier this frame may have painted over part of
 * one), and records where each was drawn. Also used as-is after a full
 * viewport repaint.                                                      */
static void draw_rat_ravens(void)
{
    for (int i = 0; i < rat_ravens_count; i++) {
        rat_raven_t *bb = &RAT_RAVENS[i];
        if (!bb->alive) {
            bb->drawn_visible = false;
            continue;
        }
        int sx = (int)(bb->x_sub >> SUBPIXEL_SHIFT);
        draw_rat_raven_shape(sx, bb->y, bb);
        bb->drawn_x = (int16_t)sx;
        bb->drawn_y = bb->y;
        bb->drawn_visible = true;
    }
}

/* ===================================================================== */
/*  Camera                                                                */
/* ===================================================================== */

/* Horizontal-only, dead-zone follow: the character can move freely
 * between CAMERA_DEADZONE_LEFT and CAMERA_DEADZONE_RIGHT (screen
 * pixels) without the camera moving at all; stepping past either edge
 * snaps the camera to bring them back to that edge. An instant snap, not
 * smoothed; see the comment on those constants in platformer_config.h.
 * Clamped so the camera never shows past either end of the level
 * (world_width, picked per level by generate_level()).                  */
static void update_camera(void)
{
    int world_x = (int)(player_x_sub >> SUBPIXEL_SHIFT);
    int screen_x = world_x - camera_x;

    if (screen_x < CAMERA_DEADZONE_LEFT)
        camera_x = world_x - CAMERA_DEADZONE_LEFT;
    else if (screen_x > CAMERA_DEADZONE_RIGHT)
        camera_x = world_x - CAMERA_DEADZONE_RIGHT;

    if (camera_x < 0)
        camera_x = 0;
    if (camera_x > world_width - WIDTH)
        camera_x = world_width - WIDTH;
}

/* One frame's worth of drawing, called once after however many physics
 * ticks just ran. Two passes: ERASE (repaint the background wherever
 * something moved away from), then DRAW (status lines, rat_ravens, the
 * player last, so on top). If the camera moved this frame, everything on
 * screen shifted, so the erase pass is replaced by a repaint of the
 * whole new viewport. The status lines are drawn before the sprites so
 * that, on the rare jump high enough to reach up among them, the player
 * is painted over the text rather than blanked out by it.               */
static void render_frame(void)
{
    update_camera();

    if (camera_x != drawn_camera_x) {
        /* A scroll repaint skips the band above PLAYFIELD_TOP_Y, where only
         * the status lines live, so they do not blank and flash each time
         * the camera moves. The whole screen is still repainted for a fresh
         * start (drawn_camera_x < 0), or when the player is up in that band
         * (a very high jump), since then it has to be cleaned up too; that
         * repaint marks every status line for a redraw (redraw_region()). */
        int player_top = (int)(player_y_sub >> SUBPIXEL_SHIFT);
        if (drawn_camera_x < 0 || player_top < PLAYFIELD_TOP_Y || drawn_y < PLAYFIELD_TOP_Y)
            redraw_region(camera_x, 0, WIDTH, HEIGHT);
        else
            redraw_region(camera_x, PLAYFIELD_TOP_Y, WIDTH, HEIGHT - PLAYFIELD_TOP_Y);
        for (int i = 0; i < foggy_count; i++)
            foggy_drawn_x[i] = FOGGY_ZONES[i].x;
        drawn_camera_x = camera_x;
    } else {
        /* A foggy zone that drifted since it was last painted: repaint the
         * whole area it covered before and after, since its texture (and
         * any pickup riding in it) moves with it.                           */
        for (int i = 0; i < foggy_count; i++) {
            const solid_t *z = &FOGGY_ZONES[i];
            if (z->x == foggy_drawn_x[i])
                continue;
            int lo = z->x < foggy_drawn_x[i] ? z->x : foggy_drawn_x[i];
            int d = z->x < foggy_drawn_x[i] ? foggy_drawn_x[i] - z->x : z->x - foggy_drawn_x[i];
            redraw_region(lo, z->y, z->w + d, z->h);
            foggy_drawn_x[i] = z->x;
        }
        /* Erase any pickup(s) collected this frame; each gets its own
         * small redraw since a taken icon's footprint is not always
         * fully covered by the player's own erase.                         */
        for (int i = 0; i < just_taken_count; i++) {
            const pickup_t *p = &PICKUPS[just_taken[i]];
            redraw_region(p->x, p->y, PICKUP_SIZE, PICKUP_SIZE);
        }
        erase_rat_ravens_if_moved();
        erase_player_if_moved();
    }
    just_taken_count = 0;

    draw_status_lines();
    draw_rat_ravens();
    draw_player();
#if ENABLE_DEBUG_READOUT
    draw_debug_readout();
#endif
}

/* Which message the fourth status line shows. State messages take
 * priority, most urgent first: sick, then jewel active, then
 * standing in a foggy zone; when none applies it falls back to the
 * level's own hint (or nothing).                                        */
static const char *level_message(void)
{
    if (sick)
        return MSG_SICK;
    if (powerup_boost_active)
        return MSG_JEWEL;
    if (player_in_foggy((int)(player_x_sub >> SUBPIXEL_SHIFT),
                        (int)(player_y_sub >> SUBPIXEL_SHIFT)))
        return MSG_IN_FOG;
    return rules.message;
}

/* Does the status line on screen rows y..y+h-1 need redrawing even if
 * its text is unchanged: forced, or painted over by redraw_region()
 * since it was last drawn (see status_dirty_y0/y1).                      */
static bool status_line_stale(int y, int h)
{
    return status_force_redraw || (y < status_dirty_y1 && y + h > status_dirty_y0);
}

/* Always-on, near the top of the screen: level and the MM:SS countdown
 * on one line, score right below it on a second, both bigger than the
 * driver's normal 8x10 text via draw_big_text() at STATUS_SCALE, the
 * level's best-ever winning score on a third line at normal size
 * (scale 1), and the message line (level_message()) on a fourth. The
 * debug readout (row 0) stays literally at the top; these sit just below
 * it. Each line only actually redraws when its own text changed, or when
 * status_line_stale() says something painted over it: the timer changes
 * once a second and level/score/high-score change far less often than
 * that, so skipping an unchanged redraw avoids repainting a dozen-plus
 * scaled glyphs on every single physics tick for no visible difference.
 * STATUS_SCALE/STATUS_LEVEL_TIME_Y/STATUS_SCORE_Y/STATUS_HIGHSCORE_Y/
 * STATUS_MESSAGE_Y are in platformer_config.h.                           */
static void draw_status_lines(void)
{
    static char drawn_level_time_buf[32];
    static char drawn_score_buf[24];
    static char drawn_highscore_buf[32];
    static char drawn_message_buf[40];
    char level_time_buf[32], score_buf[24], highscore_buf[32], message_buf[40];
    int total_s = timer_remaining_ms / 1000;
    int mm = total_s / 60;
    int ss = total_s % 60;

    snprintf(level_time_buf, sizeof level_time_buf, "LEVEL:%d TIME:%02d:%02d", level_num, mm, ss);
    snprintf(score_buf, sizeof score_buf, "SCORE:%d", (int)score);
    snprintf(highscore_buf, sizeof highscore_buf, "LEVEL HIGH SCORE:%d", (int)level_high_score);

    if (status_line_stale(STATUS_LEVEL_TIME_Y, GLYPH_HEIGHT * STATUS_SCALE) ||
        strcmp(level_time_buf, drawn_level_time_buf) != 0) {
        fill(COL_BG, 0, STATUS_LEVEL_TIME_Y, WIDTH, GLYPH_HEIGHT * STATUS_SCALE);
        draw_big_text(8, STATUS_LEVEL_TIME_Y, level_time_buf, STATUS_SCALE, COL_WHITE);
        strcpy(drawn_level_time_buf, level_time_buf);
    }
    if (status_line_stale(STATUS_SCORE_Y, GLYPH_HEIGHT * STATUS_SCALE) ||
        strcmp(score_buf, drawn_score_buf) != 0) {
        fill(COL_BG, 0, STATUS_SCORE_Y, WIDTH, GLYPH_HEIGHT * STATUS_SCALE);
        draw_big_text(8, STATUS_SCORE_Y, score_buf, STATUS_SCALE, COL_WHITE);
        strcpy(drawn_score_buf, score_buf);
    }
    if (status_line_stale(STATUS_HIGHSCORE_Y, GLYPH_HEIGHT) ||
        strcmp(highscore_buf, drawn_highscore_buf) != 0) {
        fill(COL_BG, 0, STATUS_HIGHSCORE_Y, WIDTH, GLYPH_HEIGHT);
        draw_big_text(8, STATUS_HIGHSCORE_Y, highscore_buf, 1, COL_WHITE);
        strcpy(drawn_highscore_buf, highscore_buf);
    }
    snprintf(message_buf, sizeof message_buf, "%s", level_message());
    if (status_line_stale(STATUS_MESSAGE_Y, GLYPH_HEIGHT) ||
        strcmp(message_buf, drawn_message_buf) != 0) {
        fill(COL_BG, 0, STATUS_MESSAGE_Y, WIDTH, GLYPH_HEIGHT);
        draw_big_text(8, STATUS_MESSAGE_Y, message_buf, 1, COL_MESSAGE);
        strcpy(drawn_message_buf, message_buf);
    }
    status_force_redraw = false;
    status_dirty_y0 = PLAYFIELD_TOP_Y;   /* empty again */
    status_dirty_y1 = 0;
}

#if ENABLE_DEBUG_READOUT
static void draw_debug_readout(void)
{
    char buf[64];  /* generous: gcc can't prove the %d fields stay small */
    int pwr_secs = 0;
    if (powerup_boost_active)
        pwr_secs = (int)((powerup_ticks_remaining * MS_PER_TICK + 999) / 1000);
    snprintf(buf, sizeof buf, "X:%4d Y:%3d VY:%4d G:%d AJ:%d PWR:%2d",
             drawn_x, drawn_y, (int)(player_vy >> SUBPIXEL_SHIFT), grounded ? 1 : 0,
             air_jumps_left, pwr_secs);
    put_row(0, buf, COL_LABEL);
}
#endif

/* Waits `ms` with the keyboard buffer continuously drained the whole
 * time, not just once, so a still-held or mashed key cannot skip past a
 * screen that is meant to stay up at least that long. Also pumps
 * sfx_update(): physics (and its usual sfx_update() call) is not ticking
 * here, and a death's sad tone still needs to stop on time rather than
 * ringing for the whole pause.                                           */
static void pause_draining_keys(uint32_t ms)
{
    drain_keys();
    uint32_t until = to_ms_since_boot(get_absolute_time()) + ms;
    while (to_ms_since_boot(get_absolute_time()) < until) {
        drain_keys();
        sfx_update();
        sleep_ms(KEY_DRAIN_POLL_MS);
    }
}

/* The death screen's tail: at least END_SCREEN_PAUSE_MS with keys
 * drained (pause_draining_keys()), then a genuinely fresh keypress to
 * continue (wait_any_key(), which itself starts with one more
 * drain_keys(), redundant here but harmless, and keeps that function
 * correct on its own for its other caller, the splash screen). Returns
 * true if that key was Q or ESC.                                         */
static bool end_screen_pause_and_wait(void)
{
    put_row(ROW_END_PROMPT, "PRESS ANY KEY TO CONTINUE", COL_WHITE);
    pause_draining_keys(END_SCREEN_PAUSE_MS);
    return is_quit_key(wait_any_key());
}

/* death_reason (set alongside player_died=true in step_physics(), see its
 * own comment) is always non-NULL by the time this runs; every path
 * that sets player_died sets it in the same breath. No time is shown at
 * all: the status line already shows the (unpenalized, still-live) score
 * at the top the whole time, so death has no reason to repeat it.        */
static bool show_death_screen(void)
{
    put_row(ROW_END_TITLE, "YOU DIED", COL_YELLOW);
    put_row(ROW_END_REASON, death_reason, COL_WHITE);

    /* The level message line is replaced by the quit hint while this
     * screen is up; status_force_redraw brings the normal message back
     * on the next status redraw.                                          */
    fill(COL_BG, 0, STATUS_MESSAGE_Y, WIDTH, GLYPH_HEIGHT);
    draw_big_text(8, STATUS_MESSAGE_Y, MSG_DEATH, 1, COL_MESSAGE);
    status_force_redraw = true;

    return end_screen_pause_and_wait();
}

/* Win screen pieces: the big running score, and the smaller line under
 * it (TIME BONUS / MISSED COINS), each cleared and redrawn centred.      */
static void win_draw_score(void)
{
    char buf[32];
    snprintf(buf, sizeof buf, "SCORE:%d", (int)score);
    fill(COL_BG, 0, WIN_SCORE_Y, WIDTH, GLYPH_HEIGHT * WIN_SCORE_SCALE);
    draw_big_text((WIDTH - (int)strlen(buf) * font_8x10.width * WIN_SCORE_SCALE) / 2,
                  WIN_SCORE_Y, buf, WIN_SCORE_SCALE, COL_WHITE);
}

static void win_draw_line(const char *text)
{
    fill(COL_BG, 0, WIN_LINE_Y, WIDTH, GLYPH_HEIGHT * WIN_LINE_SCALE);
    draw_big_text((WIDTH - (int)strlen(text) * font_8x10.width * WIN_LINE_SCALE) / 2,
                  WIN_LINE_Y, text, WIN_LINE_SCALE, COL_WHITE);
}

/* One step of a win-screen countdown: a short beep every
 * SCORE_COUNTDOWN_BEEP_EVERY steps, otherwise a plain
 * SCORE_COUNTDOWN_TICK_MS wait.                                          */
static void win_countdown_wait(int tick)
{
    if (tick % SCORE_COUNTDOWN_BEEP_EVERY == 0)
        audio_play_sound_blocking(SFX_COUNTDOWN_FREQ, SFX_COUNTDOWN_FREQ, SFX_COUNTDOWN_MS);
    else
        sleep_ms(SCORE_COUNTDOWN_TICK_MS);
}

/* Reaching the far right edge of the level. The caller (run_demo())
 * generates a new level afterward instead of retrying this one, unless
 * this function returns true (the player pressed ENTER to retry the
 * SAME level instead, to try to beat level_high_score); see win_level()
 * and start_run(), one of which runs right after this returns and
 * resets `score` to 0 either way. The countdowns below are REAL, not
 * just a visual effect: they settle what `score` actually is at the
 * moment it gets compared against level_high_score, just below.
 *
 * Shows the score and the time left on the clock in seconds (what the
 * TIME: status line was showing, as "TIME BONUS: <N>s"), pauses for
 * WIN_STEP_PAUSE_MS, then quickly turns those seconds into a bonus: the
 * seconds shown tick down while the score ticks up by
 * TIME_BONUS_PER_SECOND per second, one second per
 * SCORE_COUNTDOWN_TICK_MS (a whole 120-second level's worth counts down
 * in a few real seconds), with a short beep (SFX_COUNTDOWN_MS) every
 * SCORE_COUNTDOWN_BEEP_EVERY steps. The general idea is that a faster
 * win leaves more time on the clock and so earns a bigger bonus. Then
 * the same again for any gold coins left uncollected
 * (MISSED_COIN_PENALTY each, score floored at 0), then the win song.
 * Once the score is final, level_high_score is updated if it was beaten,
 * and the player is offered a choice: press ENTER to retry this same
 * level layout and try to beat it, or press any other key to move on to
 * a genuinely new level as usual.                                        */
static bool show_win_screen(void)
{
    char buf[32];
    int remaining = timer_remaining_ms / 1000;   /* the same seconds the TIME: status line shows */

    sfx_silence();

    /* The "SCORE:" line in the second status line near the top of the
     * screen is blanked on a win, since it would otherwise sit there
     * showing the pre-bonus score while the countdown below animates
     * a DIFFERENT, increasing score in the middle of the screen, which
     * reads as two conflicting numbers. Blanked once here rather than
     * redrawn each tick; nothing else touches this row for the rest of
     * the win sequence, and the next level's first render_frame()
     * (win_level(), right after this function returns) redraws it fresh
     * regardless.                                                        */
    fill(COL_BG, 0, STATUS_SCORE_Y, WIDTH, GLYPH_HEIGHT * STATUS_SCALE);

    put_row(ROW_END_TITLE, "YOU WIN!", COL_CYAN);
    win_draw_score();
    snprintf(buf, sizeof buf, "TIME BONUS: %ds", remaining);
    win_draw_line(buf);
    pause_draining_keys(WIN_STEP_PAUSE_MS);

    for (int tick = 1; remaining > 0; tick++) {
        remaining--;
        score += TIME_BONUS_PER_SECOND;
        win_draw_score();
        snprintf(buf, sizeof buf, "TIME BONUS: %ds", remaining);
        win_draw_line(buf);
        win_countdown_wait(tick);
    }

    /* Once the countdown finishes, the time bonus line is erased; it
     * would otherwise just sit there showing "0s". The score readout
     * above it stays, since that ends at a real final number.           */
    fill(COL_BG, 0, WIN_LINE_Y, WIDTH, GLYPH_HEIGHT * WIN_LINE_SCALE);

    /* Missed coin penalty: one MISSED_COIN_PENALTY off the score for
     * every gold coin pickup left uncollected, counted down the same way as
     * the time bonus above (a smaller line ticking down to 0 while the
     * score drops, floored at 0), then erased.                            */
    int missed = 0;
    for (int i = 0; i < pickups_count; i++)
        if (PICKUPS[i].type == PICK_COIN && !PICKUPS[i].taken)
            missed++;

    if (missed > 0) {
        snprintf(buf, sizeof buf, "MISSED COINS: %d", missed);
        win_draw_line(buf);
        pause_draining_keys(WIN_STEP_PAUSE_MS);

        for (int tick = 1; missed > 0; tick++) {
            missed--;
            score -= MISSED_COIN_PENALTY;
            if (score < 0)
                score = 0;
            win_draw_score();
            snprintf(buf, sizeof buf, "MISSED COINS: %d", missed);
            win_draw_line(buf);
            win_countdown_wait(tick);
        }

        fill(COL_BG, 0, WIN_LINE_Y, WIDTH, GLYPH_HEIGHT * WIN_LINE_SCALE);
    }

    /* The final score is now on screen: play the short happy song.       */
    static const audio_note_t win_notes[] = { WIN_SONG_NOTES };
    for (int i = 0; i < NELEMS(win_notes); i++) {
        audio_play_sound_blocking(win_notes[i].left_frequency, win_notes[i].right_frequency,
                                  win_notes[i].duration_ms);
        if (win_notes[i].left_frequency != SILENCE)
            sleep_ms(WIN_NOTE_GAP_MS);   /* short gap after each sounded note */
    }

    if (score > level_high_score)
        level_high_score = score;

    put_row(ROW_END_PROMPT, "PRESS ANY KEY TO CONTINUE", COL_WHITE);
    put_row(ROW_END_PROMPT2, "PRESS ENTER TO RETRY LEVEL", COL_WHITE);
    pause_draining_keys(END_SCREEN_PAUSE_MS);

    uint8_t key = wait_any_key();
    return key == KEY_ENTER || key == KEY_RETURN;   /* ENTER retries this level */
}

/* ===================================================================== */
/*  Keyboard (raw south-bridge FIFO polling)                              */
/* ===================================================================== */
static inline uint16_t key_event(void)      { return sb_read_keyboard(); }
static inline uint8_t  ev_state(uint16_t e) { return (e >> 8) & 0xFF; }
static inline uint8_t  ev_code(uint16_t e)  { return e & 0xFF; }

static bool is_mod_key(uint8_t c)
{
    return c == KEY_MOD_SHL || c == KEY_MOD_SHR || c == KEY_MOD_CTRL ||
           c == KEY_MOD_ALT || c == KEY_MOD_SYM;
}

static void go_bootsel(void)
{
    put_row(ROW_BOOTSEL, "REBOOTING TO BOOTSEL...", COL_YELLOW);
    sleep_ms(BOOTSEL_MSG_MS);
    reset_usb_boot(0, 0);
    while (1)
        tight_loop_contents();
}

/* True for the keys that leave the game: Q (either case, wait_any_key()
 * already lowercases letters) and ESC.                                   */
static bool is_quit_key(uint8_t c)
{
    return c == 'q' || c == 'Q' || c == KEY_ESC;
}

/* Leaves the game and returns to the PicoCalc UF2 Loader's menu: shows a
 * message, asks the loader for its menu (see LOADER_COMMAND_MAGIC in the
 * config header) and reboots with the watchdog. If the game was flashed
 * straight to the chip with no loader, nothing reads the request and it
 * simply restarts. On the desktop build there is no loader, so it just
 * closes the program. Both PicoCalc chips take the same path.             */
static void exit_to_loader(void)
{
    sfx_silence();
#ifdef PLATFORM_DESKTOP
    exit(0);
#else
    put_row(ROW_BOOTSEL, "BACK TO THE LOADER...", COL_YELLOW);
    sleep_ms(LOADER_EXIT_MSG_MS);

    watchdog_hw->scratch[LOADER_SCRATCH_MODE] = LOADER_BOOT_MODE_SD;
    watchdog_hw->scratch[LOADER_SCRATCH_ARGUMENT] = 0;
    watchdog_hw->scratch[LOADER_SCRATCH_MAGIC] = LOADER_COMMAND_MAGIC;
    watchdog_reboot(0, 0, LOADER_REBOOT_DELAY_MS);

    while (1)
        tight_loop_contents();      /* the reboot comes in a few milliseconds */
#endif
}

static void drain_keys(void)
{
    while (key_event() != 0)
        tight_loop_contents();
}

/* Drains the keyboard, then waits for a fresh keypress (modifiers alone
 * don't count; `~` goes to BOOTSEL) and returns its code, lowercased if
 * it is a letter, so callers can test for a specific key (KEY_ENTER,
 * 'q', 'y') or just ignore the result.                                   */
static uint8_t wait_any_key(void)
{
    drain_keys();
    for (;;) {
        uint16_t e = key_event();
        if (e && ev_state(e) == KEY_STATE_PRESSED) {
            uint8_t c = ev_code(e);
            if (is_mod_key(c))
                continue;
            if (c == '~')
                go_bootsel();       /* does not return */
            c = (c >= 'A' && c <= 'Z') ? (uint8_t)(c + 0x20) : c;
            drain_keys();
            return c;
        }
        sleep_ms(KEY_WAIT_POLL_MS);
    }
}

/* Asks "QUIT?" over the game screen; true only for Y. Silences any sound
 * effect first, since physics (which ends one) stops while this waits.
 * The caller must forget left/right-held afterwards (wait_any_key()
 * drains the buffer, so a key release during the prompt is lost).        */
static bool confirm_quit(void)
{
    sfx_silence();
    put_row(ROW_CONFIRM, "QUIT?", COL_YELLOW);
    put_row(ROW_CONFIRM_HINT, "Y = YES    ANY OTHER KEY = NO", COL_WHITE);

    return wait_any_key() == 'y';
}

/* Blank screen with "CHOOSE LEVEL: ", then one or two digits (a single
 * digit needs ENTER; two digits finish on their own). Returns the level
 * number (1..99), or 0 if the player pressed ESC to cancel. Backspace
 * erases a digit; a level of 0 is ignored.                               */
static int choose_level(void)
{
    char digits[3] = "";
    int n = 0;

    sfx_silence();
    lcd_clear_screen();
    drain_keys();
    for (;;) {
        char buf[32];
        snprintf(buf, sizeof buf, "CHOOSE LEVEL: %s ", digits);
        put_row(ROW_CHOOSE_LEVEL, buf, COL_WHITE);

        uint16_t e = key_event();
        if (!e || ev_state(e) != KEY_STATE_PRESSED) {
            sleep_ms(KEY_WAIT_POLL_MS);
            continue;
        }
        uint8_t c = ev_code(e);
        if (c == '~')
            go_bootsel();       /* does not return */
        if (c == KEY_ESC) {
            drain_keys();
            return 0;
        }
        if (c == KEY_BACKSPACE && n > 0) {
            digits[--n] = '\0';
        } else if (c >= '0' && c <= '9' && n < 2) {
            digits[n++] = (char)c;
            digits[n] = '\0';
        }
        if (n == 2 || (n == 1 && (c == KEY_ENTER || c == KEY_RETURN))) {
            int level = (n == 1) ? digits[0] - '0' : (digits[0] - '0') * 10 + (digits[1] - '0');
            if (level > 0) {
                drain_keys();
                return level;
            }
            n = 0;              /* level 0 is not allowed: start over */
            digits[0] = '\0';
        }
    }
}

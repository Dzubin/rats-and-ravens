/*
 * platformer_variables.c - every file-scope (global) type and variable of
 * platformer.c: the structs, the level data arrays, and the game/player/
 * sound/rendering state. It is #included by platformer.c right before
 * main() (it is not compiled on its own), so the main file holds only
 * logic. Nothing here contains code; the functions that use these are in
 * platformer.c.
 *
 * Author: Thomas Dzubin
 */

/* A fully solid axis-aligned rectangle, in WORLD coordinates (not screen;
 * see the camera/render section below): the player cannot pass through
 * any side of one. The ground is just more entries here (one per
 * segment, split around each gap), so drawing and collision treat every
 * obstacle the same way; see redraw_region() and step_physics().
 * Fixed-capacity, not `const`: filled at runtime by generate_level(), a
 * few hundred bytes to comfortably cover MAX_SOLIDS even at
 * LEVEL_SCREENS; see the memory comment on generate_level() below.  */
typedef struct {
    int16_t  x, y, w, h;
    uint16_t colour;
} solid_t;

static solid_t SOLIDS[MAX_SOLIDS];
static int     solids_count;

/* "Foggy" zones: same rectangle shape as a solid, but NOT solid; see
 * player_in_foggy() and the FOGGY_* constants in platformer_config.h.
 * Not part of SOLIDS[]: nothing here ever blocks movement, only slows it.
 * Each generated zone is independently either ground-flush or floating
 * (clear air above and below it); see gen_foggy_zones() below.         */
static solid_t FOGGY_ZONES[MAX_FOGGY_ZONES];
static int     foggy_count;

/* Pickups: the jewel and every gold coin pickup share one generic
 * mechanism instead of separate one-off state per type. PICK_JEWEL
 * replaces the launch velocity for POWERUP_DURATION_MS (see
 * step_physics()'s liftoff), PICK_COIN adds COIN_VALUE to score. A
 * pickup is just a position and a type; `taken` is the only field that
 * changes after generation. Collection is a plain position overlap test
 * in step_physics(), so it does not matter whether a pickup sits in
 * open air, on a solid, or inside a foggy zone; nothing here or in the
 * collection check cares which (see pick_pickup_spot() below, which is
 * exactly why pickup placement is one mechanism for every type: any
 * pickup being allowed inside a foggy zone needs that to be true
 * generically, not just true for gold coin by coincidence).
 * `fog` is the FOGGY_ZONES[] index a pickup was placed inside (-1 for
 * none): foggy zones drift, and a pickup placed inside one moves with it
 * (see the drift loop in step_physics()) rather than being left behind
 * in mid-air, possibly out of jump reach. `spawn_x` puts it back on a
 * retry of the same level, same as the zones themselves.                */
typedef enum { PICK_JEWEL, PICK_COIN } pickup_type_t;

typedef struct {
    int16_t       x, y;
    pickup_type_t type;
    bool          taken;
    int8_t        fog;        /* FOGGY_ZONES[] index it rides with, -1 = none */
    int16_t       spawn_x;
} pickup_t;

static pickup_t PICKUPS[MAX_PICKUPS];
static int      pickups_count;

/* Rat_ravens: a patrol hazard. `y` is always RAT_RAVEN_Y for a
 * ground-patrol one (it never changes), or a height between
 * FLYING_RAT_RAVEN_Y_MIN/MAX for a flying one, which bobs up and down
 * over time (see `flying`, gen_rat_ravens() and RAT_RAVEN_ALT_* in the
 * config header). `x_sub` is live position (subpixels, like the player);
 * `spawn_x_sub`/`spawn_dir` are what generate_level() placed it at, used
 * to reset it (position/direction/alive) on a death-retry of the SAME
 * level, same as pickups' `taken` flags reset; see reset_rat_ravens().
 * `drawn_x`/`drawn_y`/`drawn_visible` are rendering-only bookkeeping
 * (last painted position, and whether it needs erasing if it moved or
 * died), the same role `drawn_x`/`drawn_y` play for the player.
 * `spawn_y` resets a flying rat_raven's bobbing `y` on a retry.          */
struct rat_raven {
    int32_t x_sub;
    int16_t y;                /* fixed for a ground rat_raven; bobs for a flying one */
    int8_t  dir;             /* +1 right, -1 left */
    bool    alive;            /* false once stomped, or once it touches the
                                 * player at all (sickness hit, shield hit,
                                 * or a fatal second hit; every outcome
                                 * uses it up)                              */
    bool    flying;           /* true = patrols at `y` (see above), reverses
                                 * off towers/platforms/edges, twice the
                                 * speed, ignores the ground/gaps entirely;
                                 * false = the original ground patrol       */
    int32_t spawn_x_sub;
    int8_t  spawn_dir;
    int16_t spawn_y;
    int16_t drawn_x, drawn_y;
    bool    drawn_visible;
};

static rat_raven_t RAT_RAVENS[MAX_RAT_RAVENS];
static int       rat_ravens_count;

/* State of the PRNG in platformer.c (rand_u32() and friends).          */
static uint32_t rng_state;

static int world_width;   /* runtime; picked fresh per level, see generate_level() */

/* 1-based level number: set to 1 at the start of a fresh game (run_demo()),
 * incremented on every win (win_level()), unaffected by a death-retry,
 * since that replays the SAME level (start_run(), not generate_level()).
 * Drives rat_raven count (see gen_rat_ravens()) and is shown on the always-on
 * status line (see draw_status_lines()).                                 */
static int level_num;

/* Everything that makes a level special, worked out once per level by
 * pick_level_rules() (called at the top of generate_level()) so the
 * generators and the message line just read these fields instead of each
 * testing level_num themselves. The multipliers scale the gold coin, jewel
 * power-up and rat_raven counts; `message` is the level's own hint.        */
typedef struct {
    int         coin_mult, jewel_mult, rat_raven_mult;
    int         rat_raven_speed_pct;      /* 100 = normal rat_raven speed */
    bool        no_gaps, no_rat_ravens, fog;
    const char *message;
} level_rules_t;

static level_rules_t rules = { 1, 1, 1, 100, false, false, false, "" };

/* The best score this specific level layout has ever been won with.
 * Reset to 0 whenever a genuinely new level is generated (see
 * generate_level()'s reset block below), so it is scoped to level_num's
 * current layout, not to the whole game. Updated in show_win_screen()
 * right after the win countdown settles on a final score, and never
 * touched by reset_player(), so it survives both a death-retry and a
 * same-level retry from the win screen (see the `retry` return of
 * show_win_screen()).                                                    */
static int32_t level_high_score;

/* Gaps: generated first (as a scratch list, not SOLIDS[] entries
 * themselves), then turned into ground segments by gen_ground(). Kept
 * within a jumpable width and away from both ends of the level (see
 * LEVEL_SAFE_MARGIN) so the player always starts and finishes on solid
 * ground.                                                                */
static int16_t gap_x[MAX_GAPS], gap_w[MAX_GAPS];
static int     gaps_n;

/* Ground segments come first in SOLIDS[]; see gen_ground().            */
static int ground_count;   /* how many of SOLIDS[]'s entries are ground segments;
                             * they are always added first, so it is also the
                             * count pick_ground_span() below searches            */

/* Remembers which SOLIDS[] entries are towers/platforms (always added in
 * one contiguous run each, right after gen_ground()), so pick_pickup_spot()
 * can pick "a random tower" or "a random platform" without needing a
 * type field on solid_t itself. Drawing and collision never need to
 * know the difference between a tower, a platform, or the ground.       */
static int towers_begin, towers_n;
static int platforms_begin, platforms_n;

/* Sideways drift of each foggy zone (see FOGGY_SPEED_DIV): live position
 * in subpixels, direction (0 = does not move), where it was last painted,
 * and where it started (for a retry of the same level).                  */
static int32_t foggy_x_sub[MAX_FOGGY_ZONES];
static int8_t  foggy_dir[MAX_FOGGY_ZONES];
static int32_t foggy_dx[MAX_FOGGY_ZONES];      /* how far it moved this tick, subpixels (0 if blocked) */
static int16_t foggy_drawn_x[MAX_FOGGY_ZONES];
static int16_t foggy_spawn_x[MAX_FOGGY_ZONES];
static int8_t  foggy_spawn_dir[MAX_FOGGY_ZONES];

static int rat_raven_alt_ticks;   /* ticks since the flying rat_ravens last changed altitude */

static int32_t player_x_sub, player_y_sub;  /* top-left, in subpixels    */
static int32_t player_vy;                   /* vertical speed, subpixels/tick */
static bool    grounded;
static int     player_facing = 1;           /* +1 right, -1 left; which way the sprite looks */
static bool    left_held, right_held;
static bool    jump_requested;              /* edge trigger: start a jump  */
static int     down_buffer_ticks;           /* a DOWN press stays usable for this many
                                              * more ticks (foggy zones only); see
                                              * DOWN_BUFFER_TICKS in platformer_config.h  */
static int     air_jumps_left;              /* double-jump charges left before landing */
static bool    used_air_jump;               /* double-jumped this trip (until landing);
                                              * drives the player's colour, see
                                              * current_player_colour()                 */
static int32_t foggy_move_remaining;       /* queued vertical distance inside a foggy
                                              * zone, subpixels: negative = climb owed,
                                              * positive = descend owed; see
                                              * step_physics()                           */
static int     foggy_idle_ticks;           /* ticks spent idle in/on a foggy zone; see FOGGY_SINK_* */
static bool    on_foggy_ledge;           /* resting on a foggy zone's landing ledge
                                              * (see step_physics()'s vertical resolve);
                                              * recomputed fresh every tick, same as
                                              * grounded; lets a DOWN press there step back
                                              * down into the zone instead of doing nothing */
static bool    paused;

/* Jewel EFFECT state, separate from PICKUPS[]'s `taken` flag,
 * which just tracks whether the icon has been collected. This is how
 * long the boost lasts once it has been: counted in physics ticks, like
 * the level timer, so it pauses along with everything else.              */
static bool     powerup_boost_active;       /* still within its POWERUP_DURATION_TICKS window */
static int32_t  powerup_ticks_remaining;    /* ticks until the boost ends, while active */

/* Pickups just collected this frame, so render_frame() knows which
 * PICKUPS[] icons need erasing (a taken pickup's own footprint is not
 * always fully covered by the player's erase, so each needs its own
 * small redraw). Sized for every pickup in the level, since one tick can
 * collect more than one (two stacked vertically fit inside the player's
 * height) and a frame can run several ticks.                             */
static int16_t just_taken[MAX_PICKUPS];
static int     just_taken_count;

/* Score: +COIN_VALUE per gold coin pickup and +STOMP_VALUE per stomped rat_raven while playing, then the time bonus on the win screen. See
 * step_physics()'s pickup-collection and stomp blocks.                   */
static int32_t score;

/* Level timer: counts down once per physics tick (so it pauses along
 * with everything else, same as the jewel's timer above) from
 * TIMER_DURATION_MS; reaching 0 is a death, same as falling
 * into the gap. See player_died and show_death_screen().                 */
static int32_t timer_remaining_ms;
static bool    player_died;

/* Which of the three causes set player_died this run: a plain pointer
 * to a static string literal, set alongside player_died=true at each of
 * its three set sites in step_physics() (timer, fall, rat_raven-while-sick),
 * read once by show_death_screen() to display a one-line reason
 * underneath the "YOU DIED" message. NULL until death actually happens.  */
static const char *death_reason;

/* Reaching the far right edge of the level; see the win check in
 * step_physics() and win_level() below.                                  */
static bool    player_won;

/* Touched a rat_raven while normal: the character is "sick", turning
 * black (outlined in white, same as rat_ravens; see draw_player_shape()),
 * until either a jewel pickup cures it instantly (gold coin does NOT),
 * or SICK_CURE_TICKS of survival cures it back to "well" on its own (see
 * sick_ticks_remaining below). Touching a rat_raven again while already
 * sick is a death instead; see the rat_raven-collision block in
 * step_physics().                                                        */
static bool    sick;
static int32_t sick_ticks_remaining;
static int     rat_raven_invuln_ticks;         /* grace period after any rat_raven
                                              * touch; see RAT_RAVEN_HIT_INVULN_TICKS */

static int      drawn_x, drawn_y;           /* last painted WORLD pixel pos   */
static uint16_t drawn_colour;               /* last painted player colour     */

/* Camera: how far right (in WORLD pixels) the visible screen's left edge
 * currently is; see update_camera() and render_frame() below.
 * drawn_camera_x is the camera position the screen actually shows right
 * now; when it differs from camera_x, everything on screen needs
 * repainting, not just the player (see render_frame()). Started at an
 * impossible value so the very first render_frame() call always does
 * that full repaint.                                                     */
static int camera_x;
static int drawn_camera_x = -1;

/* Forces every status line to redraw on the next draw_status_lines() call
 * even if its text has not changed since the last time it drew. Set at
 * start-up and by show_death_screen() (which draws its own text over the
 * message line). Without it, a line whose text happens not to have
 * changed (score especially, which only updates on a gold coin pickup)
 * would stay blank or wrong until it NEXT changed.                       */
static bool status_force_redraw = true;

/* The band of screen rows, above PLAYFIELD_TOP_Y, that redraw_region()
 * has painted over since the status lines were last drawn: a full-screen
 * repaint, or the player's erase near the top of a very high jump. Every
 * status line overlapping it redraws next time even if its text is
 * unchanged, the same reason as status_force_redraw, but only the lines
 * actually touched. Empty when status_dirty_y0 >= status_dirty_y1.       */
static int  status_dirty_y0 = PLAYFIELD_TOP_Y;
static int  status_dirty_y1 = 0;

/* Sound-effect state machine; see sfx_play() in platformer.c.           */
static int      sfx_ticks_remaining;
static uint16_t sfx_freq2, sfx_freq3;   /* the tones still to come, in order */
static int      sfx_ticks2, sfx_ticks3;

/* Which kind of solid draw_solid_detail() is texturing (SOLID_TEXTURES). */
enum { SOLID_GROUND, SOLID_TOWER, SOLID_PLATFORM };

#if PLAYER_USE_SPRITE
/* The player as a picture: the art in player_sprite.h, with 'B' in the
 * body colour passed in and 'D' a darker shade of it, mirrored to face the
 * way the player is walking, and the white outline (1px while sick, 3px
 * with the jewel) drawn over its edge pixels as before.          */
static uint16_t player_pix[PLAYER_W * PLAYER_H];
#endif

#if RAT_RAVEN_USE_SPRITES
/* The rat and raven pictures as RGB565 pixel blocks, built once from the
 * ASCII art in rat_raven_sprites.h. The rat has a second, mirrored copy for
 * walking left; the raven has its two wing frames, also mirrored. The background colour
 * fills the empty pixels (no transparency).                              */
static uint16_t rat_pix[2][SPRITE_SIZE * SPRITE_SIZE];
static uint16_t raven_pix[2][2][SPRITE_SIZE * SPRITE_SIZE];   /* [wing frame][0 = faces right, 1 = left] */
static bool     sprites_ready;
#endif

# Rats and Ravens: game description

Design notes moved out of the header comment of `platformer.c` (author: Thomas Dzubin).
The code comments next to each function still carry the details; this file is the overview.

Every level is randomly generated; see generate_level() and the rest
of the "Random level generation" section further down: its own width
(always LEVEL_SCREENS screens), gaps, towers,
platforms, foggy zones, pickups, and rat_ravens, all placed by a
handful of small `gen_*()` functions with retry-based placement (reject
a candidate spot that overlaps something it shouldn't, try again)
rather than an exhaustive solvability proof. Reasonable constraints
are derived from the known jump physics (gap widths within jump
range, EVERY tower within the double jump's reach; see TOWER_H_MAX's
comment for why that one is a hard requirement, not just a nicety;
pickups placed reachably). A horizontal-only, dead-zone-following
camera scrolls to keep up with however wide the current level turns
out to be; see update_camera() and render_frame(). The rat_ravens
below are the only hazard, and the player, rat_ravens and pickups are
small sprites; still no level THEMES: geometry is random, content is
not. A one-line debug readout (position, vertical velocity, grounded
state) is available while the core feel is being tuned; see
ENABLE_DEBUG_READOUT in platformer_config.h.

Rat_ravens: a patrol hazard (RAT_RAVENS[] below), drawn as a rat
(ground) or a raven (flying) from rat_raven_sprites.h, the same block
size as the player (RAT_RAVEN_SIZE == PLAYER_W); RAT_RAVEN_USE_SPRITES
= 0 falls back to black squares outlined in white. Two kinds, a
`flying` flag apart (rat_raven_t): GROUND
ones patrol left and right at RAT_RAVEN_SPEED, reversing off a tower or
either end of the level or the edge of a gap (they never cross one);
FLYING ones patrol at a random fixed height
between FLYING_RAT_RAVEN_Y_MIN/MAX instead of the ground, at
FLYING_RAT_RAVEN_SPEED (twice a ground one's), reversing off a
PLATFORM too since they fly at head height, and ignoring the
ground/gaps entirely. Either kind's speed is halved for as long as it
is inside a foggy zone (rat_raven_in_foggy()). Neither kind ever
spawns within RAT_RAVEN_MIN_SPAWN_DIST of the player's own start
position, so a level cannot kill the player on tick one before they
have a chance to react. Count scales with the level number
(level_num), not level width: RAT_RAVENS_PER_LEVEL per level, capped
at RAT_RAVENS_LEVEL_CAP; see gen_rat_ravens().
Levels 1-2 spawn six rat_ravens, levels 3-4 spawn twelve, levels 5-6
spawn twenty-four, levels 7-8 and up spawn forty (the cap). Each one has a FLYING_RAT_RAVEN_CHANCE_PCT (25%) chance
of being flying, but at least one flying rat_raven is guaranteed per
level regardless.

Sickness: touching a rat_raven while normal makes the character "sick",
turning it black, outlined in white the same way (see `sick`), and
consumes that rat_raven, removing it from play, same as a stomp-kill.
Cured either instantly by collecting a jewel pickup only (gold coin
does not cure it, though a jewel pickup plays its own rising
three-tone sound), or on its own after SICK_CURE_TICKS (30 seconds
survived, tick-based so it pauses with the game) back to "well"
(plain green again), needing two fresh rat_raven hits before the next
death. Touching a (different) rat_raven while already sick is that
second hit: a death. If the jewel is active at the moment of
a hit, it acts as a one-time shield instead: the character survives
without becoming sick, but the power-up itself is used up immediately
(ends early, back to a plain green block). It can survive a single
rat_raven collision without becoming sick, but the jewel
becomes inactive. All of the above is gated by a short
RAT_RAVEN_HIT_INVULN_TICKS grace period after any rat_raven touch of any
kind, so an adjacent rat_raven on the very same tick cannot register as
a separate second touch. Dropping onto a rat_raven from above (falling
off a ledge, or on the way down out of a jump) stomps it instead,
killing the rat_raven and leaving the player entirely untouched (no
sickness, no shield spent). See the rat_raven movement/collision
blocks in step_physics().

Score, the level timer, and the level number: score comes from gold coin
pickups (+COIN_VALUE each, COIN_PER_LEVEL of them per level, a
fixed count) and stomping rat_ravens (+STOMP_VALUE each), plus the win
screen's time bonus; jewel pickups never add score, and neither
does simply moving left or right. It is shown bigger than the driver's normal
text (draw_big_text(), a hand-rolled scaled-glyph renderer, since the
driver has no native text scaling) on an always-on status line just
below the debug readout's row, alongside the timer and the current
level number (draw_status_lines(): level and time on one line, score
on the one right below it). Each redraws whenever its own text
changes OR something painted over it (a full-screen repaint, or the
player's erase near the top of a very high jump); see
status_dirty_y0/y1 and status_line_stale(). Without that, a line
whose text happens not to have changed (score/level especially) would
stay blank after a repaint until it next changed. The timer ticks
down once per physics step, so it (like everything else) pauses
along with the game. Reaching 0, or falling into a gap, is a death;
reaching the level's far right edge is a win. A death shows its
reason (a one-line message under "YOU DIED" saying why; see
show_death_screen()); a win shows a bigger score readout and the
time LEFT in seconds
followed by a rapid seconds/score countdown (see show_win_screen()'s
own comment): a real, permanent score bonus, not just a visual
effect, so a faster playing time to win results in more time points
being added to the score before it is compared against
level_high_score, the best score this specific level layout has ever
been won with (shown on its own always-on status line; see
draw_status_lines()). Both end screens then enforce at least a
2-second pause, with the keyboard buffer continuously drained the
whole time, not just once, so a still-held or mashed key cannot skip
past it, before either accepts a fresh keypress to continue (death
shares this with the win screen via end_screen_pause_and_wait();
the win screen also offers a choice, described below, so it runs its
own equivalent tail instead). Death retries the SAME level layout
from the beginning (score/timer/pickups-taken/rat_raven positions all
reset; see start_run()) and does NOT advance level_num. Winning
offers the same choice, from show_win_screen(): press ENTER to retry
this SAME level layout instead (to try to beat level_high_score), or
any other key to generate a genuinely NEW, harder level (level_num++,
more rat_ravens) and start fresh on it. Either way, score always
resets to 0 for the new attempt, same as everything else (see
reset_player()); it never carries between levels or between retries.

Pickups: PICKUPS[] below is a generic list (position + type + taken)
covering both the jewel and every gold coin pickup, the same
mechanism, drawn/collected/erased identically regardless of type or
where it is placed (open air, on a solid, or inside a foggy zone;
see the comment on PICKUPS[] for why nothing here cares which). Gold coin
is worth COIN_VALUE score and disappears once collected; by design,
gold coin is never on the ground or floor, always on a platform or
tower, floating in open air, or inside a foggy zone. A pickup placed
inside a foggy zone rides along with it as the zone drifts (see
pickup_t's `fog`), so it can never be left stranded up in the air.
No pickup is ever placed overlapping a solid.

Every obstacle, the ground included, is a fully solid rectangle in
SOLIDS[] below: the character cannot pass through any side of one,
not just land on top; walking into a tower's side stops you, and
jumping into a platform's underside from below stops you there too.
See step_physics() for the collision resolution (horizontal pass,
then vertical pass, each against every entry in SOLIDS[]).

Physics is fixed-point (no float) on a fixed 16ms timestep: position
and velocity are stored in subpixels (SUBPIXEL_SHIFT bits of
fraction per pixel), and the main loop measures real elapsed time and
runs however many whole 16ms steps that covers, capped so a stall
(e.g. a slow key-repeat frame) cannot make the game leap forward when
it resumes. This keeps the feel identical on RP2040 (no hardware
FPU), RP2350, and the desktop build. See step_physics().

Every jump is a fixed, predictable height; there is no "hold to jump
higher." How high a jump goes depends only on which of three launch
velocities fired it (see platformer_config.h): the ground jump
(~99px), the weaker double jump (see below), or the jewel
(~150px on its own, a real energy boost, not a change to gravity).
The screen's top edge is also a hard ceiling: nothing can send the
character above it, however jumps are combined.

Double jump: pressing SPACE/UP again while airborne (and not yet
used up) gives one more, weaker jump. It refills the moment you
land. Chained right at the peak of a ground jump, with good timing,
it reaches ~141px, enough to clear EVERY generated tower
(TOWER_H_MIN..TOWER_H_MAX, 72..120px) and reach every platform
without any power-up: power-ups in this project are always optional
(a skilled path must exist without one), the power-up is only the
*easy* way. See MAX_AIR_JUMPS and AIR_JUMP_VELOCITY in
platformer_config.h.

Jewel: at least MIN_JEWELS of these are placed per level
(see gen_pickups()). Never adds score. Touching one makes every
jump, ground or air, launch at POWERUP_JUMP_VELOCITY instead of its
usual velocity, for POWERUP_DURATION_MS: a straightforwardly stronger
jump, not a gravity change, so a single powered-up jump clears even
the tallest generated tower on its own. It also shields against one
rat_raven hit while active (see "Sickness" above); either way, wearing
off on its own or being spent on a rat_raven hit, it just ends.

Foggy zones (a thick cloud / dense branches), in FOGGY_ZONES[]
below, are NOT solid, so walking or jumping straight into one works,
but gravity is off inside it: UP/DOWN each queue a small, bounded
step (FOGGY_STEP_PX) of slow climb/descend, worked off gradually
rather than fired all at once, so it takes several separate presses
to cross one; see FOGGY_STEP_PX and FOGGY_CLIMB_SPEED in
platformer_config.h. Horizontal movement is also slowed. A zone's
top edge is also a generic one-way landing ledge (see
step_physics()'s vertical resolve): climb clear of it and gravity
drops you the last sliver onto it, grounded, with a real jump and
the double jump both refilled (and DOWN there steps back down into
the zone instead of only being able to jump away; see
on_foggy_ledge). A DOWN press is buffered for DOWN_BUFFER_TICKS
rather than consumed or dropped on the exact tick it arrives (see
down_buffer_ticks): landing on a ledge and the key press are
independently timed, and a press arriving a tick or two before
landing completes would otherwise be silently dropped. The power-up
overrides all of it (a jump still launches at full power-up
strength, with normal gravity, even inside one). Each generated zone
is independently either ground-flush or floating; descending past a
floating one's bottom edge resumes normal gravity and falls the rest
of the way to the ground, the same generic mechanism as clearing a
top edge, with no special-casing. See player_in_foggy() and
gen_foggy_zones() in the "Random level generation" section below.

The character's FILL colour is a minor visible cue: cyan while a
double jump is still "spent" for the current trip (until landing),
black while sick (see above); see current_player_colour(). A white
OUTLINE is a separate layer on top of whichever fill colour that is:
shown while sick (for the same reason as rat_ravens, solid black
would be invisible) and while the jewel is active too,
without recolouring the character; see draw_player_shape(), which
adds that outline as a separate layer on top of whichever fill
colour current_player_colour() returns.

Drawing works in WORLD coordinates everywhere except the final pixel
write (fill_world() translates by camera_x); see render_frame(). Each
frame is two passes: first ERASE everything that moved (repaint its
old footprint against the known background, whichever
SOLIDS[]/FOGGY_ZONES[]/PICKUPS[] pixels fall inside it; see
redraw_region()), then DRAW the status lines, every rat_raven and the
player on top. Erasing everything before drawing anything means one
object's erase can never bite into another object already drawn this
frame. When the camera DID move, everything on screen shifted, so the
erase pass is replaced by a repaint of the whole viewport; see
update_camera() and render_frame().

Built on the Raspberry Pi Pico SDK and the LCD / south-bridge
keyboard drivers from "picocalc-text-starter" by Blair Leduc
(drivers/lcd.c, drivers/southbridge.c). We poll the keyboard FIFO
ourselves with sb_read_keyboard() (see key_event) so we see raw
press, release, and hold events; we therefore do NOT call
picocalc_init().


## Controls

```
  Splash screen ......... any key starts the demo.
  LEFT / RIGHT .......... move (auto full speed while held; no
                          acceleration or friction yet)
  SPACE or UP ........... jump. Press again in mid-air for one more,
                          weaker jump (double jump).
  ENTER or P ............ pause / resume, shows/hides a "PAUSED"
                          message on screen
  ESC ................... firmware: quit to the splash screen, no
                          confirmation. Desktop (PLATFORM_DESKTOP):
                          confirms first, then exits the program,
                          since there is no splash to usefully
                          return to on a real desktop app.
  Q ..................... firmware: quit to the splash screen, asks
                          to confirm first (in case of a stuck or
                          panic quit). Desktop: confirms, then exits
                          the program the same way ESC does.
  ~  (SHIFT + backtick) . reboot into BOOTSEL (USB drive) mode
```

/*
 * platformer_functions.h - prototypes for every static function in
 * platformer.c that is used before its definition, so main() and the two
 * functions right after it can sit at the top of the file. Included by
 * platformer.c only (everything here is `static`, private to that file).
 *
 * Author: Thomas Dzubin
 */
#ifndef PLATFORMER_FUNCTIONS_H
#define PLATFORMER_FUNCTIONS_H

#include <stdbool.h>
#include <stdint.h>

#include "platformer_config.h"

/* The full definition is in platformer.c. */
typedef struct rat_raven rat_raven_t;

/* main() and what it calls */
static void    splash_screen(void);
static uint8_t wait_any_key(void);
static void    run_demo(void);

/* level generation and per-attempt state */
static void generate_level(void);
static void reset_player(void);
static void start_run(void);
static void win_level(void);
static void step_physics(void);

/* camera */
static void update_camera(void);

/* sound */
static void sfx_play_sad(void);
static void sfx_silence(void);

/* drawing */
static void fill(uint16_t colour, int x, int y, int w, int h);
static void fill_world(uint16_t colour, int wx, int y, int w, int h);
static void put_row(int row, const char *str, uint16_t fg);
static void draw_big_text(int x, int y, const char *str, int scale, uint16_t colour);
static bool rect_intersect(int ax, int ay, int aw, int ah,
                           int bx, int by, int bw, int bh,
                           int *ox, int *oy, int *ow, int *oh);
static void redraw_region(int x, int y, int w, int h);
static void blit_clipped(const uint16_t *pix, int wx, int y, int w, int h);
static uint16_t current_player_colour(void);
static void draw_player_shape(int x, int y, uint16_t colour);
static void erase_player_if_moved(void);
static void draw_player(void);
static void draw_rat_raven_shape(int x, int y, const rat_raven_t *bb);
static void erase_rat_ravens_if_moved(void);
static void draw_rat_ravens(void);
static void render_frame(void);
static void draw_status_lines(void);
static bool end_screen_pause_and_wait(void);
static bool show_death_screen(void);
static bool show_win_screen(void);
#if ENABLE_DEBUG_READOUT
static void draw_debug_readout(void);
#endif

/* keyboard */
static inline uint16_t key_event(void);
static inline uint8_t  ev_state(uint16_t e);
static inline uint8_t  ev_code(uint16_t e);
static bool is_mod_key(uint8_t c);
static void go_bootsel(void);
static void drain_keys(void);
static bool confirm_quit(void);
static int  choose_level(void);

#endif /* PLATFORMER_FUNCTIONS_H */

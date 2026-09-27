/* rat_raven_sprites.h - 16x16 rat and raven pictures for the rat_ravens.
 * Author: Thomas Dzubin.
 *
 * Each picture is 16 rows of 16 characters; '.' is background and the other
 * letters are colours (see sprite_colour() in platformer.c). The rat and the raven face right (they
 * are mirrored for the other direction) and the raven has two wing frames.
 * Only used when RAT_RAVEN_USE_SPRITES is 1 in platformer_config.h.          */
#ifndef RAT_RAVEN_SPRITES_H
#define RAT_RAVEN_SPRITES_H

#define SPRITE_SIZE 16

#define SPRITE_COL_RAT_BODY   RGB(240, 215, 175)
#define SPRITE_COL_RAT_DARK   RGB(185, 150, 115)
#define SPRITE_COL_PINK       RGB(255, 175, 190)
#define SPRITE_COL_EYE        RGB(255,  70,  70)
#define SPRITE_COL_RAVEN_BODY  RGB(140, 110, 200)
#define SPRITE_COL_RAVEN_WING  RGB( 95,  75, 150)
#define SPRITE_COL_RAVEN_BELLY RGB(120,  95, 175)
#define SPRITE_COL_BEAK       RGB(255, 180,  40)

static const char *const RAT_ART[SPRITE_SIZE] = {
    "................",
    "................",
    "................",
    "................",
    "..........gg....",
    ".........gppg...",
    "....ggggggggggg.",
    "..gggggggggggeg.",
    ".pgggggggggggggp",
    ".pdgggggggggggp.",
    "..ddgggggggggg..",
    "...dddggggggdd..",
    "....dddddddddd..",
    "....dd.d..d.dd..",
    "....dd.d..d.dd..",
    "...ddd.dd.dd.ddd",
};

/* wings up */
static const char *const RAVEN_ART_A[SPRITE_SIZE] = {
    "................",
    "................",
    "................",
    "....n...........",
    "....nn..........",
    "....nnn...bbb...",
    "....nnnn.bbbbb..",
    "...nnnnnnbbbebk.",
    "nn.bbbbbbbbbbbkk",
    ".nbbbbbbbbbbbbk.",
    "..bbwwbbbwwwbb..",
    "...wwwwwwwwww...",
    "....k.....k.....",
    "................",
    "................",
    "................",
};

/* wings down */
static const char *const RAVEN_ART_B[SPRITE_SIZE] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "..........bbb...",
    "nn.......bbbbb..",
    ".nbbbbbbbbbbebk.",
    "..bbbbbbbbbbbbkk",
    "..bbnnbbbwwwbbk.",
    "...nnnnnwwwww...",
    "....nnnnnwww....",
    ".....nnnn.......",
    "......nn........",
    "................",
    "................",
};

#endif /* RAT_RAVEN_SPRITES_H */

/* pickup_sprites.h - 8x8 pictures for the pickups.
 * Author: Thomas Dzubin.
 *
 * A gold coin and a cyan jewel. Each is 8
 * rows of 8 characters; '.' is whatever is behind it and the letters are
 * colours (see pickup_pixel_colour() in platformer.c). Only used when
 * PICKUP_USE_SPRITES is 1 in platformer_config.h.                        */
#ifndef PICKUP_SPRITES_H
#define PICKUP_SPRITES_H

#define PICKUP_ART_SIZE 8

#define PICKUP_COL_COIN_EDGE   RGB(200, 120,  20)
#define PICKUP_COL_COIN_BODY   RGB(255, 200,  50)
#define PICKUP_COL_COIN_SHINE  RGB(255, 245, 170)
#define PICKUP_COL_JEWEL_EDGE    RGB( 40, 150, 220)
#define PICKUP_COL_JEWEL_BODY    RGB(120, 225, 255)
#define PICKUP_COL_JEWEL_SHINE   RGB(255, 255, 255)

static const char *const COIN_ART[PICKUP_ART_SIZE] = {
    "..oooo..",
    ".oyyyyo.",
    "oyywyyyo",
    "oyywyyyo",
    "oyyyyyyo",
    "oyyyyyyo",
    ".oyyyyo.",
    "..oooo..",
};

static const char *const JEWEL_ART[PICKUP_ART_SIZE] = {
    "...dd...",
    "..dccd..",
    ".dcwccd.",
    "dcwccccd",
    "dccccccd",
    ".dccccd.",
    "..dccd..",
    "...dd...",
};

#endif /* PICKUP_SPRITES_H */

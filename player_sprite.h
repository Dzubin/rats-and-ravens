/* player_sprite.h - the 16x24 picture of the player character.
 * Author: Thomas Dzubin.
 *
 * 16 characters wide by 24 rows tall, facing right (it is mirrored to face
 * left): 'B' is the body colour (green normally, cyan after a double jump,
 * the fading colour while sick), 'D' a darker shade of the body colour,
 * 'w' white and 'k' black for the eyes and mouth. Only used when
 * PLAYER_USE_SPRITE is 1 in platformer_config.h.                          */
#ifndef PLAYER_SPRITE_H
#define PLAYER_SPRITE_H

static const char *const PLAYER_ART[24] = {
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBwwwBBwwwBBB",
    "BBBBBwwkBBwwkBBB",
    "BBBBBwwkBBwwkBBB",
    "BBBBBwwwBBwwwBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBkBBBBkBBB",
    "BBBBBBBBkkkkBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "DBBBBBBBBBBBBBBD",
    "DDBBBBBBBBBBBBDD",
    "DDBBBBBBBBBBBBDD",
    "DDDBBBBBBBBBBDDD",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBBBBBBBBBBB",
    "DDDDBBBBBBBBDDDD",
    "DDDDBBBBBBBBDDDD",
};

#endif /* PLAYER_SPRITE_H */

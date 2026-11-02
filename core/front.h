/* batrider360: the N64 front end's title and splash, drawn into the game's 640x480 texture (the board's
 * orientation, doubled). The splash from the kit's COIN-OP GAME STUDIO art (tools/mkfront.py -> build/gen/front_assets.h);
 * test 10: no N64 logo over the title (user), the arcade's own is drawn. */
#ifndef BR_FRONT_H
#define BR_FRONT_H
#include <stdint.h>
#include "vdp.h"

enum { FRONT_TEX_W = BR_W * 2, FRONT_TEX_H = BR_H * 2 };
enum { FRONT_START, FRONT_PRACTICE, FRONT_OPTIONS, FRONT_ITEMS };
enum { FRONT_CHASE, FRONT_ORIGINAL };

int front_init(void);                  /* 0 = the pictures unpacked */

/* The title's text layer in v (a copy of the board's title state): the menu with sel highlighted, the MODE line
 * (note: a line of text under it, or NULL), ARRANGED BY / COIN-OP GAME STUDIO, and the highlight's palette. */
void front_title_text(Vdp *v, int sel, int mode, const char *note);
/* The spare text palettes front_title_text uses: FRONT_PAL_PINK (the N64 title's highlight), FRONT_PAL_AMBER,
 * FRONT_PAL_RED (ramps shaped as the board's white 0x30). */
enum { FRONT_PAL_RED = 0x3d, FRONT_PAL_AMBER = 0x3e, FRONT_PAL_PINK = 0x3f };
void front_text_palettes(Vdp *v);
void front_splash(uint32_t *tex);
void front_doubled(const uint32_t *pic, uint32_t *tex);   /* the board's picture alone */
void front_fade(uint32_t *tex, int level);                /* level 0 (black) .. 256 */
#endif

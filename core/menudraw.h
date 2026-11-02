/* batrider360 test 11 (user: "redesign the menus using the gadgets borders and font"): the menus drawn as GADGETS
 * panels on the front end's game texture: Baffo's list frame (Secret bosses' / Status', 9-sliced from
 * gadgets_menu_piece) with its title plate and dark grid body, the text in the board's own 8x8 font with the
 * gadgets' tints. Coordinates are the player's upright texels: x 0..MD_W-1 across, y 0..MD_H-1 down (the texture is
 * FRONT_TEX_W x FRONT_TEX_H in the board's orientation, ROT270). Video thread only. */
#ifndef BR_MENUDRAW_H
#define BR_MENUDRAW_H
#include <stdint.h>

enum { MD_W = 480, MD_H = 640 };
/* tints (gadgets_draw.c's, + grey) */
enum { MD_WHITE, MD_RED, MD_YELLOW, MD_AMBER, MD_BLUE, MD_GREEN, MD_CYAN, MD_GREY, MD_TINTS };
/* the frame's bands (texels): the top band holds the title plate, the body's dark area starts MD_BODY_Y down */
enum { MD_TOP = 56, MD_BOTTOM = 16, MD_BODY_Y = 54, MD_BODY_X = 11, MD_BODY_END = 13, MD_PLATE_Y = 6, MD_PLATE_H = 27 };

/* the board's font from its text glyphs (Vdp.txgfx: 1024 x 64 one-byte pixels); kept once the letters are there.
 * Returns 1 when the font is in. */
int md_font(const uint8_t *glyphs);
void md_target(uint32_t *tex);
void md_fill(int x, int y, int w, int h, uint32_t argb);
void md_darken(int x, int y, int w, int h, int keep256);   /* the picture under it times keep256 / 256 */
/* the frame at (x, y), w x h (w >= 120, h >= MD_TOP + MD_BOTTOM): plate and body dark, the body's grid */
void md_panel(int x, int y, int w, int h);
void md_hline(int x, int y, int w);                        /* a grid line across the body */
/* the text at half / 2 times the font's size (test 12: half steps, 3 = 1.5x) */
void md_text(int x, int y, int half, int tint, const char *s);
int md_text_w(int half, const char *s);
#endif

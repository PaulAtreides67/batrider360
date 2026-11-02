/* batrider360: GP9001 + text layer state and drawing. */
#ifndef BR_VDP_H
#define BR_VDP_H
#include <stdint.h>
#include "board.h"

typedef struct {
	uint16_t vram[0x1c00];             /* 3 tilemaps x 0x800 words + 0x400 sprite words */
	uint16_t voffs;
	uint8_t reg;
	uint16_t scrollx[8];               /* scroll registers 0-7 as written: BG x/y, FG x/y, TOP x/y, sprites x/y */
	uint16_t scrolly[8];               /* unused (kept for the state's layout) */
	uint8_t bank[8];                   /* object banks 0x5000C1-CF */
	int irq;                           /* IRQ2 line, set at line 230, cleared by register 0x0E/0x0F */
	uint16_t txram[0x1000], pal[0x800], lineselect[0x100], linescroll[0x100];
	uint8_t txgfx[1024 * 64];
} Vdp;

extern Vdp vdp;
const Vdp *br_vdp_snapshot(void);      /* board.c: the state at the picture's line (split board) */
extern int vdp_layers;

int vdp_init(const uint8_t *gfx);
void vdp_reset(void);
uint16_t vdp_read(uint32_t a, int vpos);
void vdp_write(uint32_t a, uint16_t d, uint16_t mask);
void vdp_text_glyphs(const uint8_t *ram);
void vdp_text_dma(const uint8_t *ram);
void vdp_render(const Vdp *v, uint32_t *out); /* uses static scratch: one caller at a time */
/* The N64 front end's hitbox disc (spec 6.2): centre x (along the 320 axis), y (across) and radius r in 1/128 board
 * pixels; drawn after the sprites at priority level (anything of a higher priority stays over it), as a red disc of
 * radius r + 128 with a white disc of radius r on it. */
typedef struct { int x, y, r, level; } VdpDisc;
/* Test 14 (HITBOX ALL): a 68000 hitbox rectangle, centre x / y and half sizes in 1/128 board pixels, outlined over
 * everything but the text layer in its kind's colour */
enum { VDP_BOX_ENEMY, VDP_BOX_TARGET, VDP_BOX_BULLET, VDP_BOX_KINDS };
typedef struct { int16_t x, y, hw, hh; uint8_t kind; } VdpBox;
void vdp_render_discs(const Vdp *v, uint32_t *out, const VdpDisc *d, int n, const VdpBox *b, int nb);
#endif

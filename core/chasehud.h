/* batrider360: the parts of CHASE!'s in-game HUD that the N64 draws in host code (manual p.11 items 1, 2, 3, 7, 8):
 * the redrawn top row (1P + score, the top score in amber, the boss time left in seconds) and the boss HP bar.
 * CHASE!'s 68000 patch (batrider-chase src/hud.s) draws the rest: the rail and the stage line. */
#ifndef BR_CHASEHUD_H
#define BR_CHASEHUD_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Once per board frame in a CHASE! credit (see WHERE TO CALL IT below) (CHASE!'s program running). It draws only
 * while CHASE!'s own state says a credit is in play (the 68000 word 0x201000 == 2) and puts the board's own rows
 * back when that ends. Writes text-layer cells in 68000 RAM (shown from the board's next text DMA) and, for the thin
 * bar, two glyphs of the text layer (vdp.txgfx 0x349/0x34A, saved and put back). */
void chasehud_frame(void);

/* WHERE TO CALL IT. After br_run_frame works for the top row, but NOT for the bar: CHASE!'s chase_frame (hud.s,
 * which rewrites line 36's stage line every frame) runs when the 68000's main loop wakes from its vblank wait, and
 * in play that is often late (slowdown), after br_run_frame has returned and before the next text DMA, so hud.s's
 * cells win over the bar (seen in the PC harness: the bar shows with hud.s's blanks and CHASE tag cut into it).
 * The N64 draws from that very point (its hook at the vblank wait's return). The same here: CHASE!'s h_vbl ends
 * `jsr chase_frame; jmp 0x2006`, and the jmp (4 bytes 4EF8 2006 at CHASEHUD_VBL_SITE in CHASE!'s program) is the
 * spot. chasehud_patch puts the board's ILLEGAL there (only when those bytes are found); the host's BrHook, on
 * ILLEGAL with PPC == CHASEHUD_VBL_SITE, calls chasehud_frame and sets the PC to CHASEHUD_VBL_NEXT. */
enum { CHASEHUD_VBL_SITE = 0x120C98, CHASEHUD_VBL_NEXT = 0x2006 };
int chasehud_patch(unsigned char *prg); /* 0 = patched; -1 = the site holds other bytes (left alone) */

/* Test 11 ("ORMAL COURSE" on the ranking): h_vbl's `jsr chase_frame` just before CHASEHUD_VBL_SITE is a host hook too
 * (chasehud_patch). At it: chasehud_pre(), then the jsr as it was (return address CHASEHUD_PRE_SITE + 6, PC = the
 * returned target). When play ends, chase_frame's hud_clear blanks the rail's columns 0-3 on lines 4-35 after the next
 * screen has drawn there: the ranking's big letters are 2 x 2 text cells and its course name's first letter sits in
 * columns 2-3. chasehud_frame then puts back every cell the rail never draws (tiles from 0x80 on) that the clear
 * blanked, from the copy chasehud_pre kept. */
enum { CHASEHUD_PRE_SITE = 0x120C92 };
uint32_t chasehud_pre(void);

/* The board was reset or another program set: forget what was saved (no RAM is written). */
void chasehud_reset(void);

#ifdef __cplusplus
}
#endif
#endif

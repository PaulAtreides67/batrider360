/* batrider360 test 11: the menu sounds, made once at boot by the board's own sound side and mixed by the host.
 *
 * Every menu page makes the game's sounds (user: ibara360's consistency; ibara360 plays the PS2's on all its pages):
 * the cursor (the N64 front end's SE 0x24, an FM sound on the YM2151's channel 7), decide (SE 0x22) and the pause
 * (SE 0x28). Test 10 sent them to the board's sound CPU, so the OPTIONS pages were silent, the title's cursor too (the
 * 68000's SE routine drops SEs in the attract without demo sounds) and a page over a credit had none (the board
 * stands). Here the Z80 plays each code once at boot, the 68000 held, and the frames are kept as PCM.
 *
 * menusnd_render: after br_init with a program in (br_set_program), before the front end's first br_reset; leaves the
 * board reset. menusnd_mix: one thread (the sound thread), the voices started by menusnd_play from another thread are
 * handed over through menusnd_mix's own argument (the main loop gathers them a frame). No allocation after render. */
#ifndef BR_MENUSND_H
#define BR_MENUSND_H
#include <stdint.h>

enum { MS_CURSOR, MS_DECIDE, MS_PAUSE, MS_SOUNDS };

int menusnd_render(void);                  /* 0 = all made; < 0 = out of memory (no menu sounds then) */
int menusnd_frames(int id);                /* its length in board frames (BR_AUDIO_PER_FRAME samples each), 0 = none */
/* Adds the frame's BR_AUDIO_PER_FRAME samples of the menu sounds to out: starts = a mask (1 << MS_*) of the sounds
 * started this frame (a sound started again restarts), volume 0..256. */
void menusnd_mix(int16_t *out, unsigned starts, int volume);
void menusnd_stop(void);
/* Test 14 (faster boot): the rendered sounds as a file's bytes (MENUSND_FILE_MAX at most; returns the length, 0 =
 * nothing rendered), and back (0 = taken, -1 = not this build's file or out of memory: render them then). */
enum { MENUSND_FILE_MAX = 16 + 3 * 90 * 1600 * 2 };
int menusnd_pack(uint8_t *buf);
int menusnd_unpack(const uint8_t *buf, int len);
#endif

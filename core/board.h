/* batrider360: the Armed Police Batrider board (Raizing RA9704), emulated.
 * 68000 (Musashi) + Z80 (superzazu) + YM2151 (ymfm) + 2 x OKIM6295 + GP9001 + RAM text layer.
 * Written from MAME's raizing_batrider.cpp / gp9001.cpp / toaplan_txtilemap.cpp (ref/mame). */
#ifndef BR_BOARD_H
#define BR_BOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	BR_W = 320, BR_H = 240,            /* the board's picture, before ROT270 */
	BR_PRG_SIZE = 0x200000,            /* 68000 program, big-endian bytes */
	BR_SND_SIZE = 0x40000,             /* Z80 program + banks */
	BR_GFX_SIZE = 0x1000000,           /* GP9001 ROMs, MAME order rom-1, rom-3, rom-2, rom-4 */
	BR_OKI_SIZE = 0x100000,
	BR_AUDIO_RATE = 48000,
	BR_AUDIO_PER_FRAME = 800,          /* 48 kHz at the 360's 60 Hz (the board's 59.637 Hz plays 0.6% fast) */
	BR_RAM_BASE = 0x200000, BR_RAM_SIZE = 0x10000,
};

/* IN word (0x500000): bit 0 up, 1 down, 2 left, 3 right, 4 shot, 5 bomb, 6 special (P1); <<8 for P2. */
enum { BR_UP = 1, BR_DOWN = 2, BR_LEFT = 4, BR_RIGHT = 8, BR_B1 = 0x10, BR_B2 = 0x20, BR_B3 = 0x40 };
/* SYS byte (0x500003): service 0x01, test 0x04, coin1 0x08, coin2 0x10, start1 0x20, start2 0x40. */
enum { BR_SERVICE = 0x01, BR_TEST = 0x04, BR_COIN1 = 0x08, BR_COIN2 = 0x10, BR_START1 = 0x20, BR_START2 = 0x40 };

typedef struct {
	uint8_t *prg;                      /* BR_PRG_SIZE, writable: host patches go in here */
	const uint8_t *snd;
	const uint8_t *gfx;
	const uint8_t *oki[2];
} BrRoms;

/* Host hook: an illegal opcode the patch put in the program. Return nonzero when handled (the PC is then
 * wherever the hook left it), zero for the 68000's own illegal-instruction exception. */
typedef int (*BrHook)(int opcode);

int br_init(const BrRoms *roms);       /* 0 on success (decodes the graphics: allocates ~33 MB) */
void br_reset(void);
void br_set_program(uint8_t *prg);     /* another BR_PRG_SIZE program (ORIGINAL / CHASE!); takes effect at br_reset */
void br_set_hook(BrHook hook);
void br_set_inputs(uint16_t in, uint8_t sys);
void br_set_dips(uint16_t dsw, uint8_t dswc);
void br_run_frame(void);               /* one frame, 262 lines; the picture is drawn at the start of vblank */

const uint32_t *br_frame(void);        /* BR_W x BR_H, 0x00RRGGBB */
const int16_t *br_audio(void);         /* BR_AUDIO_PER_FRAME mono samples of the last frame */
uint8_t *br_ram(void);                 /* 0x200000-0x20FFFF, big-endian bytes */
uint32_t br_frame_count(void);
/* The tune the 68000 last started (MUSIC TEST's numbers 0x00..0x19), -1 = none; its title from the sound ROM's
 * own list (trailing blanks trimmed; "" when unknown). */
int br_music(void);
int br_music_title(int n, char *out, int size);
const uint8_t *br_text_glyphs(void);   /* 1024 x 8x8 one-byte pixels, as last DMA'd */

/* Split board (the 360's threads): br_run_frame then neither draws nor makes the YM's sound. After each frame the
 * host takes the VDP copy made at the picture's line (br_vdp_snapshot, for vdp_render) and the frame's sound inputs
 * (br_sound_take), and hands them to other threads; br_sound_mix (one thread only) makes the frame's
 * BR_AUDIO_PER_FRAME samples from them, the same as the unsplit board would. */
enum { BR_YM_LOG = 2048, BR_SOUND_MAX = 1200 };
typedef struct { uint16_t pos; uint8_t port, data; } BrYmWrite;
typedef struct {
	int ycount, ylog_n, on[2];
	BrYmWrite ylog[BR_YM_LOG];
	int32_t obuf[2][BR_SOUND_MAX];
} BrSound;
void br_set_split(int on);              /* br_vdp_snapshot() is in vdp.h */
/* The 68000 stands still (no instructions, no vblank IRQ) while br_run_frame runs the Z80, YM and OKIs: the title
 * and the host menu, where the music plays on. Off = the whole board again. */
void br_hold_main(int on);
void br_sound_take(BrSound *s);
void br_sound_mix(const BrSound *s, int16_t *out);

/* Timing of br_run_frame's parts, in the clock's ticks (0 = off). */
typedef uint64_t (*BrClock)(void);
typedef struct { uint64_t m68k, z80, sound; } BrProfile;
void br_set_clock(BrClock c);
void br_profile_take(BrProfile *p);    /* the totals since the last take */

/* 68000 access for hooks and the front end. */
uint32_t br_reg(int which);            /* Musashi register enum (M68K_REG_D0 ...) */
void br_set_reg(int which, uint32_t v);
uint8_t br_rd8(uint32_t a);
uint16_t br_rd16(uint32_t a);
void br_wr8(uint32_t a, uint8_t v);
void br_wr16(uint32_t a, uint16_t v);

/* Volumes 0..256 (256 = the board's mix). */
void br_set_volume(int music_ym, int samples_oki);
void br_set_volumes(int ym, int oki0, int oki1);  /* each OKI its own (MUSIC = YM + OKI 1, EFFECTS = OKI 0) */

#ifdef __cplusplus
}
#endif
#endif

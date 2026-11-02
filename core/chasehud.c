/* batrider360: CHASE!'s HUD parts that the N64 draws in host code, ported from the N64 build (Coin-Op Game Studio's
 * kit, decompiled in batrider-chase/work): its per-frame HUD function 0x80290AD8 hands four draw jobs to a cell cache
 * (0x8028D0C8, which only replays the cells when the inputs are unchanged, so drawing every frame is the same thing):
 *   job 0 = 0x80290DC8, the top row (line 39): manual p.11 items 1 (score), 2 (top score, amber), 3 (boss time);
 *   job 2 = 0x8028F8D8, the boss HP bar (line 36): item 7, amber, red in its last quarter;
 *   job 1 = 0x8028F158, the stage line (line 38): the game's own row with the lives, the EXTEND / MAX LIVES /
 *   CASH IN messages and the CHASE tag (test 6; see draw_stage below);
 *   job 3 = 0x8028DE48 (the rail) is what CHASE!'s 68000 patch (batrider-chase src/hud.s) draws on this board.
 * Item 8, the boss's name, is the game's own: the 68000 writes the stage name and then the boss's name to line 37
 * (row 2 of its HUD buffer, copied at 0x10A0) and none of the N64's jobs touch that line.
 *
 * Text layer: cell = 0x200000 + (line + col*64)*2, line 0 = bottom, 30 cols x 40 lines, word = pal<<10 | tile.
 * Ordering: the vblank IRQ (0x1C24) first DMAs the text RAM (0x500082), then copies the 68000's dirty HUD rows from
 * its buffer 0x203D50 (one 0x80-byte row per line from 39 down, dirty bytes 0x204050..) into the tilemap; the main
 * loop then wakes from its vblank wait and CHASE!'s chase_frame (hud.s) redraws the rail and the stage line. The
 * N64 draws right after that (hook 0x2052), so its cells are the last word before the next DMA; here the same
 * point is CHASE!'s h_vbl after `jsr chase_frame` (chasehud_patch / CHASEHUD_VBL_SITE, chasehud.h). Called after
 * br_run_frame instead, the top row still holds (nothing else writes line 39 once its dirty byte is cleared), but
 * in play the main loop often wakes late, after br_run_frame returned, and hud.s's line 36 then beats the bar.
 * Like the N64 (Mu_204050 = 0 every frame), the top row's dirty byte is cleared so the 68000 stops copying that
 * row; when the HUD ends it is set again and the 68000 puts its own row back.
 *
 * hud.s also puts its own stage line (EXTEND / MAX LIVES / CASH IN flashes, cols 9..19, and the CHASE tag,
 * cols 25..29) on line 36, where the N64 has the boss bar alone (the MAME build keeps it there). Here those cells
 * are blanked every frame and the stage line is the N64's, on line 38, so the bar never hides the CHASE tag.
 * No allocation; all state is static (one board). */
#include "chasehud.h"

#include <string.h>

#include "board.h"
#include "stio.h"
#include "vdp.h"

enum {
	TEXT = 0x200000,
	TOP_LINE = 39, BAR_LINE = 36, COLS = 30,
	RAIL_LINE0 = 4, RAIL_LINES = 32, RAIL_COLS = 4,   /* hud.s's rail: columns 0-3, lines 4-35 (hud_clear's) */
	HUDBUF = 0x203D50,                 /* the 68000's HUD rows: row 0 = line 39 (30 words), row n at +n*0x80 */
	HUDDIRTY = 0x204050,               /* = HUDBUF + 0x300: one byte per row, the IRQ copies a row when set */
	STAGE_LINE = 38,                   /* = HUD row 1 (HUDBUF + 0x80) */
	P1 = 0x204062, P1_TEAM = 0x20407C, /* P1's status byte (bit 7: in the game) and team words */
	TEAMSZ = 0x20F9FD,                 /* pilots in the team - 1 */
	CS_MSG = 0x20101E, CS_MSGT = 0x201020, CS_BANK = 0x201026,  /* stage line: 1 EXTEND, 2 MAX LIVES, 3 CASH IN */
	GLOBAL_TIMEOUT = 0x20E9B8,         /* timer type -1: the game's own stage timeout word */
	CS_STATE = 0x201000,               /* CHASE!'s state word: 2 = a credit in play (chase.s update_state) */
	ENEMIES = 0x20869A, ENEMY_N = 96, ENEMY_SIZE = 0x72,
	EN_FLAGS = 0x00, EN_SCRIPT = 0x02, EN_PHASE = 0x20, EN_HP = 0x50,
	/* palettes (word bits 10..15) */
	PAL_WHITE = 0x30 << 10, PAL_BLUE = 0x31 << 10, PAL_RED = 0x32 << 10, PAL_AMBER = 0x34 << 10,
	/* the bar's tiles. Thin (the N64's CHASE_HUD_BAR_THIN = 1, its default): two glyphs it writes into the text
	 * layer's glyph RAM (0x8028D598(1)); otherwise the board font's blocks. */
	THIN_FULL = 0x349, THIN_PART = 0x34A,
	BLOCK_AMBER = 0x14, BLOCK_RED = 0x13, BLOCK_PART = 0x12,
};

/* The N64 (CHASE_HUD_BAR_THIN) draws the bar with its own thin glyphs; 0 = the font's block tiles instead. */
static const int bar_thin = 1;

/* Bosses for the HP bar: N64 table 0x803CD640, 44 x {script pointer, phase word +0x20, flag mask, flag value}.
 * An enemy is the boss while its script and phase match an entry and (flags & mask) == value (the entry's state:
 * on the screen, not entering or dying); with several, the last in slot order counts. */
static const struct { uint32_t script; uint16_t phase, mask, value; } bosses[44] = {
	{ 0x531B4, 0x00, 0xF201, 0x8200 }, { 0x531B4, 0x76, 0xF001, 0x8000 }, { 0x531B4, 0x7A, 0xF001, 0x8000 },
	{ 0x54E38, 0x00, 0xF001, 0x8000 }, { 0x566E2, 0x00, 0xF001, 0x8000 }, { 0x581C8, 0x00, 0xF001, 0x8000 },
	{ 0x581C8, 0x04, 0xF001, 0x8000 }, { 0x581C8, 0x06, 0xF001, 0x8000 }, { 0x60CC4, 0x00, 0xF001, 0x8000 },
	{ 0x60CC4, 0x02, 0xF001, 0x8000 }, { 0x63AB2, 0x00, 0xF001, 0x8000 }, { 0x63AB2, 0x02, 0xF001, 0x8000 },
	{ 0x6563A, 0x00, 0xF001, 0x8000 }, { 0x65BFC, 0x02, 0xF001, 0x8000 }, { 0x65D5A, 0x02, 0xF001, 0x8000 },
	{ 0x65E82, 0x02, 0xF001, 0x8000 }, { 0x75AE2, 0x00, 0xF001, 0x8000 }, { 0x7B282, 0x00, 0xF001, 0x8000 },
	{ 0x7B282, 0x02, 0xF001, 0x8000 }, { 0x7BB96, 0x00, 0xF001, 0x8000 }, { 0x7D424, 0x00, 0xF001, 0x8000 },
	{ 0x7A37C, 0x12, 0xF001, 0x8000 }, { 0x79B0C, 0x00, 0xF001, 0x8000 }, { 0x799BE, 0x1C, 0xF001, 0x8000 },
	{ 0x4DE40, 0x00, 0xF001, 0x8000 }, { 0x4DE40, 0x02, 0xF001, 0x8000 }, { 0x516D0, 0x00, 0xF001, 0x8000 },
	{ 0x49960, 0x02, 0xF001, 0x8000 }, { 0x4C7F4, 0x00, 0xF001, 0x8000 }, { 0x67CAA, 0x00, 0xF001, 0x8000 },
	{ 0x69456, 0x00, 0xF001, 0x8000 }, { 0x6AD02, 0x00, 0xF001, 0x8000 }, { 0x6AE04, 0x74, 0xF001, 0x8000 },
	{ 0x6D604, 0x02, 0xF001, 0x8000 }, { 0x6D604, 0x0A, 0xF001, 0x8000 }, { 0x6E6C2, 0x00, 0xF001, 0x8000 },
	{ 0x6E6C2, 0x02, 0xF001, 0x8000 }, { 0x6E6C2, 0x04, 0xF001, 0x8000 }, { 0x6E6C2, 0x06, 0xF001, 0x8000 },
	{ 0x6E6C2, 0x08, 0xF001, 0x8000 }, { 0x6E6C2, 0x0A, 0xF001, 0x8000 }, { 0x5A348, 0x00, 0xF001, 0x8000 },
	{ 0x5ED6A, 0x00, 0xF001, 0x8000 }, { 0x5C73E, 0x00, 0xF001, 0x8000 },
};

/* Where each boss keeps its time limit: N64 table 0x803CD510, 38 x {script, phase (-1 = any), type}; read by
 * 0x80290258 through the jump table 0x803C70DC. Types (enemy offsets): -1 the game's word 0x20E9B8, 0 none,
 * 1 long +0x42, 2 long +0x46, 3 minus word +0x48 (a count up: shown as "---"), 4 word +0x4A, 5 word +0x68,
 * 6 word +0x46, 7 byte +0x60 in seconds. Values are frames. */
static const struct { uint32_t script; int16_t phase, type; } timers[38] = {
	{ 0x531B4, 0, 2 }, { 0x54E38, 0, 4 }, { 0x54E38, 6, 4 }, { 0x54E38, 48, 4 },
	{ 0x54E38, 50, 4 }, { 0x54E38, 52, 4 }, { 0x54E38, 54, 4 }, { 0x566E2, 0, 5 },
	{ 0x581C8, 100, 4 }, { 0x585B8, 100, 4 }, { 0x60CC4, 0, 1 }, { 0x63AB2, 0, -1 },
	{ 0x63AB2, 2, -1 }, { 0x6563A, 0, 2 }, { 0x65BFC, 2, 2 }, { 0x65D5A, 2, 2 },
	{ 0x65E82, 2, 2 }, { 0x75AE2, 0, -1 }, { 0x7B282, 0, -1 }, { 0x7B282, 2, -1 },
	{ 0x7BB96, 0, -1 }, { 0x7D424, 0, -1 }, { 0x7A37C, 18, -1 }, { 0x79B0C, 0, -1 },
	{ 0x799BE, 28, -1 }, { 0x4DE40, 2, 2 }, { 0x516D0, 2, 2 }, { 0x49960, 2, 2 },
	{ 0x4C7F4, 42, 2 }, { 0x67CAA, 0, 3 }, { 0x69456, 0, 3 }, { 0x6AD02, 0, 2 },
	{ 0x6AE04, 116, 6 }, { 0x6D604, -1, 7 }, { 0x6E6C2, -1, 7 }, { 0x5A348, 0, 2 },
	{ 0x5ED6A, 0, 2 }, { 0x5C73E, 0, 2 },
};

static uint8_t *R;
static int rd8(uint32_t a) { return R[a - 0x200000]; }
static unsigned rd16(uint32_t a) { return (unsigned)(R[a - 0x200000] << 8 | R[a - 0x200000 + 1]); }
static uint32_t rd32(uint32_t a) { return (uint32_t)rd16(a) << 16 | rd16(a + 2); }
static void wr16(uint32_t a, unsigned v) { R[a - 0x200000] = (uint8_t)(v >> 8); R[a - 0x200000 + 1] = (uint8_t)v; }

static uint32_t cell(int line, int col) { return TEXT + (uint32_t)(line + col * 64) * 2; }
static void put(int line, int col, unsigned w) { if (col >= 0 && col < COLS) wr16(cell(line, col), w); }
static void puts_at(int line, int col, const char *s, unsigned pal)
{
	for (; *s; s++, col++)
		put(line, col, pal | (uint8_t)*s);
}

/* ---- state ---- */
static int on;                         /* drawing (CHASE!'s state was 2 last frame) */
static uint32_t boss_id;               /* 0x80621044: script of the boss the bar measures */
static unsigned boss_max;              /* 0x80621048: its highest HP seen = the full bar */
static int bar_on;                     /* the bar covers line 36 */
static uint16_t bar_saved[COLS];       /* line 36 as it was before the bar (cols hud.s never writes go back) */
static int glyphs_saved;
static uint8_t glyph_keep[2][64];      /* the font's own 0x349 / 0x34A while the thin bar's are in */

/* ---- the boss: N64 0x8028F6F8 (HP) and 0x80290258 (time) ---- */

/* HP of the boss (0 = none) and its script, which names it for the bar's maximum. */
static unsigned boss_hp(uint32_t *id)
{
	unsigned hp = 0;
	*id = 0;
	for (int i = 0; i < ENEMY_N; i++) {
		uint32_t e = ENEMIES + (uint32_t)i * ENEMY_SIZE;
		unsigned h = rd16(e + EN_HP);
		if (!h)
			continue;
		uint32_t s = rd32(e + EN_SCRIPT) & 0xFFFFFF;
		unsigned ph = rd16(e + EN_PHASE), fl = rd16(e + EN_FLAGS);
		for (int j = 0; j < 44; j++)
			if (bosses[j].script == s && bosses[j].phase == ph) {
				/* the first entry with this script and phase decides; a boss not in its fighting state is
				 * skipped, it does not end the search for a later one */
				if ((fl & bosses[j].mask) == bosses[j].value) {
					hp = h;
					*id = s;
				}
				break;
			}
	}
	return hp;
}

/* Frames left on the boss's clock (0 = no boss clock), as a signed 32-bit value: type 3 is negative. */
static int32_t boss_time(void)
{
	int32_t t = 0;
	for (int i = 0; i < ENEMY_N; i++) {
		uint32_t e = ENEMIES + (uint32_t)i * ENEMY_SIZE;
		uint32_t s = rd32(e + EN_SCRIPT) & 0xFFFFFF;
		int ph = (int16_t)rd16(e + EN_PHASE), type = 0, found = 0;
		/* an entry for this script and phase wins; else the last "any phase" entry for the script */
		for (int j = 0; j < 38; j++) {
			if (timers[j].script != s)
				continue;
			if (timers[j].phase == ph) {
				type = timers[j].type;
				found = 1;
				break;
			}
			if (timers[j].phase < 0) {
				type = timers[j].type;
				found = 1;
			}
		}
		if (!found)
			continue;
		int32_t v;
		switch (type) {
		case -1: v = (int32_t)rd16(GLOBAL_TIMEOUT); break;
		case 1: v = (int32_t)rd32(e + 0x42); break;
		case 2: v = (int32_t)rd32(e + 0x46); break;
		case 3: v = -(int32_t)rd16(e + 0x48); break;
		case 4: v = (int32_t)rd16(e + 0x4A); break;
		case 5: v = (int32_t)rd16(e + 0x68); break;
		case 6: v = (int32_t)rd16(e + 0x46); break;
		case 7: v = rd8(e + 0x60) * 60; break;
		default: v = 0; break;
		}
		if (v)
			t = v;                     /* the last boss in slot order with a running clock */
	}
	return t;
}

/* ---- job 0: the top row (N64 0x80290DC8) ---- */
static void draw_top(int32_t frames)
{
	for (int c = 0; c < COLS; c++)
		put(TOP_LINE, c, 0);
	puts_at(TOP_LINE, 0, "1P", PAL_BLUE);
	/* the 68000's own row (its buffer, which it keeps up to date): score cols 3..9 in white, top score cols
	 * 13..19 moved one left in amber; its "1up"/rank, "top" and the 2P part are left out */
	for (int i = 0; i < 7; i++) {
		put(TOP_LINE, 3 + i, PAL_WHITE | (rd16(HUDBUF + 6 + i * 2) & 0x3FF));
		put(TOP_LINE, 12 + i, PAL_AMBER | (rd16(HUDBUF + 0x1A + i * 2) & 0x3FF));
	}
	if (!frames)
		return;
	puts_at(TOP_LINE, 20, "TIME", PAL_BLUE);
	int32_t sec = frames / 60;         /* C's division truncates toward zero, as the N64's does */
	if ((uint32_t)sec < 1000) {        /* unsigned: a count up (negative) shows "---" */
		unsigned pal = frames < 660 ? PAL_RED : PAL_AMBER; /* red in the last 11 seconds */
		put(TOP_LINE, 25, pal | (unsigned)('0' + frames / 6000 % 10));
		put(TOP_LINE, 26, pal | (unsigned)('0' + sec / 10 % 10));
		put(TOP_LINE, 27, pal | (unsigned)('0' + sec % 10));
	} else {
		puts_at(TOP_LINE, 25, "---", PAL_WHITE);
	}
	put(TOP_LINE, 29, PAL_BLUE | 'S');
}

/* ---- the thin bar's glyphs (N64 0x8028D598(1)): 8x8, 4 bpp, pixel x = along the board's 320-pixel lines =
 * up the upright screen. 0x349: x 2..5 colour 4 on every row (a band 4 pixels high across the cell);
 * 0x34A: the same band in colour 1 (a cell less than half full). ---- */
static void thin_glyph(uint8_t *g, int colour)
{
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			g[y * 8 + x] = (uint8_t)(x >= 2 && x < 6 ? colour : 0);
}

static void glyphs_in(void)
{
	uint8_t want[2][64];
	thin_glyph(want[0], 4);
	thin_glyph(want[1], 1);
	for (int k = 0; k < 2; k++) {
		uint8_t *g = vdp.txgfx + (THIN_FULL + k) * 64;
		if (memcmp(g, want[k], 64)) {
			/* the font's own glyph (first time, or the 68000 DMA'd the glyphs again): keep it, put ours in */
			memcpy(glyph_keep[k], g, 64);
			memcpy(g, want[k], 64);
		}
	}
	glyphs_saved = 1;
}

static void glyphs_out(void)
{
	if (!glyphs_saved)
		return;
	memcpy(vdp.txgfx + THIN_FULL * 64, glyph_keep[0], 64);
	memcpy(vdp.txgfx + THIN_PART * 64, glyph_keep[1], 64);
	glyphs_saved = 0;
}

/* hud.s's columns on line 36: messages 9..19, the CHASE tag 25..29 (blanked here every frame) */
static int hud_s_col(int c) { return (c >= 9 && c <= 19) || c >= 25; }

static void hud_s_blank(void)
{
	for (int c = 0; c < COLS; c++)
		if (hud_s_col(c))
			put(BAR_LINE, c, 0);
}

/* ---- job 1: the stage line (N64 0x8028F158) ----
 * Line 38 cleared, then: the 68000's own row 1 (HUDBUF + 0x80), cols 0..7 as they are (the riders in stock);
 * with P1 in the game, 'x' + the lives (chase.s count_lives: riders flying among the first TEAMSZ + 1, at most 4,
 * plus the bank; 9 at most, amber, red under 2) at col 2n, 2n + 1 (n = TEAMSZ + 1, at most 4), the rest from col
 * 2n + 3 (else from col 8). There either the message (EXTEND / MAX LIVES / CASH IN, amber, while it lasts) or the
 * row's cols 8..18 again, as words: split at blanks and where digits meet letters ("#12ADVANCE" -> "#12 ADVANCE"),
 * set one blank apart, white, as long as a word ends by col 24; tiles past 0x7E are skipped. CHASE at col 25. */
static int is_digit(unsigned w) { return w - '0' < 10; }
static int is_letter(unsigned w) { return (w | 0x20) - 'a' < 26; }

static void draw_stage(void)
{
	const uint32_t row = HUDBUF + 0x80;
	for (int c = 0; c < COLS; c++)
		put(STAGE_LINE, c, 0);
	for (int c = 0; c < 8; c++)
		put(STAGE_LINE, c, rd16(row + c * 2));
	int at = 8;
	if (rd8(P1) & 0x80) {
		int n = rd8(TEAMSZ) + 1;
		if (n > 4)
			n = 4;
		int lives = (int)rd16(CS_BANK);
		for (int i = 0; i < n; i++) {
			const unsigned w = rd16(P1_TEAM + i * 2);
			if (w != 0xFFFF && !(w & 1))
				lives++;
		}
		put(STAGE_LINE, 2 * n, PAL_WHITE | 'x');
		put(STAGE_LINE, 2 * n + 1, (lives < 2 ? PAL_RED : PAL_AMBER) | (unsigned)('0' + (lives > 9 ? 9 : lives)));
		at = 2 * n + 3;
	}
	const unsigned msg = rd16(CS_MSGT) ? rd16(CS_MSG) : 0;
	if (msg >= 1 && msg <= 3) {
		static const char *const m[3] = { "EXTEND", "MAX LIVES", "CASH IN" };
		puts_at(STAGE_LINE, at, m[msg - 1], PAL_AMBER);
	} else {
		char word[30];
		int len = 0, first = 1, col = at;
		unsigned prev = 0;
		for (int c = 8; c <= 19; c++) {
			const unsigned w = c < 19 ? rd16(row + c * 2) & 0x3FF : 0;  /* col 19 = the end of the row */
			const int printable = w > 0x20 && w < 0x7F;
			const int boundary = (is_digit(prev) && is_letter(w)) || (is_letter(prev) && is_digit(w));
			if (printable && (!len || !boundary)) {
				if (len < 30)
					word[len++] = (char)w;
				prev = w;
				continue;
			}
			if (!printable && w != 0 && w != 0x20)
				continue;                  /* an icon tile: skipped, the word goes on */
			if (len) {                     /* a blank, the row's end, or digits meeting letters: the word out */
				const int start = col + !first;
				if (start + len - 1 >= 24)
					break;
				if (!first)
					put(STAGE_LINE, col, PAL_WHITE | ' ');
				for (int i = 0; i < len; i++)
					put(STAGE_LINE, start + i, PAL_WHITE | (uint8_t)word[i]);
				col = start + len;
				first = 0;
				len = 0;
			}
			if (printable)                 /* digits meeting letters: this one starts the next word */
				word[len++] = (char)w;
			prev = w;
		}
	}
	puts_at(STAGE_LINE, 25, "CHASE", PAL_AMBER);
}

static void bar_off(void)
{
	if (!bar_on)
		return;
	/* the cells hud.s never rewrites go back as they were; hud.s redrew its own this frame already */
	for (int c = 0; c < COLS; c++)
		if (!hud_s_col(c))
			wr16(cell(BAR_LINE, c), bar_saved[c]);
	bar_on = 0;
	glyphs_out();
}

/* ---- job 2: the boss HP bar (N64 0x8028F8D8) ---- */
static void draw_bar(unsigned hp, uint32_t id)
{
	if (!hp) {                         /* (the N64's minimum, CHASE_HUD_BOSS_HP, is 0) */
		boss_id = 0;
		boss_max = 0;
		bar_off();
		return;
	}
	if (id != boss_id) {               /* another boss: its HP now is the full bar */
		boss_id = id;
		boss_max = hp;
	} else if (hp > boss_max) {
		boss_max = hp;
	}
	if (!bar_on) {
		for (int c = 0; c < COLS; c++)
			bar_saved[c] = (uint16_t)rd16(cell(BAR_LINE, c));
		bar_on = 1;
	}
	if (bar_thin)
		glyphs_in();
	const int amber = boss_max < hp * 4;    /* red from a quarter of the full bar down */
	const unsigned pal = amber ? PAL_AMBER : PAL_RED;
	const unsigned full = bar_thin ? THIN_FULL : amber ? BLOCK_AMBER : BLOCK_RED;
	const unsigned part = bar_thin ? THIN_PART : BLOCK_PART;
	const unsigned w = hp * 240 / boss_max; /* 30 cells of 8 */
	for (int c = 0; c < COLS; c++) {
		const unsigned u = (unsigned)c * 8;
		unsigned word = pal;            /* empty: tile 0 */
		if (w >= u + 8 || (w > u && w - u > 3))
			word = pal | full;
		else if (w > u)
			word = pal | part;          /* 1..3 eighths */
		put(BAR_LINE, c, word);
	}
}

static uint32_t chase_frame_at;        /* the target of h_vbl's `jsr chase_frame` */
static uint16_t rail_keep[RAIL_LINES][RAIL_COLS];

uint32_t chasehud_pre(void)
{
	R = br_ram();
	for (int l = 0; l < RAIL_LINES; l++)
		for (int c = 0; c < RAIL_COLS; c++)
			rail_keep[l][c] = (uint16_t)rd16(cell(RAIL_LINE0 + l, c));
	return chase_frame_at;
}

/* After hud_clear: the cells it blanked that held something the rail never draws go back */
static void rail_restore(void)
{
	for (int l = 0; l < RAIL_LINES; l++)
		for (int c = 0; c < RAIL_COLS; c++) {
			const unsigned k = rail_keep[l][c];
			if ((k & 0x380) && rd16(cell(RAIL_LINE0 + l, c)) == (PAL_WHITE | ' '))
				wr16(cell(RAIL_LINE0 + l, c), k);
		}
}

int chasehud_patch(unsigned char *prg)
{
	static const uint8_t jmp2006[4] = { 0x4E, 0xF8, 0x20, 0x06 };   /* jmp (0x2006).w */
	if (memcmp(prg + CHASEHUD_VBL_SITE, jmp2006, 4))
		return -1;
	/* test 11: `jsr <abs.l>` before it */
	uint8_t *j = prg + CHASEHUD_PRE_SITE;
	if (j[0] != 0x4E || j[1] != 0xB9)
		return -1;
	chase_frame_at = (uint32_t)j[2] << 24 | (uint32_t)j[3] << 16 | (uint32_t)j[4] << 8 | j[5];
	j[0] = 0x4A;
	j[1] = 0xFC;
	prg[CHASEHUD_VBL_SITE] = 0x4A;     /* ILLEGAL, the board's host-hook opcode */
	prg[CHASEHUD_VBL_SITE + 1] = 0xFC;
	return 0;
}

void chasehud_reset(void)
{
	on = 0;
	boss_id = 0;
	boss_max = 0;
	bar_on = 0;
	glyphs_saved = 0;
}

/* test 8: save states */
void chasehud_state_io(Stio *s)
{
	STIO(s, on);
	STIO(s, boss_id);
	STIO(s, boss_max);
	STIO(s, bar_on);
	STIO(s, bar_saved);
	STIO(s, glyphs_saved);
	STIO(s, glyph_keep);
}

void chasehud_frame(void)
{
	R = br_ram();
	if (rd16(CS_STATE) != 2) {
		if (on) {
			/* the credit's play ended (stage clear, continue, game over): the board's rows come back */
			R[HUDDIRTY - 0x200000] = 1;
			R[HUDDIRTY + 1 - 0x200000] = 1;
			bar_off();
			hud_s_blank();
			rail_restore();
			boss_id = 0;
			boss_max = 0;
			on = 0;
		}
		return;
	}
	on = 1;
	R[HUDDIRTY - 0x200000] = 0;        /* the 68000 stops copying its top row over ours (N64: Mu_204050 = 0) */
	R[HUDDIRTY + 1 - 0x200000] = 0;    /* and its row 1 over the stage line */
	uint32_t id;
	unsigned hp = boss_hp(&id);
	draw_top(boss_time());
	hud_s_blank();
	draw_stage();
	draw_bar(hp, id);
}

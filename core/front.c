/* batrider360: the N64 front end's title and splash (front.h). */
#include "front.h"

#include <string.h>

#include "zlib.h"
#include "front_assets.h"

_Static_assert((int)FRONT_W == (int)FRONT_TEX_W && (int)FRONT_H == (int)FRONT_TEX_H,"front_assets.h is the texture's size");

static uint8_t splash[FRONT_TEX_H * FRONT_TEX_W * 4];

static int unpack(const uint8_t *z, size_t zn, uint8_t *dst, size_t n)
{
	z_stream s;
	memset(&s, 0, sizeof s);
	if (inflateInit(&s) != Z_OK)
		return -1;
	s.next_in = (Bytef *)z;
	s.avail_in = (uInt)zn;
	s.next_out = dst;
	s.avail_out = (uInt)n;
	const int r = inflate(&s, Z_FINISH);
	inflateEnd(&s);
	return r == Z_STREAM_END && s.total_out == n ? 0 : -1;
}

int front_init(void)
{
	return unpack(splash_z, sizeof splash_z, splash, sizeof splash);
}

/* ---- text: the board's own text layer and font ----
 * The upright picture is 240x320; the text layer's tile (col, line) sits in txram[col * 64 + line] with line
 * counted from the bottom through the line scroll (as the game's own title text). x, y: the glyph's upright top left,
 * x a multiple of 8, y as the N64 title's (4 below a multiple of 8 there). */
enum { PAL_WHITE = 0x30, PAL_BLUE = 0x31, PAL_RED = 0x3d, PAL_AMBER = 0x3e, PAL_PINK = 0x3f };

static void text(Vdp *v, int x, int y, int pal, const char *s)
{
	const int row = v->lineselect[0] & 0xff;
	const int sx = (v->linescroll[row] & 0x1ff) - 0x1d4;
	for (; *s; s++, x += 8) {
		const int col = x >> 3;
		const int line = ((BR_W - 1 - (y + 7) + sx) & 0x1ff) >> 3;
		if (col >= 0 && col < 64)
			v->txram[col * 64 + line] = (uint16_t)(pal << 10 | (uint8_t)*s);
	}
}

/* Spare text palettes, shaped as 0x30 (index 1 darkest .. 8 white at the glyph's top; 9 the black edge): the N64
 * title's pink highlight (from the manual's N64 title shot), and the game's amber and red ramps (its palettes 0x34
 * and 0x32 in the attract; the title screen loads others there). */
static void ramp(Vdp *v, int pal, const uint32_t rgb[8])
{
	uint16_t *p = v->pal + 0x400 + pal * 16;
	memcpy(p, v->pal + 0x400 + PAL_WHITE * 16, 16 * sizeof *p);
	for (int i = 0; i < 8; i++)
		p[1 + i] = (uint16_t)((rgb[i] >> 19 & 31) | (rgb[i] >> 11 & 31) << 5 | (rgb[i] >> 3 & 31) << 10);
}

void front_text_palettes(Vdp *v)
{
	static const uint32_t pink[8] = { 0x848484, 0xBD5ABD, 0xFF7BFF, 0xFF94FF, 0xFFB5FF, 0xFFCEFF, 0xFFE7FF, 0xFFFFFF };
	static const uint32_t amber[8] = { 0xFF9400, 0xFFAD00, 0xFFD600, 0xFFFF00, 0xFFFF7B, 0xFFFFA5, 0xFFFFCE, 0xFFFFFF };
	static const uint32_t red[8] = { 0x731010, 0xB52929, 0xFF4242, 0xFF7373, 0xEFA5A5, 0xEFBDBD, 0xF7DEDE, 0xFFFFFF };
	ramp(v, PAL_PINK, pink);
	ramp(v, PAL_AMBER, amber);
	ramp(v, PAL_RED, red);
}

void front_title_text(Vdp *v, int sel, int mode, const char *note)
{
	static const char *const items[FRONT_ITEMS] = { "START", "PRACTICE", "OPTIONS" };
	for (int i = 0; i < 0x1000; i++)
		v->txram[i] = 0x20;
	front_text_palettes(v);
	for (int i = 0; i < FRONT_ITEMS; i++) {
		const int y = 156 + i * 16;
		if (i == sel)
			text(v, 72, y, PAL_PINK, ">");
		text(v, 88, y, i == sel ? PAL_PINK : PAL_WHITE, items[i]);
	}
	/* until the host menu (test 3): LB / RB change MODE here */
	text(v, 64, 220, PAL_AMBER, mode == FRONT_CHASE ? "MODE  CHASE!" : "MODE  ORIGINAL");
	if (note)
		text(v, (240 - 8 * (int)strlen(note)) / 2 & ~7, 236, PAL_RED, note);
	text(v, 72, 300, PAL_WHITE, "ARRANGED BY");
	text(v, 40, 308, PAL_BLUE, "COIN-OP GAME STUDIO");
}

/* ---- pictures ---- */

void front_doubled(const uint32_t *pic, uint32_t *tex)
{
	for (int y = 0; y < BR_H; y++) {
		uint32_t *a = tex + y * 2 * FRONT_TEX_W, *b = a + FRONT_TEX_W;
		const uint32_t *s = pic + y * BR_W;
		for (int x = 0; x < BR_W; x++)
			a[x * 2] = a[x * 2 + 1] = 0xFF000000u | s[x];
		memcpy(b, a, FRONT_TEX_W * sizeof *a);
	}
}

void front_splash(uint32_t *tex)
{
	const uint8_t *s = splash;
	for (int i = 0; i < FRONT_TEX_W * FRONT_TEX_H; i++, s += 4)
		tex[i] = 0xFF000000u | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
}

void front_fade(uint32_t *tex, int level)
{
	if (level >= 256)
		return;
	if (level < 0)
		level = 0;
	for (int i = 0; i < FRONT_TEX_W * FRONT_TEX_H; i++) {
		const uint32_t c = tex[i];
		const uint32_t rb = ((c & 0xFF00FF) * (uint32_t)level >> 8) & 0xFF00FF;
		const uint32_t g = ((c & 0x00FF00) * (uint32_t)level >> 8) & 0x00FF00;
		tex[i] = 0xFF000000u | rb | g;
	}
}

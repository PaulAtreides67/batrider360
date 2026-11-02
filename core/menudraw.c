/* batrider360 test 11: the menus as GADGETS panels (menudraw.h). */
#include "menudraw.h"

#include <string.h>

#include "front.h"
#include "gadgets.h"

_Static_assert((int)MD_W == (int)FRONT_TEX_H && (int)MD_H == (int)FRONT_TEX_W, "the upright texels are the texture's turned");

static uint32_t *T;

/* upright (x, y) = texture row x, column FRONT_TEX_W - 1 - y (as fe.c's texel) */
static inline uint32_t *at(int x, int y)
{
	return T + x * FRONT_TEX_W + (FRONT_TEX_W - 1 - y);
}

void md_target(uint32_t *tex)
{
	T = tex;
}

static uint32_t blend(uint32_t d, uint32_t s, unsigned a)
{
	const uint32_t rb = (((s & 0xFF00FF) * a + (d & 0xFF00FF) * (256 - a)) >> 8) & 0xFF00FF;
	const uint32_t g = (((s & 0xFF00) * a + (d & 0xFF00) * (256 - a)) >> 8) & 0xFF00;
	return 0xFF000000u | rb | g;
}

static int clip(int *x, int *y, int *w, int *h)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > MD_W) *w = MD_W - *x;
	if (*y + *h > MD_H) *h = MD_H - *y;
	return *w > 0 && *h > 0;
}

void md_fill(int x, int y, int w, int h, uint32_t argb)
{
	if (!clip(&x, &y, &w, &h))
		return;
	for (int i = 0; i < w; i++)
		for (int j = 0; j < h; j++)
			*at(x + i, y + j) = argb;
}

void md_darken(int x, int y, int w, int h, int keep256)
{
	if (!clip(&x, &y, &w, &h))
		return;
	for (int i = 0; i < w; i++)
		for (int j = 0; j < h; j++) {
			uint32_t *p = at(x + i, y + j);
			*p = blend(0xFF000000u, *p, (unsigned)keep256);
		}
}

/* ---- Baffo's frame ---- */

/* piece k drawn at (x, y), only its part inside cw x ch (a repeated piece cut at the end) */
static void piece(int k, int x, int y, int cw, int ch)
{
	int w, h;
	const uint8_t *p = gadgets_menu_piece(k, &w, &h);
	if (!p)
		return;
	if (cw > w)
		cw = w;
	if (ch > h)
		ch = h;
	for (int j = 0; j < ch; j++) {
		const int yy = y + j;
		if (yy < 0 || yy >= MD_H)
			continue;
		for (int i = 0; i < cw; i++) {
			const int xx = x + i;
			const uint8_t *q = p + (j * w + i) * 4;
			if (xx < 0 || xx >= MD_W || !q[3])
				continue;
			const uint32_t s = (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2];
			uint32_t *d = at(xx, yy);
			*d = q[3] == 255 ? 0xFF000000u | s : blend(*d, s, q[3] + 1u);
		}
	}
}

/* one band (top, body or bottom: GAD_MK_TL / BL / EL) across w, ch high */
static void band(int k0, int x, int y, int w, int ch)
{
	int lw, mw, rw, h;
	if (!gadgets_menu_piece(k0, &lw, &h) || !gadgets_menu_piece(k0 + 1, &mw, &h) ||
		!gadgets_menu_piece(k0 + 2, &rw, &h))
		return;
	piece(k0, x, y, lw, ch);
	for (int mx = x + lw; mx < x + w - rw; mx += mw)
		piece(k0 + 1, mx, y, x + w - rw - mx, ch);
	piece(k0 + 2, x + w - rw, y, rw, ch);
}

enum { GRID = 0xFF1C1C1Cu, GRID_STEP = 20 };   /* list_frame's grid: (28, 28, 28), every 41 native pixels */

void md_panel(int x, int y, int w, int h)
{
	int pw, ph;
	if (!gadgets_menu_piece(GAD_MK_BL, &pw, &ph)) {   /* no art: a plain dark box */
		md_darken(x, y, w, h, 64);
		return;
	}
	band(GAD_MK_TL, x, y, w, MD_TOP);
	for (int by = y + MD_TOP; by < y + h - MD_BOTTOM; by += ph)
		band(GAD_MK_BL, x, by, w, y + h - MD_BOTTOM - by);
	band(GAD_MK_EL, x, y + h - MD_BOTTOM, w, MD_BOTTOM);
	/* the plate and the body emptied (the source frame's own title and rows), the body's upright grid lines */
	md_fill(x + MD_PLATE_Y, y + MD_PLATE_Y, w - 2 * MD_PLATE_Y, MD_PLATE_H, 0xFF000000u);
	const int bx = x + MD_BODY_X, by = y + MD_BODY_Y, bw = w - 2 * MD_BODY_X, bh = h - MD_BODY_Y - MD_BODY_END;
	md_fill(bx, by, bw, bh, 0xFF000000u);
	for (int gx = x + 10; gx < x + w - 10; gx += GRID_STEP)
		if (gx >= bx && gx < bx + bw)
			md_fill(gx, by, 1, bh, GRID);
}

void md_hline(int x, int y, int w)
{
	md_fill(x, y, w, 1, GRID);
}

/* ---- the board's font, the gadgets' tints ---- */

static const uint8_t tints[MD_TINTS][2][3] = {   /* bright end, dim end of the font's shades 2..8 (gadgets_draw.c) */
	{ { 255, 255, 255 }, { 150, 150, 160 } }, { { 255, 120, 120 }, { 200, 0, 0 } },
	{ { 255, 255, 170 }, { 230, 190, 0 } }, { { 255, 210, 120 }, { 230, 120, 0 } },
	{ { 170, 220, 255 }, { 40, 110, 255 } }, { { 180, 255, 180 }, { 0, 170, 0 } },
	{ { 170, 255, 255 }, { 0, 170, 200 } }, { { 175, 175, 180 }, { 95, 95, 105 } },
};
static uint32_t shade[MD_TINTS][16];
static uint8_t font[128][64];
static int have_font;

int md_font(const uint8_t *glyphs)
{
	if (have_font || !glyphs)
		return have_font;
	unsigned sum = 0;
	for (int i = 0x41 * 64; i < 0x5B * 64; i++)
		sum += glyphs[i];
	if (!sum)
		return 0;
	for (int ch = 0; ch < 128; ch++)
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++)   /* upright (x, y) = board (7 - y, x) */
				font[ch][y * 8 + x] = glyphs[ch * 64 + x * 8 + (7 - y)];
	for (int t = 0; t < MD_TINTS; t++)
		for (int v = 1; v < 16; v++) {
			if (v >= 9) {
				shade[t][v] = 0xFF1E1E1E;
				continue;
			}
			const int k = v < 2 ? 0 : v - 2;
			uint32_t c = 0xFF000000u;
			for (int ch = 0; ch < 3; ch++) {
				const int a = tints[t][0][ch], b = tints[t][1][ch];
				c |= (uint32_t)(a + (b - a) * k / 6) << (16 - 8 * ch);
			}
			shade[t][v] = c;
		}
	have_font = 1;
	return 1;
}

int md_text_w(int half, const char *s)
{
	return 4 * half * (int)strlen(s);
}

/* test 12: the size in half steps (2 = 8 texels a letter, 3 = 12, 4 = 16); 1.5x doubles every other pixel */
void md_text(int x, int y, int half, int tint, const char *s)
{
	if (!have_font)
		return;
	const int n = 4 * half;
	for (; *s; s++, x += n) {
		const uint8_t *g = font[*s & 127];
		for (int j = 0; j < n; j++) {
			const int yy = y + j;
			if (yy < 0 || yy >= MD_H)
				continue;
			const uint8_t *row = g + (2 * j / half) * 8;
			for (int i = 0; i < n; i++) {
				const int xx = x + i, v = row[2 * i / half];
				if (v && xx >= 0 && xx < MD_W)
					*at(xx, yy) = shade[tint][v & 15];
			}
		}
	}
}

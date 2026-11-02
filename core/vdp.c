/* batrider360: GP9001 VDP + Toaplan text layer + palette, drawn in software at 320x240 (from a copy of the state:
 * the 360 draws on another thread while the board runs on).
 * Written from MAME's gp9001.cpp and toaplan_txtilemap.cpp (ref/mame); the layer and sprite offsets are MAME's.
 * Flip screen (the 0x80 scroll registers, the text layer's X flip) is not drawn: Batrider's FLIP DIP stays off. */
#include "vdp.h"

#include <stdlib.h>
#include <string.h>

Vdp vdp;
int vdp_layers = 0x1f;            /* debug: BG, FG, TOP, sprites, text */

static uint8_t *gfx8;                  /* the GP9001 ROMs as 8x8 cells of 64 one-byte pixels (0x80000 cells) */
enum { CELLS = 0x80000 };

/* MAME layout: 8x8, 4 bpp, planes { FRAC(1,2)+8, FRAC(1,2), 8, 0 }, rows 16 bits apart, 16 bytes a cell per half.
 * Test 14 (faster boot): a row at once. spread[v] holds the 8 pixels' bit for plane byte v, one byte each in pixel
 * order (as stored, so either byte order works); the shifts stay inside each byte (0/1 moved up at most 3 bits). */
int vdp_init(const uint8_t *gfx)
{
	gfx8 = malloc((size_t)CELLS * 64);
	if (!gfx8)
		return -1;
	static uint64_t spread[256];
	for (int v = 0; v < 256; v++) {
		uint8_t b[8];
		for (int x = 0; x < 8; x++)
			b[x] = (uint8_t)((v >> (7 - x)) & 1);
		memcpy(&spread[v], b, 8);
	}
	const uint8_t *lo = gfx, *hi = gfx + 0x800000;
	for (uint32_t c = 0; c < CELLS; c++) {
		uint8_t *d = gfx8 + (size_t)c * 64;
		for (int y = 0; y < 8; y++) {
			uint32_t o = c * 16 + y * 2;
			const uint64_t r = spread[hi[o + 1]] << 3 | spread[hi[o]] << 2 | spread[lo[o + 1]] << 1 | spread[lo[o]];
			memcpy(d + y * 8, &r, 8);
		}
	}
	vdp_reset();
	return 0;
}

void vdp_reset(void)
{
	vdp.voffs = 0;
	vdp.reg = 0;
	memset(vdp.scrollx, 0, sizeof vdp.scrollx);
	memset(vdp.scrolly, 0, sizeof vdp.scrolly);
	memset(vdp.bank, 0, sizeof vdp.bank);
	vdp.irq = 0;
}

/* VRAM through the auto-incrementing pointer: 0x0000-0x2FFF tilemaps, 0x3000-0x37FF sprites (mirrored at 0x3800). */
static uint16_t *vram_word(uint32_t offs)
{
	uint32_t a = (offs * 2) & 0x3fff;
	if (a >= 0x3800)
		a -= 0x800;
	return &vdp.vram[a >> 1];
}

/* 68000 side: 0x400000-0x40000D, MAME's offset ^ (0xC/2). */
uint16_t vdp_read(uint32_t a, int vpos)
{
	switch (((a & 0xe) ^ 0xc) & 0xc) { /* MAME: offset & (0xc/2), the low word bit ignored */
	case 0x4:
		return *vram_word(vdp.voffs++);
	case 0xc:
		return ((vpos + 15) % 262) >= 245;
	}
	return 0xffff;
}

void vdp_write(uint32_t a, uint16_t d, uint16_t mask)
{
	switch (((a & 0xe) ^ 0xc) & 0xc) { /* MAME: offset & (0xc/2), the low word bit ignored */
	case 0x0:
		vdp.voffs = (uint16_t)((vdp.voffs & ~mask) | (d & mask));
		break;
	case 0x4: {
		uint16_t *w = vram_word(vdp.voffs++);
		*w = (uint16_t)((*w & ~mask) | (d & mask));
		break;
	}
	case 0x8:
		if (mask & 0xff)
			vdp.reg = d & 0x8f;
		break;
	case 0xc: {
		int r = vdp.reg & 0x7f;
		if (r < 8)
			vdp.scrollx[r] = (uint16_t)((vdp.scrollx[r] & ~mask) | (d & mask)); /* scrollx[] holds all 8 registers */
		else if (r == 0x0e || r == 0x0f)
			vdp.irq = 0;
		break;
	}
	}
}

static const int xoff[3] = {-0x1d6, -0x1d8, -0x1da};

static uint16_t pix[BR_H][BR_W];
static uint8_t pri[BR_H][BR_W];

static uint32_t bank_cb(const Vdp *v, uint32_t code)
{
	return ((uint32_t)v->bank[(code >> 15) & 7] << 15) | (code & 0x7fff);
}

static void draw_layer(const Vdp *vs, int l)
{
	const uint16_t *v = vs->vram + l * 0x800;
	int sx = vs->scrollx[l * 2] + xoff[l];
	int sy = vs->scrollx[l * 2 + 1] - 0x1ef;
	for (int y = 0; y < BR_H; y++) {
		int ty = (y + sy) & 0x1ff;
		for (int x = 0; x < BR_W; x++) {
			int tx = (x + sx) & 0x1ff;
			int idx = ((ty >> 4) * 32 + (tx >> 4)) * 2;
			uint16_t attrib = v[idx];
			uint32_t cell = bank_cb(vs, (uint32_t)v[idx + 1] << 2) + ((ty >> 3) & 1) * 2 + ((tx >> 3) & 1);
			uint8_t p = gfx8[(size_t)(cell % CELLS) * 64 + (ty & 7) * 8 + (tx & 7)];
			if (!p)
				continue;
			uint8_t pr = (attrib >> 8) & 0x0e;   /* MAME: ((attrib & 0xfff) << 4) >> 12 & PRIMASK_TMAPS */
			if (pr >= pri[y][x]) {
				pix[y][x] = (uint16_t)(((attrib & 0x7f) << 4) | p);
				pri[y][x] = pr;
			}
		}
	}
}

static void draw_sprites(const Vdp *v)
{
	const uint16_t *src = v->vram + 0x1800;
	int clk = 0, clk_max = 432 * 262;
	int scrollx = (int16_t)v->scrollx[6] - 0x1cc, scrolly = (int16_t)v->scrollx[7] - 0x1ef;
	int old_x = (-scrollx) & 0x1ff, old_y = (-scrolly) & 0x1ff;
	for (int offs = 0; offs < 0x400; offs += 4) {
		clk += 8;
		if (clk > clk_max)
			return;
		uint16_t attrib = src[offs];
		if (!(attrib & 0x8000))
			continue;
		int priority = (attrib >> 8) & 0xf;
		uint32_t sprite = bank_cb(v, ((uint32_t)(attrib & 3) << 16) | src[offs + 1]);
		uint32_t color = (attrib >> 2) & 0x3f;
		int w = ((src[offs + 2] & 0x0f) + 1) * 8, h = ((src[offs + 3] & 0x0f) + 1) * 8;
		int sxb, syb;
		if (!(attrib & 0x4000)) {
			sxb = ((src[offs + 2] >> 7) - scrollx) & 0x1ff;
			syb = ((src[offs + 3] >> 7) - scrolly) & 0x1ff;
		} else {
			sxb = (old_x + (src[offs + 2] >> 7)) & 0x1ff;
			syb = (old_y + (src[offs + 3] >> 7)) & 0x1ff;
		}
		old_x = sxb;
		old_y = syb;
		int flipx = attrib & 0x1000, flipy = attrib & 0x2000;
		if (flipx) {
			sxb -= 7;
			if (sxb >= 0x1c0)
				sxb -= 0x200;
		} else if (sxb >= 0x180)
			sxb -= 0x200;
		if (flipy) {
			syb -= 7;
			if (syb >= 0x1c0)
				syb -= 0x200;
		} else if (syb >= 0x180)
			syb -= 0x200;
		for (int dy = 0; dy < h; dy += 8) {
			int sy = flipy ? syb - dy : syb + dy;
			for (int dx = 0; dx < w; dx += 8) {
				clk += 32;
				if (clk > clk_max)
					return;
				int sx = flipx ? sxb - dx : sxb + dx;
				sprite %= CELLS;
				const uint8_t *s = gfx8 + (size_t)sprite * 64;
				for (int yy = 0; yy < 8; yy++) {
					int py = sy + (flipy ? 7 - yy : yy);
					if ((unsigned)py >= BR_H) {
						s += 8;
						continue;
					}
					for (int xx = 0; xx < 8; xx++, s++) {
						int px = sx + (flipx ? 7 - xx : xx);
						if ((unsigned)px >= BR_W || priority < pri[py][px] || !*s)
							continue;
						pix[py][px] = (uint16_t)(color * 16 + *s);
						pri[py][px] = (uint8_t)priority;
					}
				}
				sprite++;
			}
		}
	}
}

/* Text glyphs (0x500080's DMA): 1024 x 8x8, 4 bpp, two 16-bit words a row, pixels in the order of the words' bytes
 * low then high, high nibble first (MAME's batrider_tx_tilelayout with WORD_XOR_LE). */
void vdp_text_glyphs(const uint8_t *ram)
{
	for (int t = 0; t < 1024; t++)
		for (int y = 0; y < 8; y++) {
			const uint8_t *r = ram + t * 32 + y * 4; /* big-endian words w0 = r[0..1], w1 = r[2..3] */
			uint8_t *d = vdp.txgfx + t * 64 + y * 8;
			d[0] = r[1] >> 4; d[1] = r[1] & 15; d[2] = r[0] >> 4; d[3] = r[0] & 15;
			d[4] = r[3] >> 4; d[5] = r[3] & 15; d[6] = r[2] >> 4; d[7] = r[2] & 15;
		}
}

/* 0x500082's DMA: text tilemap, palette, line select and line scroll from main RAM 0x200000-0x2033FF. */
void vdp_text_dma(const uint8_t *ram)
{
	for (int i = 0; i < 0x1000; i++)
		vdp.txram[i] = (uint16_t)(ram[i * 2] << 8 | ram[i * 2 + 1]);
	for (int i = 0; i < 0x800; i++)
		vdp.pal[i] = (uint16_t)(ram[0x2000 + i * 2] << 8 | ram[0x2000 + i * 2 + 1]);
	for (int i = 0; i < 0x100; i++) {
		vdp.lineselect[i] = (uint16_t)(ram[0x3000 + i * 2] << 8 | ram[0x3000 + i * 2 + 1]);
		vdp.linescroll[i] = (uint16_t)(ram[0x3200 + i * 2] << 8 | ram[0x3200 + i * 2 + 1]);
	}
}

static void draw_text(const Vdp *v)
{
	for (int y = 0; y < BR_H; y++) {
		int row = v->lineselect[y] & 0xff;
		int sx = (v->linescroll[row] & 0x1ff) - 0x1d4;
		const uint16_t *tr = v->txram + (row >> 3) * 64;
		for (int x = 0; x < BR_W; x++) {
			int col = (x + sx) & 0x1ff;
			uint16_t a = tr[col >> 3];
			uint8_t p = v->txgfx[(a & 0x3ff) * 64 + (row & 7) * 8 + (col & 7)];
			if (p)
				pix[y][x] = (uint16_t)(0x400 + ((a >> 10) & 0x3f) * 16 + p);
		}
	}
}

/* Pixels whose centre lies within r of (cx, cy) (1/128 px), under anything above level. */
static void draw_disc(int cx, int cy, int r, int level, uint16_t colour)
{
	const long long r2 = (long long)r * r;
	int y0 = (cy - r) >> 7, y1 = (cy + r) >> 7, x0 = (cx - r) >> 7, x1 = (cx + r) >> 7;
	if (y0 < 0) y0 = 0;
	if (x0 < 0) x0 = 0;
	if (y1 >= BR_H) y1 = BR_H - 1;
	if (x1 >= BR_W) x1 = BR_W - 1;
	for (int y = y0; y <= y1; y++) {
		const long long dy = y * 128 + 64 - cy;
		for (int x = x0; x <= x1; x++) {
			const long long dx = x * 128 + 64 - cx;
			if (dx * dx + dy * dy <= r2 && pri[y][x] < level)
				pix[y][x] = colour;
		}
	}
}

enum { DISC_RED = 0x800, DISC_WHITE = 0x801, BOX_COLOUR = 0x802, COLOURS = BOX_COLOUR + VDP_BOX_KINDS };
/* past the palette's 0x800 colours: the disc's two, then test 14's box kinds (enemies red, shot-only targets blue,
 * bullets magenta) */
static const uint32_t box_rgb[VDP_BOX_KINDS] = { 0xFF3030, 0x30A0FF, 0xFF40FF };

/* test 14: a box's outline, one pixel, over the layers and sprites */
static void draw_box(const VdpBox *b)
{
	const int x0 = (b->x - b->hw) >> 7, x1 = (b->x + b->hw - 1) >> 7;
	const int y0 = (b->y - b->hh) >> 7, y1 = (b->y + b->hh - 1) >> 7;
	const uint16_t c = (uint16_t)(BOX_COLOUR + (b->kind < VDP_BOX_KINDS ? b->kind : 0));
	for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < BR_W; x++) {
		if (y0 >= 0 && y0 < BR_H)
			pix[y0][x] = c;
		if (y1 >= 0 && y1 < BR_H)
			pix[y1][x] = c;
	}
	for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < BR_H; y++) {
		if (x0 >= 0 && x0 < BR_W)
			pix[y][x0] = c;
		if (x1 >= 0 && x1 < BR_W)
			pix[y][x1] = c;
	}
}

void vdp_render_discs(const Vdp *v, uint32_t *out, const VdpDisc *d, int n, const VdpBox *b, int nb)
{
	memset(pix, 0, sizeof pix);
	memset(pri, 0, sizeof pri);
	if (vdp_layers & 1) draw_layer(v, 0);
	if (vdp_layers & 2) draw_layer(v, 1);
	if (vdp_layers & 4) draw_layer(v, 2);
	if (vdp_layers & 8) draw_sprites(v);
	for (int i = 0; i < n; i++) {
		draw_disc(d[i].x, d[i].y, d[i].r + 128, d[i].level, DISC_RED);
		draw_disc(d[i].x, d[i].y, d[i].r, d[i].level, DISC_WHITE);
	}
	for (int i = 0; i < nb; i++)
		draw_box(&b[i]);
	if (vdp_layers & 16) draw_text(v);
	uint32_t rgb[COLOURS];
	for (int i = 0; i < 0x800; i++) {
		uint32_t c = v->pal[i], r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
		rgb[i] = (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
	}
	rgb[DISC_RED] = 0xF80000;           /* RGBA5551 0xF801, as the N64 */
	rgb[DISC_WHITE] = 0xFFFFFF;
	for (int k = 0; k < VDP_BOX_KINDS; k++)
		rgb[BOX_COLOUR + k] = box_rgb[k];
	for (int y = 0; y < BR_H; y++)
		for (int x = 0; x < BR_W; x++)
			*out++ = rgb[pix[y][x]];       /* every value is < COLOURS (layers, sprites and text stay < 0x800) */
}

void vdp_render(const Vdp *v, uint32_t *out)
{
	vdp_render_discs(v, out, NULL, 0, NULL, 0);
}

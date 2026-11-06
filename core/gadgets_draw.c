/* batrider360: GADGETS' panels on the 360's 720p canvas (viewer-upright, 0xAARRGGBB).
 * The art is Baffo's (tools/mkgadgets.py -> build/gen/gadgets_art.h): the static canvas per screen (wallpaper +
 * frames), the CHASE! frames as a layer, pre-scaled sprites, and every rectangle a live value goes in. Titles and
 * labels are written with the board's own 8x8 font, taken from br_text_glyphs() at run time (ROM data: not in the
 * .xex) and cached. A frame = one memcpy of the cached canvas + small blits; integer maths only, no divides in the
 * pixel loops (a frame runs on its own hardware thread, budget a few ms on Xenon). */
#include "gadgets.h"

#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "gadgets_art.h"

enum { NPIX = GAD_YOKO_W * GAD_YOKO_H };   /* the same for every layout */

static uint8_t spr[GA_SPR_RAW];             /* RGBA */
static uint32_t base[GAD_LAYOUTS][NPIX];   /* the static canvases, titles in once the font is there */
/* test 21: the wallpaper alone (a panel switched off shows it); allocated at init: a static array of this size went
 * into the .xex image (+11 MB) */
static uint32_t *wall[GAD_LAYOUTS];
static uint32_t chase_layer[GAD_LAYOUTS][GA_CHASE_PIX];
static int inited, titled;
static unsigned frame_n;

static const int16_t *chase_box(int layout)
{
	return layout == GAD_TATE ? ga_chase1_box : layout == GAD_YOKO_ORIG ? ga_chase2_box : ga_chase0_box;
}

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

int gadgets_init(void)
{
	if (inited)
		return 0;
	static uint8_t rgb[NPIX * 3];
	static uint8_t rgba[GA_CHASE_PIX * 4];
	if (unpack(ga_spr_z, sizeof ga_spr_z, spr, sizeof spr))
		return -1;
	const uint8_t *bz[GAD_LAYOUTS] = { ga_bg0_z, ga_bg1_z, ga_bg2_z };
	const size_t bn[GAD_LAYOUTS] = { sizeof ga_bg0_z, sizeof ga_bg1_z, sizeof ga_bg2_z };
	const uint8_t *wz[GAD_LAYOUTS] = { ga_wall0_z, ga_wall1_z, ga_wall2_z };
	const size_t wn[GAD_LAYOUTS] = { sizeof ga_wall0_z, sizeof ga_wall1_z, sizeof ga_wall2_z };
	const uint8_t *cz[GAD_LAYOUTS] = { ga_chase0_z, ga_chase1_z, ga_chase2_z };
	const size_t cn[GAD_LAYOUTS] = { sizeof ga_chase0_z, sizeof ga_chase1_z, sizeof ga_chase2_z };
	for (int s = 0; s < GAD_LAYOUTS; s++) {
		if (!wall[s] && !(wall[s] = malloc((size_t)NPIX * sizeof *wall[s])))
			return -5;
		if (unpack(wz[s], wn[s], rgb, sizeof rgb))
			return -4;
		for (int i = 0; i < NPIX; i++)
			wall[s][i] = 0xFF000000u | (uint32_t)rgb[i * 3] << 16 | (uint32_t)rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2];
		if (unpack(bz[s], bn[s], rgb, sizeof rgb))
			return -2;
		for (int i = 0; i < NPIX; i++)
			base[s][i] = 0xFF000000u | (uint32_t)rgb[i * 3] << 16 | (uint32_t)rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2];
		const int n = chase_box(s)[2] * chase_box(s)[3];
		if (unpack(cz[s], cn[s], rgba, (size_t)n * 4))
			return -3;
		for (int i = 0; i < n; i++)
			chase_layer[s][i] = (uint32_t)rgba[i * 4 + 3] << 24 | (uint32_t)rgba[i * 4] << 16 |
				(uint32_t)rgba[i * 4 + 1] << 8 | rgba[i * 4 + 2];
	}
	inited = 1;
	return 0;
}

/* ---- pixels ---- */

typedef struct {
	uint32_t *px;
	int w, h;
} Canvas;

static uint32_t blend(uint32_t d, uint32_t s, unsigned a)
{
	const uint32_t rb = (((s & 0xFF00FF) * a + (d & 0xFF00FF) * (256 - a)) >> 8) & 0xFF00FF;
	const uint32_t g = (((s & 0xFF00) * a + (d & 0xFF00) * (256 - a)) >> 8) & 0xFF00;
	return 0xFF000000u | rb | g;
}

static void fill(Canvas c, int x, int y, int w, int h, uint32_t col)
{
	if (x < 0) w += x, x = 0;
	if (y < 0) h += y, y = 0;
	if (x + w > c.w) w = c.w - x;
	if (y + h > c.h) h = c.h - y;
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			c.px[(y + j) * c.w + x + i] = col;
}

static void frame_rect(Canvas c, int x, int y, int w, int h, uint32_t col)
{
	fill(c, x, y, w, 2, col);
	fill(c, x, y + h - 2, w, 2, col);
	fill(c, x, y, 2, h, col);
	fill(c, x + w - 2, y, 2, h, col);
}

/* test 21: a switched-off panel's box goes back to the wallpaper */
static void unpaint(uint32_t *canvas, int screen, int panel)
{
	const int16_t *r = ga_panel[screen][panel];
	if (r[2] <= 0 || r[3] <= 0)
		return;
	const int w = screen == GAD_TATE ? GAD_TATE_W : GAD_YOKO_W, h = screen == GAD_TATE ? GAD_TATE_H : GAD_YOKO_H;
	const int x0 = r[0] < 0 ? 0 : r[0], y0 = r[1] < 0 ? 0 : r[1];
	const int x1 = r[0] + r[2] > w ? w : r[0] + r[2], y1 = r[1] + r[3] > h ? h : r[1] + r[3];
	for (int y = y0; y < y1; y++)
		memcpy(canvas + y * w + x0, wall[screen] + y * w + x0, (size_t)(x1 - x0) * 4);
}

/* sprite k at (x, y); mul = 0xRRGGBB multiplied in (0xFFFFFF = as is), as MAME's element colours */
static void blit(Canvas c, int k, int x, int y, uint32_t mul)
{
	const int w = ga_spr[k].w, h = ga_spr[k].h;
	const uint8_t *p = spr + ga_spr[k].off;
	const unsigned mr = (mul >> 16 & 255) + 1, mg = (mul >> 8 & 255) + 1, mb = (mul & 255) + 1;
	for (int j = 0; j < h; j++) {
		const int yy = y + j;
		if (yy < 0 || yy >= c.h)
			continue;
		for (int i = 0; i < w; i++) {
			const int xx = x + i;
			const uint8_t *q = p + (j * w + i) * 4;
			if (xx < 0 || xx >= c.w || !q[3])
				continue;
			const uint32_t s = (uint32_t)(q[0] * mr >> 8) << 16 | (uint32_t)(q[1] * mg >> 8) << 8 | (q[2] * mb >> 8);
			uint32_t *d = &c.px[yy * c.w + xx];
			*d = q[3] == 255 ? 0xFF000000u | s : blend(*d, s, q[3] + 1u);
		}
	}
}

static void blit_centred(Canvas c, int k, const int16_t *r, uint32_t mul)
{
	blit(c, k, r[0] + (r[2] - ga_spr[k].w) / 2, r[1] + (r[3] - ga_spr[k].h) / 2, mul);
}

/* ---- the board's font ---- */

enum { T_WHITE, T_RED, T_YELLOW, T_AMBER, T_BLUE, T_GREEN, T_CYAN, T_GREY, NTINT };
static const uint8_t tints[NTINT][2][3] = {   /* bright end, dim end of the font's shades 2..8 (gadget_mock.py) */
	{ { 255, 255, 255 }, { 150, 150, 160 } }, { { 255, 120, 120 }, { 200, 0, 0 } },
	{ { 255, 255, 170 }, { 230, 190, 0 } }, { { 255, 210, 120 }, { 230, 120, 0 } },
	{ { 170, 220, 255 }, { 40, 110, 255 } }, { { 180, 255, 180 }, { 0, 170, 0 } },
	{ { 170, 255, 255 }, { 0, 170, 200 } }, { { 110, 110, 110 }, { 60, 60, 60 } },   /* test 21: grey, the dim value */
};
static uint32_t shade[NTINT][16];           /* by the glyph's pixel value; 0 = clear */
static uint8_t font[128][64];               /* upright glyphs */

static int font_take(const uint8_t *glyphs)
{
	if (!glyphs)
		return 0;
	unsigned sum = 0;
	for (int i = 0x41 * 64; i < 0x5B * 64; i++)
		sum += glyphs[i];
	if (!sum)                                  /* not DMA'd yet */
		return 0;
	for (int ch = 0; ch < 128; ch++)
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++)   /* upright (x, y) = board (7 - y, x): a quarter turn back */
				font[ch][y * 8 + x] = glyphs[ch * 64 + x * 8 + (7 - y)];
	for (int t = 0; t < NTINT; t++)
		for (int v = 1; v < 16; v++) {
			if (v >= 9) {
				shade[t][v] = 0xFF1E1E1E;
				continue;
			}
			const int k = v < 2 ? 0 : v - 2;  /* 0..6 */
			uint32_t c = 0xFF000000u;
			for (int ch = 0; ch < 3; ch++) {
				const int a = tints[t][0][ch], b = tints[t][1][ch];
				c |= (uint32_t)(a + (b - a) * k / 6) << (16 - 8 * ch);
			}
			shade[t][v] = c;
		}
	return 1;
}

static void text(Canvas c, int x, int y, int scale, int tint, const char *s)
{
	if (!titled)
		return;
	for (; *s; s++, x += 8 * scale) {
		const uint8_t *g = font[*s & 127];
		for (int j = 0; j < 8 * scale; j++) {
			const int yy = y + j;
			if (yy < 0 || yy >= c.h)
				continue;
			const uint8_t *row = g + (j / scale) * 8;
			for (int i = 0; i < 8 * scale; i++) {
				const int xx = x + i, v = row[i / scale];
				if (v && xx >= 0 && xx < c.w)
					c.px[yy * c.w + xx] = shade[tint][v & 15];
			}
		}
	}
}

/* the largest scale (<= max) at which s fits the rectangle with a margin */
static int fit(const int16_t *r, const char *s, int max)
{
	const int n = (int)strlen(s);
	int sc = max;
	while (sc > 1 && (8 * sc * n > r[2] - 4 || 8 * sc > r[3] - 2))
		sc--;
	return sc;
}

static void text_in(Canvas c, const int16_t *r, int align, int scale, int tint, const char *s)
{
	const int w = 8 * scale * (int)strlen(s);
	const int x = align < 0 ? r[0] + 2 : align > 0 ? r[0] + r[2] - 2 - w : r[0] + (r[2] - w) / 2;
	text(c, x, r[1] + (r[3] - 8 * scale) / 2, scale, tint, s);
}

static char *num(char *b, uint32_t v)
{
	char t[12];
	int n = 0;
	do
		t[n++] = (char)('0' + v % 10);
	while ((v /= 10) && n < 11);
	for (int i = 0; i < n; i++)
		b[i] = t[n - 1 - i];
	b[n] = 0;
	return b;
}

/* ---- the static canvases' titles ---- */

static void put_titles(void)
{
	for (unsigned i = 0; i < sizeof ga_text / sizeof ga_text[0]; i++) {
		const GaText *t = &ga_text[i];
		if (t->layer == 0) {
			const Canvas c = { base[t->screen], t->screen == GAD_TATE ? GAD_TATE_W : GAD_YOKO_W,
				t->screen == GAD_TATE ? GAD_TATE_H : GAD_YOKO_H };
			text(c, t->x, t->y, t->scale, t->tint, t->s);
		} else {
			const int16_t *b = chase_box(t->screen);
			const Canvas c = { chase_layer[t->screen], b[2], b[3] };
			text(c, t->x - b[0], t->y - b[1], t->scale, t->tint, t->s);
		}
	}
}

/* ---- per layout sprite bases (the generator's names carry Y / T / O; O only where its sizes differ from Y's) ---- */
typedef struct {
	int dig_rank, dig_boss, dig_type, port_main, port_small, item, medal, stick, button, check, minus, wtick;
} Sprites;
static const Sprites SP[GAD_LAYOUTS] = {
	{ GS_DYR_0, GS_DYB_0, GS_DYT_0, GS_PTYM_0, GS_PTYS_0, GS_ITY_0, GS_MDY_0, GS_STY_0, GS_BTY, GS_CKY_1, GS_CKY_2,
	  GS_WKY },
	{ GS_DTR_0, GS_DTB_0, GS_DTT_0, GS_PTTM_0, GS_PTTS_0, GS_ITT_0, GS_MDT_0, GS_STT_0, GS_BTT, -1, -1, GS_WKT },
	{ GS_DOR_0, GS_DYB_0, GS_DYT_0, GS_PTYM_0, GS_PTYS_0, GS_ITO_0, GS_MDO_0, GS_STY_0, GS_BTY, GS_CKO_1, GS_CKO_2,
	  GS_WKY },
};
static int stick_sprite(int base_k, int dir)
{
	static const int8_t order[16] = { 0, 1, 2, -1, 3, 5, 6, -1, 4, 8, 7, -1, -1, -1, -1, -1 };  /* 0,1,2,4,8,5,6,10,9 */
	return order[dir & 15] < 0 ? -1 : base_k + order[dir & 15];
}

#define RECT(n) (ga_rect[screen][GA_##n])
#define HAS(n) (ga_rect[screen][GA_##n][2] > 0)
#define ON(p) (mask & (p))

/* Baffo's digit rows: v's digits right to left into n places; places past v's length are left empty */
static void digits(Canvas c, int screen, int base_k, int first, int n, uint32_t v, int none)
{
	(void)screen;
	for (int i = n - 1, k = 0; i >= 0; i--, k++) {
		const int16_t *r = ga_rect[screen][first + i];
		if (r[2] <= 0)
			continue;
		if (none)
			blit_centred(c, base_k + 16, r, 0xFFFFFF);
		else if (k == 0 || v)
			blit_centred(c, base_k + (int)(v % 10), r, 0xFFFFFF);
		v /= 10;
	}
}

/* test 21 (user: the secret extends): "1UP n/3 B n/3"; a counter at 3 is amber. The counters: the 1UP drops (0x20E460,
 * at a death with two lives or fewer) and the large bombs (0x20E45E); the game gives the secret extend at 3.
 * test 22: the row's label "Sp.Extend" is on the row's top line (mkgadgets.py); the value is right-aligned on the
 * bottom line, because label + value (160 px) is wider than the STATUS row in every layout (147 px 16:9, 117 TATE). */
static void sp_extend(Canvas c, const int16_t *r, int up, int bomb)
{
	char a[8] = "1UP0/3", k[6] = "B0/3";
	a[3] = (char)('0' + (up < 0 ? 0 : up > 9 ? 9 : up));
	k[1] = (char)('0' + (bomb < 0 ? 0 : bomb > 9 ? 9 : bomb));
	const int y = r[1] + r[3] - 9;
	const int kx = r[0] + r[2] - 2 - 8 * (int)strlen(k);
	text(c, kx, y, 1, bomb >= 3 ? T_AMBER : T_WHITE, k);
	text(c, kx - 8 - 8 * (int)strlen(a), y, 1, up >= 3 ? T_AMBER : T_WHITE, a);
}

const uint8_t *gadgets_menu_piece(int i, int *w, int *h)
{
	if (!inited || i < 0 || i >= GAD_MK_N)
		return NULL;
	const int k = GS_MK_TL + i;
	*w = ga_spr[k].w;
	*h = ga_spr[k].h;
	return spr + ga_spr[k].off;
}

void gadgets_blank(uint32_t *canvas, int screen)
{
	(void)screen;
	for (int i = 0; i < NPIX; i++)
		canvas[i] = 0xFF000000u;
}

void gadgets_draw(uint32_t *canvas, int screen, int chase, const GadgetState *g, const uint8_t *glyphs, unsigned mask)
{
	screen = screen == GAD_TATE || screen == GAD_YOKO_ORIG ? screen : GAD_YOKO;
	if (!inited && gadgets_init()) {
		gadgets_blank(canvas, screen);
		return;
	}
	if (!titled && font_take(glyphs)) {
		titled = 1;
		put_titles();
	}
	frame_n++;
	const Canvas c = { canvas, screen == GAD_TATE ? GAD_TATE_W : GAD_YOKO_W, screen == GAD_TATE ? GAD_TATE_H : GAD_YOKO_H };
	memcpy(canvas, base[screen], sizeof base[0]);
	/* test 21: the panels not in mask show the wallpaper (their frames are in the static canvas) */
	if ((mask & GAD_P_ALL) != GAD_P_ALL)
		for (int k = 0; k < GAD_P_N; k++)
			if (!(mask & (1u << k)))
				unpaint(canvas, screen, k);
	chase = chase && ON(GAD_P_CHASE);
	const Sprites *sp = &SP[screen];
	const int live = g->in_credit;     /* test 21: outside a credit (attract, demo) the pictures stay off */
	char b[48];

	/* CHASE!'s frames (a CHASE! credit's panels; in ORIGINAL their place shows the wallpaper) */
	if (chase && HAS(CH_BAR)) {
		const int16_t *bx = chase_box(screen);
		const uint32_t *l = chase_layer[screen];
		for (int j = 0; j < bx[3]; j++) {
			uint32_t *d = canvas + (bx[1] + j) * c.w + bx[0];
			for (int i = 0; i < bx[2]; i++) {
				const uint32_t s = l[j * bx[2] + i];
				const unsigned a = s >> 24;
				if (a == 255)
					d[i] = s;
				else if (a)
					d[i] = blend(d[i], s, a + 1);
			}
		}
	}

	/* rank bar (red) and rank start (dark red over it), the % digits; 100% (rank 0) reads 100.00 */
	if (ON(GAD_P_RANK_BAR)) {
		const int16_t *r = RECT(RANK_BAR);
		fill(c, r[0], r[1], (int)((int64_t)r[2] * g->rank_bar_q16 >> 16), r[3], 0xFFFF0000);
		fill(c, r[0], r[1], (int)((int64_t)r[2] * g->rank_start_bar_q16 >> 16), r[3], 0xFF800000);
		if (g->rank_ppm >= 1000000) {
			const int16_t *a = RECT(RANK_D0), *z = RECT(RANK_D5);
			const int wbox = z[0] + z[2] - a[0];          /* the six digits' box */
			const int sc2 = wbox >= 96 ? 2 : 1;           /* test 21: 100.00 at scale 2 when it fits */
			const int x = a[0] + (wbox - 48 * sc2) / 2, y = a[1] + (a[3] - 8 * sc2) / 2;
			text(c, x, y, sc2, T_WHITE, "100.00");
		} else
			digits(c, screen, sp->dig_rank, GA_RANK_D0, 6, (uint32_t)g->rank_ppm % 1000000, !g->in_credit);
	}

	/* the next six drops */
	if (ON(GAD_P_ITEM_DROPS) && live)
		for (int i = 0; i < GAD_NEXT_DROPS; i++)
			blit_centred(c, sp->item + g->next_items[i], ga_rect[screen][GA_NEXT_0 + i], 0xFFFFFF);

	/* Item Count F2..F7: the count at the slot's bottom right, red at the cap */
	if (ON(GAD_P_ITEM_COUNT))
		for (int i = 0; i < 6; i++) {
			const int16_t *r = ga_rect[screen][GA_CNT_0 + i];
			const int sc = r[3] >= 30 ? 2 : 1;
			num(b, (uint32_t)g->drops[i + 1]);
			text(c, r[0] + r[2] - 2 - 8 * sc * (int)strlen(b), r[1] + r[3] - 2 - 8 * sc, sc,
				g->drop_at_cap[i + 1] ? T_RED : T_WHITE, b);
		}

	/* the medal gadget: F1 count, the next medal's picture and value */
	if (ON(GAD_P_MEDAL)) {
		const int16_t *r = RECT(MEDAL_CNT);
		num(b, (uint32_t)g->drops[GAD_MEDAL]);
		text_in(c, r, 0, fit(r, b, 3), T_WHITE, b);
		if (live)
			blit_centred(c, sp->medal + g->medal_step, RECT(MEDAL_PIC), 0xFFFFFF);
		r = RECT(MEDAL_VAL);
		num(b, (uint32_t)g->medal_value);
		text_in(c, r, 0, fit(r, b, 3), g->medal_step == 18 ? T_AMBER : T_YELLOW, b);
	}

	/* boss: HP and timer, three digits each, the minus when none */
	if (ON(GAD_P_BOSS_INFO)) {
		digits(c, screen, sp->dig_boss, GA_BOSS_HP0, 3, (uint32_t)(g->boss_hp < 0 ? 0 : g->boss_hp), g->boss_hp < 0);
		digits(c, screen, sp->dig_boss, GA_BOSS_T0, 3, (uint32_t)(g->boss_secs < 0 ? 0 : g->boss_secs),
			g->boss_secs < 0);
	}

	/* secret bosses */
	if (ON(GAD_P_SECRET_BOSSES) && sp->check >= 0)
		for (int i = 0; i < GAD_CONDS; i++) {
			const int16_t *r = ga_rect[screen][GA_COND_0 + i];
			/* test 21: ADVANCE SPECIAL's every secret boss met; the last three (ENVY, BLUNT, SOBUT: HIGH WAY's by the
			 * stage order, not a course's) keep the game's own state */
			const int st = g->all_secrets && i < GAD_CONDS - 3 ? GAD_COND_MET : g->cond[i];
			if (st == GAD_COND_MET)
				blit(c, sp->check, r[0], r[1] + r[3] - ga_spr[sp->check].h, 0xFFFFFF);
			else if (st == GAD_COND_NA)
				blit_centred(c, sp->minus, r, 0xFFFFFF);
		}

	/* shot charge (P1, as Baffo's): red, flashing yellow when full */
	if (ON(GAD_P_CHARGE)) {
		const int v = g->p[0].shot_charge > 0x48 ? 0x48 : g->p[0].shot_charge;
		uint32_t col = 0xFFFF0000;
		if (g->p[0].shot_charge == 0x48) {
			int a = (int)(frame_n % 20) * 255 / 10 - 255;
			col = 0xFFFF0000 | (uint32_t)(a < 0 ? -a : a) << 8;
		}
		if (HAS(CHARGE_V)) {
			const int16_t *r = RECT(CHARGE_V);
			const int h = r[3] * v / 0x48;
			fill(c, r[0], r[1] + r[3] - h, r[2], h, col);
		}
	}

	/* current player: P2's pictures over P1's, as Baffo's two overlaid sets (P2's are empty unless P2 plays) */
	if (ON(GAD_P_PLAYER)) {
		const GadgetPlayer *p = g->p[1].active ? &g->p[1] : &g->p[0];
		if (p->active) {
			if (p->pilot >= 0 && p->pilot < 18)
				blit_centred(c, sp->port_main + p->pilot, RECT(PL_MAIN), 0xFFFFFF);
			if (p->next >= 0 && p->next < 18)
				blit_centred(c, sp->port_small + p->next, RECT(PL_NEXT), 0xFFFFFF);
			if (p->third >= 0 && p->third < 18)
				blit_centred(c, sp->port_small + p->third, RECT(PL_THIRD), 0xFFFFFF);
			blit_centred(c, sp->dig_type + 10 + p->type, RECT(PL_TYPE), 0xFFFFFF);
		}
	}

	/* input (the controller): the player whose input changed last */
	if (ON(GAD_P_CONTROLLER)) {
		const int in = g->p[g->current].in;
		const int k = stick_sprite(sp->stick, in & 15);
		if (k >= 0)
			blit_centred(c, k, RECT(IN_STICK), 0xFFFFFF);
		static const uint8_t bit[4] = { 0x10, 0x20, 0x40, 0x80 };
		for (int i = 0; i < 4; i++)
			if (in & bit[i])
				blit_centred(c, sp->button, ga_rect[screen][GA_IN_A + i], 0xFFFFFF);
	}

	/* STATUS (test 21): the course (0x20E6A6: 0 TRAINING, 2 NORMAL, 4 ADVANCE, 6 SPECIAL; the front end's PRACTICE runs
	 * stage-edit numbers (idx + 4) * 2 = 8 and up), enemies, bullets, the next extend, the secret extends */
	if (ON(GAD_P_STATUS)) {
		static const char *const course[4] = { "TRAINING", "NORMAL", "ADVANCE", "SPECIAL" };
		const char *cs = !live ? "-" : g->course >= 8 ? "PRACTICE" : course[(g->course >> 1) & 3];
		text_in(c, RECT(STATUS_0), 1, 1, live && g->course >= 8 ? T_CYAN : T_WHITE, cs);
		text_in(c, RECT(STATUS_1), 1, 1, T_WHITE, num(b, (uint32_t)g->enemies));
		text_in(c, RECT(STATUS_2), 1, 1, T_WHITE, num(b, (uint32_t)g->bullets));
		/* Extend: CHASE!'s own next extend in a CHASE! credit, else P1's (player +0xF8); 0xFFFFFFFF = none left */
		{
			const uint32_t ex = g->chase ? g->next_extend : g->p[0].extend;
			if (!live)
				strcpy(b, "-");
			else if (ex == 0xFFFFFFFF)
				strcpy(b, "NONE");
			else
				num(b, ex);
			text_in(c, RECT(STATUS_3), 1, 1, T_WHITE, b);
		}
		{
			const int16_t *r = RECT(STATUS_4);
			if (live)
				sp_extend(c, r, (int)g->drops[GAD_1UP], (int)g->drops[GAD_BIG_BOMB]);
			else
				text(c, r[0] + r[2] - 2 - 8, r[1] + r[3] - 9, 1, T_WHITE, "-");     /* test 22: the value's line */
		}
	}

	/* WEAPON: the current player (P1 when they are not playing) */
	if (ON(GAD_P_WEAPON)) {
		const GadgetPlayer *p = g->p[g->current].active ? &g->p[g->current] : &g->p[0];
		strcpy(b, "LV ");
		num(b + 3, (uint32_t)p->shot_lv);
		text_in(c, RECT(WEAPON_0), 1, 1, p->shot_lv >= 4 ? T_YELLOW : T_WHITE, p->active ? b : "-");
		text_in(c, RECT(WEAPON_1), 1, 1, T_WHITE, p->active ? num(b, (uint32_t)p->options) : "-");
		/* the options' formation: only the Battle Garegga guests have one ("-" for the others); short forms
		 * where the row is narrow (TATE). test 21: V is STANDARD (STD) */
		static const char *const form[2][6] = { { "STANDARD", "BACK", "FRONT", "CIRCLE", "CONTROL", "SPECIAL" },
			{ "STD", "BACK", "FRONT", "CIRC", "CTRL", "SP" } };
		const int wide = RECT(WEAPON_2)[2] >= 130;
		text_in(c, RECT(WEAPON_2), 1, 1, p->formation == 5 ? T_AMBER : T_WHITE,
			p->active && p->formation >= 0 ? form[!wide][p->formation] : "-");
		text_in(c, RECT(WEAPON_3), 1, 1, T_WHITE, p->active ? num(b, (uint32_t)p->bombs) : "-");
		/* Bomb (test 21): the frames of the bomb running (amber, its gauge draining against the rider's last bomb, or
		 * filling with the frames when the rider has none yet); outside a bomb the last bomb's frames dim, gauge empty */
		{
			const int16_t *row = RECT(WEAPON_4), *gb = RECT(WBOMB);
			fill(c, gb[0], gb[1], gb[2], gb[3], 0xFF101010);
			if (!p->active) {
				strcpy(b, "-");
				text_in(c, row, 1, 1, T_WHITE, b);
			} else if (p->bomb_on) {
				num(b, (uint32_t)p->bomb_frames);
				strcat(b, "F");
				text_in(c, row, 1, 1, T_AMBER, b);
				int fw;
				if (p->bomb_last > 0) {
					int left = p->bomb_last - p->bomb_frames;
					fw = gb[2] * (left < 0 ? 0 : left) / p->bomb_last;
				} else {
					const int e = p->bomb_frames > 240 ? 240 : p->bomb_frames;
					fw = gb[2] * e / 240;
				}
				fill(c, gb[0], gb[1], fw, gb[3], 0xFFE0A000);
			} else {
				if (p->bomb_last > 0)
					strcat(num(b, (uint32_t)p->bomb_last), "F");
				else
					strcpy(b, "-");
				text_in(c, row, 1, 1, T_GREY, b);
			}
			frame_rect(c, gb[0] - 2, gb[1] - 2, gb[2] + 4, gb[3] + 4, 0xFF949494);
		}
		for (int i = 0; i < 2; i++) {
			const int16_t *r = ga_rect[screen][GA_WEAPON_5 + i];
			const int s = r[3] - 6, x = r[0] + r[2] - s - 2, y = r[1] + 3;
			frame_rect(c, x, y, s, s, 0xFF949494);
			if (i ? g->sp_option : g->sp_power)
				blit(c, sp->wtick, x + (s - ga_spr[sp->wtick].w) / 2, y + s - ga_spr[sp->wtick].h, 0xFFFFFF);
		}
	}

	/* STAGES: name, then the score made in it (the stage being played yellow, the rest to come green), TOTAL */
	if (ON(GAD_P_STAGES) && HAS(STG_0)) {
		const int pl = g->current;
		for (int i = 0; i < GAD_STAGES + 1; i++) {
			const int16_t *r = ga_rect[screen][GA_STG_0 + i];
			const int16_t top[4] = { r[0], r[1], r[2], (int16_t)(r[3] / 2) };
			const int16_t bot[4] = { r[0], (int16_t)(r[1] + r[3] / 2), r[2], (int16_t)(r[3] - r[3] / 2) };
			if (i == GAD_STAGES) {
				text_in(c, top, -1, 1, T_AMBER, "TOTAL");
				text_in(c, bot, 1, 1, g->invalid ? T_RED : T_AMBER, num(b, g->stage_total[pl]));   /* test 13 */
				continue;
			}
			const int cur = i == g->stage_cur, done = g->stage_played[i];
			const char *nm = gad_scene_name(g->stage_row_scene[i]);
			text_in(c, top, -1, 1, cur ? T_YELLOW : done ? T_WHITE : T_GREEN,
				!live ? "-" : *nm ? nm : "?");
			text_in(c, bot, 1, 1, g->stage_red[i] ? T_RED : cur ? T_YELLOW : T_WHITE, done ? num(b, g->stage_score[pl][i]) :
				"-");
		}
	}

	/* MUSIC (test 6): the sound ROM's title of the tune playing (white) and its composer (green); YOKO one line
	 * "BGM title ... composer", TATE the two on their own lines */
	if (ON(GAD_P_MUSIC) && HAS(MUSIC_0) && g->music >= 0) {
		const int16_t *r = RECT(MUSIC_0);
		if (HAS(MUSIC_1)) {
			text_in(c, r, -1, 1, T_WHITE, g->music_title);
			text_in(c, RECT(MUSIC_1), 1, 1, T_GREEN, g->music_by);
		} else {
			text_in(c, r, -1, 1, T_AMBER, "BGM");
			const int16_t t[4] = { (int16_t)(r[0] + 32), r[1], (int16_t)(r[2] - 32), r[3] };
			text_in(c, t, -1, 1, T_WHITE, g->music_title);
			text_in(c, r, 1, 1, T_GREEN, g->music_by);
		}
	}

	/* CHASE! (test 21): the chain bar, the chain count, the x3 multiplier, the forward arrow, C and the lives */
	if (chase && HAS(CH_BAR)) {
		const int16_t *r = RECT(CH_BAR);
		static const uint32_t bar[3] = { 0xFF286EFF, 0xFFE01010, 0xFFF0F0F0 };
		fill(c, r[0], r[1], (int)((int64_t)r[2] * g->timer_q16 >> 16), r[3], bar[g->timer_state]);
		r = RECT(CH_CHAIN);
		num(b, g->chain);
		text_in(c, r, 0, fit(r, b, 3), T_WHITE, b);
		r = RECT(CH_MULT);
		b[0] = 'x', b[1] = (char)('0' + g->mult), b[2] = 0;
		text_in(c, r, 0, fit(r, b, 4), g->mult == 8 ? T_AMBER : T_WHITE, b);
		r = RECT(CH_C);
		if (g->break_ready)
			text_in(c, r, 0, fit(r, "C", 3), T_AMBER, "C");
		r = RECT(CH_FWD);
		text_in(c, r, 0, fit(r, "^", 3), g->forward ? T_AMBER : T_BLUE, g->forward ? "^" : "v");
		r = RECT(CH_LIVES);
		num(b, (uint32_t)(g->lives > 9 ? 9 : g->lives));
		text_in(c, r, 0, fit(r, b, 5), g->lives == 1 ? T_RED : T_WHITE, b);
	}
}

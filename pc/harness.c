/* batrider360 PC harness: the board headless. Frames as PPM, sound as WAV, RAM dumps, scripted inputs.
 *   harness <romdir> <outdir> <frames> [script]
 * romdir: MAME's batrider.zip (or batriderj.zip), or a folder of its members extracted flat.
 * script lines:  <frame> in <hex16> | sys <hex8> | dips <hex16> <hex8> | poke <hex addr> <hex8> | snap | ram | glyphs
 *   (# comments)
 *   in/sys take effect from that frame on; snap writes <outdir>/f<frame>.ppm, ram <outdir>/r<frame>.bin. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board.h"
#include "vdp.h"
#include "romset.h"
#include "progpatch.h"
#include "m68k.h"
#include "front.h"
#include "fe.h"
#include <stdarg.h>
#include "chasehud.h"
#include "menusnd.h"
/* BR_CHASEHUD=vbl: chasehud_frame from CHASE!'s vblank wait (chasehud.h), as the N64 draws; =1: after the frame */
static int chud_hook(int opcode)
{
	if (opcode == 0x4AFC && (br_reg(M68K_REG_PPC) & 0xFFFFFF) == CHASEHUD_PRE_SITE) {
		const uint32_t to = chasehud_pre(), sp = br_reg(M68K_REG_SP) - 4;   /* test 11: then the jsr */
		br_set_reg(M68K_REG_SP, sp);
		br_wr16(sp, (uint16_t)((CHASEHUD_PRE_SITE + 6) >> 16));
		br_wr16(sp + 2, (uint16_t)(CHASEHUD_PRE_SITE + 6));
		br_set_reg(M68K_REG_PC, to);
		return 1;
	}
	if (opcode != 0x4AFC || (br_reg(M68K_REG_PPC) & 0xFFFFFF) != CHASEHUD_VBL_SITE)
		return 0;
	chasehud_frame();
	br_set_reg(M68K_REG_PC, CHASEHUD_VBL_NEXT);
	return 1;
}

static uint8_t *load(const char *dir, const char *name, size_t size)
{
	char p[1024];
	snprintf(p, sizeof p, "%s/%s", dir, name);
	FILE *f = fopen(p, "rb");
	if (!f) {
		fprintf(stderr, "missing %s\n", p);
		exit(1);
	}
	uint8_t *b = malloc(size);
	if (fread(b, 1, size, f) != size) {
		fprintf(stderr, "short %s\n", p);
		exit(1);
	}
	fclose(f);
	return b;
}

static uint64_t now_ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

typedef struct {
	long frame;
	char op[8];
	unsigned a, b;
} Cmd;

/* ---- GADGETS (core/gadgets.c) ----
 * BR_GADGETS=<file>: the gadget values, one line every BR_GADGETS_EVERY frames (default 60).
 * BR_GADGETS_PNG=<frame>[,<frame>...]: the 360's canvases with the game pasted in at those frames,
 *   <outdir>/gy<frame>.ppm (YOKO 1280x720) and gt<frame>.ppm (TATE 720x1280). */
#include "gadgets.h"

static void gad_ppm(const char *path, const uint32_t *c, int w, int h)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (int i = 0; i < w * h; i++) {
		uint8_t px[3] = {(uint8_t)(c[i] >> 16), (uint8_t)(c[i] >> 8), (uint8_t)c[i]};
		fwrite(px, 1, 3, f);
	}
	fclose(f);
}

/* the board's picture, upright (upright (x, y) = board (319 - y, x)), nearest-scaled into the canvas rectangle */
static void gad_paste(uint32_t *c, int cw, const uint32_t *pic, int x0, int y0, int w, int h)
{
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const int ux = x * BR_H / w, uy = y * BR_W / h;
			c[(y0 + y) * cw + x0 + x] = 0xFF000000u | pic[ux * BR_W + (BR_W - 1 - uy)];
		}
}

static void gad_frame(long fr, const char *od, const uint32_t *pic)
{
	static GadgetState g;
	static FILE *lf;
	static int every = 60, init;
	const char *e = getenv("BR_MODE");
	gadgets_update(&g, br_ram(), e && !strcmp(e, "chase"));
	{
		char title[24];
		const int tune = br_music();
		br_music_title(tune, title, sizeof title);
		gadgets_music(&g, tune, title);
	}
	if (!init) {
		init = 1;
		if (getenv("BR_GADGETS"))
			lf = fopen(getenv("BR_GADGETS"), "w");
		if (getenv("BR_GADGETS_EVERY"))
			every = atoi(getenv("BR_GADGETS_EVERY"));
		if (every < 1)
			every = 1;
		if (getenv("BR_GADGETS_PNG") && gadgets_init())
			fprintf(stderr, "gadgets_init failed\n");
	}
	if (lf && fr % every == 0) {
		const GadgetPlayer *p = &g.p[0];
		fprintf(lf, "%ld credit %d course %d rank %06X/%06X ppm %d bar %d/%d | drops next %d [%d%d%d%d%d%d] cnt",
			fr, g.in_credit, g.course, g.rank, g.rank_start, g.rank_ppm, g.rank_bar_q16, g.rank_start_bar_q16,
			g.next_drop, g.next_items[0], g.next_items[1], g.next_items[2], g.next_items[3], g.next_items[4],
			g.next_items[5]);
		for (int i = 0; i < GAD_DROPS; i++)
			fprintf(lf, " %d%s", g.drops[i], g.drop_at_cap[i] ? "!" : "");
		fprintf(lf, " medal %d=%d | boss %s hp %d t %d s %d | P%d pilot %d/%d/%d %c act %d in %02X charge %d"
			" | LV %d (%d/%d) opt %d lv %d bombs %d+%d rate %d form %d sp %d/%d | cond",
			g.medal_rung, g.medal_value, g.boss_name ? g.boss_name : "-", g.boss_hp, g.boss_timer, g.boss_secs,
			g.current + 1, p->pilot, p->next, p->third, 'A' + p->type, p->active, p->in, p->shot_charge,
			p->shot_lv, p->shot_p, p->shot_p_need, p->options, p->option_lv, p->bombs, p->bomb_gauge, p->rate,
			p->formation, g.sp_power, g.sp_option);
		for (int i = 0; i < GAD_CONDS; i++)
			fprintf(lf, "%c", "o+-"[g.cond[i]]);
		fprintf(lf, " | music %02X %s / %s", g.music & 0xFF, g.music_title, g.music_by);
		fprintf(lf, " | stage %d %s(%d) %d:%02d en %d bu %d | rows", g.stage_no, g.stage_name, g.stage_scene,
			g.stage_min, g.stage_sec, g.enemies, g.bullets);
		for (int r = 0; r < GAD_STAGES; r++)
			fprintf(lf, " %s%s=%u", r == g.stage_cur ? "*" : "", gad_scene_name(g.stage_row_scene[r]),
				g.stage_score[0][r]);
		fprintf(lf, " tot %u score %u", g.stage_total[0], p->score);
		if (g.chase)
			fprintf(lf, " | CHASE chain %u x%d t %d/%d st %d fz %d C %d fwd %d lives %d (+%d) ext %u msg %d",
				g.chain, g.mult, g.timer, g.timer_max, g.timer_state, g.timer_frozen, g.break_ready, g.forward,
				g.lives, g.banked, g.to_extend, g.msg);
		fputc('\n', lf);
	}
	const char *pl = getenv("BR_GADGETS_PNG");
	for (const char *s = pl; s && *s;) {
		char *end;
		const long f = strtol(s, &end, 10);
		if (end == s)
			break;
		if (f == fr) {
			static uint32_t cy[GAD_YOKO_W * GAD_YOKO_H], ct[GAD_TATE_W * GAD_TATE_H];
			char p[1024];
			uint64_t t0 = now_ns();
			for (int k = 0; k < 10; k++)
				gadgets_draw(cy, GAD_YOKO, g.chase, &g, br_text_glyphs());
			printf("gadgets_draw YOKO %.0f us\n", (now_ns() - t0) / 1e4);
			gad_paste(cy, GAD_YOKO_W, pic, GAD_YOKO_GX, GAD_YOKO_GY, GAD_YOKO_GW, GAD_YOKO_GH);
			snprintf(p, sizeof p, "%s/gy%05ld.ppm", od, fr);
			gad_ppm(p, cy, GAD_YOKO_W, GAD_YOKO_H);
			gadgets_draw(ct, GAD_TATE, g.chase, &g, br_text_glyphs());
			gad_paste(ct, GAD_TATE_W, pic, GAD_TATE_GX, GAD_TATE_GY, GAD_TATE_GW, GAD_TATE_GH);
			snprintf(p, sizeof p, "%s/gt%05ld.ppm", od, fr);
			gad_ppm(p, ct, GAD_TATE_W, GAD_TATE_H);
		}
		s = *end == ',' ? end + 1 : end;
	}
}

/* ---- BR_FE=1: the front end (core/fe.c) driven as the 360's main.c does, split board ----
 * script lines: <frame> pad <hex FE_* bits held from then on> | tap <hex> [frames, default 2] | snap | ram |
 *   poke <hex addr> <hex8> | regs | mark <text>   (# comments)
 * snap writes <outdir>/f<frame>.ppm: fe_video's picture turned upright (480x640). Files (host.txt, options.txt,
 * rankings_*.bin) live in <outdir>; BR_FE_FILES=<dir> reads/writes them elsewhere (to carry them between runs). */
static char fe_dir[1024];
static long fe_fr;
#ifdef OKI_TRACE
#include "oki.h"
void oki_trace(const Oki *o, int v, int phrase, uint32_t start, uint32_t stop, int att)
{
	printf("%6ld oki %p voice %d phrase %02X att %d start %05X stop %05X pages %X %X %X %X\n", fe_fr, (const void *)o, v,
		phrase, att, start, stop, o->page[0], o->page[1], o->page[2], o->page[3]);
}
#endif

static int fe_load(const char *name, void *buf, int max)
{
	char p[1200];
	snprintf(p, sizeof p, "%s/%s", fe_dir, name);
	FILE *f = fopen(p, "rb");
	if (!f)
		return -1;
	int n = (int)fread(buf, 1, (size_t)max, f);
	fclose(f);
	return n;
}

static int fe_save(const char *name, const void *buf, int len)
{
	char p[1200];
	snprintf(p, sizeof p, "%s/%s", fe_dir, name);
	FILE *f = fopen(p, "wb");
	if (!f)
		return -1;
	fwrite(buf, 1, (size_t)len, f);
	fclose(f);
	return 0;
}

static void fe_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("%6ld ", fe_fr);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

static void fe_ppm(const char *path, const uint32_t *tex)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", FRONT_TEX_H, FRONT_TEX_W);
	for (int uy = 0; uy < FRONT_TEX_W; uy++)
		for (int ux = 0; ux < FRONT_TEX_H; ux++) {
			const uint32_t c = tex[ux * FRONT_TEX_W + (FRONT_TEX_W - 1 - uy)];
			uint8_t b[3] = {(uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
			fwrite(b, 1, 3, f);
		}
	fclose(f);
}

/* ---- test 8: save states and replays (core/snap.c) ---- */
#include "snap.h"
static uint8_t t8_state[SNAP_RAW_MAX], t8_start[SNAP_RAW_MAX], t8_ret[SNAP_RAW_MAX];
static size_t t8_state_n, t8_start_n;
static SnapRun t8_runs[400000];
static SnapPads t8_pads = { t8_runs, 0, 400000, 0, 0 };
static int t8_rec, t8_prev_credit, t8_gone;
static uint32_t t8_end_hash;
/* per recorded frame: the hashes of RAM, VDP, 68000 registers, Z80 RAM; RAM itself for the first T8_RAMS frames */
enum { T8_FRAMES = 400000, T8_RAMS = 3000 };
static uint32_t (*t8_fh)[4];
static uint8_t *t8_fram;

static uint32_t fnv(const void *v, size_t n, uint32_t h)
{
	const uint8_t *p = v;
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 0x01000193u;
	return h;
}

extern uint8_t *br_zram_for_tests(void);
static void t8_parts(uint32_t h[4])
{
	h[0] = fnv(br_ram(), BR_RAM_SIZE, 0x811C9DC5u);
	h[1] = fnv(&vdp, sizeof vdp, 0x811C9DC5u);
	uint32_t r[18];
	for (int i = 0; i < 16; i++)
		r[i] = br_reg(M68K_REG_D0 + i);
	r[16] = br_reg(M68K_REG_PC);
	r[17] = br_reg(M68K_REG_SR);
	h[2] = fnv(r, sizeof r, 0x811C9DC5u);
	h[3] = fnv(br_zram_for_tests(), 0x2000, 0x811C9DC5u);
}

static void t8_frame_done(uint32_t k)
{
	if (!t8_fh) {
		t8_fh = calloc(T8_FRAMES, sizeof *t8_fh);
		t8_fram = malloc((size_t)T8_RAMS * BR_RAM_SIZE);
	}
	if (k >= T8_FRAMES)
		return;
	t8_parts(t8_fh[k]);
	if (k < T8_RAMS)
		memcpy(t8_fram + (size_t)k * BR_RAM_SIZE, br_ram(), BR_RAM_SIZE);
}

/* RAM, the VDP, the 68000's PC/SR and the frame count */
static uint32_t t8_hash(void)
{
	uint32_t h = 0x811C9DC5u;
	const uint8_t *p = br_ram();
	for (int i = 0; i < BR_RAM_SIZE; i++)
		h = (h ^ p[i]) * 0x01000193u;
	p = (const uint8_t *)&vdp;
	for (size_t i = 0; i < sizeof vdp; i++)
		h = (h ^ p[i]) * 0x01000193u;
	h = (h ^ br_reg(M68K_REG_PC)) * 0x01000193u;
	h = (h ^ br_reg(M68K_REG_SR)) * 0x01000193u;
	return (h ^ br_frame_count()) * 0x01000193u;
}

static void t8_ssave(void)
{
	t8_state_n = snap_save(t8_state, sizeof t8_state, 0);
	static uint8_t packed[SNAP_RAW_MAX], work[SNAP_ZWORK_DEFLATE];
	const size_t pn = snap_pack(t8_state, t8_state_n, packed, sizeof packed, work, sizeof work);
	printf("%6ld ssave %u bytes (packed %u), hash %08X\n", fe_fr, (unsigned)t8_state_n, (unsigned)pn, t8_hash());
}

static void t8_sload(void)
{
	const int r = snap_load(t8_state, t8_state_n);
	fe_state_loaded(1);
	printf("%6ld sload %d, hash %08X\n", fe_fr, r, t8_hash());
}

static void t8_recstart(void)
{
	t8_start_n = snap_save(t8_start, sizeof t8_start, 1);
	t8_pads.n = t8_pads.frames = 0;
	t8_pads.full = 0;
	t8_rec = 1;
	printf("%6ld recstart (%u bytes), hash %08X\n", fe_fr, (unsigned)t8_start_n, t8_hash());
}

static void t8_recstop(void)
{
	t8_rec = 0;
	t8_end_hash = t8_hash();
	printf("%6ld recstop: %u pads in %u runs, hash %08X\n", fe_fr, (unsigned)t8_pads.frames, (unsigned)t8_pads.n,
		t8_end_hash);
}

/* the replay from its start state, every recorded pad through fe_frame, then the hash against the recording's end;
 * the board back as it was after */
static void t8_play(BrSound *bs, int16_t *aud)
{
	const size_t rn = snap_save(t8_ret, sizeof t8_ret, 1);
	if (snap_load(t8_start, t8_start_n)) {
		printf("%6ld play: the start state was refused\n", fe_fr);
		return;
	}
	fe_playback(1);
	SnapReader rd = { t8_pads.run, t8_pads.n, 0, 0, 0 };
	FePad pad;
	static FeOut o;
	int bad = 0;
	while (snap_pads_next(&rd, &pad)) {
		fe_frame(&pad, &o);
		if (o.run)
			br_run_frame();
		fe_after_board_frame();
		if (o.run) {
			br_sound_take(bs);
			br_sound_mix(bs, aud);
		}
		const uint32_t k = rd.frame - 1;
		uint32_t h[4];
		t8_parts(h);
		if (!bad && k < T8_FRAMES && memcmp(h, t8_fh[k], sizeof h)) {
			bad = 1;
			printf("%6ld play: first difference at replay frame %u: RAM %s VDP %s 68000 %s Z80 %s\n", fe_fr,
				(unsigned)k, h[0] != t8_fh[k][0] ? "diff" : "same", h[1] != t8_fh[k][1] ? "diff" : "same",
				h[2] != t8_fh[k][2] ? "diff" : "same", h[3] != t8_fh[k][3] ? "diff" : "same");
			if (k < T8_RAMS) {
				const uint8_t *a = t8_fram + (size_t)k * BR_RAM_SIZE, *b = br_ram();
				int shown = 0;
				for (int i = 0; i < BR_RAM_SIZE && shown < 40; i++)
					if (a[i] != b[i]) {
						printf("   %06X rec %02X play %02X\n", 0x200000 + i, a[i], b[i]);
						shown++;
					}
			}
		}
	}
	const uint32_t h = t8_hash();
	printf("%6ld play: %u frames, hash %08X: %s\n", fe_fr, (unsigned)rd.frame, h,
		h == t8_end_hash ? "MATCH" : "DIFFERENT");
	fe_playback(0);
	snap_load(t8_ret, rn);
	fe_state_loaded(1);
}

static void t8_auto_edge(const FeOut *out, BrSound *bs, int16_t *aud)
{
	/* (main.c's rule: the credit is over after 60 frames out of it; it flickers out for a frame at a QUIT) */
	if (out->in_credit && !t8_prev_credit && !t8_rec)
		t8_recstart();
	t8_gone = out->in_credit ? 0 : t8_gone + 1;
	if (t8_rec && t8_gone >= 60) {
		t8_recstop();
		t8_play(bs, aud);
	}
	t8_prev_credit = out->in_credit;
}

/* the SAVE / LOAD STATE page's slots, kept in memory as main.c does (picture: the board's last frame, 4 x 4 averaged) */
static struct { int used; char text[32]; uint16_t pic[FE_THUMB_W * FE_THUMB_H]; uint8_t st[SNAP_RAW_MAX]; size_t n; } t8_slot[2][FE_SLOTS];

static void t8_plat_slot(int mode, int i, FeSlot *o)
{
	o->used = t8_slot[mode][i].used;
	snprintf(o->text, sizeof o->text, "%s", t8_slot[mode][i].text);
	o->pic = o->used ? t8_slot[mode][i].pic : NULL;
}

static void t8_request(const FeOut *o)
{
	if (o->req != FE_REQ_SAVE_STATE && o->req != FE_REQ_LOAD_STATE)
		return;
	const int m = o->req_arg / FE_SLOTS, i = o->req_arg % FE_SLOTS;
	if (o->req == FE_REQ_LOAD_STATE) {
		printf("%6ld load slot %d: %d\n", fe_fr, i + 1, snap_load(t8_slot[m][i].st, t8_slot[m][i].n));
		fe_state_loaded(0);
		return;
	}
	static uint32_t pic[BR_W * BR_H];
	vdp_render(br_vdp_snapshot(), pic);
	for (int y = 0; y < FE_THUMB_H; y++)
		for (int x = 0; x < FE_THUMB_W; x++) {
			uint32_t r = 0, g = 0, b = 0;
			for (int k = 0; k < 16; k++) {
				const uint32_t c = pic[(y * 4 + (k >> 2)) * BR_W + x * 4 + (k & 3)];
				r += c >> 16 & 255; g += c >> 8 & 255; b += c & 255;
			}
			t8_slot[m][i].pic[y * FE_THUMB_W + x] = (uint16_t)((r / 16 >> 3) << 11 | (g / 16 >> 2) << 5 | (b / 16 >> 3));
		}
	t8_slot[m][i].n = snap_save(t8_slot[m][i].st, SNAP_RAW_MAX, 0);
	t8_slot[m][i].used = 1;
	snprintf(t8_slot[m][i].text, 32, "ST%d %u", br_rd8(0x20E6A9), (unsigned)(br_rd16(0x204156) << 16 | br_rd16(0x204158)));
	printf("%6ld saved slot %d\n", fe_fr, i + 1);
}

static int fe_run(BrRoms *r, const char *od, long nframes, const char *script)
{
	static uint8_t p0[BR_PRG_SIZE], p1[BR_PRG_SIZE];
	uint8_t *progs[2] = { p0, p1 };
	if (progpatch_make(r->prg, p0, 1) || progpatch_make(r->prg, p1, 0)) {
		fprintf(stderr, "progpatch_make failed\n");
		return 1;
	}
	if (front_init()) {
		fprintf(stderr, "front_init failed\n");
		return 1;
	}
	if (gadgets_init())                 /* test 11: the menus' frames are the gadgets' art */
		fprintf(stderr, "gadgets_init failed\n");
	snprintf(fe_dir, sizeof fe_dir, "%s", getenv("BR_FE_FILES") ? getenv("BR_FE_FILES") : od);
	FePlatform pl = { fe_load, fe_save, fe_log, NULL, t8_plat_slot, NULL };
	/* test 11: the menu sounds as main.c makes them; each also into <outdir>/menusnd<id>.raw (s16 48 kHz mono) */
	const uint64_t ms_t0 = now_ns();
	if (menusnd_render())
		fprintf(stderr, "menusnd_render: out of memory\n");
	printf("menusnd_render: %.1f ms\n", (now_ns() - ms_t0) / 1e6);
	for (int id = 0; id < MS_SOUNDS; id++) {
		char mp[1200];
		snprintf(mp, sizeof mp, "%s/menusnd%d.raw", od, id);
		FILE *mf = fopen(mp, "wb");
		int16_t buf[BR_AUDIO_PER_FRAME];
		for (int f = 0; mf && f < menusnd_frames(id); f++) {
			memset(buf, 0, sizeof buf);
			menusnd_mix(buf, f == 0 ? 1u << id : 0, 256);
			fwrite(buf, 2, BR_AUDIO_PER_FRAME, mf);
		}
		if (mf)
			fclose(mf);
		printf("menusnd %d: %d frames\n", id, menusnd_frames(id));
	}
	br_set_split(1);
	if (fe_init(&pl, progs)) {
		fprintf(stderr, "fe_init failed\n");
		return 1;
	}
	typedef struct { long frame; char op[8]; unsigned a, b; char text[64]; } FeCmd;
	static FeCmd cmd[4096];
	int ncmd = 0;
	FILE *sf = script ? fopen(script, "r") : NULL;
	char line[256];
	while (sf && fgets(line, sizeof line, sf) && ncmd < 4096) {
		FeCmd c = {0};
		int k = 0;
		if (line[0] == '#' || sscanf(line, "%ld %7s %n", &c.frame, c.op, &k) < 2)
			continue;
		sscanf(line + k, "%x %x", &c.a, &c.b);
		snprintf(c.text, sizeof c.text, "%s", line + k);
		c.text[strcspn(c.text, "\r\n")] = 0;
		cmd[ncmd++] = c;
	}
	if (sf)
		fclose(sf);
	char p[1200];
	snprintf(p, sizeof p, "%s/audio.wav", od);
	FILE *wav = fopen(p, "wb");
	uint8_t hdr[44] = {0};
	fwrite(hdr, 1, 44, wav);
	static FeOut out;
	static Vdp vcopy;
	static FeVideoJob jcopy;
	static BrSound bs;
	static int16_t aud[BR_AUDIO_PER_FRAME];
	static uint32_t tex[FRONT_TEX_W * FRONT_TEX_H];
	/* BR_WATCH=<hex address>[,...]: RAM bytes printed when they change */
	unsigned fw[16];
	int nfw = 0, fwl[16];
	for (const char *ws = getenv("BR_WATCH"); ws && *ws && nfw < 16;) {
		fw[nfw] = (unsigned)strtoul(ws, (char **)&ws, 16);
		fwl[nfw++] = -1;
		if (*ws == ',')
			ws++;
	}
	uint16_t held = 0, last = 0;
	long tap_end = -1;
	uint16_t tap_bits = 0;
	for (fe_fr = 0; fe_fr < nframes; fe_fr++) {
		int snap = 0, dump = 0;
		for (int i = 0; i < ncmd; i++) {
			if (cmd[i].frame != fe_fr)
				continue;
			if (!strcmp(cmd[i].op, "pad"))
				held = (uint16_t)cmd[i].a;
			else if (!strcmp(cmd[i].op, "tap")) {
				tap_bits = (uint16_t)cmd[i].a;
				tap_end = fe_fr + (cmd[i].b ? (long)cmd[i].b : 2);
			} else if (!strcmp(cmd[i].op, "snap"))
				snap = 1;
			else if (!strcmp(cmd[i].op, "ram"))
				dump = 1;
			else if (!strcmp(cmd[i].op, "poke"))
				br_wr8(cmd[i].a, (uint8_t)cmd[i].b);
			else if (!strcmp(cmd[i].op, "regs"))
				printf("%6ld PC %06X SR %04X SP %06X A5 %06X\n", fe_fr, br_reg(M68K_REG_PC), br_reg(M68K_REG_SR),
					br_reg(M68K_REG_SP), br_reg(M68K_REG_A5));
			else if (!strcmp(cmd[i].op, "vram")) {
				/* the sprite list (+ tilemaps) as the 68000 left it: <outdir>/v<frame>.bin, big-endian words */
				snprintf(p, sizeof p, "%s/v%05ld.bin", od, fe_fr);
				FILE *vf = fopen(p, "wb");
				for (int k = 0; vf && k < 0x1c00; k++) {
					fputc(vdp.vram[k] >> 8, vf);
					fputc(vdp.vram[k] & 0xff, vf);
				}
				if (vf)
					fclose(vf);
			} else if (!strcmp(cmd[i].op, "pal")) {
				/* test 10: the palette RAM as the board has it: <outdir>/p<frame>.bin, big-endian words */
				snprintf(p, sizeof p, "%s/p%05ld.bin", od, fe_fr);
				FILE *pf = fopen(p, "wb");
				for (int k = 0; pf && k < 0x800; k++) {
					fputc(vdp.pal[k] >> 8, pf);
					fputc(vdp.pal[k] & 0xff, pf);
				}
				if (pf)
					fclose(pf);
			} else if (!strcmp(cmd[i].op, "mark"))
				printf("%6ld ---- %s\n", fe_fr, cmd[i].text);
			/* test 8: ssave / sload (one state in memory), hash, recstart / recstop / play (a replay in memory) */
			else if (!strcmp(cmd[i].op, "ssave"))
				t8_ssave();
			else if (!strcmp(cmd[i].op, "sload"))
				t8_sload();
			else if (!strcmp(cmd[i].op, "hash"))
				printf("%6ld hash %08X\n", fe_fr, t8_hash());
			else if (!strcmp(cmd[i].op, "rstart"))
				t8_recstart();
			else if (!strcmp(cmd[i].op, "rstop"))
				t8_recstop();
			else if (!strcmp(cmd[i].op, "play"))
				t8_play(&bs, aud);
		}
		uint16_t now = held;
		if (fe_fr < tap_end)
			now |= tap_bits;
		{
			/* test 9: the game's buttons through CONTROLS, as main.c's game_bits (no triggers here) */
			static const uint16_t key[6] = { FE_A, FE_B, FE_X, FE_Y, FE_LB, FE_RB };
			unsigned down = 0;
			for (int k = 0; k < 6; k++)
				if (now & key[k])
					down |= 1u << k;
			now |= fe_controls(down);
		}
		FePad pad = { now, (uint16_t)(now & ~last) };
		last = now;
		fe_frame(&pad, &out);
		if (t8_rec && out.rec)
			snap_pads_add(&t8_pads, &pad);
		if (out.run)
			br_run_frame();
		fe_after_board_frame();
		/* BR_HBDEBUG: the sprites (as vdp.c places them) that cover the hitbox disc's centre, with their priority */
		if (getenv("BR_HBDEBUG") && out.job.ndisc && fe_fr % 50 == 0) {
			const Vdp *v = br_vdp_snapshot();
			const uint16_t *src = v->vram + 0x1800;
			const int cx = out.job.disc[0].x >> 7, cy = out.job.disc[0].y >> 7;
			int scx = (int16_t)v->scrollx[6] - 0x1cc, scy = (int16_t)v->scrollx[7] - 0x1ef;
			int ox = (-scx) & 0x1ff, oy = (-scy) & 0x1ff;
			printf("%6ld disc %d,%d r %d:", fe_fr, cx, cy, out.job.disc[0].r);
			for (int o = 0; o < 0x400; o += 4) {
				const uint16_t at = src[o];
				if (!(at & 0x8000))
					continue;
				int w = ((src[o + 2] & 15) + 1) * 8, h = ((src[o + 3] & 15) + 1) * 8, sx, sy;
				if (!(at & 0x4000)) {
					sx = ((src[o + 2] >> 7) - scx) & 0x1ff;
					sy = ((src[o + 3] >> 7) - scy) & 0x1ff;
				} else {
					sx = (ox + (src[o + 2] >> 7)) & 0x1ff;
					sy = (oy + (src[o + 3] >> 7)) & 0x1ff;
				}
				ox = sx;
				oy = sy;
				if (sx >= 0x180) sx -= 0x200;
				if (sy >= 0x180) sy -= 0x200;
				if (cx >= sx - 8 && cx < sx + w + 8 && cy >= sy - 8 && cy < sy + h + 8)
					printf(" [%d pri %X col %02X %dx%d at %d,%d]", o / 4, (at >> 8) & 15, (at >> 2) & 0x3f, w, h, sx, sy);
			}
			printf("\n");
		}
		for (int i = 0; i < nfw; i++) {
			const int v = br_rd8(fw[i]);
			if (v != fwl[i])
				printf("%6ld %06X = %02X\n", fe_fr, fw[i], v);
			fwl[i] = v;
		}
		if (out.run) {
			br_sound_take(&bs);
			br_sound_mix(&bs, aud);
			/* BR_SNDSTAT: each OKI's level (RMS of its frame samples) every 60 frames: which chip carries what */
			static double ok_acc[2];
			for (int c = 0; c < 2 && getenv("BR_SNDSTAT"); c++)
				for (int k = 0; k < bs.on[c]; k++)
					ok_acc[c] += (double)bs.obuf[c][k] * bs.obuf[c][k] / 1e6;
			if (getenv("BR_SNDSTAT") && fe_fr % 60 == 59) {
				printf("%6ld oki0 %.0f oki1 %.0f\n", fe_fr, ok_acc[0], ok_acc[1]);
				ok_acc[0] = ok_acc[1] = 0;
			}
		} else {
			memset(aud, 0, sizeof aud);
		}
		/* BR_RECAUTO: main.c's rule, a replay from each credit's start to its end, played back and compared at once */
		if (t8_rec && out.rec)
			t8_frame_done(t8_pads.frames - 1);
		t8_request(&out);
		if (getenv("BR_RECAUTO"))
			t8_auto_edge(&out, &bs, aud);
		menusnd_mix(aud, out.se, out.se_vol);
		if (out.se)
			printf("%6ld menu sound %X\n", fe_fr, out.se);
		fwrite(aud, 2, BR_AUDIO_PER_FRAME, wav);
		if (snap) {
			memcpy(&vcopy, out.vsrc, sizeof vcopy);
			jcopy = out.job;
			fe_video(&vcopy, &jcopy, tex);
			snprintf(p, sizeof p, "%s/f%05ld.ppm", od, fe_fr);
			fe_ppm(p, tex);
		}
		if (dump) {
			snprintf(p, sizeof p, "%s/r%05ld.bin", od, fe_fr);
			FILE *f = fopen(p, "wb");
			fwrite(br_ram(), 1, BR_RAM_SIZE, f);
			fclose(f);
		}
	}
	long n = nframes * BR_AUDIO_PER_FRAME * 2;
	uint32_t v;
	fseek(wav, 0, SEEK_SET);
	fwrite("RIFF", 1, 4, wav); v = (uint32_t)(36 + n); fwrite(&v, 4, 1, wav);
	fwrite("WAVEfmt ", 1, 8, wav); v = 16; fwrite(&v, 4, 1, wav);
	uint16_t s1 = 1; fwrite(&s1, 2, 1, wav); fwrite(&s1, 2, 1, wav);
	v = BR_AUDIO_RATE; fwrite(&v, 4, 1, wav); v = BR_AUDIO_RATE * 2; fwrite(&v, 4, 1, wav);
	s1 = 2; fwrite(&s1, 2, 1, wav); s1 = 16; fwrite(&s1, 2, 1, wav);
	fwrite("data", 1, 4, wav); v = (uint32_t)n; fwrite(&v, 4, 1, wav);
	fclose(wav);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "harness <romdir> <outdir> <frames> [script]\n");
		return 2;
	}
	const char *rd = argv[1], *od = argv[2];
	long nframes = atol(argv[3]);

	BrRoms r;
	size_t rl = strlen(rd);
	if (rl > 4 && !strcmp(rd + rl - 4, ".zip")) {
		FILE *f = fopen(rd, "rb");
		if (!f) {
			fprintf(stderr, "missing %s\n", rd);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long zl = ftell(f);
		fseek(f, 0, SEEK_SET);
		uint8_t *zip = malloc((size_t)zl);
		if (fread(zip, 1, (size_t)zl, f) != (size_t)zl)
			return 1;
		fclose(f);
		char err[200];
		if (romset_load(zip, (size_t)zl, &r, err, sizeof err)) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}
		free(zip);
	} else {
		static uint8_t prg[BR_PRG_SIZE];
		const char *even[2] = {"prg0b.u22", "prg2.u21"}, *odd[2] = {"prg1b.u23", "prg3.u24"};
		for (int h = 0; h < 2; h++) {
			uint8_t *e = load(rd, even[h], 0x80000), *o = load(rd, odd[h], 0x80000);
			for (int i = 0; i < 0x80000; i++) {
				prg[h * 0x100000 + i * 2] = e[i];
				prg[h * 0x100000 + i * 2 + 1] = o[i];
			}
		}
		static uint8_t gfx[BR_GFX_SIZE];
		const char *g[4] = {"rom-1.bin", "rom-3.bin", "rom-2.bin", "rom-4.bin"};
		for (int i = 0; i < 4; i++) {
			uint8_t *b = load(rd, g[i], 0x400000);
			memcpy(gfx + i * 0x400000, b, 0x400000);
			free(b);
		}
		r = (BrRoms){prg, load(rd, "snd.u77", BR_SND_SIZE), gfx, {load(rd, "rom-5.bin", BR_OKI_SIZE), load(rd, "rom-6.bin", BR_OKI_SIZE)}};
	}
	if (getenv("BR_LAYERS"))
		vdp_layers = (int)strtol(getenv("BR_LAYERS"), NULL, 0);
	if (getenv("BR_VOL")) {
		int y = 256, o = 256;
		sscanf(getenv("BR_VOL"), "%d,%d", &y, &o);
		br_set_volume(y, o);
	}
	if (br_init(&r)) {
		fprintf(stderr, "init failed\n");
		return 1;
	}
	if (getenv("BR_FE"))
		return fe_run(&r, od, nframes, argc > 4 ? argv[4] : NULL);
	/* BR_MODE=orig | chase: the 360's programs (boot check skipped; CHASE!'s patch) */
	if (getenv("BR_MODE")) {
		static uint8_t prg[BR_PRG_SIZE];
		int e = progpatch_make(r.prg, prg, !strcmp(getenv("BR_MODE"), "chase"));
		if (e) {
			fprintf(stderr, "progpatch_make: %d\n", e);
			return 1;
		}
		/* BR_UNPATCH=<hex addr>,<hex len>[;...]: those bytes back to batriderj's (which patch run does what) */
		for (const char *s = getenv("BR_UNPATCH"); s && *s;) {
			char *e;
			unsigned long a = strtoul(s, &e, 16), n = strtoul(e + 1, &e, 16);
			memcpy(prg + a, r.prg + a, n);
			s = *e ? e + 1 : e;
		}
		/* BR_FILL=<hex addr>,<hex len>: those bytes set to 0xFF (is a "free" ROM area read as data?) */
		for (const char *s = getenv("BR_FILL"); s && *s;) {
			char *e;
			unsigned long a = strtoul(s, &e, 16), n = strtoul(e + 1, &e, 16);
			memset(prg + a, 0xff, n);
			s = *e ? e + 1 : e;
		}
		if (getenv("BR_CHASEHUD") && !strcmp(getenv("BR_CHASEHUD"), "vbl")) {
			if (chasehud_patch(prg))
				fprintf(stderr, "chasehud_patch: the vblank site holds other bytes\n");
			br_set_hook(chud_hook);
		}
		br_set_program(prg);
		br_reset();
	}
	/* BR_WATCH=<hex address>[,...]: RAM bytes printed when they change */
	unsigned watch[8];
	int nwatch = 0;
	if (getenv("BR_WATCH"))
		for (const char *s = getenv("BR_WATCH"); *s && nwatch < 8;) {
			watch[nwatch++] = (unsigned)strtoul(s, (char **)&s, 16);
			if (*s == ',')
				s++;
		}
	/* BR_HOLD=<hex address>:<hex byte>[,...]: RAM bytes written before every frame (e.g. 204177:80 keeps P1 in
	 * the blinking after a respawn, so a scripted credit reaches the later stages) */
	unsigned hold[8][2];
	int nhold = 0;
	if (getenv("BR_HOLD"))
		for (const char *s = getenv("BR_HOLD"); *s && nhold < 8;) {
			hold[nhold][0] = (unsigned)strtoul(s, (char **)&s, 16);
			if (*s == ':')
				s++;
			hold[nhold++][1] = (unsigned)strtoul(s, (char **)&s, 16);
			if (*s == ',')
				s++;
		}
	int wlast[8];
	for (int i = 0; i < 8; i++)
		wlast[i] = -1;

	Cmd *cmd = NULL;
	int ncmd = 0;
	if (argc > 4) {
		FILE *f = fopen(argv[4], "r");
		char line[256];
		while (f && fgets(line, sizeof line, f)) {
			Cmd c = {0};
			if (line[0] == '#' || sscanf(line, "%ld %7s %x %x", &c.frame, c.op, &c.a, &c.b) < 2)
				continue;
			cmd = realloc(cmd, sizeof *cmd * (ncmd + 1));
			cmd[ncmd++] = c;
		}
		if (f)
			fclose(f);
	}

	/* BR_SPLIT=1: the 360's split board, run in turn here (must give the same frames and sound); its parts timed */
	const int split = getenv("BR_SPLIT") != NULL;
	static BrSound bs;
	static int16_t aud[BR_AUDIO_PER_FRAME];
	static uint32_t pic[BR_W * BR_H];
	uint64_t t_vdp = 0, t_mix = 0;
	br_set_split(split);
	br_set_clock(now_ns);

	char p[1024];
	snprintf(p, sizeof p, "%s/audio.wav", od);
	FILE *wav = fopen(p, "wb");
	uint8_t hdr[44] = {0};
	fwrite(hdr, 1, 44, wav);
	uint16_t in = 0;
	uint8_t sys = 0;
	clock_t t0 = clock();
	for (long fr = 0; fr < nframes; fr++) {
		int snap = 0, dump = 0;
		for (int i = 0; i < ncmd; i++) {
			if (cmd[i].frame != fr)
				continue;
			if (!strcmp(cmd[i].op, "in"))
				in = (uint16_t)cmd[i].a;
			else if (!strcmp(cmd[i].op, "sys"))
				sys = (uint8_t)cmd[i].a;
			else if (!strcmp(cmd[i].op, "dips"))
				br_set_dips((uint16_t)cmd[i].a, (uint8_t)cmd[i].b);
			else if (!strcmp(cmd[i].op, "poke"))
				br_wr8(cmd[i].a, (uint8_t)cmd[i].b);
			else if (!strcmp(cmd[i].op, "snap"))
				snap = 1;
			else if (!strcmp(cmd[i].op, "ram"))
				dump = 1;
			else if (!strcmp(cmd[i].op, "glyphs"))
				dump = 2;
			else if (!strcmp(cmd[i].op, "hold"))
				br_hold_main((int)cmd[i].a);
			else if (!strcmp(cmd[i].op, "vram")) {
				/* the GP9001's tilemaps + sprites as the 68000 left them: <outdir>/v<frame>.bin, big-endian words */
				char vp[1024];
				snprintf(vp, sizeof vp, "%s/v%ld.bin", od, fr);
				FILE *vf = fopen(vp, "wb");
				for (int k = 0; vf && k < 0x1c00; k++) {
					fputc(vdp.vram[k] >> 8, vf);
					fputc(vdp.vram[k] & 0xff, vf);
				}
				if (vf)
					fclose(vf);
			}
			else if (!strcmp(cmd[i].op, "regs"))
				printf("frame %ld: PC %06X SR %04X SP %06X\n", fr, br_reg(M68K_REG_PC), br_reg(M68K_REG_SR),
					br_reg(M68K_REG_SP));
			else if (!strcmp(cmd[i].op, "title"))
				progpatch_goto_title();
			else if (!strcmp(cmd[i].op, "front") || !strcmp(cmd[i].op, "splash")) {
				/* the 360's title (menu item a, MODE b) or splash from the board's state now: <outdir>/t<frame>.ppm */
				static Vdp tv;
				static uint32_t tp[BR_W * BR_H], tt[FRONT_TEX_W * FRONT_TEX_H];
				static int inited;
				if (!inited && front_init()) {
					fprintf(stderr, "front_init failed\n");
					return 1;
				}
				inited = 1;
				if (cmd[i].op[0] == 'f') {
					memcpy(&tv, &vdp, sizeof tv);
					front_title_text(&tv, (int)cmd[i].a, (int)cmd[i].b, NULL);
					int keep = vdp_layers;
					vdp_layers = 0x1f;
					vdp_render(&tv, tp);
					vdp_layers = keep;
					front_doubled(tp, tt);
				} else {
					front_splash(tt);
				}
				snprintf(p, sizeof p, "%s/t%05ld.ppm", od, fr);
				FILE *f = fopen(p, "wb");
				fprintf(f, "P6\n%d %d\n255\n", FRONT_TEX_W, FRONT_TEX_H);
				for (int k = 0; k < FRONT_TEX_W * FRONT_TEX_H; k++) {
					uint8_t c[3] = {(uint8_t)(tt[k] >> 16), (uint8_t)(tt[k] >> 8), (uint8_t)tt[k]};
					fwrite(c, 1, 3, f);
				}
				fclose(f);
			}
		}
		for (int i = 0; i < nhold; i++)
			br_wr8(hold[i][0], (uint8_t)hold[i][1]);
		br_set_inputs(in, sys);
		br_run_frame();
		/* BR_CHASEHUD=1: CHASE!'s host-drawn HUD parts (top row, boss bar) after each frame, as fe.c does */
		{
			static int chud = -1;
			if (chud < 0)
				chud = getenv("BR_CHASEHUD") && strcmp(getenv("BR_CHASEHUD"), "vbl");
			if (chud)
				chasehud_frame();
		}
		for (int i = 0; i < nwatch; i++) {
			int v = br_rd8(watch[i]);
			if (v != wlast[i])
				printf("frame %ld: %06X = %02X\n", fr, watch[i], v);
			wlast[i] = v;
		}
		const int16_t *fa = br_audio();
		const uint32_t *fp = br_frame();
		if (split) {
			uint64_t t = now_ns();
			br_sound_take(&bs);
			br_sound_mix(&bs, aud);
			uint64_t t1 = now_ns();
			vdp_render(br_vdp_snapshot(), pic);
			t_vdp += now_ns() - t1;
			t_mix += t1 - t;
			fa = aud;
			fp = pic;
			/* BR_SCROLLLOG: the scroll registers each frame's picture used (test 1b's scroll steps) */
			static FILE *sl;
			if (getenv("BR_SCROLLLOG") && (sl || (sl = fopen(getenv("BR_SCROLLLOG"), "w")))) {
				const Vdp *v = br_vdp_snapshot();
				fprintf(sl, "%ld", fr);
				for (int i = 0; i < 8; i++)
					fprintf(sl, " %04X", v->scrollx[i]);
				fputc('\n', sl);
			}
		}
		fwrite(fa, 2, BR_AUDIO_PER_FRAME, wav);
		if (getenv("BR_GADGETS") || getenv("BR_GADGETS_PNG"))
			gad_frame(fr, od, fp);
		if (snap) {
			snprintf(p, sizeof p, "%s/f%05ld.ppm", od, fr);
			FILE *f = fopen(p, "wb");
			fprintf(f, "P6\n%d %d\n255\n", BR_W, BR_H);
			const uint32_t *px = fp;
			for (int i = 0; i < BR_W * BR_H; i++) {
				uint8_t c[3] = {(uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i]};
				fwrite(c, 1, 3, f);
			}
			fclose(f);
		}
		if (dump) {
			snprintf(p, sizeof p, dump == 2 ? "%s/g%05ld.bin" : "%s/r%05ld.bin", od, fr);
			FILE *f = fopen(p, "wb");
			if (dump == 2)
				fwrite(br_text_glyphs(), 1, 1024 * 64, f);
			else
				fwrite(br_ram(), 1, BR_RAM_SIZE, f);
			fclose(f);
		}
	}
	double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
	long n = nframes * BR_AUDIO_PER_FRAME * 2;
	uint32_t v;
	fseek(wav, 0, SEEK_SET);
	fwrite("RIFF", 1, 4, wav); v = (uint32_t)(36 + n); fwrite(&v, 4, 1, wav);
	fwrite("WAVEfmt ", 1, 8, wav); v = 16; fwrite(&v, 4, 1, wav);
	uint16_t s = 1; fwrite(&s, 2, 1, wav); fwrite(&s, 2, 1, wav);
	v = BR_AUDIO_RATE; fwrite(&v, 4, 1, wav); v = BR_AUDIO_RATE * 2; fwrite(&v, 4, 1, wav);
	s = 2; fwrite(&s, 2, 1, wav); s = 16; fwrite(&s, 2, 1, wav);
	fwrite("data", 1, 4, wav); v = (uint32_t)n; fwrite(&v, 4, 1, wav);
	fclose(wav);
	printf("%ld frames in %.2f s (%.1f fps)\n", nframes, secs, nframes / (secs > 0 ? secs : 1));
	BrProfile pr;
	br_profile_take(&pr);
	printf("per frame: 68000 %.0f us, Z80 %.0f us, sound lines%s %.0f us", pr.m68k / 1e3 / nframes,
		pr.z80 / 1e3 / nframes, split ? "" : " + picture", pr.sound / 1e3 / nframes);
	if (split)
		printf(", sound mix %.0f us, picture %.0f us", t_mix / 1e3 / nframes, t_vdp / 1e3 / nframes);
	printf("\n");
	return 0;
}

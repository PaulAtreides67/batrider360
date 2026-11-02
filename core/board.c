/* batrider360: the Batrider board: memory maps, latches, IRQs, the frame's schedule and the sound mix.
 * From MAME's raizing_batrider.cpp (batrider_68k_mem, batrider_sound_z80_mem / _port, the machine config). */
#include "board.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef BR_TRACE
#define TR(...) fprintf(stderr, __VA_ARGS__)
#else
#define TR(...) ((void)0)
#endif

#include "m68k.h"
#include "oki.h"
#include "stio.h"
#include "vdp.h"
#include "ym.h"
#include "z80.h"

enum {
	LINES = 262, CYC68K_LINE = 1024,    /* 16 MHz / (6.75 MHz / 432) */
	Z80_LINE_X3 = 1024,                 /* Z80 cycles a line x3: 32/6 MHz x 64 us = 341.33 */
	OKI_LINE_X5 = 1024,                 /* OKI clocks a line x5: 3.2 MHz x 64 us = 204.8 */
	YM_PER_LINE = 4,                    /* 4 MHz / 64 a sample = 4 a line */
	IRQ_LINE = 230,                     /* gp9001.cpp: "the IRQ appears to fire at line 0xe6" */
	MAXS = 1200,
};

static BrRoms roms;
static uint8_t ram[BR_RAM_SIZE];
static uint8_t zram[0x2000];
static uint16_t in_word;
static uint8_t sys_byte, dswc_byte;
static uint16_t dsw_word;
static uint8_t latch[4];               /* 0,1: 68k -> Z80; 2,3: Z80 -> 68k */
static int irq4, nmi_line, busreq, zbank;
static int vpos;
static int cyc_over;
static uint32_t frames;
/* The tune playing (test 6, GADGETS' Music panel): the 68000's command pair on the latches, 0x20 then nn
 * (0x2E7E): nn 00..19 = a tune (MUSIC TEST's numbers), 0x21 / 0x20,0xE0 = stop; 0x20,nn >= 0x20 = an effect. */
enum { MUSIC_N = 0x1A, MUSIC_NAMES = 0x3E200 };   /* snd.u77: the driver's titles, 24 bytes each, "null." first */
static int music_now = -1;
static uint32_t music_since;
static BrHook hook;
static void audio_reset(void);

static z80 zcpu;
static long z80_due_x3;
static Oki oki[2];
static int oki_phase[2];

static uint32_t picture[BR_W * BR_H];
static int16_t audio[BR_AUDIO_PER_FRAME];
static int32_t ybuf[MAXS], obuf[2][MAXS];
static int ycount, on[2];
/* Volumes (256 = the board's mix). OKI 1 plays the title tune and other music, OKI 0 the voices and effects
 * (test 4, the PC harness), so the host menu's MUSIC = the YM + OKI 1 and EFFECTS = OKI 0. */
static int vol_ym = 256, vol_oki0 = 256, vol_oki1 = 256;

/* Split board (the 360): the YM's sound and the picture are made on other threads. The YM writes are logged with
 * their sample position for br_sound_mix's copy of the chip, and the VDP state is copied at the picture's line. */
static int split;
/* The 68000 held (br_hold_main): the title, the host menu. The Z80, YM and OKIs go on, so the music plays on. */
static int hold_main;
static BrYmWrite ylog[BR_YM_LOG];
static int ylog_n;
static Vdp snap;
static BrClock clock_fn;
static BrProfile prof;

static void update_irq(void)
{
	m68k_set_irq(irq4 ? 4 : vdp.irq ? 2 : 0);
}

static void set_nmi(int s)
{
	if (s && !nmi_line)
		z80_gen_nmi(&zcpu);
	nmi_line = s;
}

/* ---- Z80 ---- */

static uint8_t z_rd(void *u, uint16_t a)
{
	(void)u;
	if (a < 0x8000)
		return roms.snd[a];
	if (a < 0xc000)
		return roms.snd[(zbank << 14) | (a & 0x3fff)];
	if (a < 0xe000)
		return zram[a & 0x1fff];
	return 0;
}

static void z_wr(void *u, uint16_t a, uint8_t d)
{
	(void)u;
	if (a >= 0xc000 && a < 0xe000)
		zram[a & 0x1fff] = d;
}

static uint8_t z_in(z80 *z, uint8_t p)
{
	(void)z;
	switch (p) {
	case 0x48: return latch[0];
	case 0x4a: return latch[1];
	case 0x80: case 0x81: return ym_read(p & 1);
	case 0x82: return oki_read(&oki[0]);
	case 0x84: return oki_read(&oki[1]);
	}
	return 0;
}

/* raizing.cpp raizing_oki_bankswitch_w: two banks a byte, low nibble then high nibble at the next offset. */
static void oki_bank(int offset, uint8_t data)
{
	for (int k = 0; k < 2; k++, offset++, data >>= 4)
		oki[(offset & 4) >> 2].page[offset & 3] = data & 15;
}

static void z_out(z80 *z, uint8_t p, uint8_t d)
{
	(void)z;
	switch (p) {
	case 0x40: latch[2] = d; TR("%u.%d z80>2 %02X\n", frames + 1, vpos, d); return;
	case 0x42: latch[3] = d; TR("%u.%d z80>3 %02X\n", frames + 1, vpos, d); return;
	case 0x44: irq4 = 1; update_irq(); return;
	case 0x46: set_nmi(0); return;
	case 0x80: case 0x81:
		ym_write(p & 1, d);
#ifdef OKI_TRACE
		{
			static uint8_t ya;
			if (!(p & 1))
				ya = d;
			else if (ya == 0x08 && (d & 0x78))
				fprintf(stdout, "%8u ym keyon ch %d ops %X\n", frames + 1, d & 7, d >> 3 & 15);
		}
#endif
		if (split && ylog_n < BR_YM_LOG)
			ylog[ylog_n++] = (BrYmWrite){ (uint16_t)ycount, (uint8_t)(p & 1), d };
		return;
	case 0x82: oki_write(&oki[0], d); return;
	case 0x84: oki_write(&oki[1], d); return;
	case 0x88: zbank = d & 15; return;
	}
	if (p >= 0xc0 && p <= 0xc6)
		oki_bank(p - 0xc0, d);
}

static void z80_run_to(long due_x3)
{
	z80_due_x3 = due_x3;
	if (busreq) {
		zcpu.cyc = (unsigned long)(due_x3 / 3);
		return;
	}
	while ((long)zcpu.cyc * 3 < due_x3)
		z80_step(&zcpu);
}

static void z80_reset(void)
{
	z80_init(&zcpu);
	zcpu.read_byte = z_rd;
	zcpu.write_byte = z_wr;
	zcpu.port_in = z_in;
	zcpu.port_out = z_out;
	zcpu.cyc = (unsigned long)(z80_due_x3 / 3);
}

/* ---- 68000 ---- */

static int hpos(void)
{
	int h = m68k_cycles_run() * 432 / CYC68K_LINE;
	return h > 431 ? 431 : h;
}

static uint16_t vdpcount(void)
{
	int h = hpos();
	int hs = (h > 325 && h < 380);
	int vs = (vpos >= 232 && vpos <= 245);
	uint16_t s = 0xff00;
	int v = (vpos + 15) % 262;
	if (hs)
		s &= ~0x8000;
	if (vs)
		s &= ~0x4000;
	if (hs || vs)
		s &= ~0x0100;
	s |= v < 256 ? (v & 0xff) : 0xff;
	return s;
}

static uint16_t io_read(uint32_t a)
{
	switch (a & 0xfe) {
	case 0x00: return in_word;
	case 0x02: return (uint16_t)(dswc_byte << 8 | sys_byte);
	case 0x04: return dsw_word;
	case 0x06: return vdpcount();
	case 0x08: return latch[2];
	case 0x0a: return latch[3];
	case 0x0c: return (uint16_t)busreq;
	}
	return 0;
}

static void io_write(uint32_t a, uint16_t d, uint16_t mask)
{
	uint32_t o = a & 0xfe;
	int lo = mask & 0xff;
	if (o == 0x20 && lo) {
		latch[0] = (uint8_t)d;
		TR("%u.%d 68k>z0 %02X\n", frames + 1, vpos, latch[0]);
		set_nmi(1);
	} else if (o == 0x22 && lo) {
		latch[1] = (uint8_t)d;
		if (latch[0] == 0x20 && latch[1] < MUSIC_N) {
			music_now = latch[1];
			music_since = frames;
		} else if (latch[0] == 0x21 || (latch[0] == 0x20 && latch[1] == 0xE0)) {
			music_now = -1;
		}
		TR("%u.%d 68k>z1 %02X\n", frames + 1, vpos, latch[1]);
		set_nmi(1);
	} else if (o == 0x26) {
		irq4 = 0;
		update_irq();
	} else if (o == 0x60 && lo) {
		busreq = d & 1;
	} else if (o == 0x80) {
		vdp_text_glyphs(ram);
	} else if (o == 0x82) {
		vdp_text_dma(ram);
	} else if (o >= 0xc0 && o <= 0xce && lo) {
		vdp.bank[(o - 0xc0) >> 1] = d & 15;
	}
}

static uint16_t rd16(uint32_t a)
{
	a &= 0xfffffe;
	if (a < 0x200000)
		return (uint16_t)(roms.prg[a] << 8 | roms.prg[a + 1]);
	if (a < 0x210000)
		return (uint16_t)(ram[a & 0xffff] << 8 | ram[(a & 0xffff) + 1]);
	if (a >= 0x300000 && a < 0x380000)
		return roms.snd[((a - 0x300000) >> 1) & 0x3ffff];
	if (a >= 0x400000 && a < 0x40000e) {
		uint16_t v = vdp_read(a, vpos);
		return v;
	}
	if ((a & 0xffff00) == 0x500000)
		return io_read(a);
	return 0;
}

static void wr16(uint32_t a, uint16_t d, uint16_t mask)
{
	a &= 0xfffffe;
	if (a >= 0x200000 && a < 0x210000) {
		uint32_t o = a & 0xffff;
		if (mask & 0xff00)
			ram[o] = (uint8_t)(d >> 8);
		if (mask & 0x00ff)
			ram[o + 1] = (uint8_t)d;
	} else if (a >= 0x400000 && a < 0x40000e) {
		int was = vdp.irq;
		vdp_write(a, d, mask);
		if (was != vdp.irq)
			update_irq();
	} else if ((a & 0xffff00) == 0x500000) {
		io_write(a, d, mask);
	}
}

unsigned int m68k_read_memory_8(unsigned int a)
{
	uint16_t w = rd16(a);
#ifdef READ_TRACE
	if ((a & 0xffffff) >= 0x7cc0 && (a & 0xffffff) < 0x7cf0)
		fprintf(stdout, "%8u read8 %06X by PC %06X\n", frames + 1, a & 0xffffff, m68k_get_reg(NULL, M68K_REG_PPC));
#endif
	return (a & 1) ? (w & 0xff) : (w >> 8);
}

unsigned int m68k_read_memory_16(unsigned int a) { return rd16(a); }
unsigned int m68k_read_memory_32(unsigned int a) { return (uint32_t)rd16(a) << 16 | rd16(a + 2); }
unsigned int m68k_read_disassembler_8(unsigned int a) { return m68k_read_memory_8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return rd16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_memory_32(a); }

void m68k_write_memory_8(unsigned int a, unsigned int v)
{
	if (a & 1)
		wr16(a, (uint16_t)(v & 0xff), 0x00ff);
	else
		wr16(a, (uint16_t)((v & 0xff) << 8), 0xff00);
}

void m68k_write_memory_16(unsigned int a, unsigned int v) { wr16(a, (uint16_t)v, 0xffff); }

void m68k_write_memory_32(unsigned int a, unsigned int v)
{
	wr16(a, (uint16_t)(v >> 16), 0xffff);
	wr16(a + 2, (uint16_t)v, 0xffff);
}

static void on_reset_instr(void)
{
	/* the 68000's RESET line resets the Z80 (raizing.cpp reset_audiocpu) */
	z80_reset();
	set_nmi(0);
}

static int on_illg(int opcode)
{
	return hook ? hook(opcode) : 0;
}

/* ---- the frame ---- */

int br_init(const BrRoms *r)
{
	roms = *r;
	if (vdp_init(roms.gfx))
		return -1;
	oki_init(&oki[0], roms.oki[0]);
	oki_init(&oki[1], roms.oki[1]);
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_set_reset_instr_callback(on_reset_instr);
	m68k_set_illg_instr_callback(on_illg);
	br_reset();
	return 0;
}

void br_reset(void)
{
	memset(ram, 0, sizeof ram);
	memset(zram, 0, sizeof zram);
	memset(latch, 0, sizeof latch);
	irq4 = nmi_line = busreq = zbank = 0;
	cyc_over = 0;
	vpos = 0;
	frames = 0;
	music_now = -1;
	vdp_reset();
	oki_reset(&oki[0]);
	oki_reset(&oki[1]);
	ym_reset();
	z80_due_x3 = 0;
	z80_reset();
	oki_phase[0] = oki_phase[1] = 0;
	audio_reset();
	m68k_set_irq(0);
	m68k_pulse_reset();
}

void br_set_program(uint8_t *prg) { roms.prg = prg; }
void br_set_hook(BrHook h) { hook = h; }
void br_set_inputs(uint16_t in, uint8_t sys) { in_word = in; sys_byte = sys; }
void br_set_dips(uint16_t dsw, uint8_t dswc) { dsw_word = dsw; dswc_byte = dswc; }
void br_set_volume(int y, int o) { vol_ym = y; vol_oki0 = vol_oki1 = o; }
void br_set_volumes(int y, int o0, int o1) { vol_ym = y; vol_oki0 = o0; vol_oki1 = o1; }

static void sound_line(void)
{
	if (ycount + YM_PER_LINE <= MAXS) {
		if (split)
			ym_tick(YM_PER_LINE);
		else
			ym_generate(ybuf + ycount, YM_PER_LINE);
		ycount += YM_PER_LINE;
	}
	static const int div_x5[2] = {132 * 5, 165 * 5}; /* pin 7 high / low */
	for (int c = 0; c < 2; c++) {
		oki_phase[c] += OKI_LINE_X5;
		int n = oki_phase[c] / div_x5[c];
		oki_phase[c] -= n * div_x5[c];
		if (n && on[c] + n <= MAXS) {
			oki_generate(&oki[c], obuf[c] + on[c], n);
			on[c] += n;
		}
	}
}

/* Band-limited resampling: the frame's n input samples spread over its BR_AUDIO_PER_FRAME outputs through a
 * Blackman-windowed sinc (TAPS input samples each side, cutoff fc of the input's Nyquist), from a table of PHASES
 * fractional positions; TAPS input samples of latency, so the filter never reaches past the frame's last sample. */
enum { TAPS = 24, PHASES = 256, HIST = 2 * TAPS };

typedef struct {
	float fc;
	float kern[PHASES + 1][2 * TAPS];  /* kern[ph][j]: weight of x[i0 - TAPS + 1 + j] at fraction ph / PHASES */
	float x[HIST + MAXS];              /* HIST samples of history, then the frame's input */
} Rs;

static Rs rs[3];

static void rs_init(Rs *r, float fc)
{
	memset(r, 0, sizeof *r);
	r->fc = fc;
	for (int ph = 0; ph <= PHASES; ph++) {
		float f = (float)ph / PHASES, sum = 0;
		for (int j = 0; j < 2 * TAPS; j++) {
			float d = f + (TAPS - 1) - j;    /* distance from the output position to this input sample */
			float t = fc * d, s = t == 0 ? 1.0f : sinf(3.14159265f * t) / (3.14159265f * t);
			float w = 0.42f + 0.5f * cosf(3.14159265f * d / TAPS) + 0.08f * cosf(2 * 3.14159265f * d / TAPS);
			r->kern[ph][j] = fabsf(d) >= TAPS ? 0 : s * w;
			sum += r->kern[ph][j];
		}
		for (int j = 0; j < 2 * TAPS; j++)
			r->kern[ph][j] /= sum;            /* unity gain at DC */
	}
}

static void audio_reset(void)
{
	rs_init(&rs[0], 0.5f);               /* YM2151 62.5 kHz -> 48 kHz: cut at ~15.6 kHz (MAME has ~0.1% of its energy above 16 kHz) */
	rs_init(&rs[1], 0.9f);               /* OKIs ~24 / ~19 kHz up to 48 kHz: their own band */
	rs_init(&rs[2], 0.9f);
}

static void resample_add(Rs *r, float *dst, const int32_t *in, int n, float scale)
{
	if (!n)
		return;
	for (int i = 0; i < n; i++)
		r->x[HIST + i] = (float)in[i];
	for (int k = 0; k < BR_AUDIO_PER_FRAME; k++) {
		/* position in x: in[i] sits at HIST + i; output k at (k + 1) * n / 800 samples into the frame, TAPS late */
		float q = HIST - 1 - TAPS + (float)(k + 1) * n / BR_AUDIO_PER_FRAME;
		int i0 = (int)q;
		int ph = (int)((q - i0) * PHASES + 0.5f);
		const float *kv = r->kern[ph], *xv = r->x + i0 - TAPS + 1;
		float acc = 0;
		for (int j = 0; j < 2 * TAPS; j++)
			acc += xv[j] * kv[j];
		dst[k] += acc * scale;
	}
	memmove(r->x, r->x + n, HIST * sizeof r->x[0]);
}

/* ym_generate: (L+R)/4 in 16-bit units; OKI: signal x volume in units of 1/65536, 0.5 each on the board */
static void mix_out(const int32_t *y, int yn, const int32_t *o0, int n0, const int32_t *o1, int n1, int16_t *out)
{
	float mix[BR_AUDIO_PER_FRAME] = {0};
	resample_add(&rs[0], mix, y, yn, vol_ym / 256.0f / 32768.0f);
	resample_add(&rs[1], mix, o0, n0, vol_oki0 / 256.0f * 0.5f / 65536.0f);
	resample_add(&rs[2], mix, o1, n1, vol_oki1 / 256.0f * 0.5f / 65536.0f);
	for (int k = 0; k < BR_AUDIO_PER_FRAME; k++) {
		float v = mix[k] * 32767.0f;
		out[k] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
	}
}

void br_run_frame(void)
{
	ycount = on[0] = on[1] = 0;
	ylog_n = 0;
	uint64_t t = 0, t1;
	if (clock_fn)
		t = clock_fn();
	for (vpos = 0; vpos < LINES; vpos++) {
		if (vpos == IRQ_LINE && frames && !hold_main) {
			vdp.irq = 1;
			update_irq();
		}
		/* the last instruction's overshoot is owed by the next line */
		if (!hold_main) {
			int want = CYC68K_LINE - cyc_over;
			cyc_over = m68k_execute(want > 0 ? want : 1) - want;
		}
		if (clock_fn) {
			t1 = clock_fn();
			prof.m68k += t1 - t;
			t = t1;
		}
		z80_run_to(z80_due_x3 + Z80_LINE_X3);
		if (clock_fn) {
			t1 = clock_fn();
			prof.z80 += t1 - t;
			t = t1;
		}
		sound_line();
		if (vpos == BR_H - 1) {
			if (split)
				memcpy(&snap, &vdp, sizeof snap);
			else
				vdp_render(&vdp, picture);
		}
		if (clock_fn) {
			t1 = clock_fn();
			prof.sound += t1 - t;
			t = t1;
		}
	}
	long over_x3 = (long)zcpu.cyc * 3 - z80_due_x3;   /* the last instruction's overshoot carries into the next frame */
	z80_due_x3 = 0;
	zcpu.cyc = (unsigned long)(over_x3 > 0 ? over_x3 / 3 : 0);

	if (!split)
		mix_out(ybuf, ycount, obuf[0], on[0], obuf[1], on[1], audio);
	frames++;
}

/* Test 8, save states: the board between two frames. The Z80's and the OKIs' structs keep their callbacks / ROM
 * pointers from the live ones; the picture's copy (split board) becomes the loaded VDP, so a paused game shows it. */
void br_state_io(Stio *s)
{
	stio(s, ram, sizeof ram);
	stio(s, zram, sizeof zram);
	STIO(s, in_word);
	STIO(s, sys_byte);
	STIO(s, dswc_byte);
	STIO(s, dsw_word);
	STIO(s, latch);
	STIO(s, irq4);
	STIO(s, nmi_line);
	STIO(s, busreq);
	STIO(s, zbank);
	STIO(s, cyc_over);
	STIO(s, frames);
	STIO(s, music_now);
	STIO(s, music_since);
	STIO(s, hold_main);
	z80 z = zcpu;
	STIO(s, z);
	STIO(s, z80_due_x3);
	Oki o[2] = { oki[0], oki[1] };
	STIO(s, o);
	STIO(s, oki_phase);
	STIO(s, vdp);
	m68k_state_io(s);
	ym_state_io(s);
	if (!s->load || s->err)
		return;
	z.read_byte = zcpu.read_byte;
	z.write_byte = zcpu.write_byte;
	z.port_in = zcpu.port_in;
	z.port_out = zcpu.port_out;
	z.userdata = zcpu.userdata;
	zcpu = z;
	for (int c = 0; c < 2; c++) {
		o[c].rom = oki[c].rom;
		oki[c] = o[c];
	}
	memcpy(&snap, &vdp, sizeof snap);
	vpos = 0;
	update_irq();
}

void br_set_split(int on_) { split = on_; }
void br_hold_main(int on_) { hold_main = on_; }
const Vdp *br_vdp_snapshot(void) { return &snap; }

void br_sound_take(BrSound *s)
{
	s->ycount = ycount;
	s->ylog_n = ylog_n;
	memcpy(s->ylog, ylog, (size_t)ylog_n * sizeof ylog[0]);
	s->on[0] = on[0];
	s->on[1] = on[1];
	memcpy(s->obuf[0], obuf[0], (size_t)on[0] * sizeof obuf[0][0]);
	memcpy(s->obuf[1], obuf[1], (size_t)on[1] * sizeof obuf[1][0]);
}

/* The sound thread: chip 1 replays the frame's writes at their sample positions, then the same mix as br_run_frame's. */
void br_sound_mix(const BrSound *s, int16_t *out)
{
	static int32_t y[MAXS];
	int pos = 0;
	for (int i = 0; i <= s->ylog_n; i++) {
		int until = i < s->ylog_n ? s->ylog[i].pos : s->ycount;
		if (until > pos) {
			ym2_generate(y + pos, until - pos);
			pos = until;
		}
		if (i < s->ylog_n)
			ym2_write(s->ylog[i].port, s->ylog[i].data);
	}
	mix_out(y, s->ycount, s->obuf[0], s->on[0], s->obuf[1], s->on[1], out);
}

void br_set_clock(BrClock c) { clock_fn = c; }
void br_profile_take(BrProfile *p)
{
	*p = prof;
	memset(&prof, 0, sizeof prof);
}

const uint32_t *br_frame(void) { return picture; }
const int16_t *br_audio(void) { return audio; }
uint8_t *br_ram(void) { return ram; }
uint32_t br_frame_count(void) { return frames; }
int br_music(void)
{
	/* the two jingles stop by themselves: stage clear 0x11 (~5 s), game over 0x12 (~4 s) */
	if ((music_now == 0x11 && frames - music_since > 330) || (music_now == 0x12 && frames - music_since > 270))
		music_now = -1;
	return music_now;
}
int br_music_title(int n, char *out, int size)
{
	out[0] = 0;
	if (n < 0 || n >= MUSIC_N || size < 2 || !roms.snd)
		return 0;
	const uint8_t *t = roms.snd + MUSIC_NAMES + (n + 1) * 24;   /* the list's entry 0 is "null." */
	int len = 0;
	for (int i = 0; i < 24 && t[i] >= 0x20 && t[i] < 0x7F && len < size - 1; i++)
		out[len++] = (char)t[i];
	while (len && out[len - 1] == ' ')
		len--;
	out[len] = 0;
	return len;
}
const uint8_t *br_text_glyphs(void) { return vdp.txgfx; }
uint8_t *br_zram_for_tests(void) { return zram; }   /* the PC harness's replay checks */
uint32_t br_reg(int which) { return m68k_get_reg(NULL, (m68k_register_t)which); }
void br_set_reg(int which, uint32_t v) { m68k_set_reg((m68k_register_t)which, v); }
uint8_t br_rd8(uint32_t a) { return (uint8_t)m68k_read_memory_8(a); }
uint16_t br_rd16(uint32_t a) { return rd16(a); }
void br_wr8(uint32_t a, uint8_t v) { m68k_write_memory_8(a, v); }
void br_wr16(uint32_t a, uint16_t v) { m68k_write_memory_16(a, v); }

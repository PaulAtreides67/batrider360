/* batrider360: OKIM6295 (4 ADPCM voices), written from MAME's okim6295.cpp / okiadpcm.cpp (ref/mame).
 * The ROM is read through Raizing's GAL banking (raizing.cpp raizing_oki_bankswitch_w / install_raizing_okibank):
 * the phrase table's four 0x100-byte quarters and the four 64 KB windows each take one of 16 pages of the 1 MB ROM. */
#include "oki.h"

#include <math.h>
#include <string.h>

static int diff_lookup[49 * 16];
static const int8_t index_shift[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
/* /0x20: 0 dB .. -24 dB, then silence */
static const int volume_table[16] = {0x20, 0x16, 0x10, 0x0b, 0x08, 0x06, 0x04, 0x03, 0x02, 0, 0, 0, 0, 0, 0, 0};

static void tables(void)
{
	static const int8_t nbl2bit[16][4] = {
		{1, 0, 0, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}, {1, 0, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 1}, {1, 1, 1, 0}, {1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1}, {-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}};
	for (int step = 0; step <= 48; step++) {
		int stepval = (int)floor(16.0 * pow(11.0 / 10.0, (double)step));
		for (int nib = 0; nib < 16; nib++)
			diff_lookup[step * 16 + nib] = nbl2bit[nib][0] *
				(stepval * nbl2bit[nib][1] + stepval / 2 * nbl2bit[nib][2] + stepval / 4 * nbl2bit[nib][3] + stepval / 8);
	}
}

void oki_init(Oki *o, const uint8_t *rom)
{
	static int done;
	if (!done) {
		tables();
		done = 1;
	}
	memset(o, 0, sizeof *o);
	o->rom = rom;
	o->command = -1;
}

void oki_reset(Oki *o)
{
	for (int v = 0; v < 4; v++)
		o->voice[v].playing = 0;
	memset(o->page, 0, sizeof o->page);
	o->command = -1;
}

static uint8_t rd(const Oki *o, uint32_t a)
{
	a &= 0x3ffff;
	uint32_t page = a < 0x400 ? o->page[a >> 8] : o->page[a >> 16];
	return o->rom[(page << 16) | (a & 0xffff)];
}

uint8_t oki_read(const Oki *o)
{
	uint8_t r = 0xf0;
	for (int v = 0; v < 4; v++)
		if (o->voice[v].playing)
			r |= 1 << v;
	return r;
}

void oki_write(Oki *o, uint8_t command)
{
	if (o->command != -1) {
		int mask = command >> 4;
		for (int v = 0; v < 4; v++, mask >>= 1) {
			if (!(mask & 1))
				continue;
			OkiVoice *vo = &o->voice[v];
			if (vo->playing)
				continue;
			uint32_t base = (uint32_t)o->command * 8;
			uint32_t start = (rd(o, base) << 16 | rd(o, base + 1) << 8 | rd(o, base + 2)) & 0x3ffff;
			uint32_t stop = (rd(o, base + 3) << 16 | rd(o, base + 4) << 8 | rd(o, base + 5)) & 0x3ffff;
			if (start < stop) {
#ifdef OKI_TRACE
				{ extern void oki_trace(const Oki *, int, int, uint32_t, uint32_t, int); oki_trace(o, v, o->command, start, stop, command & 15); }
#endif
				vo->playing = 1;
				vo->base = start;
				vo->sample = 0;
				vo->count = 2 * (stop - start + 1);
				vo->signal = 0;
				vo->step = 0;
				vo->volume = volume_table[command & 15];
			}
		}
		o->command = -1;
	} else if (command & 0x80) {
		o->command = command & 0x7f;
	} else {
		int mask = command >> 3;
		for (int v = 0; v < 4; v++, mask >>= 1)
			if (mask & 1)
				o->voice[v].playing = 0;
	}
}

/* n samples, each the sum of the voices in units of 1/(0x20 * 2048) (MAME's add_int(.., 2048) with the /0x20 table). */
void oki_generate(Oki *o, int32_t *dst, int n)
{
	memset(dst, 0, n * sizeof *dst);
	for (int v = 0; v < 4; v++) {
		OkiVoice *vo = &o->voice[v];
		for (int i = 0; i < n && vo->playing; i++) {
			int nibble = (rd(o, vo->base + vo->sample / 2) >> (((vo->sample & 1) << 2) ^ 4)) & 15;
			vo->signal += diff_lookup[vo->step * 16 + nibble];
			if (vo->signal > 2047)
				vo->signal = 2047;
			else if (vo->signal < -2048)
				vo->signal = -2048;
			vo->step += index_shift[nibble & 7];
			if (vo->step > 48)
				vo->step = 48;
			else if (vo->step < 0)
				vo->step = 0;
			dst[i] += vo->signal * vo->volume;
			if (++vo->sample >= vo->count)
				vo->playing = 0;
		}
	}
}
